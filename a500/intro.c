#include "sunglasses.h"
#include "hardware.h"
#include "bltop.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

static BitmapT *amiga_logo;
static BitmapT *ghostown;
static BitmapT *riverwash;
static BitmapT *whelpz;

static CopInsT *pal[4];
static CopListT *cp;

static void Load() {
  amiga_logo = LoadILBM("data/logo-amiga.ilbm");
  ghostown = LoadILBM("data/logo-ghostown.ilbm");
  riverwash = LoadILBM("data/logo-riverwash.ilbm");
  whelpz = LoadILBM("data/logo-whelpz.ilbm");
}

static void UnLoad() {
  DeletePalette(amiga_logo->palette);
  DeleteBitmap(amiga_logo);
  DeletePalette(ghostown->palette);
  DeleteBitmap(ghostown);
  DeletePalette(riverwash->palette);
  DeleteBitmap(riverwash);
  DeletePalette(whelpz->palette);
  DeleteBitmap(whelpz);
}

static void Init() {
  cp = NewCopList(60);

  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopMakePlayfield(cp, NULL, screen[0], DEPTH);
  pal[0] = CopLoadColor(cp, 0, 7, 0);
  pal[1] = CopLoadColor(cp, 8, 15, 0);
  pal[2] = CopLoadColor(cp, 16, 23, 0);
  pal[3] = CopLoadColor(cp, 24, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteCopList(cp);
}

static void InterruptHandler1() {
  if (frameFromStart < 16)
    FadeIn(amiga_logo->palette, pal[0]);
}

static void Init1() {
  Init();
  BitmapCopy(screen[0], 0, 216, amiga_logo);
}

static void InterruptHandler2() {
  if (frameFromStart < 16)
    FadeIn(riverwash->palette, pal[1]);
}

static void Init2() {
  BlitterSetSync(screen[0], 3, 224, 216, riverwash->width, riverwash->height, -1);
  BitmapCopy(screen[0], 224, 216, riverwash);
}

static void InterruptHandler3() {
  if (frameFromStart < 16)
    FadeIn(ghostown->palette, pal[2]);
  if (frameTillEnd < 16)
    FadeOut(ghostown->palette, pal[2]);
}

static void Init3() {
  BlitterSetSync(screen[0], 4, 96, 64, ghostown->width, ghostown->height, -1);
  BitmapCopy(screen[0], 96, 64, ghostown);
}

static void InterruptHandler4() {
  if (frameFromStart < 16)
    FadeIn(whelpz->palette, pal[3]);
  if (frameTillEnd < 16) {
    FadeOut(amiga_logo->palette, pal[0]);
    FadeOut(riverwash->palette, pal[1]);
    FadeOut(whelpz->palette, pal[3]);
  }
}

static void Init4() {
  BlitterSetSync(screen[0], 3, 96, 64, whelpz->width, whelpz->height, -1);
  BitmapCopy(screen[0], 96, 64, whelpz);
}

EffectT Intro1 = { Load, NULL, Init1, NULL, NULL, InterruptHandler1 };
EffectT Intro2 = { NULL, NULL, Init2, NULL, NULL, InterruptHandler2 };
EffectT Intro3 = { NULL, NULL, Init3, NULL, NULL, InterruptHandler3 };
EffectT Intro4 = { NULL, UnLoad, Init4, Kill, NULL, InterruptHandler4 };
