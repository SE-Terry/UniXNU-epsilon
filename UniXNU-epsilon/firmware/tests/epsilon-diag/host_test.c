/*
 * Host test for EpsilonDiag's report logic (DiagCore.c built with
 * EPSILON_DIAG_HOST_TEST). Run by scripts/test-epsilon-diag.sh.
 *
 *   host_test [preview.rgb]
 *
 * Checks field decode, the PAC verdict, the memory map summary, line and
 * screen budgets and font coverage. With an argument it also renders the
 * report as it would be laid out on a 1350x1800 panel (raw RGB24) from
 * SAMPLE values. The sample values are not measurements of the Duo.
 *
 * This tests the formatting and decode code only. It does not execute the
 * UEFI application and says nothing about behaviour on hardware.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DiagCore.h"

#define PANEL_W  1350
#define PANEL_H  1800

static char      gLines[128][DIAG_LINE_MAX];
static unsigned  gCount;
static unsigned  gFailures;

static void
Collect (void *Context, const char *Line)
{
  (void)Context;
  if (strlen (Line) >= DIAG_LINE_MAX) {
    printf ("FAIL line too long: %s\n", Line);
    gFailures++;
  }

  if (gCount < 128) {
    strcpy (gLines[gCount++], Line);
  }
}

static void
Expect (const char *What, int Condition)
{
  if (!Condition) {
    printf ("FAIL %s\n", What);
    gFailures++;
  }
}

static int
Has (const char *Line)
{
  unsigned  i;

  for (i = 0; i < gCount; i++) {
    if (strcmp (gLines[i], Line) == 0) {
      return 1;
    }
  }

  printf ("FAIL missing line: %s\n", Line);
  gFailures++;
  return 0;
}

static void
Reset (void)
{
  gCount = 0;
}

/* The order EpsilonDiagMain emits in, with caller-supplied values. */
static void
FullReport (
  uint64_t El, uint64_t Midr, uint64_t Frq, uint64_t Pfr0, uint64_t Pfr1,
  uint64_t Isar1, uint64_t Isar2, const DIAG_GOP_INFO *Gop,
  DIAG_RANGE *Ranges, unsigned RangeCount
  )
{
  Collect (NULL, "UNIXNU: DIAG.VERSION=1");
  DiagReportCurrentEl (El, Collect, NULL);
  DiagReportMidr (Midr, Collect, NULL);
  DiagReportCntFrq (Frq, Collect, NULL);
  DiagReportPfr0 (Pfr0, Collect, NULL);
  DiagReportPfr1 (Pfr1, Collect, NULL);
  DiagReportIsar1 (Isar1, Collect, NULL);
  DiagReportGop (Gop, Collect, NULL);
  DiagReportMemoryMap (Ranges, RangeCount, 48, Collect, NULL);
  Collect (NULL, "UNIXNU: DIAG.NEXT=ISAR2");
  DiagReportIsar2 (Isar2, Collect, NULL);
  DiagReportPacVerdict (Isar1, Isar2, Collect, NULL);
  Collect (NULL, "UNIXNU: DIAG.DONE=1");
}

