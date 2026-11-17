#!/usr/bin/env python3
"""Host-only: generate and check the epsilon Apple device tree (ADT). Project iPhoneDuo, Phase 1 T4.

This script never talks to a device. An ADT that decodes is not a boot.

WHY A DEVICE SCRIPT EXISTS
  UniXNU format 2 generates the ADT centrally (UniXNU@21d683d scripts/unixnu/devicetree.py
  generate(), CLI scripts/make-adt.py, guide docs/porting/device-tree.md). That generator is used
  here UNCHANGED for every node and property. It cannot be run on dts/epsilon.dts directly:

      make-adt.py: error: ADT handoff requires one contiguous DRAM range

  dts/epsilon.dts records the three DRAM banks of the pinned MU static map (with a 52 MiB carveout
  between bank 0 and bank 1), and generate() accepts exactly one <base size> tuple in /memory.

METHOD (adapted from none24smu/UniXNU-zeta scripts/make-zeta-adt.py; no zeta constant is used)
  zeta publishes /chosen dram-base/dram-size as ONE envelope from the lowest bank base to the
  highest bank end, holes included, and documents that XNU uses the pair only for is_dram_addr().
  Checked for the diagnostic profile's kernel, xnu-10063.141.1 @d8b80295 (V-src):
  osfmk/arm/arm_init.c:641-642 copies the pair into gDramBase/gDramSize, read only by
  is_dram_addr() (osfmk/mach/arm/vm_param.h:303), whose callers are wimg_to_pte()
  (osfmk/arm/pmap/pmap.c:5813, uses at 5824-5854; it picks the PTE memory attribute for
  IO-type mappings). Nothing there allocates or maps RAM from the envelope. Newer XNU
  (iOS 27 track) may consume gDramBase differently; recheck there.
  This script does the same, but derives the envelope from the compiled DTB instead of hard-coding:

    1. read the compiled hardware DTB (dtc output of dts/epsilon.dts + soc:dts/sm8150.dtsi);
    2. build an in-memory "ADT view" of it whose single /memory reg is the envelope of the banks
       (optionally with only the boot CPU enabled), and write that view next to the output;
    3. merge arch profile + SoC soc.toml + OS profile + board.toml (same order as
       scripts/unixnu/profiles.py load()) and call the common generate() on the view;
    4. decode the result with an independent parser and check the bootloader-contract nodes.

  dts/epsilon.dts is not modified; it stays the bank-level record.

VALUES
  Hardware values come only from the DTB. Nothing here is measured on epsilon. Inputs that the
  board cannot supply yet are reported as UNKNOWN-PH and are NOT invented:
    - unique-chip-id / chip-id: private software seeds, never committed. Omitted unless passed
      with --unique-chip-id/--chip-id (or UNIXNU_UNIQUE_CHIP_ID). --placeholder-identity emits a
      self-describing fake pair for structure checks only; such a file must never be booted.
    - random-seed: 256 fresh host os.urandom() bytes per run (so the output is not bit-reproducible).
      This is a placeholder for structure checks. The seed is fixed once the file is written, so
      every boot of the same ADT would reuse it; DarwinBoot must overwrite /chosen/random-seed
      with fresh entropy on every boot (review finding L5; not implemented upstream yet).
    - darwinboot-memory-size, root-matching, GOP/framebuffer, boot-CPU policy, CNTFRQ: not set.

EXAMPLE
  dtc -I dts -O dtb -i <sm8150-common>/dts -o hardware.dtb dts/epsilon.dts
  python3 -I scripts/make-epsilon-adt.py --unixnu <UniXNU@21d683d> --soc <sm8150-common> \
      --dtb hardware.dtb --packed-data-ramdisk --trust-cache --output kernel.macho.adt --dump
"""
import argparse
import os
import struct
import subprocess
import sys
import tomllib
import zlib
from pathlib import Path

sys.dont_write_bytecode = True   # the UniXNU checkout is read-only input

