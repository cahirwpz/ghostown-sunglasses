#include "sunglasses.h"
#include "hardware.h"
#include "bltop.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

static BitmapT *farted;

static CopInsT *pal;
static CopListT *cp;

static void Load() {
  farted = LoadILBM("data/who-farted.ilbm");
}

static void UnLoad() {
  DeletePalette(farted->palette);
  DeleteBitmap(farted);
}

static void Init() {
  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  BitmapClear(screen[0], DEPTH);
  BitmapCopy(screen[0], 32, (HEIGHT - 32) - farted->height, farted);

  cp = NewCopList(100);

  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopShowPlayfield(cp, screen[0]);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteCopList(cp);
}

static void InterruptHandler() {
  UpdateFrameCount();

  if (frameFromStart < 16)
    FadeIn(farted->palette, pal);
  if (frameTillEnd < 16)
    FadeOut(farted->palette, pal);
}

extern EffectT EndScroll;
extern EffectT Distortions1;

static void Render() {
  PrepareEffect(&EndScroll);
  PrepareEffect(&Distortions1);
}

EffectT WhoFarted = { Load, UnLoad, Init, Kill, Render, InterruptHandler };
