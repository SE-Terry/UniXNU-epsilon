/** @file
  Read-only UEFI diagnostic for Surface Duo 1 (epsilon).

  Prints "UNIXNU: <label>=<value>" lines on the panel: exception level, CPU ID
  registers (the pointer-authentication fields in particular), the GOP mode
  and frame buffer, and a summary of the UEFI memory map. Then it holds the
  screen for HOLD_SECONDS, or until a key, and returns EFI_SUCCESS to its
  caller (UniXNU, which shows its menu again).

  What this image does to the machine:
    - reads system registers with MRS;
    - reads protocol data (GOP mode, ConIn key strokes) and the memory map;
    - allocates and frees pool memory;
    - writes pixels with GOP Blt, or text with ConOut when there is no GOP;
    - writes the same lines to the debug log.
  It does not touch storage, UEFI variables, ACPI or the device tree, does not
  load or start an image, and does not call ExitBootServices or ResetSystem.
  firmware/scripts/lint-epsilon-config.py (rule R6) checks this source for
  those calls.

  Output is drawn with GOP Blt and a built-in font, not ConOut. Both end in
  the same Blt, but the firmware text console uses 8x19 glyphs, which is about
  0.5 mm per character on this 401 ppi panel, and UniXNU draws its menu on the
  same GOP without going through ConOut. ConOut is the fallback when no GOP
  exists.

  Host build only. Not run on hardware: every behaviour described here is
  UNVERIFIED on the Duo.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

#include "DiagCore.h"

#define HOLD_SECONDS  60

#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF

UINT64
EFIAPI
EpsilonDiagReadCurrentEl (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadMidr (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadCntFrq (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadPfr0 (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadPfr1 (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadIsar1 (
  VOID
  );

UINT64
EFIAPI
EpsilonDiagReadIsar2 (
  VOID
  );

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *mGop;
STATIC UINT32                        *mLinePixels;
STATIC UINTN                         mScale;
STATIC UINTN                         mColumns;
STATIC UINTN                         mRows;
STATIC UINTN                         mRow;
STATIC UINTN                         mOriginX;
STATIC UINTN                         mOriginY;
STATIC UINT32                        mColor = COLOR_WHITE;

STATIC
VOID
DrawAt (
  IN UINTN        Row,
  IN CONST CHAR8  *Line
  )
{
  if (Row >= mRows) {
    return;
  }

  DiagRenderLine (Line, mColumns, mScale, mColor, COLOR_BLACK, mLinePixels);
  mGop->Blt (
          mGop,
          (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)mLinePixels,
          EfiBltBufferToVideo,
          0,
          0,
          mOriginX,
          mOriginY + Row * DIAG_CELL_H * mScale,
          mColumns * DIAG_CELL_W * mScale,
          DIAG_CELL_H * mScale,
          0
          );
}

STATIC
VOID
TextOut (
  IN CONST CHAR8  *Line,
  IN BOOLEAN      NewLine
  )
{
  CHAR16  Wide[DIAG_LINE_MAX + 2];
  UINTN   Index;

  if ((gST->ConOut == NULL) || (gST->ConOut->OutputString == NULL)) {
    return;
  }

  for (Index = 0; (Index < DIAG_LINE_MAX - 1) && (Line[Index] != '\0'); Index++) {
    Wide[Index] = (CHAR16)Line[Index];
  }

  Wide[Index++] = L'\r';
  if (NewLine) {
    Wide[Index++] = L'\n';
  }

  Wide[Index] = L'\0';
  gST->ConOut->OutputString (gST->ConOut, Wide);
}

STATIC
VOID
EFIAPI
Emit (
  IN VOID         *Context,
  IN CONST CHAR8  *Line
  )
{
  DEBUG ((DEBUG_ERROR, "%a\n", Line));
  if (mLinePixels != NULL) {
    DrawAt (mRow++, Line);
  } else {
    TextOut (Line, TRUE);
  }
}

STATIC
VOID
EmitHex (
  IN CONST CHAR8  *Label,
  IN UINT64       Value,
  IN UINTN        Digits
  )
{
  CHAR8  Line[DIAG_LINE_MAX];

  DiagLineHex (Line, Label, Value, Digits);
  Emit (NULL, Line);
}

/**
  Pick the GOP the firmware console uses, else the first one. Sets up the
  line buffer and clears the screen. Leaves mLinePixels NULL when there is no
  usable GOP, which selects the ConOut path.

  @return The number of GOP handles.
**/
STATIC
UINTN
DisplayInit (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     *Handles;
  UINTN                          Count;
  UINTN                          Width;
  UINTN                          Height;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;

  Count   = 0;
  Handles = NULL;
  Status  = gBS->LocateHandleBuffer (ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    Count = 0;
  } else if (Handles != NULL) {
    FreePool (Handles);
  }

  Status = EFI_NOT_FOUND;
  if (gST->ConsoleOutHandle != NULL) {
    Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&mGop);
  }

  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop);
  }

  if (EFI_ERROR (Status) || (mGop == NULL) || (mGop->Mode == NULL) || (mGop->Mode->Info == NULL)) {
    mGop = NULL;
    return Count;
  }

  Width  = mGop->Mode->Info->HorizontalResolution;
  Height = mGop->Mode->Info->VerticalResolution;
  mScale = DiagPickScale (Width, Height);

  mColumns = Width / (DIAG_CELL_W * mScale);
  if (mColumns > DIAG_LINE_MAX - 1) {
    mColumns = DIAG_LINE_MAX - 1;
  }

  mRows = Height / (DIAG_CELL_H * mScale);
  if (mRows > DIAG_LINES_MAX + 2) {
    mRows = DIAG_LINES_MAX + 2;
  }

  if ((mColumns == 0) || (mRows == 0)) {
    return Count;
  }

  mOriginX    = (Width - mColumns * DIAG_CELL_W * mScale) / 2;
  mOriginY    = (Height - mRows * DIAG_CELL_H * mScale) / 2;
  mLinePixels = AllocatePool (mColumns * DIAG_CELL_W * mScale * DIAG_CELL_H * mScale * sizeof (UINT32));
  if (mLinePixels == NULL) {
    return Count;
  }

  Black.Blue     = 0;
  Black.Green    = 0;
  Black.Red      = 0;
  Black.Reserved = 0;
  mGop->Blt (mGop, &Black, EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);
  return Count;
}