HERE = Path(__file__).resolve().parents[1]
PLACEHOLDER_UNIQUE_CHIP_ID = int.from_bytes(b'UNKN-PH\x01', 'little')   # reads "UNKN-PH" in a hex dump
PLACEHOLDER_CHIP_ID = int.from_bytes(b'UNPH', 'little')


def fail(message):
    raise SystemExit(f'make-epsilon-adt: {message}')


# ----------------------------------------------------------------------------------------------
# FDT view
# ----------------------------------------------------------------------------------------------
def write_fdt(root):
    """Serialize a unixnu.devicetree.FdtNode tree as a version-17 FDT (no memory reservations)."""
    strings, offsets, structure = bytearray(), {}, bytearray()

    def emit(entry):
        structure.extend(struct.pack('>I', 1) + entry.name.encode() + b'\0')
        structure.extend(bytes(-len(structure) % 4))
        for key, raw in entry.properties.items():
            if key not in offsets:
                offsets[key] = len(strings)
                strings.extend(key.encode() + b'\0')
            structure.extend(struct.pack('>III', 3, len(raw), offsets[key]) + raw)
            structure.extend(bytes(-len(structure) % 4))
        for child in entry.children:
            emit(child)
        structure.extend(struct.pack('>I', 2))

    emit(root)
    structure.extend(struct.pack('>I', 9))
    reserve_at = 40
    structure_at = reserve_at + 16
    strings_at = structure_at + len(structure)
    total = strings_at + len(strings)
    header = struct.pack('>10I', 0xd00dfeed, total, structure_at, strings_at, reserve_at, 17, 16, 0,
                         len(strings), len(structure))
    return header + bytes(16) + bytes(structure) + bytes(strings)


def dram_banks(devicetree, root):
    memory = [entry for entry in root.children if entry.strings('device_type') == ['memory'] and entry.enabled()]
    if len(memory) != 1:
        fail('the hardware DTB needs exactly one enabled memory node')
    banks = devicetree.registers(memory[0])
    if not banks or any(size == 0 for _, size in banks):
        fail('the memory node has no usable bank')
    banks = sorted(banks)
    for (base, size), (following, _) in zip(banks, banks[1:]):
        if base + size > following:
            fail(f'DRAM banks overlap at {following:#x}')
    return memory[0], banks


def adt_view(devicetree, root, single_cpu):
    """Replace the bank list with its envelope; optionally keep only the first enabled CPU."""
    memory, banks = dram_banks(devicetree, root)
    base, top = banks[0][0], max(address + size for address, size in banks)
    holes = [(a + s, b - (a + s)) for (a, s), (b, _) in zip(banks, banks[1:]) if b > a + s]
    address_cells = devicetree.cell_count(root, '#address-cells', 2)
    size_cells = devicetree.cell_count(root, '#size-cells', 1)
    if address_cells != 2 or size_cells != 2:
        fail('the root must use two address cells and two size cells')
    memory.properties['reg'] = struct.pack('>4I', base >> 32, base & 0xffffffff, (top - base) >> 32, (top - base) & 0xffffffff)
    disabled = []
    if single_cpu:
        cpus = next((entry for entry in root.children if entry.name == 'cpus'), None)
        enabled = [entry for entry in cpus.children if entry.enabled()] if cpus else []
        for entry in enabled[1:]:
            entry.properties['status'] = b'disabled\0'
            disabled.append(entry.name)
    return banks, (base, top - base), holes, disabled


# ----------------------------------------------------------------------------------------------
# Independent ADT decoder (does not share code with the generator)
# ----------------------------------------------------------------------------------------------
class AdtNode:
    def __init__(self):
        self.properties, self.children = {}, []

    @property
    def name(self):
        return self.properties.get('name', b'?\0').rstrip(b'\0').decode('ascii', 'replace')

    def child(self, name):
        return next((entry for entry in self.children if entry.name == name), None)

    def find(self, path):
        entry = self
        for part in filter(None, path.split('/')):
            entry = entry.child(part) if entry else None
        return entry


