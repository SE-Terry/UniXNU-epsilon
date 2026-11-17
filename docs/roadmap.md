# roadmap.md — iPhoneDuo milestone ladder (epsilon)

**Status:** 2026-11-17. iOS 27 is the primary target. Each milestone records the evidence to capture and may take multiple iterations. Any device boot needs explicit approval and the recovery and GNG-1.5 checks in [the Phase 1 plan](phase1-plan.md).

| # | Milestone | Pass criterion (log line) | Epsilon state | Needs |
|---|---|---|---|---|
| **M0** | UEFI with UniXNU payload embedded | FD builds + embeds | ✅ **DONE** (host: T1/T3, hardened) | — |
| **M1** | Diagnostic payload prints ID regs / CNTFRQ / GOP / memory map | `UNIXNU: DIAG.DONE=1` on panel or log | **Complete on hardware; normal Android return verified** | — |
| **M2** | Deliver a kernel-sized RAM payload to DarwinBoot on epsilon | Validated payload source, successful kernel and ADT loading | 32 MiB synthetic loader test passed in QEMU. `ABL.X0=0x849B5000` captured on hardware | Validate FDT and initrd ranges and implement reserved RAM transport |
| **M3** | First logged XNU entry using the iPadOS 17 arm64 diagnostic | reaches `arm_init`; record timer IRQ delivery separately | Not started | kernel-sized transport, verified ADT and early console |
| **M4** | Non-PAC xnu-12377 builds + reaches BSD init | BSD init log | Not started | **re-home the fork series as a new arm64e-no-PAC arch profile** |
| **M5** | UFS read, lowered APFS/AMFI prelink, root mount | root mounted | Not started | SM8150 UFS work and separately approved root transport |
| **M6** | launchd passes grading+AMFI; dyld runs at EL0 | launchd exec / dyld start | Not started | measure candidate footprint and RAM requirements |
| **M7** | Rewritten shared cache maps; launchd runs in it | cache mapped | Not started | measure the packaged candidate and compare it with verified storage capacity |
| **M8** | "Early boot complete"; SpringBoard + backboardd alive | processes alive | Not started | full lowered userland |
| **M9** | First composited frame / lock / home screen | frame on panel | Open hardware adaptation | assess Adreno 640 donor work and Duo display handoff |
| **M10** | Second panel, fold state, touch (the "iPhoneDuo") | dual-screen UI | Phase 6 | DSI1 driver + fold logic + HID + custom identity |

## Sequencing & gating

- **Boot chain** — Use the measured ABL entry argument to plan bounded FDT inspection, then validate ranges and lifetime for a kernel-sized transport. DarwinBoot's validation marker precedes its exit from boot services, so any hardware check intended to stop before kernel entry needs a separate validation-only path.
- **iOS 27 feasibility in parallel:** inspect the exact `ipados-27.0.1-24A446` candidate, including the device-specific shared cache and early userland. Use iOS 26 as an optional comparison if it can resolve a specific compatibility question. See [ADR 0006](decisions/0006-ios27-identity-and-scope.md).
- **Storage:** defer all writes and partition changes. The Android audit reports `sda9 unixnu` at 13.75 GiB. Donor sizes are planning figures. Measure the actual candidate and verify the partition identity, contents and usable capacity before choosing a storage plan.
- **Parallel sub-projects**, each large on its own: SM8150 UFS-writable, SMP, **GPU (Adreno 640)**, dual display.

## Honest outcome expectation
Even if every milestone lands, the result is **slow, RAM-booted, research-grade**, with workarounds (watchdog disabled, tccd unconfined), no Bluetooth/cellular, and **possibly no UI without a GPU driver**. This is a research port, not a daily-driver OS — consistent with CLAUDE.md's "success is not guaranteed."

## Current position
**M0 and M1 are complete. M2 loader behavior is verified in QEMU, and the ABL argument has been captured on hardware.** Android returned after T5 on the same build and slot, with matching GPT, boot partition and ESP hashes. See [the T5 device report](../evidence/t5/device-2026-11-17/report.md) and [the M2 probe report](../evidence/m2/device-2026-11-17.md).
