/** @file
  EpsilonDiag report logic. See DiagCore.h. Pure computation: no UEFI
  services, no hardware access, no allocation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "DiagCore.h"

// ---------------------------------------------------------------------------
// Line formatting
// ---------------------------------------------------------------------------

STATIC
UINTN
Append (
  IN OUT CHAR8        *Line,
  IN     UINTN        Length,
  IN     CONST CHAR8  *Text
  )
{
  while ((*Text != '\0') && (Length < DIAG_LINE_MAX - 1)) {
    Line[Length++] = *Text++;
  }

  Line[Length] = '\0';
  return Length;
}

STATIC
UINTN
AppendHex (
  IN OUT CHAR8   *Line,
  IN     UINTN   Length,
  IN     UINT64  Value,
  IN     UINTN   Digits
  )
{
  STATIC CONST CHAR8  Hex[] = "0123456789ABCDEF";
  UINTN               Index;

  for (Index = Digits; (Index > 0) && (Length < DIAG_LINE_MAX - 1); Index--) {
    Line[Length++] = Hex[(Value >> ((Index - 1) * 4)) & 0xF];
  }

  Line[Length] = '\0';
  return Length;
}

STATIC
UINTN
AppendDec (
  IN OUT CHAR8   *Line,
  IN     UINTN   Length,
  IN     UINT64  Value
  )
{
  CHAR8  Digits[21];
  UINTN  Count;

  Count = 0;
  do {
    Digits[Count++] = (CHAR8)('0' + (Value % 10));
    Value          /= 10;
  } while (Value != 0);

  while ((Count > 0) && (Length < DIAG_LINE_MAX - 1)) {
    Line[Length++] = Digits[--Count];
  }

  Line[Length] = '\0';
  return Length;
}

STATIC
UINTN
Begin (
  OUT CHAR8        *Line,
  IN  CONST CHAR8  *Label
  )
{
  UINTN  Length;

  Length = Append (Line, 0, "UNIXNU: ");
  Length = Append (Line, Length, Label);
  return Append (Line, Length, "=");
}

VOID
DiagLineHex (
  OUT CHAR8        *Line,
  IN  CONST CHAR8  *Label,
  IN  UINT64       Value,
  IN  UINTN        Digits
  )
{
  AppendHex (Line, Begin (Line, Label), Value, Digits);
}

VOID
DiagLineDec (
  OUT CHAR8        *Line,
  IN  CONST CHAR8  *Label,
  IN  UINT64       Value
  )
{
  AppendDec (Line, Begin (Line, Label), Value);
}

STATIC
VOID
EmitHex (
  IN DIAG_EMIT    Emit,
  IN VOID         *Context,
  IN CONST CHAR8  *Label,
  IN UINT64       Value,
  IN UINTN        Digits
  )
{
  CHAR8  Line[DIAG_LINE_MAX];

  DiagLineHex (Line, Label, Value, Digits);
  Emit (Context, Line);
}

STATIC
VOID
EmitDec (
  IN DIAG_EMIT    Emit,
  IN VOID         *Context,
  IN CONST CHAR8  *Label,
  IN UINT64       Value
  )
{
  CHAR8  Line[DIAG_LINE_MAX];

  DiagLineDec (Line, Label, Value);
  Emit (Context, Line);
}

//
// "UNIXNU: <Label>=<hex> (<decimal>)" for values people read in decimal.
//
STATIC
VOID
EmitHexDec (
  IN DIAG_EMIT    Emit,
  IN VOID         *Context,
  IN CONST CHAR8  *Label,
  IN UINT64       Value,
  IN UINTN        Digits
  )
{
  CHAR8  Line[DIAG_LINE_MAX];
  UINTN  Length;

  Length = AppendHex (Line, Begin (Line, Label), Value, Digits);
  Length = Append (Line, Length, " (");
  Length = AppendDec (Line, Length, Value);
  Append (Line, Length, ")");
  Emit (Context, Line);
}

//
// "UNIXNU: <Label><Index>=<start>+<size>".
//
STATIC
VOID
EmitRange (
  IN DIAG_EMIT    Emit,
  IN VOID         *Context,
  IN CONST CHAR8  *Label,
  IN UINTN        Index,
  IN UINT64       Start,
  IN UINT64       Size
  )
{
  CHAR8  Line[DIAG_LINE_MAX];
  UINTN  Length;

  Length = Append (Line, 0, "UNIXNU: ");
  Length = Append (Line, Length, Label);
  Length = AppendDec (Line, Length, Index);
  Length = Append (Line, Length, "=");
  Length = AppendHex (Line, Length, Start, 16);
  Length = Append (Line, Length, "+");
  AppendHex (Line, Length, Size, 16);
  Emit (Context, Line);
}

// ---------------------------------------------------------------------------
// ID registers (Arm ARM DDI 0487, D19.2)
// ---------------------------------------------------------------------------

#define FIELD(Value, Shift)  (((Value) >> (Shift)) & 0xF)

VOID
DiagReportCurrentEl (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "CURRENTEL", Value, 16);
  EmitDec (Emit, Context, "EL", (Value >> 2) & 0x3);         // CurrentEL.EL, bits [3:2]
}

VOID
DiagReportMidr (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "MIDR_EL1", Value, 16);
  EmitHex (Emit, Context, "MIDR.IMPL", (Value >> 24) & 0xFF, 2);   // Implementer [31:24]
  EmitHex (Emit, Context, "MIDR.PART", (Value >> 4) & 0xFFF, 3);   // PartNum [15:4]
}

VOID
DiagReportCntFrq (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHexDec (Emit, Context, "CNTFRQ_EL0", Value, 16);
}

VOID
DiagReportPfr0 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "ID_AA64PFR0_EL1", Value, 16);
  EmitHex (Emit, Context, "PFR0.EL2", FIELD (Value, 8), 1);
  EmitHex (Emit, Context, "PFR0.EL3", FIELD (Value, 12), 1);
  EmitHex (Emit, Context, "PFR0.GIC", FIELD (Value, 24), 1);
}

VOID
DiagReportPfr1 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "ID_AA64PFR1_EL1", Value, 16);
  EmitHex (Emit, Context, "PFR1.BT", FIELD (Value, 0), 1);
  EmitHex (Emit, Context, "PFR1.MTE", FIELD (Value, 8), 1);
}

VOID
DiagReportIsar1 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "ID_AA64ISAR1_EL1", Value, 16);
  EmitHex (Emit, Context, "ISAR1.APA", FIELD (Value, 4), 1);    // QARMA5 address auth
  EmitHex (Emit, Context, "ISAR1.API", FIELD (Value, 8), 1);    // IMP DEF address auth
  EmitHex (Emit, Context, "ISAR1.GPA", FIELD (Value, 24), 1);   // QARMA5 generic auth
  EmitHex (Emit, Context, "ISAR1.GPI", FIELD (Value, 28), 1);   // IMP DEF generic auth
}

VOID
DiagReportIsar2 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitHex (Emit, Context, "ID_AA64ISAR2_EL1", Value, 16);
  EmitHex (Emit, Context, "ISAR2.APA3", FIELD (Value, 12), 1);  // QARMA3 address auth
  EmitHex (Emit, Context, "ISAR2.GPA3", FIELD (Value, 8), 1);   // QARMA3 generic auth
}

BOOLEAN
DiagPacAddress (
  IN UINT64  Isar1,
  IN UINT64  Isar2
  )
{
  return (FIELD (Isar1, 4) != 0) || (FIELD (Isar1, 8) != 0) || (FIELD (Isar2, 12) != 0);
}

BOOLEAN
DiagPacGeneric (
  IN UINT64  Isar1,
  IN UINT64  Isar2
  )
{
  return (FIELD (Isar1, 24) != 0) || (FIELD (Isar1, 28) != 0) || (FIELD (Isar2, 8) != 0);
}

VOID
DiagReportPacVerdict (
  IN UINT64     Isar1,
  IN UINT64     Isar2,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  )
{
  EmitDec (Emit, Context, "PAC.ADDR", DiagPacAddress (Isar1, Isar2) ? 1 : 0);
  EmitDec (Emit, Context, "PAC.GENERIC", DiagPacGeneric (Isar1, Isar2) ? 1 : 0);
}

// ---------------------------------------------------------------------------
// GOP
// ---------------------------------------------------------------------------

VOID
DiagReportGop (
  IN CONST DIAG_GOP_INFO  *Info,
  IN DIAG_EMIT            Emit,
  IN VOID                 *Context
  )
{
  EmitDec (Emit, Context, "GOP.COUNT", Info->HandleCount);
  EmitHex (Emit, Context, "GOP.MODE", Info->Mode, 8);
  EmitHex (Emit, Context, "GOP.MAXMODE", Info->MaxMode, 8);
  EmitHex (Emit, Context, "GOP.FB_BASE", Info->FrameBufferBase, 16);
  EmitHex (Emit, Context, "GOP.FB_SIZE", Info->FrameBufferSize, 16);
  EmitHexDec (Emit, Context, "GOP.HRES", Info->HorizontalResolution, 8);
  EmitHexDec (Emit, Context, "GOP.VRES", Info->VerticalResolution, 8);
  EmitHex (Emit, Context, "GOP.PIXFMT", Info->PixelFormat, 8);
  EmitHexDec (Emit, Context, "GOP.PPSL", Info->PixelsPerScanLine, 8);
}

// ---------------------------------------------------------------------------
// Memory map
// ---------------------------------------------------------------------------

typedef struct {
  UINT64    Start[DIAG_TOP_RANGES];
  UINT64    Size[DIAG_TOP_RANGES];
  UINTN     Count;                    ///< Entries held, at most DIAG_TOP_RANGES.
  UINTN     Seen;                     ///< Ranges offered.
  UINT64    Total;
} TOP_LIST;

STATIC
VOID
TopInit (
  OUT TOP_LIST  *Top
  )
{
  Top->Count = 0;
  Top->Seen  = 0;
  Top->Total = 0;
}

//
// Keep the DIAG_TOP_RANGES largest, largest first; ties keep the lower start
// first because ranges arrive in ascending order.
//
STATIC
VOID
TopOffer (
  IN OUT TOP_LIST  *Top,
  IN     UINT64    Start,
  IN     UINT64    Size
  )
{
  UINTN  Slot;
  UINTN  Index;

  Top->Seen++;
  Top->Total += Size;

  for (Slot = 0; Slot < Top->Count; Slot++) {
    if (Size > Top->Size[Slot]) {
      break;
    }
  }

  if (Slot >= DIAG_TOP_RANGES) {
    return;
  }

  if (Top->Count < DIAG_TOP_RANGES) {
    Top->Count++;
  }

  for (Index = Top->Count - 1; Index > Slot; Index--) {
    Top->Start[Index] = Top->Start[Index - 1];
    Top->Size[Index]  = Top->Size[Index - 1];
  }

  Top->Start[Slot] = Start;
  Top->Size[Slot]  = Size;
}

STATIC
VOID
TopEmit (
  IN CONST TOP_LIST  *Top,
  IN CONST CHAR8     *CountLabel,
  IN CONST CHAR8     *TotalLabel,
  IN CONST CHAR8     *RangeLabel,
  IN DIAG_EMIT       Emit,
  IN VOID            *Context
  )
{
  UINTN  Index;

  EmitDec (Emit, Context, CountLabel, Top->Seen);
  EmitHex (Emit, Context, TotalLabel, Top->Total, 16);
  for (Index = 0; Index < Top->Count; Index++) {
    EmitRange (Emit, Context, RangeLabel, Index, Top->Start[Index], Top->Size[Index]);
  }
}

VOID
DiagReportMemoryMap (
  IN OUT DIAG_RANGE  *Ranges,
  IN     UINTN       Count,
  IN     UINTN       DescriptorSize,
  IN     DIAG_EMIT   Emit,
  IN     VOID        *Context
  )
{
  UINTN       Index;
  UINTN       Inner;
  UINTN       Merged;
  UINT64      Cursor;
  DIAG_RANGE  Key;
  TOP_LIST    Conv;
  TOP_LIST    Rsvd;
  TOP_LIST    Hole;

  EmitDec (Emit, Context, "MEM.DESC", Count);
  EmitDec (Emit, Context, "MEM.DESCSZ", DescriptorSize);
  if (Count == 0) {
    return;
  }

  //
  // Insertion sort by start address: a UEFI memory map has a few hundred
  // descriptors at most and is usually sorted already.
  //
  for (Index = 1; Index < Count; Index++) {
    Key = Ranges[Index];
    for (Inner = Index; (Inner > 0) && (Ranges[Inner - 1].Start > Key.Start); Inner--) {
      Ranges[Inner] = Ranges[Inner - 1];
    }

    Ranges[Inner] = Key;
  }

  //
  // Holes: address space between descriptors that the map does not describe
  // at all. Firmware often leaves carve-outs out of the map instead of
  // typing them EfiReservedMemoryType.
  //
  TopInit (&Hole);
  Cursor = Ranges[0].End;
  for (Index = 1; Index < Count; Index++) {
    if (Ranges[Index].Start > Cursor) {
      TopOffer (&Hole, Cursor, Ranges[Index].Start - Cursor);
    }

    if (Ranges[Index].End > Cursor) {
      Cursor = Ranges[Index].End;
    }
  }

  EmitHex (Emit, Context, "MEM.LO", Ranges[0].Start, 16);
  EmitHex (Emit, Context, "MEM.HI", Cursor, 16);

  //
  // Merge neighbours of the same class that touch, so a region the firmware
  // split by attribute is reported once.
  //
  Merged = 0;
  for (Index = 1; Index < Count; Index++) {
    if ((Ranges[Index].Class == Ranges[Merged].Class) && (Ranges[Index].Start == Ranges[Merged].End)) {
      Ranges[Merged].End = Ranges[Index].End;
    } else {
      Ranges[++Merged] = Ranges[Index];
    }
  }

  TopInit (&Conv);
  TopInit (&Rsvd);
  for (Index = 0; Index <= Merged; Index++) {
    if (Ranges[Index].Class == DIAG_CLASS_CONV) {
      TopOffer (&Conv, Ranges[Index].Start, Ranges[Index].End - Ranges[Index].Start);
    } else if (Ranges[Index].Class == DIAG_CLASS_RSVD) {
      TopOffer (&Rsvd, Ranges[Index].Start, Ranges[Index].End - Ranges[Index].Start);
    }
  }

  TopEmit (&Conv, "MEM.CONV.N", "MEM.CONV.TOTAL", "MEM.CONV", Emit, Context);
  TopEmit (&Rsvd, "MEM.RSVD.N", "MEM.RSVD.TOTAL", "MEM.RSVD", Emit, Context);
  TopEmit (&Hole, "MEM.HOLE.N", "MEM.HOLE.TOTAL", "MEM.HOLE", Emit, Context);
}

// ---------------------------------------------------------------------------
// Font and rendering
// ---------------------------------------------------------------------------

typedef struct {
  CHAR8    Character;
  UINT8    Rows[DIAG_FONT_H];
} GLYPH;

STATIC CONST UINT8  mBlock[DIAG_FONT_H] = { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };

STATIC CONST GLYPH  mFont[] = {
  { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
  { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } },
  { '#', { 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A } },
  { '%', { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } },
  { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
  { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
  { '*', { 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00 } },
  { '+', { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } },
  { ',', { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 } },
  { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
  { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } },
  { '/', { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00 } },
  { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
  { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
  { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
  { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
  { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
  { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
  { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
  { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
  { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
  { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
  { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } },
  { '=', { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 } },
  { '?', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } },
  { 'A', { 0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11 } },
  { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
  { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
  { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
  { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
  { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
  { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
  { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
  { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
  { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
  { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
  { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
  { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
  { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
  { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
  { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
  { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } },
  { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
  { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
  { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
  { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
  { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
  { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } },
  { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
  { 'Y', { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 } },
  { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
  { '_', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F } },
  { 'x', { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 } }
};

CONST UINT8 *
DiagGlyph (
  IN CHAR8  Character
  )
{
  UINTN  Index;

  if ((Character >= 'a') && (Character <= 'z') && (Character != 'x')) {
    Character = (CHAR8)(Character - 'a' + 'A');
  }

  for (Index = 0; Index < sizeof (mFont) / sizeof (mFont[0]); Index++) {
    if (mFont[Index].Character == Character) {
      return mFont[Index].Rows;
    }
  }

  return mBlock;
}

VOID
DiagRenderLine (
  IN  CONST CHAR8  *Line,
  IN  UINTN        Columns,
  IN  UINTN        Scale,
  IN  UINT32       Foreground,
  IN  UINT32       Background,
  OUT UINT32       *Pixels
  )
{
  UINTN        Width;
  UINTN        Height;
  UINTN        Index;
  UINTN        Column;
  UINTN        Row;
  UINTN        Bit;
  UINTN        X;
  UINTN        Y;
  CONST UINT8  *Glyph;

  Width  = Columns * DIAG_CELL_W * Scale;
  Height = DIAG_CELL_H * Scale;
  for (Index = 0; Index < Width * Height; Index++) {
    Pixels[Index] = Background;
  }

  for (Column = 0; (Column < Columns) && (Line[Column] != '\0'); Column++) {
    Glyph = DiagGlyph (Line[Column]);
    for (Row = 0; Row < DIAG_FONT_H; Row++) {
      for (Bit = 0; Bit < DIAG_FONT_W; Bit++) {
        if ((Glyph[Row] & (0x10 >> Bit)) == 0) {
          continue;
        }

        //
        // One row of spacing above the glyph and one below.
        //
        for (Y = (Row + 1) * Scale; Y < (Row + 2) * Scale; Y++) {
          for (X = (Column * DIAG_CELL_W + Bit) * Scale; X < (Column * DIAG_CELL_W + Bit + 1) * Scale; X++) {
            Pixels[Y * Width + X] = Foreground;
          }
        }
      }
    }
  }
}

UINTN
DiagPickScale (
  IN UINTN  Width,
  IN UINTN  Height
  )
{
  UINTN  ByWidth;
  UINTN  ByHeight;
  UINTN  Scale;

  ByWidth  = Width / (DIAG_CELL_W * DIAG_LINE_MAX);
  ByHeight = Height / (DIAG_CELL_H * (DIAG_LINES_MAX + 2));
  Scale    = (ByWidth < ByHeight) ? ByWidth : ByHeight;
  return (Scale == 0) ? 1 : Scale;
}
