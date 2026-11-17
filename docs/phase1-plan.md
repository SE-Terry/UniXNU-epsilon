# phase1-plan.md — Phase 1 engineering plan (UEFI foundation)

**Status:** 2026-11-17. Stock and integrated firmware, device scaffold, ADT, and the T5 diagnostic have host evidence. The T5 diagnostic and normal Android return are verified on hardware. XNU entry remains pending.

**Primary target:** iOS 27. Phase 1 prepares the Duo 1 firmware and loader. Use iPadOS 17 arm64 for the first logged XNU entry. In parallel, inspect the exact iPadOS 27.0.1 candidate on the host. iOS 26 is an optional comparison.

## 1. Guiding constraints

- Preserve Android and DuoWOA. Defer storage writes, flashing, repartitioning, and Windows resizing.
- Require specific approval for any device operation. A completed build is host evidence. Record device execution separately.

## 2. Task breakdown

| Task | Status | Evidence or next action |
|---|---|---|
| T1 stock UEFI and T2 scaffold | Host validated | Stock FD and format-2 resolution recorded in the journal |
| T3 firmware integration and T4 ADT | Host validated | Hardened FD, decoded diagnostic ADT and build logs |
| T5 diagnostic | Host validated | Supervised FD, lint, unit tests, and both build manifests in `evidence/t5/` |
| T6 runtime checks | Pending | Review the diagnostic code and boot configuration. Use a host fixture where applicable and describe its limits |
| T7 read-only Android audit | Complete | `evidence/device-audit-2026-11-16/`. UEFI registers and GOP, exact GPT boundaries, bootloader behavior and recovery readiness remain open |
| T8 recovery readiness | Pending | Establish a recovery path, backups and an exit procedure before a device boot |
| T9 supervised T5 boot | Verified on hardware | `UNIXNU: DIAG.DONE=1`, return to UniXNU, and verified Android boot |
| M2–M3 kernel transport and XNU entry | 32 MiB loader fixture passed in QEMU, and `ABL.X0` captured on hardware | Validate the FDT and initrd, then aim for an iPadOS 17 `arm_init` marker. [M2 evidence](../evidence/m2/README.md) |

## 3. Selected donor sources (pinned)

- Framework: `UniXNU/UniXNU@21d683dff6`; profile `ipados-17.7.11-21H461` (arm64).
- SoC: `guowenye/UniXNU-sm8150-common@f8bc872ce6` + `guowenye/UniXNU-raphael@1c435f2291` (XNU patches, storage model).
- Firmware base: `WOA-Project/mu_andromeda_platforms@613e2eac87` (`Platforms/SurfaceDuo1Pkg`).
- Integration pattern + payload lessons: `wochgn/UniXNU-guacamole@c8a9d18135` (`patches/aloha-guacamole.patch`).
- Method/scripts: `none24smu/UniXNU-zeta@eb489285a1` (ADT/memmap/probe/log tooling) — *methodology only, no SM8350 constants.*

## 4. Proposed repository layout (`UniXNU-epsilon`, format 2)

```
UniXNU-epsilon/
  board.toml            # schema_version=2; soc_source=sm8150-common; os_profile=ipados-17.7.11-21H461
  porting.toml          # porting record (upstream-required)
  feeds.toml            # pins sm8150-common + UniXNU commits
  AGENTS.md
  dts/epsilon.dts       # extends sm8150-common/dts/sm8150.dtsi; epsilon memory/reserved/boot-cpu
  patches/xnu/series    # epsilon-renamed from raphael bootcpu/cpu-ownership/ufs/pmgr
  scripts/
    make-epsilon-adt.py     # from zeta method; required /chosen props
    import-epsilon-uefi.py  # only if needed
    capture-usb-log.py      # from zeta
  firmware/              # DSC/FDF hooks to embed UniXNU into SurfaceDuo1Pkg
```

## 5. Kernel-sized payload design for M2–M3

