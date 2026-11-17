# package/firmware/ (placeholder)

No firmware code or payload is integrated in this scaffold.

## First acceptance test for the NEXT task (not this one)

**Goal:** show that UniXNU-owned code runs inside the DuoWOA firmware. Use a tiny
**read-only** payload built into the firmware volume (FV-embedded; this avoids
blocker B-FW1, where DuoWOA's `BootConfigShim` drops ABL's FDT pointer and breaks
initrd payload discovery).

Proposed shape (to be designed in Phase 1 T3/T5):

- A minimal DXE driver or UEFI application added to the
  `WOA-Project/mu_andromeda_platforms@613e2eac87` `SurfaceDuo1Pkg` DSC/FDF.
- It **only reads and reports**: the GOP mode/framebuffer base, `CurrentEL`, `CNTFRQ_EL0`,
  `ID_AA64ISAR1_EL1`/`ID_AA64ISAR2_EL1` (the PAC check, E1), MPIDR, and the UEFI memory
  map. It prints `UNIXNU:`-prefixed lines on the GOP console.
- It never writes storage, NV variables, GPT, or any MMIO.

Host-side acceptance (needs no device):

1. The stock epsilon FD builds reproducibly (Phase 1 T1; expected `SM8150_EFI.fd` size
   0x2EF000, sha256 to be recorded).
2. With the payload added, the FD still builds, the payload's FFS GUID appears in the
   FV, and the FD still fits the stock size limit (FD headroom is UNKNOWN-PH).

Device-side acceptance (separate step, **requires explicit user approval for each operation**,
RAM-only `fastboot boot`, after the recovery rehearsal T8):

3. The `UNIXNU:` lines appear on the left panel. Android and Windows still boot unchanged
   afterwards.

Until step 3 is observed, no epsilon hardware value may move from UNKNOWN-PH to verified.
