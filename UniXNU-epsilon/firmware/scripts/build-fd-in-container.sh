#!/bin/bash
# iPhoneDuo T3: epsilon FD with UniXNU embedded. Runs inside iphoneduo-t1-builder:1.
# Host build only; produces an FD and FV reports for size measurement.
#
# Mounts expected (see run-docker.sh):
#   /t1          ro  volume iphoneduo_t1_work (T1 tree: submodules + ext_deps)
#   /t3          rw  volume iphoneduo_t3_work (this build)
#   /in/unixnu   ro  UniXNU checkout @21d683dff6 (with drivers/DarwinBoot)
#   /in/firmware ro  this firmware/ directory
#   /out         rw  results
#
# Bring-up policy (porting-reviewer findings, Phase 1):
#   M3  UNIXNU_BOOT_OPTION defaults to 0: UniXNUDxe is not built and the stock
#       BootOrder is untouched. 1 is an opt-in for a supervised test only.
#   M1  The UniXNULib copy is board-patched (../patches/unixnu/) so config is
#       read only from the EpsilonPayloadDxe volume and block_slice is refused;
#       lint-epsilon-config.py gates the payload config.
#   M2  Packaging (PACKAGE_NOSB=1) is NoSb only. See the packaging step.
#   M4  mtools pinned; pip installs the pinned requirements without --upgrade.
set -euo pipefail
step(){ echo; echo "##### T3-STEP: $* ($(date -u +%FT%TZ))"; }
: "${UNIXNU_BOOT_OPTION:=0}" "${UNIXNU_FV_PAYLOAD:=1}" "${PAYLOAD_KIB:=128}" "${PACKAGE_NOSB:=0}" "${EPSILON_HANDOFF_PROBE:=0}"
MTOOLS_VERSION=4.0.43-1build1   # Ubuntu 24.04 noble/main, release pocket
export MONO_ENV_OPTIONS=--interpreter

step mono-cert-sync
cert-sync /etc/ssl/certs/ca-certificates.crt 2>&1 | tail -1

step tree
W=/t3/mu_andromeda_platforms
if [ ! -d "$W/.git" ]; then
  mkdir -p "$W"
  tar -C /t1/mu_andromeda_platforms --exclude=./Build -cf - . | tar -C "$W" -xf -
fi
cd "$W"
git config --global --add safe.directory '*'
git rev-parse HEAD
git checkout -q -- Platforms/SurfaceDuo1Pkg/SurfaceDuo1.dsc Platforms/SurfaceDuo1Pkg/SurfaceDuo1.fdf Platforms/AndromedaPkg/PlatformBuild.py
git apply --check /in/firmware/patches/0001-SurfaceDuo1Pkg-embed-UniXNU.patch
git apply /in/firmware/patches/0001-SurfaceDuo1Pkg-embed-UniXNU.patch
git checkout -q -- BootConfig/BootConfigShim.S Platforms/AndromedaPkg/PrePi/PrePi.c \
  Platforms/AndromedaPkg/PrePi/PeiUniCore.inf \
  Platforms/AndromedaPkg/Library/ArmPlatformLib/AArch64/ArmPlatformHelper.S
rm -f Platforms/AndromedaPkg/PrePi/{EpsilonFdtProbe,EpsilonFdtProbeResult}.h
case "$EPSILON_HANDOFF_PROBE" in
  0) ;;
  1|2)
    step handoff-probe
    git apply --check /in/firmware/patches/m2-probe/0001-capture-bootloader-x0.patch
    git apply /in/firmware/patches/m2-probe/0001-capture-bootloader-x0.patch
    ;;
  *) echo "EPSILON_HANDOFF_PROBE must be 0, 1, or 2" >&2; exit 2 ;;
esac
if [ "$EPSILON_HANDOFF_PROBE" = 2 ]; then
  step fdt-probe
  git apply --check /in/firmware/patches/m2-probe/0003-inspect-fdt-before-dxe.patch
  git apply /in/firmware/patches/m2-probe/0003-inspect-fdt-before-dxe.patch
  cp /in/firmware/patches/m2-probe/{EpsilonFdtProbe,EpsilonFdtProbeResult}.h Platforms/AndromedaPkg/PrePi/
fi
git status --short | grep -v '^??' || true