- Keep the fixed `0x2EF000` FD and use its embedded payload for the diagnostic. The supervised T5 build left 646,264 bytes free. The iPadOS 17 kernel and ADT need a separate transport.
- The Direct wrapper preserves the incoming argument through entry to the config shim. The opt-in probe measured `0x849B5000` on hardware. Validate the candidate FDT range and lifetime before accessing it, then validate and reserve the full initrd interval and measure its payload capacity.
- Preserve discovery isolation. The hardened UniXNU loader accepts the embedded diagnostic volume identified by its vendor GUID. A second read-only RAM volume needs an explicitly identified source and tests for rejection of unrelated FAT volumes, invalid or overlapping ranges, and writable block targets.
- The T5 DarwinBoot binary accepted and entered a synthetic 32 MiB Mach-O in QEMU and refused a corrupt ADT. The first hardware kernel checkpoint is a logged `arm_init` on the boot core. Record actual timer delivery and acknowledgment separately from the DT declaration of virtual INTID 19. Refresh the ADT random seed per boot before XNU entry.

## 6. Host prerequisites

Use the x86-64 Linux Mu toolchain for epsilon firmware and pin dependencies. Use the UniXNU toolchain for the kernel. Keep proprietary restore images and generated userland outside Git.

## 7. Test evidence

- T5 host: firmware size, hash, volume headroom, module manifest, config lint and diagnostic logic tests. Independently rebuilt firmware bytes can differ, so identify the exact reviewed artifact by hash.
- T5 hardware, after approval: record video from the menu through the diagnostic, take square-on stills, and record the completed marker or last checkpoint. T5 samples the UEFI boot core and its current memory map. It does not establish timer interrupt delivery.
- M2/M3: record the known input image and ADT digests, loading evidence, and the first XNU marker independently.

## 8. Acceptance criteria

- Host foundation: recorded firmware builds, resolved format-2 config, decoded ADT, and T5 build, lint and logic-test results.
- Diagnostic execution: a captured `UNIXNU: DIAG.DONE=1` and return to the normal device boot path.
- Kernel checkpoint: captured `arm_init` in the iPadOS 17 diagnostic. Record the transport and timer evidence independently.

## 9. Recovery prerequisites for a temporary boot

1. User confirms: current `boot_x` contents (stock vs dual-boot image), active slot, Android build.
2. Read-only backups exist **with sha256**: per-LUN GPT (primary+backup), `boot_a/b`, `dtbo_a/b`, `vbmeta_a/b`, `abl/xbl/xbl_config_a/b`, `persist`, `modemst1/2`, `fsg`, `fsc`, `devinfo`, `misc`, `frp`, ESP (sda7), Windows (sda8) if capacity allows. Use an already running, authorized read-only environment where possible. A temporary recovery boot requires its own approval and recovery assessment.
3. Off-device: official DuoWOA `uefi.img` (Fast Boot zip) + stock `boot.img`; note `OEMEP_Uninstall.ffu` (guides warn it may worsen a partitioned device).
4. Forced-reset combo + timing confirmed. **Do NOT press Vol-Up at the DuoWOA logo** (enters FFU full-flash tool).
5. **EDL is NOT a recovery path** (no OEM-signed firehose programmer exists in any reviewed source). Never write XBL/ABL/tz/hyp/aop/devcfg/keymaster/persist/EFS/devinfo/GPT.

A temporary boot does not flash the supplied image. The stock ABL may still update slot metadata, and recovery behavior on this unit remains unverified. Confirm the current Android/DuoWOA baseline and recovery route before relying on a power cycle.

## 10. GNG-1.5 for the first diagnostic boot

- Review T5 code and the approved default `fv:` entry. Confirm the launcher appears in the supervised `UNIXNU_BOOT_OPTION=1` firmware and is absent from the safe build.
- Verify the exact NoSb diagnostic boot-image wrapper and hash. Verify payload-volume-only config discovery, no block slices, and no fallback `BOOTAA64.EFI` in the payload FAT.
- Complete the recovery prerequisites and secure explicit approval for the test. Plan the exit before boot. Use Power off if menu input works. Returning to firmware can advance BDS into Windows.

## 11. Next host actions

1. Retain the first-run [T5 evidence](../evidence/t5/device-2026-11-17/report.md). Any additional device run requires its own approval.
2. Use the handoff probe measurement to design a bounded FDT and initrd check. Implement and validate the measured transport before attempting XNU entry.
3. For iOS 27, proceed from the confirmed 24A446 BuildManifest identity to an ISA inventory of its exact shared cache and early executables. Compare iOS 26 only if a defined compatibility question warrants the comparison.
4. Measure the packaged candidate and memory requirements before selecting root storage. Keep the existing GPT, Windows and Android untouched.
