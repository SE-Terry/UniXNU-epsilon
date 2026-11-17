# epsilon firmware integration (Phase 1 T3, T5)

Embeds UniXNU, UniXNUDxe, DarwinBoot, an FV-carried payload volume and the T5
read-only diagnostic into the stock Surface Duo 1 firmware (`WOA-Project/mu_andromeda_platforms@613e2eac87`,
`Platforms/SurfaceDuo1Pkg`).

**Status: host build only. Device not contacted. Nothing here has been booted.**
A compile is not a boot. Every hardware-facing statement below is marked `V-src`
(read in pinned source), `V-host` (measured in the host build) or `UNVERIFIED`.

**Two builds come out of this directory. Do not confuse them.**

| | Default safe build | Supervised diagnostic build (T5) |
| --- | --- | --- |
| Switch | `UNIXNU_BOOT_OPTION=0` (default) | `UNIXNU_BOOT_OPTION=1 PACKAGE_NOSB=1` |
| `UniXNUDxe` | not built | built; puts UniXNU first in `BootOrder` on every boot of this image |
| What starts UniXNU | nothing | BDS, from the boot option |
| What then runs | nothing; stock boot order, DuoWOA | UniXNU menu, 5 s countdown, then the diagnostic (`EpsilonDiag`) |
| `EpsilonDiag` in the FD | yes, never started | yes, default entry |
| Intended use | any image that might be left on the device | one supervised RAM-only boot with a person watching, after explicit user approval |

The payload `unixnu.toml` is the same file in both. Its default entry is the diagnostic,
which only matters when UniXNU runs, so the difference between the builds is the one
switch.

## Contents

| Path | Purpose |
| --- | --- |
| `patches/0001-SurfaceDuo1Pkg-embed-UniXNU.patch` | The only edits to the firmware tree: two `!include` lines, two switches, one env-driven packages path |
| `EpsilonUniXNUPkg/EpsilonUniXNU.dsc.inc` | DSC hooks (library class, PCD, components) |
| `EpsilonUniXNUPkg/EpsilonUniXNU.fdf.inc` | FDF hooks (FvMain file list) |
| `EpsilonUniXNUPkg/Driver/EpsilonPayloadDxe/` | Publishes the FV payload FAT image as a read-only block device |
| `EpsilonUniXNUPkg/Application/EpsilonDiag/` | T5 read-only diagnostic: `EpsilonDiag.c` (UEFI glue), `DiagCore.c/.h` (decode, formatting, font; no UEFI calls), `AArch64/SysReg.S` (seven `MRS` reads) |
| `EpsilonUniXNUPkg/Payload/unixnu.toml` | Config: default entry is the diagnostic as `fv:<FILE_GUID>`; the rest are action entries |
| `patches/unixnu/0001-UniXNULib-epsilon-payload-volume-only.patch` | Board patch on the build-time copy of `UniXNULib/Config.c`: config only from the payload volume, `block_slice` refused (M1) |
| `scripts/make-fv-payload.sh` | Builds the payload FAT image with mtools (reproducible) |
| `scripts/lint-epsilon-config.py` | Host lint: no slices, default is an action or exactly the diagnostic, `fv:` paths only, GUID match, no `BOOTAA64.EFI`, diagnostic sources name no state-changing service |
| `scripts/test-epsilon-diag.sh`, `tests/epsilon-diag/host_test.c` | Host test of `DiagCore.c` (field decode, PAC verdict, memory map summary, screen budget) and a 1350x1800 preview render from sample values |
| `scripts/run-docker.sh`, `scripts/build-fd-in-container.sh` | Reproduce the measured build; optional NoSb-only packaging |

## Which firmware tree

