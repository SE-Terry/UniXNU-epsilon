# ADR 0006 — iOS 27 base identity & scope: plain-arm64e, not x1

**Date:** 2026-11-16 · **Status:** Feasibility candidate, updated 2026-11-17 · Builds on ADR 0005.

## Context
Choosing the restore identity may reduce the instruction-compatibility work. The Android audit gives a useful CPU baseline. Exact ISA equivalence with zeta and the instruction requirements of the selected userland still need validation.

## Decision
1. Evaluate upstream’s pinned **`ipados-27.0.1-24A446` (iPad17,3 / J820AP / chip 0x8142 / board 12)** first. Verify the expected plain-`arm64e` baseline against the exact archive and selected device identity. Conservative alternatives: iPad8,9 (A12Z) or iPhone12,1. **Do not** target iPhone19,4 (V68AP / chip 0x8160) for bring-up.
2. **A custom "iPhoneDuo" identity is a Phase-6 display-name layer** on top of a real identity, not a bring-up concern.
3. **iOS 27 stays primary.** Keep iPadOS 17 arm64 for boot-chain diagnostics and use iOS 26 as an optional comparison when it answers a specific compatibility question.

## Rationale
- iPhone19,4 ships an **`arm64e.x1`** userland requiring PAuth_LR, CPA, CSSC, TPRO, MTE-guards, RCPC2 — the bulk of zeta's iOS-27 work (trap-emulation at 645 ns–1.9 µs/CSSC vs 3–14 ns native; zeta itself says pervasive traps aren't viable → whole-function translation). **[SOURCE/CLAIMED]**
- 27.0.1 (24A446) also ships for **A12X/A12Z/A13** devices, which lack CSSC/PAuth_LR/CPA, so a plain-`arm64e` 27 userland bounded at ~ARMv8.3/8.4 must exist → gap shrinks to ≈ **PAC + JSCVT + FCMA = iOS-26 scope**. **[INFERENCE — settle by inspecting the cache]**
- No Apple Image4 personalization to satisfy: zeta's trust cache is an **owner-built static module keyed by CDHash**, not chip-id/board-id (`make-zeta-trust-bundle.py`, SOURCE). So identity choice is about **which userland/DeviceTree we inherit**, and we edit only display properties.
- `ipados-27.0.1-24A446` is already pinned upstream (`arch=arm64e`), minimizing re-homing.

## Correction to ADR 0005 framing
- "offline rewrite pipeline" → zeta's route is a **hybrid**: offline cache pointer-lowering + kernel page-in signed text-fixup table + kernel trap-emulation (CSSC/FJCVTZS) + RCPC2 branch islands + owner-re-signed daemons.
- iPhone19,4 is a **real** Apple identity (existence EXTERNAL-verified; contents not) — not fictional.

## Consequences / open items
- **First host experiment:** with the exact 24A446 restore archive kept outside Git, record the archive hash, BuildManifest identity and actual Mach-O CPU subtypes. Inventory instructions in the selected shared cache and in early executables such as dyld and launchd. Classify unsupported instructions by whether they are on required startup paths, feature-guarded or unresolved. Use that evidence to decide whether to prototype iOS 27 directly or run a focused iOS 26 comparison.
- The non-PAC xnu-12377 lives only in the `none24smu` fork (predates format-2) → re-homing it as a new arch profile is prerequisite work.
- An epsilon graphics path is still unverified. The Pixel 4 ios-flame donor reports Adreno 640 acceleration on iOS 16 through Turnip. Assess that reference separately from the first kernel-entry and userland milestones.
