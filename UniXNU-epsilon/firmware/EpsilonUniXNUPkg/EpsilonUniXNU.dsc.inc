## @file
#  DSC hooks: UniXNU + DarwinBoot + FV-embedded payload for Surface Duo 1 (epsilon).
#
#  Included at the END of Platforms/SurfaceDuo1Pkg/SurfaceDuo1.dsc
#  (WOA-Project/mu_andromeda_platforms@613e2eac87) by
#  ../patches/0001-SurfaceDuo1Pkg-embed-UniXNU.patch.
#
#  Module sources come from UniXNU/UniXNU@21d683dff6 through PACKAGES_PATH:
#    <unixnu>/package            -> UniXNUPkg
#    <unixnu>/drivers/DarwinBoot -> DarwinBootPkg   (submodule 5243c68e)
#    <device repo>/firmware      -> EpsilonUniXNUPkg
#
#  Switches (set in SurfaceDuo1.dsc [Defines] by the patch, or -D / BLD_*_ on the
#  stuart command line):
#    UNIXNU_BOOT_OPTION  0 = (bring-up default) UniXNUDxe is not built; BootOrder is
#                            exactly the stock one, so a stray key press cannot
#                            hold the device in the UniXNU menu (review M3).
#                        1 = opt-in only, for a supervised test: embed UniXNUDxe,
#                            which writes Boot#### and puts UniXNU first in
#                            BootOrder on every boot (RAM-emulated variables, so
#                            nothing persists across a reboot).
#    UNIXNU_FV_PAYLOAD   1 = embed EpsilonUniXNUPkg/Payload/payload.fat as an FV file.
#
#  Host build only. Not booted on hardware.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[LibraryClasses.common]
  # The only library class the stock platform does not already map. FdtLib
  # (BaseFdtLib), HobLib (DxeHobLib), DevicePathLib, PrintLib, UefiLib, PcdLib and
  # the entry-point libraries all resolve from Andromeda.dsc.inc as-is.
  # Built from the board-patched copy (firmware/patches/unixnu/0001-*.patch, applied
  # by scripts/build-fd-in-container.sh): config only from the EpsilonPayloadDxe
  # volume, block_slice refused (review M1).
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf

[PcdsFixedAtBuild.common]
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel|"Surface Duo"

[Components.common]
  # Boot menu and configuration reader (UEFI_APPLICATION, FILE_GUID 9B665204-...).
  UniXNUPkg/Application/UniXNU/UniXNU.inf

!if $(UNIXNU_BOOT_OPTION) == 1
  # Registers the FV application as Boot#### and moves it to the front of BootOrder.
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
!endif

  # XNU loader (UEFI_DRIVER; produces gUniXNULoaderProtocolGuid).
  DarwinBootPkg/Driver/DarwinBoot.inf

  # Publishes the FV payload FAT image as a read-only block device. Replaces
  # UniXNURamDiskDxe, whose FDT/initrd input does not exist on DuoWOA (B-FW1).
  EpsilonUniXNUPkg/Driver/EpsilonPayloadDxe/EpsilonPayloadDxe.inf

  # T5 read-only diagnostic (UEFI_APPLICATION, FILE_GUID E59FE912-...). Nothing
  # dispatches it: an application only runs when something loads it, and the
  # only loader is the fv: entry in Payload/unixnu.toml, reached only when
  # UNIXNU_BOOT_OPTION = 1 starts UniXNU.
  EpsilonUniXNUPkg/Application/EpsilonDiag/EpsilonDiag.inf
