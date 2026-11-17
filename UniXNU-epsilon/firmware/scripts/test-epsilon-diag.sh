#!/bin/bash
# Host test for the EpsilonDiag report logic. Touches no device.
#   test-epsilon-diag.sh [work dir]
# Compiles DiagCore.c natively with the host test and runs it. With python3
# available it also writes <work dir>/epsilon-diag-preview.png: the report laid
# out for a 1350x1800 panel from SAMPLE values (not measurements).
# This exercises formatting and decode only; it does not run the UEFI image.
set -euo pipefail
FW=$(cd "$(dirname "$0")/.." && pwd)
WORK=${1:-$(mktemp -d)}
mkdir -p "$WORK"
APP="$FW/EpsilonUniXNUPkg/Application/EpsilonDiag"
${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -DEPSILON_DIAG_HOST_TEST -I"$APP" \
  "$FW/tests/epsilon-diag/host_test.c" "$APP/DiagCore.c" -o "$WORK/epsilon-diag-host-test"
"$WORK/epsilon-diag-host-test" "$WORK/epsilon-diag-preview.rgb"
if command -v python3 >/dev/null; then
  python3 -I - "$WORK/epsilon-diag-preview.rgb" "$WORK/epsilon-diag-preview.png" <<'PY'
import struct, sys, zlib
w, h = 1350, 1800
raw = open(sys.argv[1], "rb").read()
assert len(raw) == w * h * 3
rows = b"".join(b"\x00" + raw[y * w * 3:(y + 1) * w * 3] for y in range(h))
def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
open(sys.argv[2], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                              + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
print("preview png:", sys.argv[2])
PY
fi
