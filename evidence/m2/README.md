# M2 host loader and handoff probe — 2026-11-17

## Result

The exact DarwinBoot PE32 image extracted from the verified T5 firmware ran under QEMU 8.2.2 on AAVMF at EL1. The harness starts DarwinBoot and calls its revision-2 loader protocol. The input FAT volume is exposed read-only to QEMU and staged in container RAM. DarwinBoot reads a 32 MiB ARM64 Mach-O and its ADT, validates them, exits boot services and enters the synthetic image. The synthetic entry verifies the last eight bytes of the loaded Mach-O, boot arguments, its ADT handoff, the fixture RAM disk and MMU-off entry. A second run with an invalid ADT returned to the firmware with Compromised Data.

- [Positive serial log](qemu-32mib.txt)
- [Invalid-ADT serial log](qemu-invalid-adt.txt)

These are loader results in a VM. The Duo's ABL initrd location, ownership and lifetime, on-device FAT discovery for a second volume and XNU entry remain open.

## Handoff probe

The pinned DuoWOA Direct shim preserves `x0` through entry to the config shim, which normally replaces it with the firmware config address. The optional `EPSILON_HANDOFF_PROBE=1` patches save the original register through PrePi in a diagnostic GUID HOB. EpsilonDiag prints `UNIXNU: ABL.X0=…` or reports the missing HOB. The probe never dereferences the captured address and the image carries the same stock ramdisk and DTB as the T5 wrapper.

The supervised NoSb build verified 115 images, with clean config lint. The FD is `0x2EF000` bytes and has 646,144 bytes free in `FVMAIN_COMPACT`. Host inspection verified the first config shim instruction, Android v2 wrapper digest and components, compressed-FV executables, and the single embedded configuration file. The approved RAM-only run captured `ABL.X0=0x849B5000` and `DIAG.DONE=1` on the Duo. [Device report](device-2026-11-17.md).

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `epsilon-m2-abl-handoff-probe.img` | 19,218,432 | `51f54b2088d5c76c854a8be6a8afd940e475ea309a219d36dce7787754469326` |
| `SM8150_EFI.fd` | 3,076,096 | `e405f7d5b0247e549dc22e7aa38db8c3ef2de5f6505721b08c887c77215b6b5a` |
| `UefiPlatEpsilonNoSb.bin` | 3,145,728 | `cf32c4c68b7d119144a4e2237a301be17b3e077d85670107e2e27a5735ff05b2` |

Before reading any FDT, establish the allowed physical range for header access and its lifetime through DXE. Check the FDT header and bounds before interpreting initrd properties, and validate and reserve the whole payload interval before exposing a dedicated read-only volume.

[Bounded FDT probe](fdt-bounds-probe.md) implements and host-verifies the next measurement in PrePi.

## Inputs and reproducibility

| Input | SHA-256 |
|---|---|
| T5 DarwinBoot PE32 (extracted byte-for-byte from the T5 firmware) | `3853493ed655c4fe8c9b6b07955b076ace21e79dd65987ecf40eb57336ce5185` |
| Synthetic 32 MiB Mach-O | `b1491f3117296dfd2421c95364b249309c9a7f99184ed50580a0a5db548dfabb` |
| Synthetic ADT | `4bfe3a9fa116aa180851205725b3a7128ab7d0c108809b41c779b34443c49b1c` |

Generate the base Mach-O, ADT, RAM disk and trust-cache fixtures from [DarwinBoot `5243c68`](https://github.com/UniXNU/DarwinBoot/tree/5243c68ec332a227fe3680cd5da6d5a0e127035c/Test) with `Test/make-fixtures.py` after applying [the synthetic tail check](qemu-synthetic-tail.patch). Expand the generated `__DATA` mapping to cover file bytes from offset `0x2000` through the end of the 32 MiB file. Place little-endian `0x31444E452D324D55` in the final eight bytes. The original entry and its initial zero-filled DATA area are kept.

Compile [the test harness](qemu-loader-fixture.c) as AArch64 UEFI using the pinned Mu BaseCore and upstream UniXNU headers. Stage the harness as `EFI/BOOT/BOOTAA64.EFI` on the disposable VM ESP with `DarwinBoot.efi`, `kernel.macho`, its `.adt` and `.trustcache` sidecars, and `ramdisk.bin` in the root. Run pinned UniXNU's [QEMU test runner](https://github.com/UniXNU/UniXNU/blob/21d683dff6/docs/reference/qemu.md) with AAVMF, one CPU, 1 GiB RAM and no GOP. For the refusal control, replace the ADT in a second disposable fixture with the eleven ASCII bytes `invalid-adt`.
