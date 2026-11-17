# compatibility.md — SM8150 / epsilon compatibility & blockers

**Status:** Phase 0 (2026-11-16). Sources: Tracks A/B/C/D. Boot results from donors are CLAIMED unless noted.

## 1. OS profile options

| Profile | Arch | Role | Notes |
|---|---|---|---|
| **`ipados-27.0.1-24A446`** | arm64e | **PRIMARY TARGET** | Darwin 26 syscalls on xnu-12377; MH_FILESET kernelcache; requires FEAT_PAuth — **UNVERIFIED on epsilon** (§2). Only reachable on non-PAC HW via the offline arm64e→arm64 rewrite (§3). |
| `ipados-17.7.11-21H461` | arm64 | **Optional diagnostic** | Only arm64 profile upstream ships/maintains; xnu-10063.141.1, 4 KiB pages. Stepping stone to prove boot-chain/kernel-entry; needs no PAC. iPad7,1 (J120) identity. |
| `ipados-26.5.2-23F84` | arm64e | reference | What zeta targets; requires FEAT_PAuth; offline PAC-lowering. |
| `macos-27.3-26C5035r` | arm64e | n/a | Mac17,11. |

**Decision:** Primary target **`ipados-27.0.1-24A446` (arm64e)** with arm64e/PAC on epsilon marked **UNVERIFIED-PENDING-HARDWARE**; **`ipados-17.7.11-21H461` (arm64)** retained as an optional boot-chain diagnostic and the resolvable config for host tooling today. "iOS 16" is **not** an upstream profile. See `decisions/0001`, `decisions/0005`.

## 2. The decisive constraint: no pointer authentication (PAC)

- UniXNU's own doc: *"iOS and iPadOS 27 are arm64e only"* and the arm64e profile **requires FEAT_PAuth on every CPU** (`UniXNU@21d683d:docs/porting/arm64e.md`). **[A]**
- **SM8150 Kryo 485 = Cortex-A76 + A55, ARMv8.2-A** — PAC first appears in ARMv8.3. Triangulated from LLVM `AArch64Processors.td` (A76/A55 are `HasV8_2aOps`, no `FeaturePAuth`) + Linux `cputype.h`/`sm8150.dtsi` + **the Duo's own firmware built `-march=armv8.2-a`** (MU `Andromeda.dsc.inc:640`). **[A, but not yet from an Arm/Qualcomm primary PDF, and not yet measured on the Duo.]**
- Nearest measurement: zeta (SM8350) boot CPU `ID_AA64ISAR1_EL1=0x100001` → APA/API/GPA/GPI all 0 = no PAC. **[B]**
- **✅ MEASURED (E1, 2026-11-16):** **no PAC, confirmed.** Primary basis: MIDR parts `0x804`/`0x805` = **Cortex-A76/A55 = ARMv8.2-A**, which architecturally lacks FEAT_PAuth (introduced in v8.3) regardless of kernel. Corroborated: `/proc/cpuinfo` has no `paca`/`pacg` while same-era caps (`lrcpc`,`asimddp`) ARE present (so the feature read is meaningful). The definitive `ID_AA64ISAR1/2` register read is the T5 check. `evidence/device-audit-2026-11-16/`.

**Consequence:** arm64e iOS cannot run natively. The only known route is zeta's offline arm64e→arm64 binary rewrite (§3), which is incompatible with Phase 0/1 constraints.

## 3. iOS 27 feasibility — the primary target's central problem

