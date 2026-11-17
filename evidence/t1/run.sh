#!/bin/bash
# iPhoneDuo T1: stock epsilon FD build, runs inside iphoneduo-t1-builder:1
set -euo pipefail
step(){ echo; echo "##### T1-STEP: $* ($(date -u +%FT%TZ))"; }
step toolchain
uname -a; clang --version; lld-link --version || true; ld.lld --version; aarch64-linux-gnu-gcc --version | head -1
python3 --version; mono --version | head -1; nuget help 2>/dev/null | head -1 || true; git --version
step mono-cert-sync
cert-sync /etc/ssl/certs/ca-certificates.crt 2>&1 | tail -2
step fetch
bash /builder/fetch.sh
cd /work/mu_andromeda_platforms
step pip
pip install --upgrade -r pip-requirements.txt
pip freeze | grep -iE 'edk2|fdt|setuptools' || true
export BOARD=SurfaceDuo1
step stuart-setup
for i in 1 2 3; do python3 ./Platforms/AndromedaPkg/PlatformBuild.py --setup -t RELEASE && break; echo "setup attempt $i failed"; [ $i = 3 ] && exit 21; sleep 15; done
step stuart-update
for i in 1 2 3; do python3 ./Platforms/AndromedaPkg/PlatformBuild.py --update -t RELEASE && break; echo "update attempt $i failed"; [ $i = 3 ] && exit 22; sleep 15; done
step submodule-status
git rev-parse HEAD; git submodule status --recursive
step build
bash -e build_fd_epsilon.sh
step result
ls -l ImageResources/Epsilon/SM8150_EFI.fd
stat -c 'SIZE_BYTES=%s' ImageResources/Epsilon/SM8150_EFI.fd
sha256sum ImageResources/Epsilon/SM8150_EFI.fd
ls -la Build/SurfaceDuo1Pkg/RELEASE_CLANGPDB/FV/ | head -80
echo "##### T1-DONE"
