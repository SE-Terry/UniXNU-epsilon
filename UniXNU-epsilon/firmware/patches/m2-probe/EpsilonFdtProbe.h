// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef EPSILON_FDT_PROBE_H
#define EPSILON_FDT_PROBE_H

#include "EpsilonFdtProbeResult.h"

#define EPSILON_FDT_HEADER_BYTES  40
#define EPSILON_FDT_SIZE_LIMIT    (2U * 1024U * 1024U)

// BaseFdtLib links libfdt's complete validator but exports no FdtLib wrapper for it.
int fdt_check_full (CONST VOID *Fdt, UINTN BufferSize);

STATIC
UINT64
ReadBigEndian (
  CONST unsigned char  *Bytes,
  UINTN                Count
  )
{
  UINT64  Value;

  Value = 0;
  while (Count-- != 0) {
    Value = (Value << 8) | *Bytes++;
  }

  return Value;
}

STATIC
EPSILON_FDT_PROBE_RESULT
EpsilonInspectFdt (
  UINT64  Address,
  UINT64  WindowStart,
  UINT64  WindowEnd
  )
{
  EPSILON_FDT_PROBE_RESULT  Result = { EPSILON_FDT_ADDRESS, 0, 0, 0 };
  CONST unsigned char      *Fdt;
  CONST unsigned char      *StartProperty;
  CONST unsigned char      *EndProperty;
  UINT64                   Start;
  UINT64                   End;
  UINT32                   Total;
  int                      Chosen;
  int                      StartLength;
  int                      EndLength;

  if (((Address & 7) != 0) || (Address < WindowStart) ||
      (Address >= WindowEnd) || (WindowEnd - Address < EPSILON_FDT_HEADER_BYTES)) {
    return Result;
  }

  Result.Status = EPSILON_FDT_INVALID;
  Fdt = (CONST unsigned char *)(UINTN)Address;
  Total = (UINT32)ReadBigEndian (Fdt + 4, 4);
  if ((Total < EPSILON_FDT_HEADER_BYTES) || (Total > EPSILON_FDT_SIZE_LIMIT) ||
      (Total > WindowEnd - Address) || (fdt_check_full (Fdt, Total) != 0)) {
    return Result;
  }

  Result.TotalSize = Total;
  Result.Status = EPSILON_FDT_INITRD_PROPS;
  Chosen = FdtSubnodeOffset (Fdt, 0, "chosen");
  if (Chosen < 0) {
    return Result;
  }

  StartProperty = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
  EndProperty = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProperty == 0) || (EndProperty == 0) || (StartLength != EndLength) ||
      ((StartLength != 4) && (StartLength != 8))) {
    return Result;
  }

  Start = ReadBigEndian (StartProperty, StartLength);
  End = ReadBigEndian (EndProperty, EndLength);
  Result.InitrdStart = Start;
  Result.InitrdEnd = End;
  Result.Status = EPSILON_FDT_INITRD_RANGE;
  if ((Start < WindowStart) || (End > WindowEnd) || (Start >= End) ||
      ((Start < Address + Total) && (Address < End))) {
    return Result;
  }

  Result.Status = EPSILON_FDT_BOUNDS_OK;
  return Result;
}
#endif