- "iOS 27" in build terms = iPadOS 27.0.1 (24A446), Darwin 26 syscalls on an xnu-12377 source kernel (Apple's own 27 kernel is xnu-13432, not published). **[A, from profile pins; post-cutoff — uncertain.]**
- The only demonstrated non-PAC route (zeta, for iOS **26.5.2**, not 27): offline rewrite of launchd (2,388 edits), dyld (5,269), and the dyld shared cache (**29,577,370 of 29,577,371 PAC words lowered**), re-sign every file at 4 KiB pages, merged static trust cache, grow OS container to ~22.3 GB, `--csr-config 0x2`, plus device-local kernel patches. iOS 27 adds "arm64e.x1" (FEAT_PAuth_LR, CPA/CPA2, CSSC, RCPC2, JSCVT, SHA512, MTE, TPRO) requiring kernel trap-emulation (zeta measures ~6.45 ms vs ~26 µs native per CSSC block — a large perf cost). **[B, partly DRAFT.]**
- **Incompatible with preservation:** >20 GB root images and owner-approved `userdata` writes on a 6 GB-RAM dual-boot Duo.

**Verdict:** iOS 27 is the **primary target under active research**, not an established capability. Native arm64e is blocked by the (expected) absence of PAC; the only known route is the offline rewrite pipeline above, to be developed host-side against lawful images with **no device writes**. The feasibility research runs in parallel with boot-chain bring-up (no iPadOS-17-userland gate). The single highest-value fact is **E1** (on-device PAC check), which converts the core assumption from inference to measurement. See `decisions/0005`.

## 4. Blockers (ranked)

### Firmware / boot
- **B-FW1 (critical, V-src):** **DuoWOA is not Mu-Silicium.** `BootConfigShim.S` overwrites x0 with its config address, so ABL's **FDT pointer is lost**; AndromedaPkg has no FDT/initrd handling. UniXNU discovers its payload via the FDT `/chosen/linux,initrd-*` passed in an entry-argument HOB — **that mechanism does not work on epsilon unmodified.** *Fix:* small PrePi/shim change to stash x0 + publish the HOB, **or** an FV-embedded payload that avoids initrd. (MU `BootConfigShim.S`, `ArmPlatformHelper.S`)
- **B-FW2 (V-src):** initrd placement — if stock ABL follows Qualcomm logic, the ramdisk lands ~`0x85700000`, inside DuoWOA's "HLOS 1" BsData; payloads >~30 MiB spill into DBI Dump / Display Reserved (framebuffer). Ceiling ≈ stock "Kernel" 87 MiB − shim/FD/DT. **PH:** verify actual `linux,initrd-start/end`.
- **B-FW3 (PH):** `fastboot boot` caveat — Qualcomm ABL `CmdBoot` calls `ClearUnbootable()` which rewrites the active slot's GPT "unbootable" attribute. The vendored source is a Lahaina tree, not the Duo's stock ABL binary — **Duo behavior unverified.** Means even a RAM-only test may touch one GPT attribute bit.
- **B-FW4 (V-src):** boot-image format — DuoWOA uses header v2, pagesize 4096, raw kernel field, separate dtb field, `second_offset 0xf00000`. UniXNU/Mu-Silicium defaults (gzip, appended DTB) are **untested on the Duo's ABL** (PH).
- **B-FW5 (V-src):** Secure Boot is **on** by default (MS + OEMA0 keys); embed modules in the FV or build from `UefiPlatEpsilonNoSb.cfg`.

### Kernel / SoC
- **B1 (critical, code-V):** `ArmCpu` hardcodes **virtual-timer INTID 27** (SM8350) at `ArmCpu.cpp:143,:773`. epsilon GTDT = **virtual 19 / NS-EL1 18** (PPI 1/2/3/0). Must parameterise the INTID from the ADT (as nabu's `virtual-timer-intid` patch). Also: SM8150 Mu `HanaPkg` uses the **physical** timer — zeta's `VIRTUAL_TIMER_ONLY` may not apply; measure which PPI fires.
- **B2 (V-src):** EL1 under QHEE — no EL2; QHEE PSCI MEM_PROTECT bug (DuoWOA patches it only for the Windows kernel).
- **B3:** UFS — needs an XNU driver; SMMU stream ID **0x300** (not zeta's 0xE0); `NativeUfs-sm8350` not reusable. raphael's read-only-extents model is the starting point.
- **B4:** SMP deferred — no general SMMU/DMA ownership on any port; raphael is single-CPU by design (its UFS driver refuses to start if another CPU is on).
- **B5:** no persistent writable APFS on any donor.

### Userland / drivers
- **B6:** GPU (Adreno 640) blocked on every SM8150 port; UniXNU's Adreno driver targets A740. No graphics acceleration near-term.
- **B7:** dual-display — UEFI drives left panel only; `RightDisplayDxe` experimental+disabled. XNU dual-panel = new work (Phase 6).
- **B8:** touch — N-Trig exposes single-touch only natively; multitouch needs a heat-map processor with no XNU equivalent.
- **B9:** no usable physical UART + watchdog disabled → a hang needs a manual forced reset; logging via framebuffer/QR or kernel DWC3 console.

### Storage / data safety
- **B10 (critical):** UniXNU's standard installer **shrinks `userdata` and requires it to be the last occupied partition** — with Windows on `sda8` after ESP, this **likely fails** (`UniXNU:docs/porting/dual-system-partition.md`). nabu/cepheus use `fastboot flash cust`; ios-sweet flashes `userdata`. **None usable on epsilon.** Only safe model = raphael's immutable-files-in-`/data` + RAM Data volume; **open Q:** works under Duo's FBE-encrypted `/data`?
- **B11:** no spare storage slot for an on-disk root (LUN0 fully allocated). RAM-backed root (`RamBlock`) is the non-destructive route.

## 5. Assumptions requiring hardware verification (consolidated PH list)

1. No PAC (`ID_AA64ISAR1_EL1`, cpuinfo).
2. Which timer PPI fires (phys 18 vs virt 19) and whether QHEE allows CNTP at EL1.
3. `CNTFRQ_EL0` = 19.2 MHz; `CurrentEL` = 1.
4. Live DRAM banks + GOP framebuffer base (read `boot_args->Video`).
5. Stock ABL: accepts gzip/appended-DTB? clears unbootable bit? actual `linux,initrd` range + x0 value.
6. FD headroom for UniXNU modules inside the 0x2EF000 FD.
7. UFS version/takeover state after UEFI ExitBootServices; SMMU ownership.
8. Left-panel framebuffer still refreshing after EBS (MDP autorefresh armed?).
9. FBE-encrypted `/data` compatibility with the immutable-root model.
10. `dtbo` overlay contents.
