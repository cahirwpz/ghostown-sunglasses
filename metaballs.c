#include "sunglasses.h"
#include "bltop.h"
#include "coplist.h"
#include "memory.h"
#include "ilbm.h"
#include "2d.h"
#include "fx.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5
#define SIZE 80

static WORD active = 0;

static Point2D pos[2][3];
static BitmapT *metaball;
static BitmapT *carry;
static BitmapT *bgLeft[2], *bgRight[2];
static PaletteT *palette[2];
static CopInsT *bplptr[DEPTH];
static CopInsT *pal;
static CopListT *cp;
static PaletteT *activePalette;

static TrackT *metaball1_x, *metaball1_y;
static TrackT *metaball2_x, *metaball2_y;
static TrackT *metaball3_x, *metaball3_y;

static void Load() {
  bgLeft[0] = LoadILBMCustom("data/metaball-bg-left-1.ilbm", 0);
  bgRight[0] = LoadILBMCustom("data/metaball-bg-right-1.ilbm", 0);
  bgLeft[1] = LoadILBMCustom("data/metaball-bg-left-2.ilbm", 0);
  bgRight[1] = LoadILBMCustom("data/metaball-bg-right-2.ilbm", 0);
  metaball = LoadILBMCustom("data/metaball-1.ilbm", BM_LOAD_PALETTE);
  palette[0] = metaball->palette;
  palette[1] = LoadPalette("data/metaball-2.ilbm");

  metaball1_x = TrackLookup(tracks, "metaball1_x");
  metaball1_y = TrackLookup(tracks, "metaball1_y");
  metaball2_x = TrackLookup(tracks, "metaball2_x");
  metaball2_y = TrackLookup(tracks, "metaball2_y");
  metaball3_x = TrackLookup(tracks, "metaball3_x");
  metaball3_y = TrackLookup(tracks, "metaball3_y");
}

static void UnLoad() {
  WORD i;

  for (i = 0; i < 2; i++) {
    DeleteBitmap(bgLeft[i]);
    DeleteBitmap(bgRight[i]);
    DeletePalette(palette[i]);
  }
  DeleteBitmap(metaball);
}

static void SetInitialPositions() {
  WORD i, j;

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 3; j++) {
      pos[i][j].x = 160;
      pos[i][j].x = 128;
    }
  }
}

static void InterruptHandler() {
  if (frameFromStart < 16)
    FadeIn(activePalette, pal);
  if (frameTillEnd < 16)
    FadeOut(activePalette, pal);
}

static void Init(WORD ver) {
  WORD j;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_BLITHOG;

  for (j = 0; j < 2; j++) {
    BitmapClearArea(screen[j], DEPTH, 32, 0, WIDTH - 64, HEIGHT);
    BitmapCopy(screen[j], 0, 0, bgLeft[ver]);
    BitmapCopy(screen[j], WIDTH - 32, 0, bgRight[ver]);
  }

  activePalette = palette[ver];

  CopInit(cp);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Init1() {
  cp = NewCopList(100);
  carry = NewBitmap(SIZE + 16, SIZE, 2);

  BitmapMakeDisplayable(bgLeft[0]);
  BitmapMakeDisplayable(bgRight[0]);
  BitmapMakeDisplayable(metaball);

  SetInitialPositions();
  Init(0);
}

static void Init2() {
  BitmapMakeDisplayable(bgLeft[1]);
  BitmapMakeDisplayable(bgRight[1]);

  Init(1);
}

static void Kill1() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG;
}

static void Kill2() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG;

  DeleteBitmap(carry);
  DeleteCopList(cp);
}

static void ClearMetaballs() {
  WORD *val = (WORD *)pos[active];
  WORD n = 3;
  LONG x, y;

  while (--n >= 0) {
    x = *val++; y = *val++;
    BitmapClearArea(screen[active], DEPTH, x & ~15, y, SIZE + 16, SIZE);
  }
}

static void PositionMetaballs() {
  WORD *val = (WORD *)pos[active];

  *val++ = (WIDTH - SIZE) / 2 + TrackValueGet(metaball1_x, frameCount);
  *val++ = (HEIGHT - SIZE) / 2 + TrackValueGet(metaball1_y, frameCount);
  *val++ = (WIDTH - SIZE) / 2 + TrackValueGet(metaball2_x, frameCount);
  *val++ = (HEIGHT - SIZE) / 2 + TrackValueGet(metaball2_y, frameCount);
  *val++ = (WIDTH - SIZE) / 2 + TrackValueGet(metaball3_x, frameCount);
  *val++ = (HEIGHT - SIZE) / 2 + TrackValueGet(metaball3_y, frameCount);
}

static void DrawMetaballs() {
  WORD *val = (WORD *)pos[active];
  LONG x, y;

  x = *val++; y = *val++; BitmapCopyFast(screen[active], x, y, metaball);
  x = *val++; y = *val++; BitmapAddSaturated(screen[active], x, y, metaball, carry);
  x = *val++; y = *val++; BitmapAddSaturated(screen[active], x, y, metaball, carry);
}

static void Render() {
  // LONG lines = ReadLineCounter();

  // This takes about 100 lines. Could we do better?
  ClearMetaballs();
  PositionMetaballs();
  DrawMetaballs();

  // Log("metaballs : %ld\n", ReadLineCounter() - lines);

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, CopInsSet32(bplptr[i], screen[active]->planes[i]));
  active ^= 1;
}

EffectT Metaballs1 = { Load, NULL, Init1, Kill1, Render, InterruptHandler };
EffectT Metaballs2 = { NULL, UnLoad, Init2, Kill2, Render, InterruptHandler };
