# ADR 0003 — Firmware base: DuoWOA `SurfaceDuo1Pkg`, with a payload-discovery fix

**Date:** 2026-11-16 · **Status:** Accepted (Phase 0, pending host validation)

## Context
UniXNU must be embedded into a UEFI that drives epsilon's hardware. Two candidate bases exist, and DuoWOA's firmware breaks UniXNU's standard payload discovery.

## Decision
Build on **`WOA-Project/mu_andromeda_platforms` `SurfaceDuo1Pkg@613e2eac`** (the authoritative Surface Duo 1 firmware). Embed `UniXNUDxe`+`DarwinBoot` in its firmware volume and boot via RAM-only `fastboot boot`. Solve the lost-FDT payload-discovery gap with **(P1) an FV-embedded payload first** (no initrd/FDT dependency), keeping **(P2) a PrePi/shim fix** (stash ABL x0, re-publish the entry-argument HOB) as the path for a later RAM-disk root.

## Rationale
- **(A) mu_andromeda SurfaceDuo1Pkg** is the only base with epsilon's real patched `DisplayDxe` (left panel), N-Trig touch, and stock `UFSDxe`, plus proven Windows dual-boot. **[A, source]**
- **(B) Project-Aloha `mu_aloha_platforms` SurfaceDuo1Pkg** (used by guacamole) is a generic SM8150 package covering ~26 *other* boards and **does not contain the actual Surface Duo 1** — it would require re-solving display/touch/UFS from DuoWOA blobs anyway.
- **Gap (compatibility.md B-FW1):** DuoWOA's `BootConfigShim` overwrites x0 → ABL's FDT pointer is lost; AndromedaPkg has no FDT/initrd handling. An FV-embedded payload sidesteps this entirely for the first boot.

## Alternatives rejected
- **New Mu-Silicium `epsilonPkg` on `HanaPkg`** — matches UniXNU's contract but means re-porting display/touch/UFS from scratch; much larger effort. Deferred.
- **Build on Aloha (B)** — not the real Duo firmware.

## Validation (Phase 1 T1/T3 — host builds, VALIDATED)
- Stock FD builds at **0x2EF000** with **687,984 B (22.37%)** FVMAIN_COMPACT free (T1).
- Embedding UniXNU + UniXNUDxe + DarwinBoot + an `EpsilonPayloadDxe` + a 128 KiB payload FAT **fits**: FD unchanged at 0x2EF000, **649,248 B (21.11%)** free (T3). B-FW1 solved via P1 (`EpsilonPayloadDxe` publishes the payload FV file as read-only Block I/O → stock FAT exposes `\EFI\UniXNU\unixnu.toml`; no FDT/initrd/HOB).
- mu_aloha confirmed to lack the real Surface Duo 1 board → base (A) is correct.

## ⚠ Refinement: P1 is bring-up only; XNU boot needs P2
T3 proved the **FD cannot grow cheaply**: the loaders copy exactly `0x300000` (shim+FD+config), leaving ≤ ~`0xD000` before the Direct BootShim size, the binary-patched DualBoot `patchedkernel` header, and `uefiplat.cfg`'s `SEC Heap`@0x9FF00000 all have to change together. The FV headroom (~0.65 MB compressed) is a **hard limit**. A kernelcache is tens of MB, so **P1 cannot carry XNU**. Therefore:
- **P1 (FV-embedded payload):** use for the **T5 diagnostic + config only**.
- **P2 (BootConfigShim x0-stash + PrePi re-publish of the initrd HOB):** **REQUIRED before any XNU boot** so stock `UniXNURamDiskDxe` can take the ABL-provided ramdisk. Draft P2 only after the read-only audit reveals where ABL places the FDT/ramdisk (B-FW2).

## Consequences
- Reuse `guacamole:patches/aloha-guacamole.patch` only as the **integration pattern**.
- Secure Boot on by default → `UefiPlatEpsilonNoSb.cfg` packaging for bring-up, or start the diagnostic as an `fv:<GUID>` entry (FV-image SB exemption UNVERIFIED).
- `UniXNUDxe` makes UniXNU the default boot each boot (RAM-emulated vars; `PStore` not traced — confirm before device test).
- `fastboot boot` is RAM-only but may clear one GPT "unbootable" bit (B-FW3, PH) — covered by GPT backups.