step packages
P=/t3/pkgs; rm -rf "$P"; mkdir -p "$P/unixnu/package" "$P/unixnu/drivers/DarwinBoot" "$P/firmware"
cp -a /in/unixnu/package/UniXNUPkg "$P/unixnu/package/"
cp -a /in/unixnu/drivers/DarwinBoot/DarwinBootPkg "$P/unixnu/drivers/DarwinBoot/"
cp -a /in/firmware/EpsilonUniXNUPkg "$P/firmware/"
if [ "$EPSILON_HANDOFF_PROBE" != 0 ]; then
  git -C "$P/firmware" apply --check /in/firmware/patches/m2-probe/0002-display-bootloader-x0.patch
  git -C "$P/firmware" apply /in/firmware/patches/m2-probe/0002-display-bootloader-x0.patch
fi
if [ "$EPSILON_HANDOFF_PROBE" = 2 ]; then
  git -C "$P/firmware" apply --check /in/firmware/patches/m2-probe/0004-display-fdt-findings.patch
  git -C "$P/firmware" apply /in/firmware/patches/m2-probe/0004-display-fdt-findings.patch
  cp /in/firmware/patches/m2-probe/EpsilonFdtProbeResult.h "$P/firmware/EpsilonUniXNUPkg/Application/EpsilonDiag/"
fi
# M1 board patch on the build copy only; /in/unixnu stays read-only and unmodified.
for pt in /in/firmware/patches/unixnu/*.patch; do
  echo "unixnu board patch: $(basename "$pt")"
  git -C "$P/unixnu" apply --check "$pt"
  git -C "$P/unixnu" apply "$pt"
done
grep -q IsEpsilonPayloadVolume "$P/unixnu/package/UniXNUPkg/Library/UniXNULib/Config.c"
export UNIXNU_PACKAGES_PATH="$P/unixnu/package:$P/unixnu/drivers/DarwinBoot:$P/firmware"

step diag-host-test
# T5: the diagnostic's decode/format code, compiled natively. Stops the build on a failure.
bash /in/firmware/scripts/test-epsilon-diag.sh /t3/diag-host-test
if [ "$UNIXNU_BOOT_OPTION" = 1 ]; then
  echo "BUILD KIND: SUPERVISED DIAGNOSTIC (UNIXNU_BOOT_OPTION=1: UniXNU first in BootOrder, default entry = EpsilonDiag)"
else
  echo "BUILD KIND: default safe build (UNIXNU_BOOT_OPTION=0: stock BootOrder; UniXNU and EpsilonDiag embedded but never started)"
fi

step payload
if [ "$UNIXNU_FV_PAYLOAD" = 1 ]; then
  if [ "$(dpkg-query -W -f='${Version}' mtools 2>/dev/null)" != "$MTOOLS_VERSION" ]; then
    apt-get update -qq >/dev/null; apt-get install -y -qq "mtools=$MTOOLS_VERSION" >/dev/null
  fi
  echo "mtools $(dpkg-query -W -f='${Version}' mtools)"
  ST=/t3/payload-stage; rm -rf "$ST"; mkdir -p "$ST/EFI/UniXNU"
  cp "$P/firmware/EpsilonUniXNUPkg/Payload/unixnu.toml" "$ST/EFI/UniXNU/unixnu.toml"
  python3 -I /in/firmware/scripts/lint-epsilon-config.py /in/firmware --stage "$ST"
  bash /in/firmware/scripts/make-fv-payload.sh "$ST" "$P/firmware/EpsilonUniXNUPkg/Payload/payload.fat" "$PAYLOAD_KIB"
fi

step pip
pip install -q -r pip-requirements.txt   # pinned file, no --upgrade (M4)
pip freeze 2>/dev/null | sha256sum | sed "s/^/pip-freeze-sha256 /"
export BOARD=SurfaceDuo1

step stuart-update
for i in 1 2 3; do python3 ./Platforms/AndromedaPkg/PlatformBuild.py --update -t RELEASE && break; echo "update attempt $i failed"; [ $i = 3 ] && exit 22; sleep 15; done

step build
python3 ./Platforms/AndromedaPkg/PlatformBuild.py TARGET=RELEASE \
  BLD_*_UNIXNU_BOOT_OPTION=$UNIXNU_BOOT_OPTION BLD_*_UNIXNU_FV_PAYLOAD=$UNIXNU_FV_PAYLOAD

step result
B=Build/SurfaceDuo1Pkg/RELEASE_CLANGPDB
stat -c 'FD_SIZE_BYTES=%s' $B/FV/SM8150_EFI.fd
sha256sum $B/FV/SM8150_EFI.fd
cat $B/FV/FVMAIN_COMPACT.Fv.txt | head -8
cat $B/FV/FVMAIN.Fv.txt | head -8
stat -c 'FVMAIN_UNCOMPRESSED_BYTES=%s' $B/FV/FVMAIN.Fv
echo "--- added module images"
find $B/AARCH64 \( -path '*UniXNUPkg*' -o -path '*DarwinBootPkg*' -o -path '*EpsilonUniXNUPkg*' \) -path '*/OUTPUT/*.efi' -exec stat -c '%s %n' {} +
echo "--- added FFS files"
for g in 9B665204-A133-47A2-9396-9E2B370A4E25 29B950CF-586D-49EA-9D71-F88645CCDAF6 1B64AF87-8415-4B57-9B16-680D70913B9F 8EE9E414-6E7A-4ED2-9A6F-D1B048317D1B E59FE912-1804-4BB0-B27E-DAE17C9A703A F98C36DD-FAF0-4DC4-BFDB-A98F136DB804; do
  find $B/FV/Ffs -iname "$g*.ffs" -exec stat -c '%s %n' {} + || true
done
mkdir -p /out/FV
cp $B/FV/SM8150_EFI.fd /out/
cp $B/FV/FVMAIN_COMPACT.Fv.txt $B/FV/FVMAIN_COMPACT.Fv.map $B/FV/FVMAIN_COMPACT.inf $B/FV/FVMAIN.Fv.txt $B/FV/FVMAIN.Fv.map $B/FV/FVMAIN.inf /out/FV/
cp Build/BUILDLOG_SurfaceDuo1Pkg.txt /out/ 2>/dev/null || true
# T5: the diagnostic image itself, for inspection (imports, size, strings).
find $B/AARCH64/EpsilonUniXNUPkg/Application/EpsilonDiag -path '*/OUTPUT/EpsilonDiag.efi' -exec cp {} /out/ \; 2>/dev/null || true

