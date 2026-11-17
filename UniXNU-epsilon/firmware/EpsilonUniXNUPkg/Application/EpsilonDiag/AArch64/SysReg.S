//
// EpsilonDiag: read-only AArch64 system register accessors.
//
// Every register here is readable at EL1 (and above). Nothing is written.
// ID_AA64ISAR2_EL1 is given by encoding (op0=3, op1=0, CRn=0, CRm=6, op2=2)
// because the platform assembles for armv8.2-a, which predates the name. On a
// core without the register the encoding is in the ID register space reserved
// for future use (op0=3, op1=0, CRn=0, CRm=2..7), which the Arm ARM defines as
// read-as-zero. Whether the Duo's EL2 firmware traps it is UNVERIFIED, so
// EpsilonDiag reads it last.
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroLib.h>

ASM_FUNC(EpsilonDiagReadCurrentEl)
  mrs     x0, CurrentEL
  ret

ASM_FUNC(EpsilonDiagReadMidr)
  mrs     x0, midr_el1
  ret

ASM_FUNC(EpsilonDiagReadCntFrq)
  mrs     x0, cntfrq_el0
  ret

ASM_FUNC(EpsilonDiagReadPfr0)
  mrs     x0, id_aa64pfr0_el1
  ret

ASM_FUNC(EpsilonDiagReadPfr1)
  mrs     x0, id_aa64pfr1_el1
  ret

ASM_FUNC(EpsilonDiagReadIsar1)
  mrs     x0, id_aa64isar1_el1
  ret

ASM_FUNC(EpsilonDiagReadIsar2)
  mrs     x0, S3_0_C0_C6_2
  ret