| | mu_andromeda `@613e2eac87` | mu_aloha `@0b798128` |
| --- | --- | --- |
| Surface Duo 1 board | Yes. The package is the Duo: SMBIOS "Surface Duo", 6 GiB, 1350x1800, its own ACPI tables | No. 26 `Device/<vendor>-<name>` directories, none Microsoft |
| Package shape | One board, 21 files, `SurfaceDuo1.dsc` + `.fdf` | Generic SM8150 package, 7,394 files, `TARGET_DEVICE` selects a device directory |
| FD | base `0x9FC01000`, size `0x2EF000` | base `0x9FC00000`, size `0x300000` |
| Entry | 4 KiB `BootConfigShim` in front of the FD passes the appended config in x0/x1 | Plain BootShim, no config shim |
| Secure Boot | Runtime: `SecureBootEnabled` in the appended config | Build time: separate `SurfaceDuo1NoSb.dsc` |
| Memory map | Runtime, from `uefiplat.cfg` | Compiled `PlatformMemoryMapLib` per device |
| FDT | None in the FV; ABL's pointer is discarded | Linux DTB embedded in the FV (`FDT.inc`, `DtPlatformDxe`); ABL's pointer is not carried either |
| Duo-only drivers | `SurfaceTouchHidDxe`, `TouchControllerInitDxe`, `SfpdUtils`, `ExtFgDxe`, `MsEnvDxe`, `EpsilonPkg/GpioButtons`, `SurfaceFirmwareProvisioningDataDxe`, Duo-patched `DisplayDxe` | Absent; has `SimpleFbDxe`, `RamPartitionDxe`, `CranePkg` instead |
| Submodule pins | MU_BASECORE `64f47893`, Binaries `1f458b79` | MU_BASECORE `c6d8901d`, Binaries fork `ce59dcf5` |

mu_andromeda is the epsilon firmware. mu_aloha would need a new device port for the
Duo, so it is not a candidate.

What the guacamole patch on mu_aloha does, and what carries over:

| Guacamole mechanism | On mu_andromeda |
| --- | --- |
| Add `UniXNU`, `UniXNUDxe`, `DarwinBoot` INFs to the device DSC and FDF | Replicated in the two `.inc` files |
| Map `FdtLib` and `UniXNULib` | Only `UniXNULib` is needed; `FdtLib` and `HobLib` are already mapped in `Andromeda.dsc.inc` |
| Add the UniXNU package roots to the packages path in `PlatformBuildNoSb.py` | Replicated through `UNIXNU_PACKAGES_PATH` in `PlatformBuild.py` |
| BootShim stores x0 at fixed RAM `0x9FF92000`; `ShengBootMemoryDxe` reads it | Not replicated. Fixed address from another board's memory map |
| `ShengBootMemoryDxe` publishes the initrd FAT, with a memory-scan fallback | Replaced by `EpsilonPayloadDxe` (FV transport). The scan idea is kept as option P3 |
| Build from the NoSb DSC | Not applicable; pick `UefiPlatEpsilonNoSb.cfg` at packaging time |

## Modules embedded

All from `UniXNU/UniXNU@21d683dff6`; DarwinBoot is its submodule at `5243c68e`.

| INF | Type | Library classes | Built `.efi` (V-host) |
| --- | --- | --- | --- |
| `UniXNUPkg/Application/UniXNU/UniXNU.inf` | UEFI_APPLICATION | BaseMemoryLib, UniXNULib, UefiApplicationEntryPoint, UefiBootServicesTableLib | 87,040 B |
| `UniXNUPkg/Library/UniXNULib/UniXNULib.inf` (linked into UniXNU) | library | BaseLib, BaseMemoryLib, DebugLib, DevicePathLib, FdtLib, MemoryAllocationLib, PcdLib, PrintLib, UefiBootServicesTableLib, UefiLib, UefiRuntimeServicesTableLib | — |
| `UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf` | DXE_DRIVER, depex Variable + VariableWrite arch | BaseLib, BaseMemoryLib, DebugLib, DevicePathLib, MemoryAllocationLib, PcdLib, PrintLib, UefiBootServicesTableLib, UefiDriverEntryPoint, UefiLib, UefiRuntimeServicesTableLib | 9,728 B |
| `DarwinBootPkg/Driver/DarwinBoot.inf` | UEFI_DRIVER | BaseLib, BaseMemoryLib, DebugLib, MemoryAllocationLib, PrintLib, UefiBootServicesTableLib, UefiDriverEntryPoint, UefiLib | 29,184 B |
| `EpsilonUniXNUPkg/Driver/EpsilonPayloadDxe/EpsilonPayloadDxe.inf` | DXE_DRIVER, depex TRUE | BaseLib, BaseMemoryLib, DebugLib, MemoryAllocationLib, UefiBootServicesTableLib, UefiDriverEntryPoint | 4,096 B |
| `EpsilonUniXNUPkg/Application/EpsilonDiag/EpsilonDiag.inf` | UEFI_APPLICATION, FILE_GUID `E59FE912-1804-4BB0-B27E-DAE17C9A703A` | DebugLib, MemoryAllocationLib, UefiApplicationEntryPoint, UefiBootServicesTableLib | 8,704 B |
| FV file `F98C36DD-FAF0-4DC4-BFDB-A98F136DB804` (payload FAT) | FREEFORM, RAW | — | 131,072 B |