int
main (int argc, char **argv)
{
  unsigned  i;
  size_t    j;

  /* --- formatting --- */
  {
    char  Line[DIAG_LINE_MAX];

    DiagLineHex (Line, "X", 0x0123456789ABCDEFull, 16);
    Expect ("hex16", strcmp (Line, "UNIXNU: X=0123456789ABCDEF") == 0);
    DiagLineHex (Line, "X", 0xABCull, 1);
    Expect ("hex1 keeps low nibble", strcmp (Line, "UNIXNU: X=C") == 0);
    DiagLineDec (Line, "X", 0);
    Expect ("dec0", strcmp (Line, "UNIXNU: X=0") == 0);
    DiagLineDec (Line, "X", 18446744073709551615ull);
    Expect ("dec max", strcmp (Line, "UNIXNU: X=18446744073709551615") == 0);
  }

  /* --- PAC verdict: every field alone, and none --- */
  Expect ("no PAC", !DiagPacAddress (0x0000000000100001ull, 0) && !DiagPacGeneric (0x0000000000100001ull, 0));
  Expect ("APA", DiagPacAddress (1ull << 4, 0) && !DiagPacGeneric (1ull << 4, 0));
  Expect ("API", DiagPacAddress (1ull << 8, 0) && !DiagPacGeneric (1ull << 8, 0));
  Expect ("APA3", DiagPacAddress (0, 1ull << 12) && !DiagPacGeneric (0, 1ull << 12));
  Expect ("GPA", DiagPacGeneric (1ull << 24, 0) && !DiagPacAddress (1ull << 24, 0));
  Expect ("GPI", DiagPacGeneric (1ull << 28, 0) && !DiagPacAddress (1ull << 28, 0));
  Expect ("GPA3", DiagPacGeneric (0, 1ull << 8) && !DiagPacAddress (0, 1ull << 8));
  /* Neighbouring fields must not leak in: DPB [3:0], JSCVT [15:12], LRCPC [23:20], SB [39:36]. */
  Expect ("neighbours", !DiagPacAddress (0x000000F000F0F00Full, 0x00000000000F00FFull) &&
          !DiagPacGeneric (0x000000F000F0F00Full, 0x00000000000FF0FFull & ~0xF00ull));

  Reset ();
  DiagReportIsar1 (0x00000000A0B00C50ull, Collect, NULL);
  Has ("UNIXNU: ID_AA64ISAR1_EL1=00000000A0B00C50");
  Has ("UNIXNU: ISAR1.APA=5");
  Has ("UNIXNU: ISAR1.API=C");
  Has ("UNIXNU: ISAR1.GPA=0");
  Has ("UNIXNU: ISAR1.GPI=A");
  Reset ();
  DiagReportIsar1 (0x000000000B000000ull, Collect, NULL);
  Has ("UNIXNU: ISAR1.GPA=B");
  Reset ();
  DiagReportIsar2 (0x0000000000003700ull, Collect, NULL);
  Has ("UNIXNU: ISAR2.APA3=3");
  Has ("UNIXNU: ISAR2.GPA3=7");
  Reset ();
  DiagReportCurrentEl (0x4, Collect, NULL);
  Has ("UNIXNU: EL=1");
  Reset ();
  DiagReportCurrentEl (0x8, Collect, NULL);
  Has ("UNIXNU: EL=2");
  Reset ();
  DiagReportMidr (0x51DF805Eull, Collect, NULL);
  Has ("UNIXNU: MIDR.IMPL=51");
  Has ("UNIXNU: MIDR.PART=805");
  Reset ();
  DiagReportPfr0 (0x0000000001002100ull, Collect, NULL);
  Has ("UNIXNU: PFR0.EL2=1");
  Has ("UNIXNU: PFR0.EL3=2");
  Has ("UNIXNU: PFR0.GIC=1");
  Reset ();
  DiagReportPfr1 (0x0000000000000201ull, Collect, NULL);
  Has ("UNIXNU: PFR1.BT=1");
  Has ("UNIXNU: PFR1.MTE=2");
  Reset ();
  DiagReportCntFrq (19200000, Collect, NULL);
  Has ("UNIXNU: CNTFRQ_EL0=000000000124F800 (19200000)");

  /* --- memory map: unsorted, split by attribute, overlapping end, holes --- */
  {
    DIAG_RANGE  R[] = {
      { 0x90000000ull, 0xA0000000ull, DIAG_CLASS_CONV  },   /* out of order            */
      { 0x80000000ull, 0x80100000ull, DIAG_CLASS_OTHER },
      { 0x80100000ull, 0x88000000ull, DIAG_CLASS_CONV  },   /* touches the next: merge */
      { 0x88000000ull, 0x8C000000ull, DIAG_CLASS_CONV  },
      { 0x8C000000ull, 0x8C400000ull, DIAG_CLASS_RSVD  },
      /* hole 8C400000..90000000 */
      { 0xA0000000ull, 0xA0001000ull, DIAG_CLASS_RSVD  },
      /* hole A0001000..C0000000 */
      { 0xC0000000ull, 0xC0002000ull, DIAG_CLASS_CONV  },
      { 0xC0002000ull, 0xC0003000ull, DIAG_CLASS_OTHER },
      { 0xC0003000ull, 0xC0004000ull, DIAG_CLASS_CONV  },   /* not adjacent to the conv above */
      { 0xC0004000ull, 0xC0005000ull, DIAG_CLASS_CONV  },
      { 0xC0005000ull, 0xC0006000ull, DIAG_CLASS_CONV  },
      /* hole C0006000..C0010000 */
      { 0xC0010000ull, 0xC0011000ull, DIAG_CLASS_CONV  }
    };

    Reset ();
    DiagReportMemoryMap (R, sizeof (R) / sizeof (R[0]), 48, Collect, NULL);
    Has ("UNIXNU: MEM.DESC=12");
    Has ("UNIXNU: MEM.DESCSZ=48");
    Has ("UNIXNU: MEM.LO=0000000080000000");
    Has ("UNIXNU: MEM.HI=00000000C0011000");
    Has ("UNIXNU: MEM.CONV.N=5");
    Has ("UNIXNU: MEM.CONV.TOTAL=000000001BF06000");
    Has ("UNIXNU: MEM.CONV0=0000000090000000+0000000010000000");
    Has ("UNIXNU: MEM.CONV1=0000000080100000+000000000BF00000");
    Has ("UNIXNU: MEM.CONV2=00000000C0003000+0000000000003000");
    Has ("UNIXNU: MEM.CONV3=00000000C0000000+0000000000002000");
    Has ("UNIXNU: MEM.RSVD.N=2");
    Has ("UNIXNU: MEM.RSVD.TOTAL=0000000000401000");
    Has ("UNIXNU: MEM.RSVD0=000000008C000000+0000000000400000");
    Has ("UNIXNU: MEM.RSVD1=00000000A0000000+0000000000001000");
    Has ("UNIXNU: MEM.HOLE.N=3");
    Has ("UNIXNU: MEM.HOLE0=00000000A0001000+000000001FFFF000");
    Has ("UNIXNU: MEM.HOLE1=000000008C400000+0000000003C00000");
    Has ("UNIXNU: MEM.HOLE2=00000000C0006000+000000000000A000");
    for (i = 0; i < gCount; i++) {
      Expect ("only 4 conv ranges listed", strncmp (gLines[i], "UNIXNU: MEM.CONV4", 17) != 0);
    }

    Reset ();
    DiagReportMemoryMap (R, 0, 48, Collect, NULL);
    Expect ("empty map: two lines", gCount == 2);
  }

  /* --- whole report: budgets, font coverage, preview --- */
  {
    /* SAMPLE values, shaped like a Cortex-A55-class core. Not measured on the Duo. */
    DIAG_GOP_INFO  Gop = { 0x9C000000ull, 0x02400000ull, 1350, 1800, 1, 1350, 0, 1, 1 };
    DIAG_RANGE     R[64];
    unsigned       n = 0;
    uint64_t       a;

    /* Worst case for the line budget: at least four ranges of each kind. */
    for (a = 0x80000000ull; n < 60; a += 0x4000000ull) {
      R[n].Start = a;
      R[n].End   = a + 0x1000000ull * (1 + n % 3);
      R[n].Class = (n % 3 == 0) ? DIAG_CLASS_CONV : (n % 3 == 1) ? DIAG_CLASS_RSVD : DIAG_CLASS_OTHER;
      n++;
    }

    Reset ();
    FullReport (0x4, 0x51DF805Eull, 19200000, 0x0000000010112222ull, 0x10ull,
                0x0000000000100001ull, 0, &Gop, R, n);
    printf ("report: %u lines (budget %u, plus the HOLD line)\n", gCount, DIAG_LINES_MAX);
    Expect ("line budget", gCount + 1 <= DIAG_LINES_MAX);
    Has ("UNIXNU: PAC.ADDR=0");
    Has ("UNIXNU: PAC.GENERIC=0");
    Has ("UNIXNU: GOP.FB_BASE=000000009C000000");
    Has ("UNIXNU: GOP.HRES=00000546 (1350)");
    Has ("UNIXNU: GOP.VRES=00000708 (1800)");

    {
      static const unsigned char  Block[DIAG_FONT_H] = { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };
      size_t                      Longest = 0;

      for (i = 0; i < gCount; i++) {
        if (strlen (gLines[i]) > Longest) {
          Longest = strlen (gLines[i]);
        }

        Expect ("UNIXNU: prefix", strncmp (gLines[i], "UNIXNU: ", 8) == 0);
        for (j = 0; j < strlen (gLines[i]); j++) {
          if (memcmp (DiagGlyph (gLines[i][j]), Block, DIAG_FONT_H) == 0) {
            printf ("FAIL no glyph for '%c' in: %s\n", gLines[i][j], gLines[i]);
            gFailures++;
          }
        }
      }

      printf ("longest line: %zu characters\n", Longest);
    }

    {
      size_t    Scale   = DiagPickScale (PANEL_W, PANEL_H);
      size_t    Columns = PANEL_W / (DIAG_CELL_W * Scale);
      size_t    Rows    = PANEL_H / (DIAG_CELL_H * Scale);

      if (Columns > DIAG_LINE_MAX - 1) {
        Columns = DIAG_LINE_MAX - 1;
      }

      if (Rows > DIAG_LINES_MAX + 2) {
        Rows = DIAG_LINES_MAX + 2;
      }

      printf ("1350x1800: scale %zu, %zu columns x %zu rows, cell %zux%zu px\n",
              Scale, Columns, Rows, DIAG_CELL_W * Scale, DIAG_CELL_H * Scale);
      Expect ("scale 3 on the Duo panel", Scale == 3);
      Expect ("rows fit", gCount + 1 <= Rows);
      Expect ("scale never 0", DiagPickScale (10, 10) == 1);

      if (argc > 1) {
        size_t         LineW  = Columns * DIAG_CELL_W * Scale;
        size_t         LineH  = DIAG_CELL_H * Scale;
        size_t         X0     = (PANEL_W - LineW) / 2;
        size_t         Y0     = (PANEL_H - Rows * LineH) / 2;
        UINT32         *Pix   = malloc (LineW * LineH * sizeof (UINT32));
        unsigned char  *Image = calloc (PANEL_W * PANEL_H, 3);
        FILE           *f;
        size_t         x, y;

        strcpy (gLines[gCount++], "UNIXNU: HOLD=60");
        for (i = 0; i < gCount && i < Rows; i++) {
          UINT32  Color = 0x00FFFFFF;

          if (strncmp (gLines[i], "UNIXNU: PAC.", 12) == 0) {
            Color = 0x00FFFF00;
          } else if ((strncmp (gLines[i], "UNIXNU: DIAG.", 13) == 0) || (strncmp (gLines[i], "UNIXNU: HOLD", 12) == 0)) {
            Color = 0x0000FFFF;
          }

          DiagRenderLine (gLines[i], Columns, Scale, Color, 0, Pix);
          for (y = 0; y < LineH; y++) {
            for (x = 0; x < LineW; x++) {
              unsigned char  *p = Image + ((Y0 + i * LineH + y) * PANEL_W + X0 + x) * 3;

              p[0] = (unsigned char)(Pix[y * LineW + x] >> 16);
              p[1] = (unsigned char)(Pix[y * LineW + x] >> 8);
              p[2] = (unsigned char)(Pix[y * LineW + x]);
            }
          }
        }

        f = fopen (argv[1], "wb");
        if ((f == NULL) || (fwrite (Image, 3, PANEL_W * PANEL_H, f) != (size_t)PANEL_W * PANEL_H)) {
          printf ("FAIL cannot write %s\n", argv[1]);
          gFailures++;
        }

        if (f != NULL) {
          fclose (f);
        }

        printf ("preview: %s (%dx%d RGB24, SAMPLE values)\n", argv[1], PANEL_W, PANEL_H);
        free (Pix);
        free (Image);
      }
    }
  }

  printf ("epsilon-diag host test: %u failure(s)\n", gFailures);
  return gFailures ? 1 : 0;
}
