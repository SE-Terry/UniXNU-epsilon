# ADR 0002 — Primary SoC donor: sm8150-common + raphael

**Date:** 2026-11-16 · **Status:** Accepted (Phase 0)

## Context
Five SM8150 UniXNU ports exist (sm8150-common/raphael, guacamole, nabu, cepheus). We need a reusable SoC/platform layer for epsilon.

## Decision
Adopt **`guowenye/UniXNU-sm8150-common@f8bc872ce6`** as the SoC layer, paired with **`guowenye/UniXNU-raphael@1c435f2291`** for the XNU patch series, build recipes, and non-destructive storage model. Rename all raphael-specific class/compatible names to epsilon.

## Rationale
- Only SM8150 repo in UniXNU's **canonical SoC layout**; code is **ADT-driven with no hard-coded board MMIO** (grep-verified). **[A, source]**
- BSD-2-Clause-Patent (guacamole/nabu/cepheus have **no LICENSE** → treat as all-rights-reserved).
- raphael's storage model (immutable files in `/data`, no GPT change, RAM Data) is the **only** one compatible with preserving Android+Windows (see ADR 0004).

## Alternatives rejected
- **guacamole/ios-sweet as SoC base** — different kernel lineage (xnu-8796, 16 KiB), flashes `userdata`, no LICENSE. Kept only as integration pattern + boot reference.
- **nabu/cepheus** — CI failing (nabu) / stuck in early boot (cepheus); references only.
- **zeta** — SM8350, wrong SoC; method donor only (ADR 0003 / donor-analysis).

## Consequences
- sm8150-common is **thin**: its `patches/xnu/series` is empty; the working patches live in raphael and must be carried over.
- All addresses re-derived from epsilon's own ADT; timer INTID parameterised (compatibility.md B1).
- All donor boot results are CLAIMED — epsilon must re-verify.
