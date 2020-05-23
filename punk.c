#include "sunglasses.h"
#include "hardware.h"
#include "bltop.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

static BitmapT *city;
static BitmapT *man;
static BitmapT *man_maska;
static BitmapT *napis;
static BitmapT *zmazy;
static BitmapT *zmazy_maska;
static CopInsT *pal[4];
static CopListT *cp;

static void Load() {
  city = LoadILBMCustom("data/punk-city.ilbm", BM_LOAD_PALETTE);
  napis = LoadILBMCustom("data/punk-napis.ilbm", BM_LOAD_PALETTE);
  man = LoadILBMCustom("data/punk-man.ilbm", BM_LOAD_PALETTE);
  man_maska = LoadILBMCustom("data/punk-man-maska.ilbm", 0);
  zmazy = LoadILBMCustom("data/punk-zmazy.ilbm", BM_LOAD_PALETTE);
  zmazy_maska = LoadILBMCustom("data/punk-zmazy-maska.ilbm", 0);
}

static void UnLoad() {
  DeletePalette(city->palette);
  DeleteBitmap(city);
  DeletePalette(napis->palette);
  DeleteBitmap(napis);
  DeletePalette(man->palette);
  DeleteBitmap(man);
  DeleteBitmap(man_maska);
  DeletePalette(zmazy->palette);
  DeleteBitmap(zmazy);
  DeleteBitmap(zmazy_maska);
}

static void Init() {
  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;
  BitmapClear(screen[0], DEPTH);

  cp = NewCopList(60);

  CopInit(cp);
  CopMakePlayfield(cp, NULL, screen[0], DEPTH);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  pal[0] = CopLoadColor(cp, 0, 7, 0);
  pal[1] = CopLoadColor(cp, 8, 15, 0);
  pal[2] = CopLoadColor(cp, 16, 23, 0);
  pal[3] = CopLoadColor(cp, 24, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteCopList(cp);
}

#define CITY_X 0
#define CITY_Y 160
#define NAPIS_X 13
#define NAPIS_Y 4
#define MAN_X 112
#define MAN_Y 32
#define ZMAZY_X 240
#define ZMAZY_Y 0

static void InterruptHandler1() {
  if (frameFromStart < 16)
    FadeIn(city->palette, pal[0]);
}

static void Init1() {
  BitmapMakeDisplayable(city);

  Init();
  BitmapCopy(screen[0], CITY_X, CITY_Y, city);
}

static void InterruptHandler2() {
  if (frameFromStart < 16)
    FadeIn(napis->palette, pal[1]);
}

static void Init2() {
  BitmapMakeDisplayable(napis);

  BlitterSetSync(screen[0], 3, NAPIS_X, NAPIS_Y, napis->width, napis->height, -1);
  BlitterSetSync(screen[0], 4, NAPIS_X, NAPIS_Y, napis->width, napis->height, 0);
  BitmapCopy(screen[0], NAPIS_X, NAPIS_Y, napis);
}

static void InterruptHandler3() {
  if (frameFromStart < 16)
    FadeIn(man->palette, pal[2]);
}

static void Init3() {
  BitmapMakeDisplayable(man);
  BitmapMakeDisplayable(man_maska);

  BlitterSetMaskedSync(screen[0], 3, MAN_X, MAN_Y, man_maska, 0);
  BlitterSetMaskedSync(screen[0], 4, MAN_X, MAN_Y, man_maska, -1);
  BitmapCopyMasked(screen[0], MAN_X, MAN_Y, man, man_maska);
}

static void InterruptHandler4() {
  if (frameFromStart < 16)
    FadeIn(zmazy->palette, pal[3]);
  if (frameTillEnd < 16) {
    FadeOut(city->palette, pal[0]);
    FadeOut(napis->palette, pal[1]);
    FadeOut(man->palette, pal[2]);
    FadeOut(zmazy->palette, pal[3]);
  }
}

static void Init4() {
  BitmapMakeDisplayable(zmazy);
  BitmapMakeDisplayable(zmazy_maska);

  BlitterSetMaskedSync(screen[0], 3, ZMAZY_X, ZMAZY_Y, zmazy_maska, -1);
  BlitterSetMaskedSync(screen[0], 4, ZMAZY_X, ZMAZY_Y, zmazy_maska, -1);
  BitmapCopyMasked(screen[0], ZMAZY_X, ZMAZY_Y, zmazy, zmazy_maska);
}

EffectT Punk1 = { Load, NULL, Init1, NULL, NULL, InterruptHandler1 };
EffectT Punk2 = { NULL, NULL, Init2, NULL, NULL, InterruptHandler2 };
EffectT Punk3 = { NULL, NULL, Init3, NULL, NULL, InterruptHandler3 };
EffectT Punk4 = { NULL, UnLoad, Init4, Kill, NULL, InterruptHandler4 };
