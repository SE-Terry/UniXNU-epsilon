// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef EPSILON_FDT_PROBE_RESULT_H
#define EPSILON_FDT_PROBE_RESULT_H

#define EPSILON_FDT_BOUNDS_OK     0
#define EPSILON_FDT_MEMORY_MAP    1
#define EPSILON_FDT_ADDRESS       2
#define EPSILON_FDT_INVALID       3
#define EPSILON_FDT_INITRD_PROPS  4
#define EPSILON_FDT_INITRD_RANGE  5

typedef struct {
  UINT32  Status;
  UINT32  TotalSize;
  UINT64  InitrdStart;
  UINT64  InitrdEnd;
} EPSILON_FDT_PROBE_RESULT;

#endif