STATIC
VOID
ReportGop (
  IN UINTN  HandleCount
  )
{
  DIAG_GOP_INFO  Info;

  if (mGop == NULL) {
    Emit (NULL, "UNIXNU: GOP.COUNT=0");
    return;
  }

  Info.HandleCount          = (UINT32)HandleCount;
  Info.Mode                 = mGop->Mode->Mode;
  Info.MaxMode              = mGop->Mode->MaxMode;
  Info.FrameBufferBase      = mGop->Mode->FrameBufferBase;
  Info.FrameBufferSize      = mGop->Mode->FrameBufferSize;
  Info.HorizontalResolution = mGop->Mode->Info->HorizontalResolution;
  Info.VerticalResolution   = mGop->Mode->Info->VerticalResolution;
  Info.PixelFormat          = (UINT32)mGop->Mode->Info->PixelFormat;
  Info.PixelsPerScanLine    = mGop->Mode->Info->PixelsPerScanLine;
  DiagReportGop (&Info, Emit, NULL);
}

STATIC
VOID
ReportMemoryMap (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_MEMORY_DESCRIPTOR  *Map;
  EFI_MEMORY_DESCRIPTOR  *Descriptor;
  DIAG_RANGE             *Ranges;
  UINTN                  Size;
  UINTN                  MapKey;
  UINTN                  DescriptorSize;
  UINT32                 DescriptorVersion;
  UINTN                  Count;
  UINTN                  Index;
  UINTN                  Attempt;

  Map            = NULL;
  Size           = 0;
  DescriptorSize = 0;
  Status         = gBS->GetMemoryMap (&Size, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
  for (Attempt = 0; (Status == EFI_BUFFER_TOO_SMALL) && (Attempt < 4); Attempt++) {
    //
    // The allocation below can itself split a descriptor.
    //
    Size += 8 * DescriptorSize;
    Map   = AllocatePool (Size);
    if (Map == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }

    Status = gBS->GetMemoryMap (&Size, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR (Status)) {
      FreePool (Map);
      Map = NULL;
    }
  }

  if (EFI_ERROR (Status) || (Map == NULL) || (DescriptorSize < sizeof (EFI_MEMORY_DESCRIPTOR))) {
    EmitHex ("MEM.ERROR", (UINT64)Status, 16);
    if (Map != NULL) {
      FreePool (Map);
    }

    return;
  }

  Count  = Size / DescriptorSize;
  Ranges = AllocatePool ((Count + 1) * sizeof (DIAG_RANGE));
  if (Ranges == NULL) {
    EmitHex ("MEM.ERROR", (UINT64)EFI_OUT_OF_RESOURCES, 16);
    FreePool (Map);
    return;
  }

  for (Index = 0; Index < Count; Index++) {
    Descriptor          = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Index * DescriptorSize);
    Ranges[Index].Start = Descriptor->PhysicalStart;
    Ranges[Index].End   = Descriptor->PhysicalStart + (Descriptor->NumberOfPages << EFI_PAGE_SHIFT);
    if (Descriptor->Type == EfiConventionalMemory) {
      Ranges[Index].Class = DIAG_CLASS_CONV;
    } else if (Descriptor->Type == EfiReservedMemoryType) {
      Ranges[Index].Class = DIAG_CLASS_RSVD;
    } else {
      Ranges[Index].Class = DIAG_CLASS_OTHER;
    }
  }

  DiagReportMemoryMap (Ranges, Count, DescriptorSize, Emit, NULL);
  FreePool (Ranges);
  FreePool (Map);
}

