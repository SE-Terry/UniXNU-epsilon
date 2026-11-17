# scripts/

`package.sh` is the unmodified UniXNU device packaging script
(`UniXNU/UniXNU@21d683d examples/device/scripts/package.sh`). It calls `unixnu release`,
which refuses this board while `board.toml` keeps `example = true`.

## make-epsilon-adt.py (host-only, Phase 1 T4)

Generates the Apple device tree (ADT) with the unchanged UniXNU format-2 common generator
(`UniXNU@21d683d scripts/unixnu/devicetree.py generate()`), then decodes it with an
independent parser and prints the `/chosen` bootloader-contract checklist.

The common generator rejects `dts/epsilon.dts` as it stands ("ADT handoff requires one
contiguous DRAM range"), because the DTS records three DRAM banks. The script builds an
in-memory view of the compiled DTB whose single memory range is the envelope of those banks
(zeta's method; no zeta constant), and feeds that view to the generator. `dts/epsilon.dts`
is not changed. Identity seeds are never invented: without `--unique-chip-id`/`--chip-id`
the pair is omitted and reported as UNKNOWN-PH.

```sh
dtc -I dts -O dtb -i <sm8150-common>/dts -o hardware.dtb dts/epsilon.dts
python3 -I scripts/make-epsilon-adt.py --unixnu <UniXNU@21d683d> --soc <sm8150-common> \
  --dtb hardware.dtb --packed-data-ramdisk --trust-cache --output kernel.macho.adt --dump
```

A decoding ADT is not a boot. `unixnu payload` itself still fails on this board for the same
DRAM reason, and stays blocked by `example = true`.

## To be ported next (not present yet)

These follow the methodology of `none24smu/UniXNU-zeta@eb489285a1` (Surface Duo 2,
SM8350). Only the method transfers. Every SM8350 constant must be re-derived for
SM8150/epsilon. Do not copy zeta addresses, INTIDs, stream IDs or memory maps.

| Planned script | Purpose | epsilon inputs it needs |
|---|---|---|
| `capture-usb-log.py` | Host-side log capture over USB (DWC3 `0xA600000`, peripheral mode). This replaces the UART, which has no documented retail pads (blocker B9) | Kernel-side DWC3 console. USB host mode on Duo is UNKNOWN-PH |

The raphael archive recipes (`build-raphael-cpu.py`, `build-raphael-pthread.py`) are also
not referenced from `board.toml` until epsilon equivalents exist.

No script here may write to a device. Device steps are orchestrator-owned and need user
approval for each operation (parent project `CLAUDE.md` §5).