def decode_adt(blob):
    def parse(cursor):
        if cursor + 8 > len(blob):
            fail('ADT decode: truncated node header')
        property_count, child_count = struct.unpack_from('<II', blob, cursor)
        cursor += 8
        entry = AdtNode()
        for _ in range(property_count):
            if cursor + 36 > len(blob):
                fail('ADT decode: truncated property header')
            raw_name, length = struct.unpack_from('<32sI', blob, cursor)
            length &= 0x7fffffff   # Apple marks placeholder properties in the top bit
            cursor += 36
            if cursor + length > len(blob):
                fail('ADT decode: property value runs past the end')
            name = raw_name.split(b'\0', 1)[0].decode('ascii')
            if name in entry.properties:
                fail(f'ADT decode: duplicate property {name}')
            entry.properties[name] = blob[cursor:cursor + length]
            cursor += (length + 3) & ~3
        if 'name' not in entry.properties:
            fail('ADT decode: node without a name property')
        for _ in range(child_count):
            child, cursor = parse(cursor)
            entry.children.append(child)
        return entry, cursor

    root, end = parse(0)
    if end != len(blob):
        fail(f'ADT decode: {len(blob) - end} trailing bytes')
    return root


REDACT = {'random-seed', 'unique-chip-id', 'nvram-proxy-data'}


def render(key, raw, placeholder_identity):
    if key in REDACT and not (key == 'unique-chip-id' and placeholder_identity):
        return f'<{len(raw)} bytes, not printed>'
    if not raw:
        return '<empty>'
    text = raw.rstrip(b'\0')
    if raw.endswith(b'\0') and text and all(part and all(32 <= byte < 127 for byte in part) for part in text.split(b'\0')):
        return ', '.join(f'"{part.decode()}"' for part in text.split(b'\0'))
    if len(raw) == 4:
        return f'<{struct.unpack("<I", raw)[0]:#x}>'
    if len(raw) % 8 == 0 and len(raw) <= 32:
        return '<' + ' '.join(f'{value:#x}' for value in struct.unpack(f'<{len(raw) // 8}Q', raw)) + '>'
    return raw[:24].hex() + (f'... ({len(raw)} bytes)' if len(raw) > 24 else '')


def dump(entry, placeholder_identity, depth=0, lines=None):
    lines = [] if lines is None else lines
    pad = '  ' * depth
    lines.append(f'{pad}{entry.name} {{')
    for key, raw in entry.properties.items():
        if key != 'name':
            lines.append(f'{pad}  {key} [{len(raw)}] = {render(key, raw, placeholder_identity)}')
    for child in entry.children:
        dump(child, placeholder_identity, depth + 1, lines)
    lines.append(pad + '}')
    return lines


# ----------------------------------------------------------------------------------------------
# Bootloader-contract checklist
# ----------------------------------------------------------------------------------------------
def nvram_ok(chosen):
    image = chosen.properties.get('nvram-proxy-data', b'')
    words = [chosen.properties.get(key, b'') for key in ('nvram-bank-size', 'nvram-bank-count', 'nvram-current-bank')]
    if len(image) != 8192 or [len(word) for word in words] != [4, 4, 4]:
        return False
    if [struct.unpack('<I', word)[0] for word in words] != [8192, 1, 0]:
        return False
    def header(offset, name):
        signature, checksum, blocks, label = struct.unpack_from('<BBH12s', image, offset)
        raw = bytearray(image[offset:offset + 16])
        raw[1] = 0
        total = sum(raw)
        while total > 255:
            total = (total & 255) + (total >> 8)
        return signature == 0x70 and checksum == total and label.rstrip(b'\0') == name and blocks
    return (header(0, b'nvram') and header(32, b'common') and header(32 + 16 + 2048, b'free')
            and struct.unpack_from('<I', image, 16)[0] == zlib.adler32(image[20:]))


