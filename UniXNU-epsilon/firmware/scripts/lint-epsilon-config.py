#!/usr/bin/env python3
"""Host lint for the epsilon UniXNU configuration (review findings M1, L3).

    lint-epsilon-config.py [firmware-dir] [--stage <payload stage dir>]

Rules (exit 1 on any violation):

  R1  No epsilon config declares a block slice. A slice is a Block I/O view
      onto a named GPT partition of the internal UFS disk (UniXNULib Slice.c);
      a writable one forwards WriteBlocks to Android/DuoWOA storage. The key
      UniXNU parses is `block_slice`; `slice` and `slices` are refused too so a
      schema rename cannot slip through. Checked at every table depth.
  R2  The payload unixnu.toml default is an enabled `action` entry, or exactly
      the read-only diagnostic: an `efi` entry whose path is
      `fv:<FILE_GUID of Application/EpsilonDiag/EpsilonDiag.inf>` and that
      carries no args, initrd, dtb, variables, kexts or patches. Nothing else
      may run unattended. (T5 widened this from "action only"; the diagnostic
      is the single allowed exception. Its config may only describe the menu.)
  R3  Every `efi`/`driver` path in the payload config is an `fv:<GUID>` path
      (FV images are not subject to FAT image verification; nothing is loaded
      from a FAT volume as a PE image during bring-up).
  R4  The vendor device-path GUID in EpsilonPayloadDxe.c matches the one in
      patches/unixnu/0001-*.patch, which is what config discovery accepts.
  R5  (--stage) The payload stage has no \\EFI\\BOOT\\BOOTAA64.EFI, so the
      payload volume never becomes a removable-media boot target.
  R6  The EpsilonDiag sources (comments stripped) name no service that
      changes machine state: storage and file protocols, variables, image
      load/start, ExitBootServices, ResetSystem, configuration tables,
      protocol installation, MSR. A text check, not a proof: it catches an
      edit that adds such a call, not a write through a raw pointer.

Host only. Touches no device.
"""
import pathlib
import re
import sys
import tomllib

FORBIDDEN_KEYS = {"block_slice", "slice", "slices"}


def walk(node, path, out):
    if isinstance(node, dict):
        for key, value in node.items():
            sub = f"{path}.{key}" if path else key
            if key.lower() in FORBIDDEN_KEYS:
                out.append(sub)
            walk(value, sub, out)
    elif isinstance(node, list):
        for i, value in enumerate(node):
            walk(value, f"{path}[{i}]", out)


def guid_words(text):
    m = re.search(r"0x([0-9A-Fa-f]{8}),\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*\{([^}]*)\}", text)
    if not m:
        return None
    tail = [int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(4))]
    return (int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), tuple(tail))


