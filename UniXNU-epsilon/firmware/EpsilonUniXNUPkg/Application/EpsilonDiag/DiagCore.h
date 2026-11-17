/** @file
  EpsilonDiag report logic: line formatting, ID-register field decode, memory
  map summary and the built-in 5x7 font. No UEFI services are used here, so
  the same source is compiled into the UEFI application and into the host
  test (firmware/tests/epsilon-diag/host_test.c, EPSILON_DIAG_HOST_TEST).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef EPSILON_DIAG_CORE_H_
#define EPSILON_DIAG_CORE_H_

#ifdef EPSILON_DIAG_HOST_TEST
  #include <stdint.h>
typedef uint8_t   UINT8;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef uintptr_t UINTN;
typedef char      CHAR8;
typedef int       BOOLEAN;
  #define VOID    void
  #define CONST   const
  #define STATIC  static
  #define TRUE    1
  #define FALSE   0
  #define IN
  #define OUT
  #define EFIAPI
#else
  #include <Uefi.h>
#endif

//
// Every line is "UNIXNU: <label>=<value>". The longest is a memory range,
// "UNIXNU: MEM.CONV0=<16 hex>+<16 hex>" (51 characters).
//
#define DIAG_LINE_MAX     64     ///< Buffer size, including the terminator.
#define DIAG_LINES_MAX    60     ///< Report lines; the host test enforces it.
#define DIAG_TOP_RANGES   4

#define DIAG_FONT_W       5
#define DIAG_FONT_H       7
#define DIAG_CELL_W       6      ///< Glyph plus one column of spacing.
#define DIAG_CELL_H       9      ///< Glyph plus two rows of spacing.

typedef
VOID
(EFIAPI *DIAG_EMIT)(
  IN VOID         *Context,
  IN CONST CHAR8  *Line
  );

///
/// The memory-type classes the summary distinguishes. The caller maps
/// EFI_MEMORY_TYPE to one of these.
///
#define DIAG_CLASS_OTHER  0
#define DIAG_CLASS_CONV   1      ///< EfiConventionalMemory
#define DIAG_CLASS_RSVD   2      ///< EfiReservedMemoryType

typedef struct {
  UINT64    Start;
  UINT64    End;                 ///< Exclusive.
  UINT32    Class;
} DIAG_RANGE;

typedef struct {
  UINT64    FrameBufferBase;
  UINT64    FrameBufferSize;
  UINT32    HorizontalResolution;
  UINT32    VerticalResolution;
  UINT32    PixelFormat;
  UINT32    PixelsPerScanLine;
  UINT32    Mode;
  UINT32    MaxMode;
  UINT32    HandleCount;
} DIAG_GOP_INFO;

/**
  "UNIXNU: <Label>=<Value as Digits upper-case hex digits>".
**/
VOID
DiagLineHex (
  OUT CHAR8        *Line,
  IN  CONST CHAR8  *Label,
  IN  UINT64       Value,
  IN  UINTN        Digits
  );

/**
  "UNIXNU: <Label>=<Value in decimal>".
**/
VOID
DiagLineDec (
  OUT CHAR8        *Line,
  IN  CONST CHAR8  *Label,
  IN  UINT64       Value
  );

VOID
DiagReportCurrentEl (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportMidr (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportCntFrq (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportPfr0 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportPfr1 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportIsar1 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

VOID
DiagReportIsar2 (
  IN UINT64     Value,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

/**
  The verdict lines. PAC.ADDR is 1 if any of APA, API, APA3 is non-zero;
  PAC.GENERIC is 1 if any of GPA, GPI, GPA3 is non-zero.
**/
VOID
DiagReportPacVerdict (
  IN UINT64     Isar1,
  IN UINT64     Isar2,
  IN DIAG_EMIT  Emit,
  IN VOID       *Context
  );

BOOLEAN
DiagPacAddress (
  IN UINT64  Isar1,
  IN UINT64  Isar2
  );

BOOLEAN
DiagPacGeneric (
  IN UINT64  Isar1,
  IN UINT64  Isar2
  );

VOID
DiagReportGop (
  IN CONST DIAG_GOP_INFO  *Info,
  IN DIAG_EMIT            Emit,
  IN VOID                 *Context
  );

/**
  Summarise a memory map. Ranges is sorted and merged in place.

  @param[in,out] Ranges          One element per descriptor, any order.
  @param[in]     Count           Number of elements (= descriptors).
  @param[in]     DescriptorSize  Reported as MEM.DESCSZ only.
**/
VOID
DiagReportMemoryMap (
  IN OUT DIAG_RANGE  *Ranges,
  IN     UINTN       Count,
  IN     UINTN       DescriptorSize,
  IN     DIAG_EMIT   Emit,
  IN     VOID        *Context
  );

/**
  Seven rows, five pixels each; bit 4 is the left-most pixel. Lower-case
  letters other than 'x' use the upper-case glyph. An unknown character is a
  solid block.
**/
CONST UINT8 *
DiagGlyph (
  IN CHAR8  Character
  );

/**
  Draw Line into a 32-bit pixel buffer of Columns * DIAG_CELL_W * Scale by
  DIAG_CELL_H * Scale pixels. Every pixel of the buffer is written.
**/
VOID
DiagRenderLine (
  IN  CONST CHAR8  *Line,
  IN  UINTN        Columns,
  IN  UINTN        Scale,
  IN  UINT32       Foreground,
  IN  UINT32       Background,
  OUT UINT32       *Pixels
  );

/**
  The largest scale at which 64 columns and 62 rows fit Width x Height; at
  least 1.
**/
UINTN
DiagPickScale (
  IN UINTN  Width,
  IN UINTN  Height
  );

#endif