def checklist(root, placeholder_identity, source):
    """Rows of (status, item, detail). FAIL rows make the run fail; UNKNOWN-PH rows fail only with --strict."""
    rows = []
    chosen = root.find('/chosen') or AdtNode()

    def length(node, key):
        return len(node.properties[key]) if node and key in node.properties else None

    def row(ok, item, detail, missing='FAIL', present='PRESENT'):
        rows.append((present if ok else missing, item, detail))

    seed = chosen.properties.get('random-seed', b'')
    row(len(seed) >= 256 and any(seed), '/chosen/random-seed',
        f'{len(seed)} bytes; host os.urandom at generation time, not device entropy; DarwinBoot must refresh it per boot. '
        'xnu-10063 (d8b80295): PE_get_random_seed() pexpert/gen/pe_gen.c:177-189 panics "no random seed" if absent; '
        'bootseed_init_bootloader() osfmk/prng/prng_random.c:58-64 panics below 64 bytes. '
        'The 256-byte floor is kept from the zeta/xnu-12377 note (not re-verified here)',
        present='PRESENT-HOSTGEN')
    identity = length(chosen, 'unique-chip-id') == 8 and length(chosen, 'chip-id') == 4
    row(identity, '/chosen/unique-chip-id + chip-id',
        ('PLACEHOLDER pair (structure check only; never boot this file)' if placeholder_identity else
         'supplied on the command line' if identity else
         'omitted: private software seed not provided (gethostuuid() would block forever)'),
        missing='UNKNOWN-PH', present='PLACEHOLDER' if placeholder_identity else 'PRESENT')
    for key in ('certificate-production-status', 'effective-production-status-ap',
                'certificate-security-mode', 'effective-security-mode-sep'):
        raw = chosen.properties.get(key, b'')
        row(len(raw) == 4, f'/chosen/{key}', f'{struct.unpack("<I", raw)[0] if len(raw) == 4 else "absent"} ({source["os"]})')
    row(nvram_ok(chosen), '/chosen NVRAM proxy', 'empty CHRP bank, 8192 bytes, 1 bank, headers and Adler-32 verified')
    for key in ('dram-base', 'dram-size'):
        raw = chosen.properties.get(key, b'')
        row(len(raw) == 8, f'/chosen/{key}',
            f'{struct.unpack("<Q", raw)[0]:#x} = envelope of the V-src MU bank map; live banks UNKNOWN-PH' if len(raw) == 8 else 'absent')
    for key in ('osenvironment', 'crypto-hash-method', 'amfi-only-platform-code'):
        row(key in chosen.properties, f'/chosen/{key}', source['os'])
    memory_map = root.find('/chosen/memory-map')
    for key in ('PackedRAMDisk', 'TrustCache'):
        other = ' (dense RAMDisk placeholder present instead)' if key == 'PackedRAMDisk' and length(memory_map, 'RAMDisk') == 16 else ''
        row(length(memory_map, key) == 16, f'/chosen/memory-map/{key}',
            '16 zero bytes, filled by DarwinBoot' if length(memory_map, key) == 16 else 'absent' + other)
    row(root.find('/chosen/manifest-properties') is not None, '/chosen/manifest-properties', 'empty node read by AppleImage4')
    defaults = root.find('/defaults')
    raw = defaults.properties.get('kern.maxvnodes', b'') if defaults else b''
    row(len(raw) == 4, '/defaults/kern.maxvnodes', f'{struct.unpack("<I", raw)[0] if len(raw) == 4 else "absent"} ({source["os"]})')
    product = root.find('/product')
    row(product is not None and len(product.properties) > 1, '/product',
        f'{source["os"]} identity, not an epsilon identity (AppleImage4 panics without the node)')
    row(root.find('/options') is not None, '/options', 'empty platform options node')
    cpus = root.find('/cpus')
    count = len(cpus.children) if cpus else 0
    boot = cpus.child('cpu0') if cpus else None
    row(boot is not None and boot.properties.get('state') == b'running\0', '/cpus/cpu0',
        f'{count} CPU node(s); MPIDR reg V-src, timebase inherited from the SoC feed (epsilon CNTFRQ UNKNOWN-PH)')
    if count > 1:
        psci = root.find('/psci')
        row(psci is not None and psci.properties.get('method') in (b'smc\0', b'hvc\0'), '/psci', 'required with more than one CPU')
    arm_io = root.find('/arm-io')
    row(arm_io is not None and length(arm_io, 'ranges') == 24, '/arm-io',
        f'{len(arm_io.children) if arm_io else 0} exported hardware node(s) (unixnu,adt-name)')
    # Not part of the common format-2 contract; zeta's format-1 script sets them. Reported for the audit list only.
    for path, key, note in (('/chosen', 'darwinboot-memory-size', 'owned window size, [payload.adt] owned_memory_size: UNKNOWN-PH'),
                            ('/chosen', 'root-matching', 'root volume UUID, [payload.adt] root_match_volume_uuid: UNKNOWN-PH'),
                            ('/chosen', 'kernel-ctrr-to-be-enabled', 'set by zeta (format 1); common generator needs an [adt.chosen.u32] entry'),
                            ('/defaults', 'sleep-disabled', 'set by zeta (format 1); common generator needs an [adt.defaults.u32] entry'),
                            ('/', 'buttons', 'node not declared by the 17.7.11 profile')):
        node = root.find(path)
        present = node is not None and (key in node.properties or node.child(key) is not None)
        rows.append(('PRESENT' if present else 'NOT-EMITTED', f'{path.rstrip("/")}/{key}', note))
    return rows


