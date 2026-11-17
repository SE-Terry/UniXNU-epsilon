# M2 bounded FDT and initrd probe

The first M2 RAM boot measured `x0=0x849B5000`. The pinned SM8150 `AndromedaMemoryMapLib` places that address in `HLOS 1`, `[0x83722000, 0x85700000)`. `MemoryInitPeiLib.c` creates memory resource and allocation HOBs and maps the board table with matching virtual and physical addresses. The `EPSILON_HANDOFF_PROBE=2` variant checks that exact mapped RAM descriptor and that the MMU is enabled before inspecting the bootloader argument in PrePi. The prior diagnostic remains available as `EPSILON_HANDOFF_PROBE=1`.

The probe checks alignment, header containment, and the declared size against the mapped window and 2 MiB cap before calling the pinned libfdt full validator. It reads the `/chosen/linux,initrd-start` and `linux,initrd-end` properties as equally sized 32-bit or 64-bit big-endian values. It requires a nonempty interval contained in `HLOS 1` and disjoint from the FDT. It does not read the initrd or publish a RAM volume. DXE displays the measurement HOB, avoiding a second read of the original blob. The format and cap follow the [Devicetree Specification](https://devicetree-specification.readthedocs.io/en/stable/flattened-format.html) and [AArch64 Linux boot protocol](https://docs.kernel.org/arch/arm64/booting.html).

## Host verification

- AddressSanitizer and UndefinedBehaviorSanitizer checks passed for an accepted 64-bit interval, 32-bit property decoding, inaccessible and misaligned addresses, truncated and malformed FDTs, absent or invalid-sized properties, and reversed or overlapping ranges.
- The host test also decoded the previous Android-side audit DTB and reproduced `[0x84B84000, 0x856FEE3E)`. Those are the Android boot's values. The proposed device test keeps the same stock DTB and ramdisk in the wrapper as the prior M2 probe and measures the temporary-boot values independently.
- Host test source is `UniXNU-epsilon/firmware/tests/epsilon-diag/handoff_fdt_test.c`. Compile with Clang against the pinned Mu BaseFdtLib `libfdt/libfdt` sources (`fdt.c`, `fdt_check.c`, `fdt_ro.c`, `fdt_rw.c`, `fdt_sw.c`, `fdt_wip.c`, and `fdt_empty_tree.c`), with the `m2-probe` patch directory and the libfdt directory on the include path.

## Build and image

The supervised build (`EPSILON_HANDOFF_PROBE=2 UNIXNU_BOOT_OPTION=1 PACKAGE_NOSB=1`) passed config lint and verified 115 images. The FD stayed at `0x2EF000` bytes with 641,944 bytes free. Host inspection confirmed the Android v2 digest, the NoSb firmware and measurement HOB GUID, the exact embedded diagnostic application, and the same wrapper header fields, boot arguments, ramdisk and DTB as the first M2 probe.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `epsilon-m2-fdt-bounds-probe.img` | 19,218,432 | `976c0c31fcf0ac9e1582d4e25c78418ee9c42f808de91dc80d50527f52bb397f` |
| `SM8150_EFI.fd` | 3,076,096 | `315d3fc56a47404d0ea24cd6bc6967efdcd3f262c42ef7e3582795f204235e30` |
| `UefiPlatEpsilonNoSb.bin` | 3,145,728 | `96d4bee8eb2171ae907a71ad5cd0835cffb2c0715be326098085c3320a6d85ac` |

The packaged image is in `/Users/terry/Documents/Codex/2026-11-16/referenced-chatgpt-conversation-this-is-an/outputs/`. It is a host baseline. Hardware testing is on hold while the PrePi read path gains synchronous-abort recovery. All 59 local recovery files passed hash verification again on November 17.

## Panel capture

Record `ABL.X0`, `FDT.BYTES` or `FDT.STATUS`, `INITRD.START`, `INITRD.END`, and `DIAG.DONE`. A `FDT.BYTES` result means the libfdt and interval checks passed. `FDT.STATUS` values are `01` board map/MMU check failed, `02` pointer or header outside the read window, `03` invalid/truncated DTB, `04` absent or malformed initrd properties, and `05` decoded initrd interval rejected. A missing result HOB has its own marker. `DIAG.DONE` only indicates the diagnostic completed.