Not embedded: `UniXNURamDiskDxe` (its FDT input does not exist here, see B-FW1) and
`AdvancedLogDxe` (needs `QrEncoderLib`; later). `DiskIoDxe`, `Fat.inf` and `EnglishDxe`
are already in the stock FD.

## Edits, in order

1. `Platforms/AndromedaPkg/PlatformBuild.py`: `GetPlatformPackagesPath()` appends the
   directories in `UNIXNU_PACKAGES_PATH` (`<unixnu>/package`,
   `<unixnu>/drivers/DarwinBoot`, `<this dir>`).
2. `SurfaceDuo1.dsc` `[Defines]`: `UNIXNU_BOOT_OPTION = 0` (bring-up default, see
   "Boot option and launching UniXNU"), `UNIXNU_FV_PAYLOAD = 1`.
3. `SurfaceDuo1.dsc`, end of file: `!include EpsilonUniXNUPkg/EpsilonUniXNU.dsc.inc`
   (`UniXNULib` mapping, `PcdUniXNUDeviceModel`, five components).
4. `SurfaceDuo1.fdf`, last statement of `[FV.FvMain]` after `LinuxLoader.inf`:
   `!include EpsilonUniXNUPkg/EpsilonUniXNU.fdf.inc` (five `INF` lines and the payload
   `FILE FREEFORM`).

`[FD.SM8150_EFI]`, `FVMAIN_COMPACT`, `APRIORI.inc` and every stock module are
unchanged.

A fifth edit is to UniXNU, not the firmware tree: `build-fd-in-container.sh` copies
`UniXNUPkg` into the build volume and applies
`patches/unixnu/0001-UniXNULib-epsilon-payload-volume-only.patch` to that copy. The
UniXNU checkout stays read-only and unmodified.

## Boot option and launching UniXNU (review M3, L1)

`UNIXNU_BOOT_OPTION = 0` is the bring-up default. `UniXNUDxe` is then not built, no
`Boot####` is written, and `BootOrder` is the stock one: `MsBootOptionsLib` registers
"Internal Storage" (`gMsBootPolicyFileGuid`, parameter `SDD`), USB, PXE, the FFU
loader and the bootloader menu (V-src, `AndromedaPkg/Library/MsBootOptionsLib`). The
Duo boots DuoWOA exactly as with the stock FD. A key pressed at power-on cannot hold it
in the UniXNU menu (`Menu.c` stops the countdown on the first key), because UniXNU is
never started.

The cost: with 0, UniXNU, DarwinBoot, the diagnostic and the payload are in the FD but
nothing starts UniXNU. This FD has no UEFI Shell and no other loader that can name an FV file.
Launching it needs one of these, each of which is a device test that needs explicit
user approval:

1. **Opt-in build, `UNIXNU_BOOT_OPTION=1`** (`UNIXNU_BOOT_OPTION=1 scripts/run-docker.sh
   ...`). `UniXNUDxe` puts UniXNU first in `BootOrder`. Variables are RAM-emulated and
   do not survive a reboot (confirmed in source by the review), so the next power cycle
   with the stock or `=0` image boots as before. Only for a supervised test; never the
   default.
2. **Inside UniXNU, the T5 diagnostic is an `fv:<FILE_GUID>` entry**, never a PE image on
   a FAT volume (see Secure Boot). Since T5 it is also the default entry, by decision of
   the orchestrator for the supervised boot; before T5 this README said "never the
   default". The lint rule changed with it: the default may be an action or exactly this
   one FV file with no arguments, and nothing else (R2).

A future, less intrusive option is an append-only mode for `UniXNUDxe` (register
`Boot####` at the end of `BootOrder`, reachable from the Mu boot menu). It needs an
upstream change and is not implemented.

## B-FW1 and the payload path

`BootConfig/BootConfigShim.S` loads x0 with the config address and x1 with its size
before jumping to the FD, and `ArmPlatformPeiBootAction` stores those two registers
(V-src). ABL's FDT pointer is gone before SEC, so `UniXNURamDiskDxe` has no HOB and no
`/chosen/linux,initrd-*` to read.

**P1, implemented here: payload in the FV.** `EpsilonPayloadDxe` reads the RAW section
of the payload file through `EFI_FIRMWARE_VOLUME2_PROTOCOL` and installs Block I/O on
the pool copy. The stock DiskIo and FAT drivers then expose
`\EFI\UniXNU\unixnu.toml` at the path UniXNU already looks for. No physical address,
FDT, initrd or HOB is involved.

