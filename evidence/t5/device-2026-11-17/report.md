# T5 supervised RAM boot — 2026-11-17

**Result:** M1 diagnostic executed on the Duo 1, emitted `DIAG.DONE=1`, and returned Success to UniXNU. Normal Android return verified.

- Device: Surface Duo 1 / epsilon. Fastboot reported `product: surfaceduo`, `unlocked: yes`, `is-userspace: no`, and active slot `a`.
- At 01:03:47 +07, the explicitly approved `fastboot boot` sent the verified 19,218,432-byte image, SHA-256 `81b523780c4e4f642850dd4c70f5a923d4d4aafd0cbcfd3c4f6ebd82867c8624`. The send and boot both returned `OKAY`. The boot command completed in 10.159 s.
- The video shows the UniXNU 0.1.0 menu, the embedded configuration path, and the default T5 diagnostic. The still shows `Read-only diagnostic (T5) returned: Success`. No flash, lock-state change, or repartition command was issued.
- Follow-up provenance check extracted `0x2EF000` bytes from the exact wrapper at file offset `0x2070` (the page, 112-byte Direct shim, and 4,096-byte config shim). Those bytes matched the reviewed `evidence/t5/SM8150_EFI.fd` exactly, SHA-256 `f2d95f5c40c882680139737ef349b1b2f0b27bd6a683b56bea26524666de9000`.

## Verified measurements

| Item | Value shown |
|---|---|
| Exception level | EL1 (`CurrentEL=0x4`) |
| Boot-core MIDR | `0x51DF805E` (Qualcomm implementer `0x51`, part `0x805`) |
| Architectural counter frequency | 19,200,000 Hz (`0x0124F800`) |
| Arm instruction feature registers | `ID_AA64ISAR1_EL1=0x100001`, `ID_AA64ISAR2_EL1=0x0` |
| Boot-core pointer authentication | `PAC.ADDR=0`, `PAC.GENERIC=0`. The ISAR2 read completed |
| UEFI graphics | Two GOP handles enumerated. The selected GOP reported `1350 × 1800`, framebuffer base `0x80600000`, size `0x9450C0`, and stride 1350 pixels. The captured output was on one physical panel |
| UEFI memory map summary | 170 descriptors of 48 bytes; span `0x80000000`–`0x200000000`; conventional memory total `0x15526D000` bytes; three unmapped ranges |
| Completion marker | `UNIXNU: DIAG.DONE=1` |

The firmware, UniXNU launcher, embedded FV config and diagnostic rendering path executed on hardware. T5 sampled one core and a memory map after UniXNU allocations. DarwinBoot loading, XNU entry, actual timer interrupt delivery, presentation on the second panel, and the full descriptor map are outside this result.

## Android return and storage verification

The user physically exited the test and returned to Android. ADB confirmed firmware build `2022.902.48`, slot `_a` and `sys.boot_completed=1`. All 35 saved GPT segments across the seven LUNs matched the pre-test SHA-256 values. Both boot partitions and the EFI system partition also matched their backups. DuoWOA was reported working before the test and was not booted afterward.

## Source captures

- `IMG_2873.MOV` (user capture in Downloads), SHA-256 `73a14a0f0a065e3b3907d7f58371fcce819067af282444e76f8ddd5888e4af82`.
- [Diagnostic panel](diag-panel.png): rotated and cropped frame at 12 s from that video. Measurements transcribed visually from the crop.
- Panel crop SHA-256 `5592a939072d381cd0719cf1090216d94066d864779b045d7670b6c45e7ec509`.
- `IMG_2874.jpg` (user capture in Downloads), SHA-256 `da2e89b51d5e816677341b6cecc5bfb35dfd21becb254850a3a9bc7ffcadccc8`. Shows successful return to the UniXNU menu.
