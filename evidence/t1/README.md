# T1 evidence — stock epsilon UEFI FD build

**Status:** host build VALIDATED (orchestrator-reproduced hash). NOT booted, NOT flashed. Device not contacted.
**Date:** 2026-11-16 · **Built by:** porting-engineer (claude-fable-5-2) · **Verified by:** Opus 4.8 orchestrator (independent `shasum`).

## Artifact
- `SM8150_EFI.fd` — raw firmware device image.
  - **Size:** 3,076,096 bytes = `0x2EF000` (exactly `[FD.SM8150_EFI] Size` in `SurfaceDuo1.fdf`).
  - **SHA-256:** `3202ee47b90be7b2406e7950db0e93f3616fda1987bf3d4b440f7f024c969c78`
  - Identifies *this local build only*; not compared against a DuoWOA release binary; bit-reproducibility unproven (single build).

## FV headroom (for embedding UniXNU/DarwinBoot later)
From the build's own FV report (`FV/FVMAIN_COMPACT.Fv.txt`, build log lines 5423–5424):
- `FVMAIN_COMPACT` [77% full]: 3,076,096 (0x2EF000) total, 2,388,112 (0x247090) used, **687,984 (0xA7F70) free = 22.37%**.
- `FVMAIN` (inner, auto-sized/compressed) [99% full]: 2,120 B free — alignment slack, **not** a real constraint.
- `FVMAIN_COMPACT` is the volume that fills the fixed FD; its ~688 KB free is the budget before `PcdFdSize`/FD size must grow (and any loader assumption about `0x2EF000` with it). Current compression ≈ 5.8:1; new modules compress differently, so this is not an uncompressed budget.

## Provenance
- Source: `WOA-Project/mu_andromeda_platforms@613e2eac87b882dd1c755ba9234750bd05f5bb5d` (pin).
- Build script: repo-tracked `build_fd_epsilon.sh` → `PlatformBuild.py TARGET=RELEASE` (BOARD=SurfaceDuo1), stuart `Success`, "0110 Images Verified", ~7m38s.
- Toolchain (amd64 Docker image `iphoneduo-t1-builder:1`, Ubuntu 24.04 under Rosetta on Apple Silicon): clang 18.1.3 (CLANGPDB) + LLD, aarch64-linux-gnu-gcc 13.3.0, Python 3.13.18, Mono 6.12, stuart ext_deps Mu-Basetools v2026080000.0.4 / mu_nasm 20016.1.1 / edk2-acpica-iasl 20230628.0.1.
- Submodule pins recorded in `../../progress.md` (P1-T1) and the full `--recursive` list in the build log.
- **Two runtime-only env fixes** (no source/script change), needed for mono under amd64 emulation: `MONO_ENV_OPTIONS=--interpreter` (JIT aborts otherwise) and `cert-sync` at start (empty mono trust store → NuGet TLS failure otherwise). Fold into the image at next Dockerfile revision.

## Also in scratchpad / Docker (not copied here; may be cleared on reboot)
- Full `fd-build.log`, stuart `SETUPLOG.txt`/`UPDATE_LOG.txt`/`BUILDLOG_*`, and large `*.Fv` binaries: `<scratchpad>/t1/out/` and `<scratchpad>/t1/logs/`.
- Reusable build tree in Docker volume `iphoneduo_t1_work`; successful run container `iphoneduo_t1_run` (exit 0). Stale `iphoneduo_t1_run_attempt1/2` can be `docker rm`'d.
