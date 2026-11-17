# ADR 0004 — Non-destructive storage model (preserve Android + Windows)

**Date:** 2026-11-16 · **Status:** Accepted, sequencing updated 2026-11-17 · Priority 1 (data preservation)

## Context
The Duo has Android + Windows-on-ARM on a fully allocated LUN0 (`…userdata sda6, ESP sda7, Windows sda8`). The mission mandates preserving both OSes.

## Decision
Use **raphael's storage model**: XNU OS/SystemOS stored as **immutable, unencrypted files inside Android `/data`**, read via extents; **no GPT changes**; the Data volume lives in **RAM** (`RamBlock`). For first boots, prefer an entirely **RAM-backed root**.

## Rationale
- UniXNU's standard installer **shrinks `userdata` and requires it to be the last occupied partition** (`UniXNU:docs/porting/dual-system-partition.md`). With Windows on `sda8` after ESP, that requirement **likely fails** and endangers the dual-boot. **[A]**
- nabu/cepheus use `fastboot flash cust`; ios-sweet flashes `userdata` — all destructive. **None usable on epsilon.**
- raphael's model is the **only** donor approach that touches no partitions. **[B, source-grounded]**

## Update — device audit 2026-11-16 (changes the options)
The read-only audit found a **dedicated `unixnu` partition already present**: `sda9`, **~13.75 GiB**, as the **last** partition after Windows (`sda` order: `userdata 26 GiB → esp 260 MiB → win 64 GiB → unixnu 14 GiB`). This materially improves the storage options:
- The existing partition is a candidate for a future dedicated root. It permits evaluating a payload without the standard installer’s `userdata` shrink, subject to verification and size.
- **Still requires, before any write:** confirm `sda9`'s exact LBA range + that it is empty/ours, full GPT + partition backups with hashes (phase1-plan §9), and explicit per-operation user authorization. Writing only to `sda9` must be verified to not disturb the GPT or neighbouring partitions.
- The user reported creating `sda9 unixnu` for this project. The Android audit reports 14,761,852,928 B (13.75 GiB); contents and exact boundaries remain unverified.
- **Storage decision deferred.** Zeta’s reported root sizes exceed the current partition, but they are donor figures. Measure the actual epsilon package, including required metadata and working space, before claiming any stage fits or requires more space. Preserve Android, ESP and DuoWOA, and keep Windows and the GPT untouched.

## Open question (must verify on hardware)
Can the immutable-directory approach work under the Duo's **FBE-encrypted `/data`**? Less critical now that `sda9` exists as a root target; still relevant if we want a `/data`-resident option. Keep initial diagnostics rootless. Evaluate a RAM root separately against the measured memory budget.

## Consequences
- No on-disk root in Phase 1–3; bring-up uses RAM-disk root.
- Any future on-disk root requires a GPT audit + explicit, specific user authorization, and must still avoid repartitioning.
- Proprietary inputs (firmware blobs) obtained **read-only** (Android root shell or TWRP mass-storage read) after the GPT audit — **not** via destructive EDL dumps.
