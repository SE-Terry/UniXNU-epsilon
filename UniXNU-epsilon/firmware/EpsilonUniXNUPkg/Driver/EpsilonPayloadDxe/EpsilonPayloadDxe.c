/** @file
  Publish the UniXNU payload FAT image carried inside the firmware volume as a
  read-only block device.

  Surface Duo 1 (epsilon) runs the DuoWOA firmware (mu_andromeda_platforms).
  Its BootConfigShim overwrites x0/x1 with the appended boot-config pointer and
  size, so the FDT pointer ABL passed is gone before SEC runs and the usual
  UniXNU transport (FAT in the Android ramdisk, found through
  /chosen/linux,initrd-*) cannot work unmodified (project blocker B-FW1).

  This driver removes that dependency for small payloads: the FAT image is an
  FV FREEFORM file with one RAW section. It is located by file GUID through
  EFI_FIRMWARE_VOLUME2_PROTOCOL, which hands back a pool copy, so nothing here
  uses a physical address, an FDT, an initrd or a HOB.

  The image only has to hold what fits in the FD headroom: unixnu.toml and a
  diagnostic payload. A real XNU payload is far larger than the FD and needs
  the initrd transport (see firmware/README.md, option P2).

  Not booted on hardware. A successful build is not a boot.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/FirmwareVolume2.h>

#define PAYLOAD_BLOCK_SIZE  512
#define PAYLOAD_MEDIA_ID    0x45505346   // "EPSF"

extern EFI_GUID  gEpsilonPayloadFileGuid;

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} PAYLOAD_DEVICE_PATH;
#pragma pack()

//
// The vendor GUID identifies the payload volume to UniXNU config discovery.
// It must equal mEpsilonPayloadVendorGuid in firmware/patches/unixnu/0001-*.patch
// (checked by firmware/scripts/lint-epsilon-config.py, rule R4).
//
STATIC PAYLOAD_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } },
    { 0x5AEDCA82, 0xBD6F, 0x4986, { 0xBA, 0x69, 0x3B, 0x08, 0xEE, 0xC1, 0xF8, 0x03 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC UINT8               *mBase;
STATIC UINTN               mSize;
STATIC EFI_BLOCK_IO_MEDIA  mMedia;

STATIC
EFI_STATUS
EFIAPI
PayloadReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PayloadRead (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  if (MediaId != mMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((BufferSize % PAYLOAD_BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Lba > mMedia.LastBlock) ||
      ((BufferSize / PAYLOAD_BLOCK_SIZE) > (mMedia.LastBlock - Lba + 1)))
  {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mBase + (UINTN)Lba * PAYLOAD_BLOCK_SIZE, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PayloadWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
PayloadFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mMedia,
  PayloadReset,
  PayloadRead,
  PayloadWrite,
  PayloadFlush
};

/**
  Read the RAW section of the payload file from whichever FV holds it.
**/
STATIC
EFI_STATUS
ReadPayload (
  OUT UINT8  **Base,
  OUT UINTN  *Size
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     *Handles;
  UINTN                          Count;
  UINTN                          Index;
  EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv;
  VOID                           *Buffer;
  UINTN                          BufferSize;
  UINT32                         AuthStatus;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < Count; Index++) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv))) {
      continue;
    }

    //
    // A NULL buffer makes the FV driver allocate the copy from pool.
    //
    Buffer     = NULL;
    BufferSize = 0;
    Status     = Fv->ReadSection (Fv, &gEpsilonPayloadFileGuid, EFI_SECTION_RAW, 0, &Buffer, &BufferSize, &AuthStatus);
    if (!EFI_ERROR (Status)) {
      *Base = Buffer;
      *Size = BufferSize;
      break;
    }

    Status = EFI_NOT_FOUND;
  }

  FreePool (Handles);
  return Status;
}

EFI_STATUS
EFIAPI
EpsilonPayloadDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Status = ReadPayload (&mBase, &mSize);
  if (EFI_ERROR (Status)) {
    //
    // No payload in the FD is a supported build: UniXNU then shows its
    // built-in menu. Stay loaded-and-idle rather than failing the dispatch.
    //
    DEBUG ((DEBUG_ERROR, "EPSPAYLOAD: no payload file %g in any FV: %r\n", &gEpsilonPayloadFileGuid, Status));
    return EFI_SUCCESS;
  }

  if ((mSize < 2 * PAYLOAD_BLOCK_SIZE) || ((mSize % PAYLOAD_BLOCK_SIZE) != 0) ||
      (mBase[510] != 0x55) || (mBase[511] != 0xAA))
  {
    DEBUG ((DEBUG_ERROR, "EPSPAYLOAD: 0x%lx bytes is not a sector-aligned FAT image\n", (UINT64)mSize));
    FreePool (mBase);
    mBase = NULL;
    return EFI_SUCCESS;
  }

  mMedia.MediaId          = PAYLOAD_MEDIA_ID;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  //
  // The image is a superfloppy FAT volume, but sector 0 carries the 0x55AA
  // signature, so a partition driver may still parse it as an MBR and create
  // a child partition handle. This flag does not prevent that, and the
  // platform PartitionDxe is a Qualcomm binary whose behaviour here is not
  // verified. Either way the FAT volume's device path starts with the vendor
  // node above, which is what the board-patched UniXNU config discovery
  // matches (firmware/patches/unixnu/0001-*.patch).
  //
  mMedia.LogicalPartition = TRUE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = PAYLOAD_BLOCK_SIZE;
  mMedia.IoAlign          = 0;
  mMedia.LastBlock        = (mSize / PAYLOAD_BLOCK_SIZE) - 1;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "EPSPAYLOAD: FV payload block device, 0x%lx bytes: %r\n", (UINT64)mSize, Status));
  if (EFI_ERROR (Status)) {
    FreePool (mBase);
    mBase = NULL;
  }

  return EFI_SUCCESS;
}
