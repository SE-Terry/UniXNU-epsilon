# ADR 0005 — iOS 27 is the primary target; arm64e/PAC is the central open problem

**Date:** 2026-11-16 · **Status:** Accepted · **Supersedes:** the earlier "iOS 27 deferred" stance of this ADR (revised 2026-11-16 at user direction).

> History note: this ADR originally deferred iOS 27 until a verified `ipados-17.7.11` launchd on epsilon. That gate is **removed**. The 2026-11-17 sequencing update also makes iOS 26 comparison optional.

## Context
The project's long-term mission (CLAUDE.md) is iOS 27 on Surface Duo 1. The user has set iOS 27 as the **primary target now**, with feasibility research running in parallel rather than gated behind an iPadOS 17 userland milestone.

## Decision
1. **iOS 27 (`ipados-27.0.1-24A446`, arm64e) is the primary target** for epsilon.
2. **Plan around the expected absence of native PAC.** Android CPU identity and capabilities strongly support this conclusion. The T5 UEFI register capture is pending.
3. **No milestone gate:** iOS 27 feasibility research may begin immediately, in parallel with boot-chain bring-up.
4. **`ipados-17.7.11-21H461` (arm64) is retained as an optional boot-chain diagnostic** — a stepping stone to prove UEFI→DarwinBoot→XNU entry with a known-good arm64 kernel, not the end goal.
5. The Surface Duo 2 `none24smu/UniXNU-zeta@ios-27` branch is a **compatibility reference whose assumptions must be independently re-validated for SM8150** (different SoC: Kryo 485/Adreno 640 vs Kryo 680/Adreno 660).

## The central open problem (unchanged technical findings)
iOS 26/27 are **arm64e-only** and the arm64e profile requires **FEAT_PAuth** (`UniXNU@21d683d:docs/porting/arm64e.md`, **[A]**). Evidence that SM8150 lacks it:
- Kryo 485 = Cortex-A76/A55 = **ARMv8.2-A**; PAC first appears in ARMv8.3 (LLVM + Linux; **[A]**).
- The Duo's own firmware is compiled **`-march=armv8.2-a`** (MU `Andromeda.dsc.inc:640`, **[A]**).
- Nearest measurement: zeta (SM8350) reads `ID_AA64ISAR1_EL1=0x100001` → no PAC (**[B]**).
- **✅ MEASURED ON EPSILON (E1 done, 2026-11-16, read-only):** `/proc/cpuinfo` Features has **no `paca`/`pacg`** (`… lrcpc dcpop asimddp`); CPU confirmed Kryo 485 (parts 0x804/0x805, MIDR `0x51df80*e`). **Strong evidence that native PAC is absent.** A direct UEFI register read on the boot core remains pending. Evidence: `evidence/device-audit-2026-11-16/cpuinfo.txt`.

**Implication:** use zeta’s reported compatibility path as a reference for research. It combines offline adaptation with runtime compatibility work in source-built XNU. Validate the chosen iOS 27 archive and board requirements on the host, and measure the candidate package before selecting storage. See ADR 0004 and ADR 0006.

## Work split for the iOS 27 track
- **Feasibility (host-side):** inventory the exact 24A446 candidate’s instruction requirements against epsilon and identify required startup paths.
- **Hardware evidence (separate approval):** capture the boot core’s feature registers in UEFI with T5.
- **Diagnostic stepping stone (host, optional):** the `ipados-17.7.11` arm64 path to exercise the boot chain.

## Go / no-go for a *buildable* iOS 27 image on epsilon
Use the exact restore archive and resolved ISA requirements to assess a host prototype. Keep proprietary images outside Git. Use iOS 26 only if a control run would answer a defined question. Separately, use the iPadOS 17 arm64 diagnostic for logged XNU entry and test kernel transport and timer delivery before attempting iOS 27 on hardware.
