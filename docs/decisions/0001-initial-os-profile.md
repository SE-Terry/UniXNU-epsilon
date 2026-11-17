# ADR 0001 — OS targets: iOS 27 primary; `ipados-17.7.11-21H461` as optional diagnostic

**Date:** 2026-11-16 · **Status:** Accepted (revised 2026-11-16) · **Deciders:** Opus 4.8 orchestrator (per user direction)

> History note: this ADR originally selected `ipados-17.7.11-21H461` as *the* initial target. Revised: iOS 27 is the primary target (see ADR 0005); the arm64 profile is retained as an optional diagnostic stepping stone.

## Context
Project priority is iOS 27 on epsilon. SM8150 almost certainly lacks pointer authentication (ADR 0005), which native arm64e requires. We still need a way to exercise the boot chain with a known-good kernel.

## Decision
- **Primary target:** `ipados-27.0.1-24A446` (arm64e), via the offline arm64e→arm64 approach, with arm64e/PAC support on epsilon marked **UNVERIFIED-PENDING-HARDWARE**.
- **Optional diagnostic:** `ipados-17.7.11-21H461` (arm64; xnu-10063.141.1; 4 KiB pages) — the only arm64 profile UniXNU ships/maintains — used to prove UEFI→DarwinBoot→XNU entry and to resolve the device-repo config in the toolchain today.

## Rationale
- iOS 27 is the mission goal (CLAUDE.md) and the user's set priority.
- `ipados-17.7.11` is the lowest-risk way to get a resolvable config and a real kernel-entry test while the arm64e path is researched; it needs no PAC. **[A]**
- Keeping both lets host diagnostics proceed immediately without blocking on the (harder, unverified) arm64e work.

## Alternatives rejected
- **arm64 as the end goal** — contradicts the user's iOS 27 priority; demoted to diagnostic.
- **"iOS 16" UniXNU profile** — does not exist upstream (only in external `ios-sweet`); not used.

## Consequences
- The `UniXNU-epsilon` scaffold resolves against `ipados-17.7.11` (arm64) for diagnostics while declaring iOS 27 as the primary target in its porting record.
- Reaching iOS 27 depends on the arm64e→arm64 rewrite pipeline and on confirming PAC status on hardware (ADR 0005, E1).