# ----------------------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dtb', type=Path, required=True, help='compiled dts/epsilon.dts (dtc -i <soc>/dts)')
    parser.add_argument('--output', type=Path, required=True, help='ADT to write (kernel.macho.adt)')
    parser.add_argument('--board', type=Path, default=HERE / 'board.toml')
    parser.add_argument('--unixnu', type=Path, default=None, help='UniXNU checkout; default board.toml [package] unixnu')
    parser.add_argument('--soc', type=Path, default=None, help='SoC feed checkout (its soc.toml is merged when given)')
    parser.add_argument('--os-profile', default=None, help='profile directory name; default board.toml [board] os_profile')
    parser.add_argument('--unique-chip-id', default=os.environ.get('UNIXNU_UNIQUE_CHIP_ID'),
                        help='private platform seed, hex u64 (or UNIXNU_UNIQUE_CHIP_ID); never commit it')
    parser.add_argument('--chip-id', default=None, help='platform identity seed, hex u32; required with --unique-chip-id '
                                                        'unless board.toml [payload.adt] chip_id is set')
    parser.add_argument('--placeholder-identity', action='store_true',
                        help='emit a self-describing FAKE unique-chip-id/chip-id; the output name must contain "placeholder"')
    parser.add_argument('--data-ramdisk', action='store_true', help='dense RAMDisk memory-map placeholder')
    parser.add_argument('--packed-data-ramdisk', action='store_true', help='PackedRAMDisk memory-map placeholder')
    parser.add_argument('--trust-cache', action='store_true', help='TrustCache memory-map placeholder')
    parser.add_argument('--single-cpu', action='store_true', help='export only the first enabled CPU (no /psci)')
    parser.add_argument('--dump', action='store_true', help='print the decoded node tree')
    parser.add_argument('--strict', action='store_true', help='also fail on UNKNOWN-PH contract rows')
    args = parser.parse_args()

    board = tomllib.loads(args.board.read_text())
    if board.get('os_overrides'):
        fail('board.toml [os_overrides] is not handled here; use "unixnu payload"')
    unixnu = (args.unixnu or args.board.parent / board.get('package', {}).get('unixnu', 'sources/UniXNU')).resolve()
    if not (unixnu / 'scripts/unixnu/devicetree.py').is_file():
        fail(f'no UniXNU checkout at {unixnu}; pass --unixnu')
    pinned = board.get('package', {}).get('unixnu_revision')
    head = subprocess.run(['git', '-C', str(unixnu), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    if pinned and head != pinned:
        fail(f'UniXNU checkout is at {head or "an unknown revision"}, board.toml pins {pinned}')
    sys.path.insert(0, str(unixnu / 'scripts'))
    from unixnu import devicetree
    from unixnu.profiles import merge
    from unixnu.util import Error

    os_name = args.os_profile or board['board']['os_profile']
    os_file = unixnu / 'profiles/os' / os_name / 'profile.toml'
    os_profile = tomllib.loads(os_file.read_text())
    arch = os_profile.get('os', {}).get('arch', 'arm64')
    layers = [unixnu / 'profiles/arch' / arch / 'profile.toml', *([args.soc / 'soc.toml'] if args.soc else []), os_file, args.board]
    config = {}
    for path in layers:
        config = merge(config, tomllib.loads(path.read_text()))

    # Command-line inputs are merged over [payload.adt] in memory only; board.toml keeps them UNKNOWN-PH.
    options = config.setdefault('payload', {}).setdefault('adt', {})
    if args.placeholder_identity:
        if args.unique_chip_id or args.chip_id:
            fail('--placeholder-identity excludes a real identity')
        if 'placeholder' not in args.output.name:
            fail('--placeholder-identity: the output file name must contain "placeholder"')
        args.unique_chip_id, args.chip_id = f'{PLACEHOLDER_UNIQUE_CHIP_ID:x}', f'{PLACEHOLDER_CHIP_ID:x}'
    if args.chip_id:
        options['chip_id'] = args.chip_id
    if args.unique_chip_id and 'chip_id' not in options:
        fail('--unique-chip-id needs --chip-id (board.toml [payload.adt] chip_id is UNKNOWN-PH)')
    if args.packed_data_ramdisk and args.data_ramdisk:
        fail('choose one of --data-ramdisk and --packed-data-ramdisk')
    if args.packed_data_ramdisk:
        options['packed_data_ramdisk'] = True

    try:
        root = devicetree.read_fdt(args.dtb.read_bytes())
        banks, (base, size), holes, disabled = adt_view(devicetree, root, args.single_cpu)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        view = args.output.with_name(args.output.name + '.view.dtb')
        view.write_bytes(write_fdt(root))
        blob = devicetree.generate(view, config, unique_chip_id=args.unique_chip_id,
                                   data=args.data_ramdisk or args.packed_data_ramdisk, trust=args.trust_cache)
    except Error as error:
        fail(f'common generator: {error}')
    args.output.write_bytes(blob)

    tree = decode_adt(args.output.read_bytes())
    print(f'epsilon ADT: {args.output} ({len(blob)} bytes), UniXNU {head[:12]}, OS profile {os_name} ({arch})')
    print('config layers: ' + ', '.join(str(path) for path in layers))
    print('DRAM banks (DTB): ' + ', '.join(f'{address:#x}+{length:#x}' for address, length in banks))
    print(f'DRAM envelope published as dram-base/dram-size: {base:#x}+{size:#x}'
          + (' (holes inside it: ' + ', '.join(f'{a:#x}+{s:#x}' for a, s in holes) + ')' if holes else ''))
    if disabled:
        print('CPUs left out by --single-cpu: ' + ', '.join(disabled))
    print(f'ADT view DTB: {view}')
    if args.dump:
        print('\n'.join(dump(tree, args.placeholder_identity)))
    rows = checklist(tree, args.placeholder_identity, {'os': f'OS profile {os_name}'})
    width = max(len(item) for _, item, _ in rows)
    print('contract checklist:')
    for status, item, detail in rows:
        print(f'  {status:<15} {item:<{width}}  {detail}')
    failed = [item for status, item, _ in rows if status == 'FAIL' or (args.strict and status == 'UNKNOWN-PH')]
    if failed:
        fail('contract rows not satisfied: ' + ', '.join(failed))
    print('device: not contacted. Decoding is not a boot.')


if __name__ == '__main__':
    main()
