# UniXNU-epsilon: Microsoft Surface Duo 1 (SM8150)

This is the UniXNU format-2 device repository for Project iPhoneDuo.

**State:** this is a scaffold. It resolves with `unixnu status` on the host. No build has
been run, no boot has been attempted, and **no device has been contacted.**

| Item | Value |
|---|---|
| Board / SoC | `epsilon` / SM8150 (Kryo 485, ARMv8.2-A) |
| Primary target | iOS 27 — `ipados-27.0.1-24A446` (arm64e): **UNVERIFIED-PENDING-HARDWARE**. arm64e needs FEAT_PAuth, which SM8150 is inferred to lack. |
| Resolvable diagnostic profile | `ipados-17.7.11-21H461` (arm64, xnu-10063.141.1) |
| Framework | `UniXNU/UniXNU@21d683dff61c66f219f03ab9d8b9249026514788` |
| SoC feed | `guowenye/UniXNU-sm8150-common@f8bc872ce6254a84ecae839edcba9145a591e655` |
| Template board | `guowenye/UniXNU-raphael@1c435f2291edab0eda01bef865bdf4e7d69a9aa2` |
| Firmware base (planned) | `WOA-Project/mu_andromeda_platforms@613e2eac87` `SurfaceDuo1Pkg` with an FV-embedded payload |

`board.toml` keeps `example = true` on purpose. This makes `unixnu build`, `bootimg` and
`release` refuse to run until the UNKNOWN-PH inputs listed in `porting.toml` are measured
by the read-only device audit.

## Resolve (host only)

```sh
cd <UniXNU checkout at 21d683d>
./unixnu defconfig epsilon_defconfig -D <path>/UniXNU-epsilon --out <scratch>/out
./unixnu fetch -D <path>/UniXNU-epsilon --out <scratch>/out
./unixnu status --json -D <path>/UniXNU-epsilon --out <scratch>/out
python3 scripts/porting-record.py check --device <path>/UniXNU-epsilon
```

## Links

- [Device repository](https://unixnu.com/porting/device-repository)
- [Porting records](https://unixnu.com/porting/records)
- [Device tree](https://unixnu.com/porting/device-tree)
- [arm64e requirements](https://unixnu.com/porting/arm64e)
- [Bring-up](https://unixnu.com/porting/bring-up)
- `porting.toml` is the device record, and `AGENTS.md` lists the required reading.
