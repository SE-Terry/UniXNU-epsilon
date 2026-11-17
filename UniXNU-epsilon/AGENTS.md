# UniXNU-epsilon agent guide

This is the format-2 UniXNU device repository for Microsoft Surface Duo 1 (`epsilon`,
SM8150) in Project iPhoneDuo. It is a **scaffold**. Host resolution only. No device has
been contacted, and no hardware value has been measured on epsilon.

## Read first

1. The parent project's `../CLAUDE.md`, `../handoff.md` and `../docs/` (hardware,
   compatibility, decisions). Those rules win over this file.
2. `board.toml`, `porting.toml`, `dts/epsilon.dts`, and `patches/xnu/series`.
3. The upstream guides at `UniXNU/UniXNU@21d683d`: `docs/porting/device-repository.md`,
   `records.md`, `soc-support.md`, `device-tree.md`, and `arm64e.md`.
4. The SoC feed `guowenye/UniXNU-sm8150-common@f8bc872` (`AGENTS.md`, `soc.toml`,
   `dts/sm8150.dtsi`) and the template board `guowenye/UniXNU-raphael@1c435f2`.

## Rules

- **Targets:** iOS 27 (`ipados-27.0.1-24A446`, arm64e) is the primary target and is
  **UNVERIFIED-PENDING-HARDWARE** (no-PAC blocker). `ipados-17.7.11-21H461` (arm64) is the
  resolvable diagnostic profile. Never describe iOS 27 as working.
- **Values:** every value must be marked `V-src`, with a pinned citation, or `UNKNOWN-PH`.
  Never invent addresses. Never reuse SM8350/zeta constants. Use timer INTIDs 19
  (virtual) and 18 (NS-EL1), never 27.
- **Layout:** shared SM8150 code goes in the SoC feed, board code in `package/`, and OS ABI
  in the OS profile.
- **Safety latch:** keep `example = true` until the firmware recipe and every UNKNOWN-PH
  input are resolved.
- **Device safety:** no adb, fastboot, flashing, or partition change without explicit
  per-operation user approval. Do not edit the parent project's `progress.md` or
  `handoff.md`; only the orchestrator does that.
- **Evidence levels:** record source, host and device evidence separately in
  `porting.toml`. A resolving `unixnu status` is not a boot.

## Check commands (run from the UniXNU checkout)

```sh
./unixnu defconfig epsilon_defconfig -D <this dir> --out <scratch>
./unixnu fetch -D <this dir> --out <scratch>
./unixnu status --json -D <this dir> --out <scratch>
python3 scripts/porting-record.py check --device <this dir>
```
