#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/UniXNULoader.h>

static EFI_GUID mLoadedImage = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID mFileSystem = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID mLoader = UNIXNU_LOADER_PROTOCOL_GUID;

static void Announce(EFI_SYSTEM_TABLE *system, CHAR16 *text) {
  system->ConOut->OutputString(system->ConOut, text);
}

EFI_STATUS EFIAPI FixtureMain(EFI_HANDLE image, EFI_SYSTEM_TABLE *system) {
  EFI_BOOT_SERVICES *services = system->BootServices;
  EFI_LOADED_IMAGE_PROTOCOL *loaded;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
  EFI_FILE_PROTOCOL *root, *file;
  EFI_HANDLE driver;
  VOID *buffer, *ramdisk;
  EFI_STATUS status;
  UINTN length;
  UNIXNU_LOADER_PROTOCOL *loader;
  static UNIXNU_LOADER_BOOT_REQUEST request;

  Announce(system, L"M2 FIXTURE: entered under AAVMF\r\n");
  status = services->HandleProtocol(image, &mLoadedImage, (VOID **)&loaded);
  if (EFI_ERROR(status)) return status;
  status = services->HandleProtocol(loaded->DeviceHandle, &mFileSystem, (VOID **)&fs);
  if (EFI_ERROR(status)) return status;
  status = fs->OpenVolume(fs, &root);
  if (EFI_ERROR(status)) return status;
  length = 128 * 1024;
  status = services->AllocatePool(EfiLoaderData, length, &buffer);
  if (EFI_ERROR(status)) return status;
  status = root->Open(root, &file, L"\\DarwinBoot.efi", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) return status;
  status = file->Read(file, &length, buffer);
  file->Close(file);
  if (EFI_ERROR(status) || length == 128 * 1024) return EFI_LOAD_ERROR;
  status = services->LoadImage(FALSE, image, NULL, buffer, length, &driver);
  services->FreePool(buffer);
  if (EFI_ERROR(status)) return status;
  status = services->StartImage(driver, NULL, NULL);
  if (EFI_ERROR(status)) return status;
  status = services->LocateProtocol(&mLoader, NULL, (VOID **)&loader);
  if (EFI_ERROR(status) || loader->Revision != UNIXNU_LOADER_REVISION) return EFI_UNSUPPORTED;
  length = 4097;
  status = services->AllocatePool(EfiLoaderData, length, &ramdisk);
  if (EFI_ERROR(status)) return status;
  status = root->Open(root, &file, L"\\ramdisk.bin", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) return status;
  status = file->Read(file, &length, ramdisk);
  file->Close(file);
  root->Close(root);
  if (EFI_ERROR(status) || length != 4097) return EFI_LOAD_ERROR;
  request.Volume = loaded->DeviceHandle;
  request.KernelPath = L"\\kernel.macho";
  request.BootArgs = L"darwinboot.fixture=1";
  request.Ramdisk = ramdisk;
  request.RamdiskSize = length;
  Announce(system, L"M2 FIXTURE: requesting kernel and ADT through pinned DarwinBoot\r\n");
  status = loader->Boot(loader, &request);
  Announce(system, L"M2 FIXTURE: DarwinBoot returned to firmware\r\n");
  return status;
}
