# Recovery backups before the first supervised T5 boot

**Status (2026-11-17):** Approved read-only recovery capture complete. All seven UFS GPTs and 59 saved files verified. T5 remains pending separate approval. The kernel boot parameters show the Duo booted unlocked.

The T5 image is temporary. The stock bootloader's slot-metadata behavior still needs verification on this unit. Collect and verify the recovery artifacts before relying on a normal power cycle.

## Inventory and destination

- Start from the running, authorized Android system. Confirm the intended handset, current Android build, active slot, normal boot path and the user's most recent DuoWOA boot result. Resolve any existing Windows repair issue as part of the baseline or document the limits of recovery.
- Inventory the actual by-name mapping, byte size and hardware logical block size for each relevant UFS LUN. Check that required device nodes exist before capture. A missing or ambiguous alias is a stop for that item.
- Store captures and the full manifest off-device in a private, dated Mac directory outside Git. Radio and device-state images and their raw inventory may contain device-specific information. Use a separate physical backup location for critical recovery material if available.

## Capture and verify after approval

| Item | Capture | Verification |
|---|---|---|
| Primary and backup GPT for each relevant LUN | Protective MBR plus the primary and secondary headers and their entry arrays. Calculate all ranges from the device's logical block size, device length and GPT header fields | Verify header and array CRCs, reciprocal header LBAs, valid entry-array bounds, expected byte counts and matching partition entries. Avoid fixed 34-sector assumptions |
| Boot and recovery state | Resolve and stream the required boot-chain partitions from Phase 1 §9, plus the ESP. Record missing aliases, slot and exact source sizes | Confirm remote read success and exact byte counts. Hash each completed capture and verify the static copies against a second read where feasible |
| Device-specific state | Capture the required small EFS/calibration partitions from Phase 1 §9 privately | Confirm the by-name mapping, read status, byte counts and SHA-256. Record changes across distinct captures instead of assuming live state is static |
| Android and Windows data | Confirm existing backup coverage and recovery route with the user | A GPT backup records the layout. Separate backups protect filesystem contents |
| `sda9 unixnu` | Record the existing layout for later storage work | Defer the contents scan and full hash until there's a measured candidate payload and a reason to use this partition |

Stream approved reads to temporary files on the Mac. Keep a capture only after a successful read and exact size check, then record the source, offset and length, byte count and SHA-256. A hash establishes integrity of the saved bytes. Restore feasibility also requires a viable recovery route. Have the appropriate stock boot image and official DuoWOA recovery assets available off-device and match them to the current configuration.

## Capture result — 2026-11-17

The approved Android session captured GPT metadata for all seven UFS LUNs, boot and verification partitions, the ESP, and the scoped device-specific recovery data. All GPT header and entry-array CRCs were valid. All 59 saved files passed local SHA-256 readback. The first `persist` capture differed from its immediate second read. Its repeat capture matched both bracketing source hashes. Both copies and the audit manifest are stored in the private Downloads backup directory outside Git, with all capture hashes rechecked after the move. GPT metadata confirms `userdata` (sda6), ESP (sda7), Windows (sda8), and the 13.748 GiB `unixnu` partition (sda9). Filesystem-level recovery for Android and Windows is a separate matter.

## Recovery image staged — 2026-11-17

The private Downloads backup now includes the [DuoWOA 2601.12 Duo 1 Fast Boot release](https://github.com/WOA-Project/SurfaceDuo-Releases/releases/tag/2601.12) in `duowoa-2601.12/`. The archive matched the upstream SHA-256 and the extracted `uefi.img` passed header and ZIP integrity checks. The captured `boot_a.img`, `boot_b.img` and `esp.img` passed checksum readback and represent the installed state. A separate factory-stock Android boot image was not staged. Both the official DuoWOA image and the experimental T5 image are available on the Mac. Neither was booted in this session.

## Capture and exit for the separate T5 test

- Record video before the image starts. Capture the UniXNU menu, the panel output, the last visible checkpoint, and `UNIXNU: DIAG.DONE=1` if shown. Take readable stills of the top and bottom of the panel.
- If output stops at `DIAG.NEXT=ISAR2`, record it as an incomplete report. UEFI or hypervisor behavior remains unverified. The prior lines still contain the boot-core identity, ISAR1, GOP and memory-map data.
- Confirm a physical power-off method before proceeding. Use the menu's Power off action if input works. Returning to firmware can continue into Windows under the test image. If input is absent, the diagnostic may return to a menu that relaunches it. Follow the confirmed physical exit plan and verify the normal boot path afterward.

## Go / no-go

**T5 pending separate approval.** The host wrapper and recovery capture passed. The kernel boot parameters show `unlocked` and Verified Boot `orange`. The Play Integrity Fix module changes the conflicting Android runtime properties. The user reports DuoWOA boots normally. Confirm physical access to the Power button and obtain separate approval before running T5. [Microsoft’s Duo screen recovery guidance](https://support.microsoft.com/en-us/surface/troubleshoot-surface-duo-screen-issues) documents a fallback of holding Power for 30 seconds if the screen will not come on.
