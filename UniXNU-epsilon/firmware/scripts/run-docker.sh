#!/bin/bash
# Host wrapper for build-fd-in-container.sh. Touches no device.
#   run-docker.sh <unixnu checkout @21d683dff6> <out dir>
# Env: UNIXNU_BOOT_OPTION (default 0), UNIXNU_FV_PAYLOAD (1), PAYLOAD_KIB (128),
#      PACKAGE_NOSB (0; 1 = also build UefiPlatEpsilonNoSb.bin, never the SB variant),
#      EPSILON_HANDOFF_PROBE (0; 1 = capture entry argument, 2 = inspect bounded FDT),
#      T3_VOLUME (iphoneduo_t3_work).
set -euo pipefail
UNIXNU=${1:?path to UniXNU checkout}; OUT=${2:?output dir}
FW=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT"
VOL=${T3_VOLUME:-iphoneduo_t3_work}   # set T3_VOLUME for a clean build volume
docker volume create "$VOL" >/dev/null
exec docker run --rm --platform linux/amd64 --name iphoneduo_t3_run \
  -e UNIXNU_BOOT_OPTION="${UNIXNU_BOOT_OPTION:-0}" -e PACKAGE_NOSB="${PACKAGE_NOSB:-0}" -e UNIXNU_FV_PAYLOAD="${UNIXNU_FV_PAYLOAD:-1}" -e PAYLOAD_KIB="${PAYLOAD_KIB:-128}" \
  -e EPSILON_HANDOFF_PROBE="${EPSILON_HANDOFF_PROBE:-0}" \
  -v iphoneduo_t1_work:/t1:ro -v "$VOL":/t3 \
  -v "$UNIXNU":/in/unixnu:ro -v "$FW":/in/firmware:ro -v "$OUT":/out \
  iphoneduo-t1-builder:1 bash /in/firmware/scripts/build-fd-in-container.sh