def main(argv):
    args = argv[1:]
    stage = None
    if "--stage" in args:
        i = args.index("--stage")
        stage = pathlib.Path(args[i + 1])
        del args[i:i + 2]
    fw = pathlib.Path(args[0] if args else pathlib.Path(__file__).resolve().parent.parent).resolve()
    repo = fw.parent
    errors = []

    tomls = sorted((fw / "EpsilonUniXNUPkg").rglob("*.toml"))
    tomls += [p for p in (repo / "board.toml",) if p.exists()]
    if stage is not None:
        tomls += sorted(stage.rglob("*.toml"))
    for path in tomls:
        try:
            data = tomllib.loads(path.read_text())
        except tomllib.TOMLDecodeError as e:
            errors.append(f"{path}: TOML parse error: {e}")
            continue
        hits = []
        walk(data, "", hits)
        for h in hits:
            errors.append(f"R1 {path}: forbidden slice key '{h}' (UFS write path; GPT unverified)")

    payload = fw / "EpsilonUniXNUPkg/Payload/unixnu.toml"
    cfg = tomllib.loads(payload.read_text())
    entries = cfg.get("entry", [])
    enabled_entries = [e for e in entries if e.get("enabled", True) is True]
    default = cfg.get("boot", {}).get("default")
    target = None
    if isinstance(default, str):
        target = next((e for e in enabled_entries if e.get("id") == default), None)
    elif type(default) is int and 0 <= default < len(enabled_entries):
        target = enabled_entries[default]
    elif default is None and enabled_entries:
        target = enabled_entries[0]
    diag_dir = fw / "EpsilonUniXNUPkg/Application/EpsilonDiag"
    gm = re.search(r"^\s*FILE_GUID\s*=\s*([0-9A-Fa-f-]{36})\s*$", (diag_dir / "EpsilonDiag.inf").read_text(), re.M)
    diag_guid = gm.group(1).upper() if gm else None
    if diag_guid is None:
        errors.append(f"R2 {diag_dir / 'EpsilonDiag.inf'}: FILE_GUID not found")

    def is_diag(entry):
        extra = set(entry) - {"id", "title", "type", "path", "enabled"}
        return (diag_guid is not None and entry.get("type", "efi") == "efi"
                and str(entry.get("path", "")).upper() == f"FV:{diag_guid}" and not extra)

    if target is None or not (target.get("type") == "action" or is_diag(target)):
        errors.append(f"R2 {payload}: default entry {default!r} is neither an enabled action nor the read-only diagnostic fv:{diag_guid}")
    if target is not None and is_diag(target):
        extra_tables = sorted(set(cfg) - {"version", "boot", "ui", "entry"})
        if extra_tables:
            errors.append(f"R2 {payload}: diagnostic config has extra top-level keys: {', '.join(extra_tables)}")
    for e in entries:
        if e.get("type") in ("efi", "driver") or "path" in e:
            p = str(e.get("path", ""))
            if not p.startswith("fv:"):
                errors.append(f"R3 {payload}: entry {e.get('id')!r} path {p!r} is not fv:<GUID>")
    for d in cfg.get("driver", []):
        p = str(d.get("path", ""))
        if not p.startswith("fv:"):
            errors.append(f"R3 {payload}: driver path {p!r} is not fv:<GUID>")

    dxe = (fw / "EpsilonUniXNUPkg/Driver/EpsilonPayloadDxe/EpsilonPayloadDxe.c").read_text()
    m = re.search(r"HW_VENDOR_DP.*?\n\s*(\{\s*0x[0-9A-Fa-f]+,.*?\}\s*\})", dxe, re.S)
    patches = sorted((fw / "patches/unixnu").glob("0001-*.patch"))
    pg = None
    if patches:
        pt = patches[0].read_text()
        pm = re.search(r"mEpsilonPayloadVendorGuid = \{\n\+(.*?)\n", pt)
        pg = guid_words(pm.group(1)) if pm else None
    dg = guid_words(m.group(1)) if m else None
    if dg is None or pg is None or dg != pg:
        errors.append(f"R4 vendor GUID mismatch or not found: EpsilonPayloadDxe={dg} patch={pg}")

    forbidden = re.compile(
        r"\b(ExitBootServices|ResetSystem|SetVariable|WriteBlocks|ReadBlocks|LoadImage|StartImage|"
        r"InstallConfigurationTable|Install(Multiple)?ProtocolInterfaces?|ConnectController|"
        r"SetMode|SetWatchdogTimer|SetTime|UpdateCapsule|WriteDisk|EFI_FILE_PROTOCOL)\b|"
        r"gEfi(BlockIo|DiskIo|SimpleFileSystem|FirmwareVolume)\w*|\bmsr\b", re.I)
    sources = sorted(p for p in diag_dir.rglob("*") if p.suffix in (".c", ".h", ".S", ".inf"))
    if not sources:
        errors.append(f"R6 no EpsilonDiag sources under {diag_dir}")
    for src in sources:
        text = src.read_text()
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        if src.suffix == ".inf":
            text = re.sub(r"#[^\n]*", " ", text)
        for hit in sorted({m.group(0) for m in forbidden.finditer(text)}):
            errors.append(f"R6 {src}: '{hit}' in the read-only diagnostic")

    if stage is not None:
        for p in stage.rglob("*"):
            if p.is_file() and str(p.relative_to(stage)).lower() == "efi/boot/bootaa64.efi":
                errors.append(f"R5 {p}: removable-media loader in payload volume")

    for e in errors:
        print("LINT-FAIL", e, file=sys.stderr)
    print(f"lint-epsilon-config: {len(tomls)} toml file(s), {len(errors)} violation(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