Expected flow with the opt-in `UNIXNU_BOOT_OPTION=1` (UNVERIFIED on hardware; each
step is V-src only):

1. `UniXNUDxe` writes `Boot####` for the FV application and puts it first in
   `BootOrder`. Variables are RAM-emulated (`PcdEmuVariableNvModeEnable|TRUE`), and
   `MsBootOptionsLib` appends its own options at the end.
2. BDS starts UniXNU; it connects controllers, finds the payload FAT volume and loads
   the config.
3. With the shipped config the default entry is the diagnostic after 5 s (see "T5:
   supervised diagnostic build"). This image does not fall through to DuoWOA on its own.
   A key press during the countdown stops it (M3) and leaves the menu up.

## Config discovery and slices (review M1)

Upstream `UniXNUConfigLoad` tries the volume UniXNU started from, then every
`SimpleFileSystem` handle in handle order. On the Duo that includes the Windows ESP.
A `\EFI\UniXNU\unixnu.toml` there would be read; its `[acpi]` tables would be injected
(`Inject.c`) and the countdown would then fall through to Windows with altered ACPI.

**Fixed in code (host build only).** The board patch makes both discovery steps accept
only a volume whose device path starts with the `EpsilonPayloadDxe` vendor node
`5AEDCA82-BD6F-4986-BA69-3B08EEC1F803`. That covers the payload block device and a
partition child that a partition driver may create on it. Every other volume is
skipped. With no payload volume UniXNU uses its built-in configuration (action entries
plus "EFI loader on volume N" entries for removable-media loaders), as upstream does.
Kits and `[[driver]]` paths are read from the config's own volume, so they are covered
by the same restriction.

**Slices.** A `[[block_slice]]` is a Block I/O view onto a GPT partition named in the
config (`Slice.c`); `writable = true` forwards `WriteBlocks` to the internal UFS disk.
The board patch refuses every `block_slice` with a config error. The host lint
(`scripts/lint-epsilon-config.py`, run by the build before the payload image is made)
also fails on any `block_slice`, `slice` or `slices` key in any epsilon TOML. Rule: no
epsilon config declares a slice until the GPT layout is verified and the user approves.

**Not covered, still required before a device boot:**

- The built-in fallback still lists removable-media loaders from every volume. They are
  only started by selecting them or as default; with the shipped config present this
  path is not taken. Not changed, because it is upstream behaviour and starts a signed
  Windows loader at worst.
- The match is by vendor GUID only. Any driver that installs the same GUID would also
  match. None does in the stock tree (V-host: no text hit for `5AEDCA82` in the T1 source
  tree, and no hit for its first 8 little-endian bytes in `Binaries/`).
- The patched code is compiled, not run. That the FAT driver binds to the payload
  device, which handle carries the `SimpleFileSystem`, and that the config is then
  found are all UNVERIFIED until a supervised test.

**Limit of P1.** The FV can only carry what fits in the FD. Upstream's payload volume
defaults to 192 MiB (`payload.py`); a kernelcache alone is tens of MiB. P1 covers the
config and the T5 diagnostic payload. It cannot carry XNU.

**P2 handoff probe.** The Direct wrapper's `BootShim.S` preserves `x0` to
the config shim (verified in the pinned source). The opt-in
`EPSILON_HANDOFF_PROBE=1` build saves `x0` in the config shim, carries it through
`ArmPlatformPeiBootAction` to a diagnostic GUID HOB, and prints
`UNIXNU: ABL.X0=…` without dereferencing the address. Use the supervised
`UNIXNU_BOOT_OPTION=1` and `PACKAGE_NOSB=1` build settings for the temporary
probe image. See [M2 build and wrapper evidence](../../evidence/m2/README.md).

ABL's actual FDT and ramdisk placement and their lifetime under the firmware's
memory map remain open. Any loader transport needs validated bounds, a reserved
payload interval and discovery restricted to the intended read-only volume.
The DualBoot `patchedkernel` shim's handling of `x0` remains unverified.

**P3, fallback: scan memory for the payload FAT by label**, as guacamole's
`PublishScan` does. Needs no x0 but the same unknowns about ramdisk placement apply.

## Size

Measured in `iphoneduo-t1-builder:1`, RELEASE/CLANGPDB, stuart `Success`,
"0114 Images Verified" (stock: 0110).

| | Stock (T1) | T3, `BOOT_OPTION=1` | Hardened, `BOOT_OPTION=0` | **T5 supervised diagnostic, `BOOT_OPTION=1`** | T5 default safe, `BOOT_OPTION=0` |
| --- | --- | --- | --- | --- | --- |
| FD size | `0x2EF000` | `0x2EF000` | `0x2EF000` | `0x2EF000` | `0x2EF000` |
| `FVMAIN_COMPACT` taken | `0x247090` (2,388,112) | `0x2507E0` (2,426,848) | `0x250458` (2,425,944) | `0x251388` (2,429,832) | `0x250FF0` (2,428,912) |
| `FVMAIN_COMPACT` free | `0xA7F70` (687,984, 22.37%) | `0x9E820` (649,248, 21.11%) | `0x9EBA8` (650,152, 21.14%) | `0x9DC78` (646,264, 21.01%) | `0x9E010` (647,184, 21.04%) |
| `FVMAIN` uncompressed | `0xCF8000` | `0xD38000` | `0xD35000` | `0xD3A000` | `0xD38000` |
| Files in `FVMAIN` | — | — | 194 | 196 | 195 (no `UniXNUDxe`) |
| Images verified | 0110 | 0114 | 0113 (no `UniXNUDxe`) | 0115 | see note |
| FD SHA-256 | `3202ee47…c78` | `4d601d67…eaf1` | `2baf3114…40c4` | `f2d95f5c…9000` | `80b3bfa3…62ca` |

T5 columns: built 2026-11-16 (UTC) in the reused volume `iphoneduo_t3h_work`, incremental,
`PACKAGE_NOSB=1`; `UefiPlatEpsilonNoSb.bin` is exactly `0x300000` in both. The diagnostic
costs 2,968 B of compressed space against the hardened build (`0x250FF0` - `0x250458`).
Evidence for the supervised build: `evidence/t5/` (FD, `EpsilonDiag.efi`, FV reports,
logs, `SHA256SUMS.txt`); the safe build's FV reports are in `evidence/t5/safe-build/`.

Notes on the T5 numbers:

- **The FD is not bit-reproducible.** A second, identical supervised build in the same
  volume gave the same size and the same `FVMAIN_COMPACT` taken size but a different FD
  hash (`278fc86e…fbf5`). `EpsilonDiag.efi` was byte-identical in all three builds
  (`15a96acf…319b`). The source of the difference was not investigated. A hash therefore
  identifies one build output, not the source state; boot the exact file whose hash was
  recorded.
- "Images verified" counts `.efi` files under the build directory, not files in the FD.
  In a reused volume it includes stale outputs: the safe build reported 0115 because
  `UniXNUDxe.efi` from the previous build was still on disk. `FVMAIN.inf` is the
  authority: the safe FD has no `29B950CF-…` (`UniXNUDxe`) file.

The hardened build ran in a fresh volume (`iphoneduo_t3h_work`) with `PACKAGE_NOSB=1`;
`UefiPlatEpsilonNoSb.bin` came out at exactly `0x300000`, so the boot config still fits
behind the FD. Evidence: `evidence/t3/hardened/` (FD, FV reports, `FVMAIN.inf`, logs).
The packaged `.bin` is deliberately not kept in `evidence/`.

**Fits.** All four modules plus a 128 KiB payload image cost 38,736 B (`0x9750`) of
compressed space, 5.6% of the headroom. The payload image is almost all zeros, so its
cost so far is negligible; real payload content will compress far less well. Budget
the remaining 649,248 B as compressed bytes.

**The FD cannot grow cheaply.** The loaders copy exactly `0x300000` bytes:
`0x1000` shim + FD + config, and the config is 11,426 B. That leaves at most about
`0xD000` (52 KiB) of FD growth before the config no longer fits, and anything more
means changing the Direct BootShim size, the binary-patched DualBoot kernel header and
the `uefiplat.cfg` memory map (`SEC Heap` starts at `0x9FF00000`). Treat the
headroom as a hard limit.

## T5: supervised diagnostic build

`UNIXNU_BOOT_OPTION=1 PACKAGE_NOSB=1`. Host build only; **nothing in this section has
run on the Duo, and every runtime statement is UNVERIFIED.** Booting it, even from RAM,
needs explicit user approval and the recovery prerequisites in `docs/phase1-plan.md`.

### What the image is meant to do

1. `UniXNUDxe` puts UniXNU first in `BootOrder` (RAM-emulated variables).
2. BDS starts UniXNU. It connects controllers, reads `unixnu.toml` from the payload
   volume only (M1), and shows its menu with a 5 s countdown on entry "Read-only
   diagnostic (T5)".
3. UniXNU resolves `fv:E59FE912-1804-4BB0-B27E-DAE17C9A703A` to an FV file device path
   and calls `LoadImage`/`StartImage` (`Boot.c`, `Volume.c` `ResolveFvFile`, V-src). The
   entry has no `args`, `dtb`, `initrd`, variables or kexts, so UniXNU sets no variable
   and installs no device tree for it (V-src: `UniXNUSetEntryVariables` loops over zero
   variables; `UniXNUPrepareDeviceTree` returns at once without a `dtb`).
4. `EpsilonDiag` clears the screen, prints the report, shows `UNIXNU: HOLD=<seconds>`
   counting down from 60, and returns `EFI_SUCCESS`. A key press ends the hold early.
5. UniXNU shows its menu again with the notice "returned: Success".

### What the diagnostic touches

Reads: seven system registers with `MRS`; the GOP mode structure; the UEFI memory map;
key strokes from `ConIn`. Writes: pixels through GOP `Blt` (or text through `ConOut` when
no GOP exists), pool allocations it frees again, and the debug log. It does not use
storage, file, variable, image, configuration-table or reset services.

Checked on the built image (V-host, `llvm-objdump -d` on `EpsilonDiag.efi`, 1,185
instructions): exactly seven `mrs` (`CurrentEL`, `MIDR_EL1`, `CNTFRQ_EL0`,
`ID_AA64PFR0_EL1`, `ID_AA64PFR1_EL1`, `ID_AA64ISAR1_EL1`, `ID_AA64ISAR2_EL1`) and no
`msr`, `svc`, `hvc`, `smc`, `sys` or `eret`. Lint rule R6 fails the build if the sources
name a state-changing service. Neither check proves the absence of a stray pointer
write; the sources are short enough to read (`EpsilonDiag.c` about 450 lines, `DiagCore.c` about 650 of which 60 are the font table).

### Output channel

GOP `Blt` with a built-in 5x7 font, scaled 3x on a 1350x1800 mode (18x27 px cells, 63
columns, 62 rows; the report is 57 lines plus the `HOLD` line). `ConOut` is the fallback
when there is no GOP. The task brief preferred `ConOut` first; this is reversed on
purpose: the firmware text console also ends in the same `Blt`, but with 8x19 glyphs
(about 0.5 mm per character at 401 ppi), and UniXNU's menu draws on the GOP without
`ConOut` for the reason given in its `Ui.c`. Every line also goes to `DEBUG`.

`scripts/test-epsilon-diag.sh` renders the layout from sample values:
`evidence/t5/host-test/epsilon-diag-preview-SAMPLE.png`. The values in that picture are
made up for the layout test. They are not readings.

### Lines

Format `UNIXNU: <label>=<value>`; hex is upper-case without `0x`. Print order is as
listed. Each line is drawn before the next register is read, so a fault leaves the
progress visible.

| Line(s) | Source | Why it matters |
| --- | --- | --- |
| `DIAG.VERSION=1` | constant | Proves the FV application ran: FV payload driver, FAT, config discovery, `fv:` resolution and `StartImage` all worked |
| `CURRENTEL`, `EL` | `CurrentEL` | Exception level UEFI runs at. Expected 1 (under QHEE). XNU entry and the timer choice depend on it |
| `MIDR_EL1`, `MIDR.IMPL`, `MIDR.PART` | `MIDR_EL1` | Identity of the boot core. The Android audit saw implementer `0x51`, part `0x805` on CPU 0 |
| `CNTFRQ_EL0` (hex and decimal) | `CNTFRQ_EL0` | Timer frequency that XNU and DarwinBoot will see in this context |
| `ID_AA64PFR0_EL1`, `PFR0.EL2`, `PFR0.EL3`, `PFR0.GIC` | `ID_AA64PFR0_EL1` | Which exception levels exist; whether the GIC system-register interface is advertised |
| `ID_AA64PFR1_EL1`, `PFR1.BT`, `PFR1.MTE` | `ID_AA64PFR1_EL1` | BTI and MTE, both relevant to arm64e-era binaries |
| `ID_AA64ISAR1_EL1`, `ISAR1.APA`, `.API`, `.GPA`, `.GPI` | `ID_AA64ISAR1_EL1` bits [7:4], [11:8], [27:24], [31:28] | Pointer authentication, QARMA5 and IMPLEMENTATION DEFINED algorithms |
| `GOP.COUNT`, `.MODE`, `.MAXMODE`, `.FB_BASE`, `.FB_SIZE`, `.HRES`, `.VRES`, `.PIXFMT`, `.PPSL` | `EFI_GRAPHICS_OUTPUT_PROTOCOL` on the console handle, else the first one | The UEFI-context frame buffer base and geometry that Android could not give. `docs/hardware.md` expects one GOP, 1350x1800, base still a placeholder |
| `MEM.DESC`, `MEM.DESCSZ`, `MEM.LO`, `MEM.HI` | `gBS->GetMemoryMap` | Descriptor count and the span the map covers |
| `MEM.CONV.N`, `.TOTAL`, `MEM.CONV0..3` | same | Number, total and the four largest `EfiConventionalMemory` ranges (`start+size`), after merging touching descriptors |
| `MEM.RSVD.N`, `.TOTAL`, `MEM.RSVD0..3` | same | The same for `EfiReservedMemoryType` |
| `MEM.HOLE.N`, `.TOTAL`, `MEM.HOLE0..3` | same | Address ranges between `MEM.LO` and `MEM.HI` that no descriptor covers. Carve-outs are often left out of the map instead of being typed reserved, so these are compared with the live DT `reserved-memory` too |
| `DIAG.NEXT=ISAR2` | constant | Marker: the next read is the one register newer than these cores |
| `ID_AA64ISAR2_EL1`, `ISAR2.APA3`, `.GPA3` | encoding `S3_0_C0_C6_2`, bits [15:12], [11:8] | Pointer authentication, QARMA3 |
| `PAC.ADDR`, `PAC.GENERIC` (yellow) | derived | 1 if any of APA/API/APA3, respectively GPA/GPI/GPA3, is non-zero. `0` and `0` is the on-device confirmation that this core has no PAC |
| `DIAG.DONE=1` | constant | The report is complete; nothing faulted |
| `HOLD=<n>` | countdown | Liveness; the screen stays for 60 s |

Limits of what the lines mean: the registers are those of the one core UEFI runs on
(the audit's CPU 0, a Kryo 485 Silver); the gold and prime cores are not sampled. The
memory map is the map at the moment the application runs, after UniXNU's own
allocations.

### Getting out of the test image

This image does not continue to DuoWOA by itself.

- After the diagnostic returns, UniXNU shows its menu without a countdown. If UniXNU
  finds a physical input device it waits there. If it finds none it starts the default
  entry again after 10 s (`fallback_timeout`), so the report repeats about every 70 s
  (V-src, `Menu.c`). Whether the Duo's buttons or touch reach `ConIn` is UNVERIFIED.
- Preferred end of the test: select "Power off" in the menu if input works. Otherwise a
  forced power-off with the hardware button; the hold time is UNVERIFIED.
- "Return to firmware" makes BDS continue with the next boot option, which is Windows.
  Avoid it on the first boot: it would start Windows from a test firmware image.
- Nothing persists: variables are RAM-emulated (V-src) and nothing is flashed, so the
  next normal power-on uses the firmware on the device. That non-persistence has not
  been observed on hardware.

### Open before the boot

- How `UefiPlatEpsilonNoSb.bin` is handed to the device for a RAM-only boot is outside
  this directory and not checked here.
- Whether the `ID_AA64ISAR2_EL1` read traps to EL2 and how QHEE answers is UNVERIFIED.
  The encoding is in the reserved ID space, architecturally read-as-zero, and it is read
  last so that a fault costs only the last four lines.
- Whether the FAT driver binds to the payload device and UniXNU finds the config
  (M1, compiled but never run). If it does not, UniXNU shows its built-in menu and the
  diagnostic entry is absent; `DIAG.VERSION` never appears.

## Secure Boot

Secure Boot is not a build variant in this tree. The same FD serves both;
`build_appendable_payload_epsilon.sh` produces `UefiPlatEpsilon.bin` and
`UefiPlatEpsilonNoSb.bin`, which differ only in `SecureBootEnabled`.

**Policy (review M2): bring-up packaging is NoSb only, never `UefiPlatEpsilon.cfg`.**
FV images are `ALWAYS_EXECUTE` under the platform image-verification policy (confirmed
in source by the review), so with Secure Boot on the embedded UniXNU runs unverified.
If it then took its config, ACPI or drivers from a FAT volume, unsigned content could
steer a boot that the signed chain is meant to gate, which weakens the existing DuoWOA
install (CLAUDE.md section 5). M1 narrows the config source to the payload volume, and
the policy stays in force regardless.

- `build-fd-in-container.sh` does not call the stock
  `build_appendable_payload_epsilon.sh`, which builds both variants. Its packaging step
  (`PACKAGE_NOSB=1`, off by default) builds `UefiPlatEpsilonNoSb.bin` only, refuses any
  other `UEFI_CONFIG` (exit 30) and refuses a config without `SecureBootEnabled = 0`
  (exit 31). The packaged image is a host artifact and must not be flashed.
- Images dispatched or loaded from the FV are exempt from image verification (review:
  `DxeImageVerificationLib.c`), so the embedded modules would also start with Secure
  Boot on. That is the reason for the policy, not a reason to use the SB variant.
- Anything UniXNU loads from a FAT volume as a PE image (`[[driver]]`, an `efi` entry)
  would be verified under Secure Boot. The T5 diagnostic is therefore an FV file started
  with an `fv:<FILE_GUID>` entry, and the supervised build is packaged NoSb.

## Reproduce

```sh
firmware/scripts/run-docker.sh <UniXNU checkout @21d683dff6> <out dir>
```

Env: `UNIXNU_BOOT_OPTION` (default 0), `UNIXNU_FV_PAYLOAD` (1), `PAYLOAD_KIB` (128),
`PACKAGE_NOSB` (0), `T3_VOLUME` (`iphoneduo_t3_work`). The supervised diagnostic build is
`UNIXNU_BOOT_OPTION=1 PACKAGE_NOSB=1`; the log prints `BUILD KIND:` so the two cannot be
mixed up afterwards. `scripts/test-epsilon-diag.sh` runs in the container before the
payload step and stops the build if it fails; it also runs on the macOS host with `cc`. The UniXNU directory needs
`package/UniXNUPkg` and `drivers/DarwinBoot/DarwinBootPkg` (the submodule checked out).

Needs Docker image `iphoneduo-t1-builder:1` and volume `iphoneduo_t1_work` (mounted
read-only). The build runs in a separate volume. `mtools` is installed at run time,
pinned to `4.0.43-1build1` (Ubuntu 24.04 `noble/main`); fold it into the image at the
next Dockerfile revision. `pip` installs `pip-requirements.txt` without `--upgrade`
and the log records a SHA-256 of `pip freeze`. The build stops if the board patch does
not apply or the lint fails.

Remaining reproducibility gaps (M4, partial): `apt-get update` still runs for mtools;
stuart `--update` fetches ext_deps by its own pins; the device repo is not under
version control, so the artifacts have no revision ID (user decision).

## Unverified assumptions and risks

- Nothing has run on hardware: dispatch order, BootOrder behaviour, the menu on the
  Duo's GOP, touch or button input, and the return-to-firmware path are all untested.
- The default `UNIXNU_BOOT_OPTION = 0` leaves the boot order alone; UniXNU then has no
  launcher in this FD. The opt-in `= 1` makes UniXNU first, with a 5 s timeout that a
  key press cancels, and then runs the diagnostic. That image never continues to
  DuoWOA by itself.
- The ADT's `/chosen/random-seed` is written once by `scripts/make-epsilon-adt.py`, so
  every boot of the same file would reuse it. DarwinBoot must overwrite it with fresh
  entropy per boot (review L5). xnu-10063 panics without it
  (`pexpert/gen/pe_gen.c` `PE_get_random_seed`) and below 64 bytes
  (`osfmk/prng/prng_random.c` `bootseed_init_bootloader`). Not implemented yet;
  required before an XNU boot, not before the T5 diagnostic.
- Variables are RAM-emulated in source, and the review traced `PStoreAddress`/
  `PStoreSize` to a RAM debug buffer, not a variable store (V-src). Non-persistence of
  `BootOrder` across a warm reboot is still unobserved on the device.
- The platform builds with `BLD_*_MEMORY_PROTECTION=TRUE`. Its effect on DarwinBoot's
  kernel window and handoff is not analysed.
- DarwinBoot is built unmodified. UniXNU carries one epsilon board patch (config
  discovery and slices, M1); no other board patch exists yet. Timer INTID (B1) and other
  kernel-side blockers are untouched by this work.
- The FD hash identifies one build output only. Two identical T5 builds gave different
  FD hashes (see Size).
- The T5 diagnostic has been compiled, disassembled and its decode logic host-tested.
  It has not been executed, on the Duo or in an emulator (no QEMU on the build host).
- This FD must not be flashed. Any RAM-only boot test needs explicit user approval and
  the recovery prerequisites in `docs/phase1-plan.md`.
