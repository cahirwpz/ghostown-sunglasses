#include "sunglasses.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "fx.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

static BitmapT *sunglasses;
static PaletteT *palette, *gradient[3];
static CopInsT *pal;
static CopListT *cp;
static WORD palnum = 1;

static TrackT *event;

static void Load() {
  sunglasses = LoadILBMCustom("data/sunglasses-neons-1.ilbm", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  gradient[0] = sunglasses->palette;
  gradient[1] = LoadPalette("data/sunglasses-neons-2.ilbm");
  gradient[2] = LoadPalette("data/sunglasses-neons-3.ilbm");

  event = TrackLookup(tracks, "event");
}

static void UnLoad() {
  DeletePalette(gradient[0]);
  DeletePalette(gradient[1]);
  DeletePalette(gradient[2]);
  DeleteBitmap(sunglasses);
}

static void Prepare() {
  BitmapUnpack(sunglasses, BM_DISPLAYABLE);
}

static void Init() {
  cp = NewCopList(60);

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopShowPlayfield(cp, sunglasses);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  palette = CopyPalette(gradient[0]);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER;

  DeletePalette(palette);
  DeleteCopList(cp);
}

static void InterruptHandler() {
  WORD n;

  UpdateFrameCount();

  n = TrackValueGet(event, frameCount);
  if (n > 0)
    palnum = n;
  else
    n = palnum;
  n--;

  {
    ColorT *src = gradient[n]->colors + 16;
    ColorT *dst = palette->colors + 16;
    WORD f = frameCount * 128;
    WORD i;

    for (i = 0; i < 16; i++) {
      ColorT c = src[((i - frameCount) & 15)];
      WORD r = c.r + normfx(SIN(f) * c.r) / 4;
      WORD g = c.g + normfx(SIN(f) * c.g) / 4;
      WORD b = c.b + normfx(SIN(f) * c.b) / 4;

      if (r < 0) r = 0;
      if (r > 255) r = 255;
      if (g < 0) g = 0;
      if (g > 255) g = 255;
      if (b < 0) b = 0;
      if (b > 255) b = 255;

      dst->r = r;
      dst->g = g;
      dst->b = b;
      dst++;
    }
  }

  if (frameFromStart < 32) {
    FadeBlack(palette, pal, frameFromStart / 2);
  } else if (frameTillEnd < 32) {
    FadeBlack(palette, pal, frameTillEnd / 2);
  } else {
    ColorT *src = palette->colors + 16;
    CopInsT *dst = pal + 16;
    WORD n = 16;

    while (--n >= 0) {
      CopInsSetRGB24(dst, src->r, src->g, src->b);
      src++; dst++;
    }
  }
}

extern EffectT Neons;

static void Render() {
  PrepareEffect(&Neons);
}

EffectT ThemeNeons = { Load, UnLoad, Init, Kill, Render, InterruptHandler, Prepare };