/**
  Keep the report on screen for HOLD_SECONDS or until a key. The countdown is
  redrawn in place, so a photo also shows that the machine is still running.
**/
STATIC
VOID
Hold (
  VOID
  )
{
  EFI_INPUT_KEY  Key;
  CHAR8          Line[DIAG_LINE_MAX];
  UINTN          Second;
  UINTN          Tick;
  BOOLEAN        HaveInput;

  HaveInput = (gST->ConIn != NULL) && (gST->ConIn->ReadKeyStroke != NULL);

  //
  // Drop the key that selected the menu entry, if any.
  //
  while (HaveInput && !EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
  }

  mColor = COLOR_CYAN;
  for (Second = HOLD_SECONDS; Second > 0; Second--) {
    DiagLineDec (Line, "HOLD", Second);
    if (mLinePixels != NULL) {
      DrawAt (mRow, Line);
    } else {
      TextOut (Line, FALSE);
    }

    for (Tick = 0; Tick < 10; Tick++) {
      gBS->Stall (100000);
      if (HaveInput && !EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
        return;
      }
    }
  }
}

EFI_STATUS
EFIAPI
EpsilonDiagMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN   GopCount;
  UINT64  Isar1;
  UINT64  Isar2;

  GopCount = DisplayInit ();

  mColor = COLOR_CYAN;
  Emit (NULL, "UNIXNU: DIAG.VERSION=1");
  mColor = COLOR_WHITE;

  //
  // Each line is on screen before the next register is read, so if a read
  // faults the photo still shows how far the report got.
  //
  DiagReportCurrentEl (EpsilonDiagReadCurrentEl (), Emit, NULL);
  DiagReportMidr (EpsilonDiagReadMidr (), Emit, NULL);
  DiagReportCntFrq (EpsilonDiagReadCntFrq (), Emit, NULL);
  DiagReportPfr0 (EpsilonDiagReadPfr0 (), Emit, NULL);
  DiagReportPfr1 (EpsilonDiagReadPfr1 (), Emit, NULL);
  Isar1 = EpsilonDiagReadIsar1 ();
  DiagReportIsar1 (Isar1, Emit, NULL);

  ReportGop (GopCount);
  ReportMemoryMap ();

  //
  // ID_AA64ISAR2_EL1 last: it is the only register here that postdates the
  // Cortex-A76/A55 cores, so it is the only read with an unverified trap path.
  //
  mColor = COLOR_CYAN;
  Emit (NULL, "UNIXNU: DIAG.NEXT=ISAR2");
  mColor = COLOR_WHITE;
  Isar2  = EpsilonDiagReadIsar2 ();
  DiagReportIsar2 (Isar2, Emit, NULL);

  mColor = COLOR_YELLOW;
  DiagReportPacVerdict (Isar1, Isar2, Emit, NULL);
  mColor = COLOR_CYAN;
  Emit (NULL, "UNIXNU: DIAG.DONE=1");

  Hold ();

  if (mLinePixels != NULL) {
    FreePool (mLinePixels);
    mLinePixels = NULL;
  }

  return EFI_SUCCESS;
}
