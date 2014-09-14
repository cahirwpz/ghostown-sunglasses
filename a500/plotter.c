#include "sunglasses.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "bltop.h"
#include "circle.h"
#include "fx.h"
#include "sync.h"
#include "memory.h"

#define WIDTH 320
#define HEIGHT 206
#define DEPTH 3
#define SIZE 16
#define NUM1 37
#define ARMS1 3
#define NUM2 41
#define ARMS2 5

static UWORD active = 0;
static CopInsT *bplptr[DEPTH];
static CopInsT *pal;
static CopListT *cp;

static BitmapT *carry;
static BitmapT *tusk;
static BitmapT *flares;
static BitmapT *flare[8];
static PaletteT *palette[2];
static PaletteT *activePalette;
static TrackT *plotter_x;
static TrackT *plotter_y;
static TrackT *plotter_w;
static TrackT *plotter_h;

#define MAX_W 96
#define MAX_H 96

static struct {
  WORD xc, yc;
  WORD w, h;
} plotter;

static void Load() {
  flares = LoadILBMCustom("data/plotter-flares.ilbm", 0);
  tusk = LoadILBMCustom("data/plotter-tusk-1.ilbm", BM_LOAD_PALETTE);
  palette[0] = tusk->palette;
  palette[1] = LoadPalette("data/plotter-tusk-2.ilbm");
  plotter_x = TrackLookup(tracks, "plotter_x");
  plotter_y = TrackLookup(tracks, "plotter_y");
  plotter_w = TrackLookup(tracks, "plotter_w");
  plotter_h = TrackLookup(tracks, "plotter_h");
}

static void Prepare() {
  BitmapMakeDisplayable(flares);
  BitmapMakeDisplayable(tusk);
}

static void UnLoad() {
  DeleteBitmap(flares);
  DeleteBitmap(tusk);
  DeletePalette(palette[0]);
  DeletePalette(palette[1]);
}

static void InterruptHandler() {
  if (frameFromStart < 16)
    FadeIn(activePalette, pal);
  if (frameTillEnd < 16)
    FadeOut(activePalette, pal);
}

static void Init() {
  UWORD i;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  for (i = 0; i < 2; i++) {
    BitmapClear(screen[i], DEPTH);
    BitmapCopy(screen[i], 0, 0, tusk);
  }

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(25), WIDTH, HEIGHT);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  pal = CopLoadColor(cp, 0, 7, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Init1() {
  WORD i;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  for (i = 0; i < 8; i++) {
    flare[i] = NewBitmap(SIZE, SIZE, DEPTH);
    BitmapCopyArea(flare[i], 0, 0,
                   flares, 0, i * SIZE, SIZE, SIZE);
  }
  carry = NewBitmap(SIZE + 16, SIZE, 2);
  cp = NewCopList(50);

  activePalette = palette[0];

  Init();
}

static void Init2() {
  activePalette = palette[1];

  Init();
}

static void Kill1() {
  custom->dmacon = DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER;
}

static void Kill2() {
  custom->dmacon = DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER;

  ITER(i, 0, 7, DeleteBitmap(flare[i]));
  DeleteBitmap(carry);
  DeleteCopList(cp);
}

static void DrawPlotter1() {
  WORD i, a;
  WORD t = frameCount * 5;
  WORD da = 2 * SIN_PI / NUM1;

  plotter.xc = TrackValueGet(plotter_x, frameCount);
  plotter.yc = TrackValueGet(plotter_y, frameCount);
  plotter.w = TrackValueGet(plotter_w, frameCount);
  plotter.h = TrackValueGet(plotter_h, frameCount);

  for (i = 0, a = t; i < NUM1; i++, a += da) {
    WORD g = SIN(ARMS1 * a);
    WORD x = normfx(normfx(SIN(t + a) * g) * plotter.w) + plotter.xc + 160;
    WORD y = normfx(normfx(COS(t + a) * g) * plotter.h) + plotter.yc + 96;
    WORD f = normfx(g * 3);

    if (f < 0)
      f = -f;

    if ((i & 1) && (frameCount & 15) < 3)
      f = 7;

    BitmapAddSaturated(screen[active], x, y, flare[f], carry);
  }
}

static void DrawPlotter2() {
  WORD i, a;
  WORD t = frameCount * 5;
  WORD da = 2 * SIN_PI / NUM2;

  plotter.xc = TrackValueGet(plotter_x, frameCount);
  plotter.yc = TrackValueGet(plotter_y, frameCount);
  plotter.w = TrackValueGet(plotter_w, frameCount);
  plotter.h = TrackValueGet(plotter_h, frameCount);

  for (i = 0, a = t; i < NUM2; i++, a += da) {
    WORD g = SIN(ARMS2 * a);
    WORD x = normfx(normfx(SIN(t + a) * g) * plotter.w) + plotter.xc + 160;
    WORD y = normfx(normfx(COS(t + a) * g) * plotter.h) + plotter.yc + 96;
    WORD f = normfx(g * 3);

    if (f < 0)
      f = -f;

    if ((i & 1) && (frameCount & 15) < 3)
      f = 7;

    BitmapAddSaturated(screen[active], x, y, flare[f], carry);
  }
}

static void Render1() {
  BitmapClearArea(screen[active], DEPTH, 64, 0,
                  MAX_W * 2 + SIZE, MAX_H * 2 + SIZE);
  DrawPlotter1();

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, CopInsSet32(bplptr[i], screen[active]->planes[i]));
  active ^= 1;
}

static void Render2() {
  BitmapClearArea(screen[active], DEPTH, 64, 0,
                  MAX_W * 2 + SIZE, MAX_H * 2 + SIZE);
  DrawPlotter2();

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, CopInsSet32(bplptr[i], screen[active]->planes[i]));
  active ^= 1;
}

EffectT Plotter1 = { Load, NULL, Init1, Kill1, Render1, InterruptHandler, Prepare };
EffectT Plotter2 = { NULL, UnLoad, Init2, Kill2, Render2, InterruptHandler, Prepare };
