# Surface Duo 1 (epsilon / SM8150) — UniXNU bring-up: status & handoff

**What this is:** the first UniXNU/iOS bring-up effort on the **Microsoft Surface Duo 1** (codename `epsilon`, Qualcomm **SM8150 / Snapdragon 855**). As of 2026-11-17 no prior UniXNU port existed for this exact device. This document is an honest status + an open invitation for a more experienced porter (or the UniXNU community) to build on — or take over — the Duo 1.

**Honesty labels:** MEASURED (on-device, read-only) · HOST (built/tested on a dev machine, never booted) · CLAIMED (observed by the project but not independently re-verified by the author of a given section) · OPEN (not done).

---

## TL;DR for a porter

- **The device-specific groundwork is done and verified**: hardware audit, firmware integration into the *real* DuoWOA firmware, an ADT, and a diagnostic that **booted on hardware**. This is the part that needs the physical Duo 1 + SM8150 legwork — the on-ramp a kernel-focused dev would otherwise have to build first.
- **The hard parts are open and well-scoped**: the non-PAC xnu-12377 kernel re-homing, SM8150 UFS/SMP, **GPU (Adreno 640)**, dual display, and the arm64e→arm64 userland rewrite. These are where deep XNU/Qualcomm expertise is needed.
- **epsilon is in the same ISA class as `none24smu`'s Duo 2 (zeta)** — both are **no-PAC ARMv8.2** Qualcomm SoCs — so zeta's hybrid rewrite approach applies, re-validated for SM8150.

---

## Verified hardware facts (MEASURED, read-only Android audit — `evidence/device-audit-2026-11-16/`)

- **No pointer authentication.** `/proc/cpuinfo` has no `paca`/`pacg`; later confirmed in UEFI (T5) — `ID_AA64ISAR1/2` APA/API/GPA/GPI/APA3/GPA3 all zero. CPU = **Kryo 485** (impl `0x51`, parts `0x804` gold / `0x805` silver; A76+A55, ARMv8.2-A → architecturally no FEAT_PAuth).
- **SoC:** `qcom,msm-id = 0x153 / rev 0x00020000` (SM8150 v2); `board-id 0x08 / subtype 0x10`. RAM 5.39 GiB usable (6 GiB).
- **DRAM banks** (live `/memory`, matches the pinned firmware map): `0x80000000+0x3CC00000`, `0xC0000000+0x80000000`, `0x140000000+0xC0000000`.
- **Arch timer:** PPIs 1/2/3/0 = INTID 17/18/**19**/16 (virtual = **19** — *not* 27). **CNTFRQ 19.2 MHz** (confirmed in UEFI).
- **GIC-v3:** GICD `0x17A00000`, GICR `0x17A60000`. **QHEE** hypervisor reserved at `0x85700000+0x600000`. UEFI runs at **EL1**.
- **Framebuffer (UEFI/GOP):** **`0x80600000`**, 1350×1800×32, left panel only (MEASURED in T5; Android's `cont_splash` is at `0x9C000000`).
- **Displays:** two LG 1350×1800 DSI command-mode DSC panels. **GPU: Adreno 640.** **Touch:** Microsoft N-Trig HID-over-SPI (single-touch fallback natively).
- **Partition layout (sda, 104 GiB):** `… userdata (26 GiB) → esp (260 MiB) → win (64 GiB) → unixnu (sda9, ~13.75 GiB)`. The **`unixnu` partition was created by the device owner as the install target** — last partition, so no `userdata` shrink is needed (resolves the usual UniXNU dual-boot installer problem). Windows not yet trimmed, so `sda9` can grow later.

## Firmware foundation (HOST-built, then booted)

- **The key epsilon-specific obstacle:** DuoWOA firmware (`WOA-Project/mu_andromeda_platforms@613e2eac87`, `Platforms/SurfaceDuo1Pkg`) is **not Mu-Silicium**. Its `BootConfigShim` overwrites `x0`, so ABL's **FDT pointer is lost** before SEC — UniXNU's normal `/chosen/linux,initrd-*` payload discovery does not work unmodified.
- **Solution (HOST, builds + fits):** embed `UniXNU` + `UniXNUDxe` + `DarwinBoot` + a new **`EpsilonPayloadDxe`** into the real `SurfaceDuo1Pkg` FD. `EpsilonPayloadDxe` publishes the payload FV file as a read-only Block I/O volume → stock FAT serves `\EFI\UniXNU\unixnu.toml` with **no FDT/initrd/HOB dependency**. The FD stays at the fixed **`0x2EF000`** with ~21% FV headroom. Config discovery is hardened to the payload volume's vendor GUID only (the Windows ESP is never read), `block_slice` is refused, and bring-up is **NoSb-only** with `UNIXNU_BOOT_OPTION=0` by default.
- **The FD cannot grow cheaply** (loaders copy a fixed `0x300000`), and the FV payload is far too small for a kernelcache — so **actual XNU boot needs a kernel-sized transport** (a `BootConfigShim` x0-stash + PrePi HOB re-publish so stock `UniXNURamDiskDxe` works, or an equivalent). This is designed but not implemented (OPEN).

## On-hardware results (CLAIMED — booted via RAM-only `fastboot boot`, nothing flashed)

- **T5 diagnostic booted (2026-11-17, `evidence/t5/device-2026-11-17/`):** a read-only UEFI app printed register/GOP/memory-map lines on the panel and returned cleanly. Captured: EL1, **no PAC**, CNTFRQ 19.2 MHz, **GOP `0x80600000`**, 1350×1800, `DIAG.DONE=1`. **Android returned on the same build/slot afterwards; GPT/boot/ESP hashes matched the pre-test backup.** (Recovery backups of GPT + boot chain + EFS were taken and hash-verified first.)
- **M2 loader probe (CLAIMED):** DarwinBoot loaded a 32 MiB synthetic Mach-O + ADT under QEMU; on-device it reported the ABL-provided `x0 = 0x849B5000` (inside the `HLOS 1` window `[0x83722000,0x85700000)`) without dereferencing it. **A fault-recovery gap is known:** an unhandled synchronous abort in this firmware is a silent permanent hang (watchdog disabled, no serial), so the PrePi FDT read needs a recoverable guard before further probing.

## Compatibility reality (so nobody over-promises)

- **iOS 16 is the proven SM8150 ceiling** (the `ios-sweet`/guacamole family reaches SpringBoard). **iOS 26/27 has only ever run on the newer SM8350** (zeta). iOS 27 on SM8150 would be unprecedented.
- **iOS 27 is viable only via zeta's hybrid approach** (offline shared-cache pointer-lowering + kernel page-in text-fixups + trap-emulation of CSSC/FJCVTZS + RCPC2 islands + owner-re-signed daemons). Recommended base identity: a **plain-`arm64e`** build — upstream's pinned **`ipados-27.0.1-24A446` (iPad17,3)**, *not* the `arm64e.x1` iPhone19,4 — which should cut the gap to ≈ iOS-26 scope. A custom "iPhoneDuo" identity is a cosmetic Phase-6 layer, not a shortcut.
- **GPU is a hard UI wall:** on zeta nothing rendered until an Adreno driver was present; epsilon has no Adreno 640 driver, so a composited home screen is blocked until that's its own project.

## Donor map (what to reuse)

- **SoC/platform:** `guowenye/UniXNU-sm8150-common@f8bc872` + `guowenye/UniXNU-raphael@1c435f2` (ADT-driven, BSD-2; raphael holds the XNU patches + the immutable-files-in-`/data` storage model). Note: UniXNU's `ArmCpu` hardcodes timer INTID **27** (SM8350) — epsilon needs **19**; parameterize from the ADT (raphael's `RaphaelBootCpu` does this).
- **Firmware:** `mu_andromeda SurfaceDuo1Pkg@613e2eac` (the real Duo 1). `mu_aloha` has no Duo 1 board.
- **Method / rewrite / non-PAC kernel:** `none24smu/UniXNU-zeta` + the `none24smu/UniXNU@port/ios27-compat` fork (plain-auth/small-pages/dyld-pager patches, CSSC trap-emulation, the rewrite scripts). **SM8350 hardware constants are not reusable** for SM8150.

