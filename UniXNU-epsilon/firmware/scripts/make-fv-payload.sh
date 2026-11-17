#!/bin/bash
# Build the small FAT image that EpsilonUniXNU.fdf.inc embeds as an FV file.
#
#   make-fv-payload.sh <stage-dir> <out.fat> [size-KiB]
#
# <stage-dir> is copied to the volume root (expects EFI/UniXNU/unixnu.toml).
# Mirrors UniXNU scripts/unixnu/payload.py write_volume(): mtools, no mounting,
# fixed serial and timestamps, so the image is the same on every run.
# Default 128 KiB FAT12. The FD has about 0xA7000 bytes of *compressed* room in
# total (see ../README.md); keep the content small.
set -euo pipefail
STAGE=${1:?stage dir}; OUT=${2:?output image}; KIB=${3:-128}
for t in mformat mcopy mdir; do command -v "$t" >/dev/null || { echo "missing $t (apt-get install mtools)" >&2; exit 3; }; done
[ -f "$STAGE/EFI/UniXNU/unixnu.toml" ] || { echo "no EFI/UniXNU/unixnu.toml under $STAGE" >&2; exit 2; }
export MTOOLS_SKIP_CHECK=1 TZ=UTC SOURCE_DATE_EPOCH=315532800
find "$STAGE" -exec touch -h -d @315532800 {} +
TMP="$OUT.tmp.$$"; trap 'rm -f "$TMP"' EXIT
mformat -C -T $((KIB * 2)) -h 2 -s 16 -v EPSILONFV -N 45505346 -i "$TMP" ::
for e in "$STAGE"/*; do mcopy -i "$TMP" -s -Q -m "$e" ::/; done
mdir -i "$TMP" -/ :: >&2
SIZE=$(stat -c %s "$TMP")
[ $((SIZE % 512)) -eq 0 ] || { echo "image is not sector aligned" >&2; exit 4; }
mv "$TMP" "$OUT"; trap - EXIT
echo "payload: $OUT $SIZE bytes sha256=$(sha256sum "$OUT" | cut -d' ' -f1)"
