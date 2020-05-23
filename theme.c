#include "sunglasses.h"
#include "hardware.h"
#include "bltop.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

static BitmapT *clip[3];
static BitmapT *sunglasses;
static CopListT *cp;
static CopInsT *pal;

static TrackT *flash, *event;

static void Load() {
  sunglasses = LoadILBM("data/sunglasses.ilbm");
  clip[0] = LoadILBMCustom("data/sunglasses-clip1.ilbm", 0);
  clip[1] = LoadILBMCustom("data/sunglasses-clip2.ilbm", 0);
  clip[2] = LoadILBMCustom("data/sunglasses-clip3.ilbm", 0);

  flash = TrackLookup(tracks, "flash");
  event = TrackLookup(tracks, "event");
}

static void UnLoad() {
  DeleteBitmap(clip[0]);
  DeleteBitmap(clip[1]);
  DeleteBitmap(clip[2]);
  DeletePalette(sunglasses->palette);
  DeleteBitmap(sunglasses);
}

static void Prepare() {
  BitmapMakeDisplayable(clip[0]);
  BitmapMakeDisplayable(clip[1]);
  BitmapMakeDisplayable(clip[2]);
}

static void InterruptHandler() {
  UpdateFrameCount();

  if (frameFromStart < 32) {
    FadeBlack(sunglasses->palette, pal, frameFromStart / 2);
  } else if (frameTillEnd < 32) {
    FadeBlack(sunglasses->palette, pal, frameTillEnd / 2);
  } else {
    WORD s = TrackValueGet(flash, ReadFrameCounter());
    if (s > 0)
      FadeWhite(sunglasses->palette, pal, s / 2);
  }
}

static void Init() {
  cp = NewCopList(100);

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopShowPlayfield(cp, sunglasses);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteCopList(cp);
}

extern EffectT ThemeNeons;

static void Render() {
  PrepareEffect(&ThemeNeons);

  switch (TrackValueGet(event, frameCount)) {
    case 1: BitmapCopy(sunglasses, 31, 216, clip[0]); break;
    case 2: BitmapCopy(sunglasses, 79, 235, clip[1]); break;
    case 3: BitmapCopy(sunglasses, 95, 235, clip[2]); break;
    default: break;
  }
}

EffectT Theme = { Load, UnLoad, Init, Kill, Render, InterruptHandler, Prepare };