## What's in this repo

- `UniXNU-epsilon/` — a UniXNU format-2 device repo (resolves in the `unixnu` tool): `board.toml`, `porting.toml`, `dts/epsilon.dts` (Track-A-verified values, rest `UNKNOWN-PH`), `scripts/make-epsilon-adt.py`, and `firmware/` (the `EpsilonUniXNUPkg` integration + the `EpsilonDiag` app + the M2 probe patches).
- `docs/` — `architecture.md`, `hardware.md`, `donor-analysis.md`, `compatibility.md`, `roadmap.md` (M0–M10), `recovery-backup-plan.md`, and ADRs `0001`–`0006`.
- `evidence/` — the device audit (live board DTB + decompiled source, cpuinfo, iomem, partitions), the host firmware builds (FD + FV reports), the ADT, and the T5/M2 device captures.
- Git: branch `iphoneduo-epsilon-phase1` (commits `26aacde`, `a67c61e`, `0310715`; later T5/M2 work is in the working tree).

## Open problems (ranked, for whoever continues)

1. **Kernel-sized payload transport** (the `x0`-stash shim / HOB re-publish) — prerequisite for loading any real kernel.
2. **PrePi FDT-read fault recovery** (in progress) — a temporary AArch64 sync-abort handler so a bad `x0` returns a status instead of hanging.
3. **Re-home the non-PAC xnu-12377 fork as a format-2 `arm64e`-no-PAC arch profile** (the fork predates format-2).
4. **SM8150 UFS driver** (read-only extents, raphael model) and **root mount** targeting `sda9` (which must be measured, and likely grown, before a full cache root).
5. **SMP**, **SMMU/DMA ownership**, **GPU Adreno 640** (UI blocker), **dual display**, **touch** — each a sizeable sub-project.
6. **iOS 27 userland:** the offline arm64e→arm64 rewrite + trap-emulation, re-validated for Kryo 485 (which lacks the same `arm64e.x1` features zeta emulates).

## Invitation

The physical-device groundwork (audit, firmware integration, a verified boot path, recovery backups) is done and documented. If you have the kernel/arm64e-rewrite expertise to take the Duo 1 further than we can solo — especially `none24smu`, given the Duo 2 work — this is meant as a running start, and collaboration or handoff is welcome. Device-specific questions (firmware, GPT, the `sda9` target, the audit data) can be answered from the evidence here.