step packaging-policy
# M2: bring-up packaging is NoSb ONLY. The stock build_appendable_payload_epsilon.sh
# emits both UefiPlatEpsilon.bin (SecureBootEnabled = 1) and UefiPlatEpsilonNoSb.bin;
# it is deliberately not called. With Secure Boot on, the FV-embedded UniXNU is
# exempt from image verification (FV images are ALWAYS_EXECUTE), so a UniXNU that
# reads config from a FAT volume would let unsigned content steer a boot that the
# signed chain was meant to gate. That weakens the existing DuoWOA install
# (CLAUDE.md section 5). M1 narrows the config source, but the policy stays: never
# package this FD with UefiPlatEpsilon.cfg.
CFG=BootConfig/UefiPlatEpsilonNoSb.cfg
[ "${UEFI_CONFIG:-$CFG}" = "$CFG" ] || { echo "REFUSED: UEFI_CONFIG=$UEFI_CONFIG; bring-up packaging is $CFG only" >&2; exit 30; }
grep -Eq '^SecureBootEnabled[[:space:]]*=[[:space:]]*0[[:space:]]*$' "$CFG" || { echo "REFUSED: $CFG does not set SecureBootEnabled = 0" >&2; exit 31; }
if [ "$PACKAGE_NOSB" = 1 ]; then
  CS=$(wc -c < "$CFG")
  make -B -C ./BootConfig UEFI_FD_BASE=0x9FC01000 UEFI_FD_SIZE=0x002EF000 BOOT_CONFIG_SHIM_SIZE=0x1000 BOOT_CONFIG_SIZE="$CS" BootConfigShim.bin
  bash ./BootConfig/build_appendable_payload.sh $B/FV/SM8150_EFI.fd ./BootConfig/BootConfigShim.bin "$CFG" /out/UefiPlatEpsilonNoSb.bin 0x002EF000 0x00300000
  sha256sum /out/UefiPlatEpsilonNoSb.bin
  echo "packaged NoSb only (host artifact; must not be flashed)"
else
  echo "packaging skipped (PACKAGE_NOSB=0); when enabled it builds $CFG only"
fi
echo "##### T3-DONE"
