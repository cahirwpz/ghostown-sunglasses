#include "sunglasses.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "bltop.h"
#include "circle.h"
#include "fx.h"
#include "2d.h"
#include "random.h"
#include "sprite.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4
#define HSIZE 32
#define VSIZE 10

#define LANE_W (WIDTH + HSIZE * 2)
#define LANE_H 40
#define CARS 50

#define LANEL_Y (HEIGHT / 2 - LANE_H - 16)
#define LANER_Y (HEIGHT / 2 + 16)

typedef struct {
  WORD speed;
  WORD x, y;
  BOOL active;
  BOOL side;
} Car;

static Car cars[CARS];

static UWORD active = 0;
static CopInsT *sprptr[8];
static CopListT *cp;
static CopInsT *pal[4];
static CopInsT *bplptr[2][4];
static BitmapT *carry;

static BitmapT *lanes[2];
static BitmapT *carLeft, *carLeft0, *carLeft1;
static BitmapT *carRight, *carRight0, *carRight1;
static BitmapT *laneBg;
static BitmapT *cityTop, *cityTop0, *cityTop1;
static BitmapT *cityBottom, *cityBottom0, *cityBottom1;
static SpriteT *nullspr;
static SpriteT *sprite[8];
static PaletteT *spritePal;

static TrackT *flash;

static void Load() {
  laneBg = LoadILBMCustom("data/highway-lane.ilbm", BM_LOAD_PALETTE);
  cityTop0 = LoadILBMCustom("data/highway-city-top-1.ilbm", BM_LOAD_PALETTE);
  cityTop1 = LoadILBMCustom("data/highway-city-top-2.ilbm", BM_LOAD_PALETTE);
  cityBottom0 = LoadILBMCustom("data/highway-city-bottom-1.ilbm", BM_LOAD_PALETTE);
  cityBottom1 = LoadILBMCustom("data/highway-city-bottom-2.ilbm", BM_LOAD_PALETTE);
  carLeft0 = LoadILBMCustom("data/highway-car-left-1.ilbm", BM_LOAD_PALETTE);
  carLeft1 = LoadILBMCustom("data/highway-car-left-2.ilbm", BM_LOAD_PALETTE);
  carRight0 = LoadILBMCustom("data/highway-car-right-1.ilbm", BM_LOAD_PALETTE);
  carRight1 = LoadILBMCustom("data/highway-car-right-2.ilbm", BM_LOAD_PALETTE);
  nullspr = NewSprite(0, FALSE);

  {
    BitmapT *title = LoadILBMCustom("data/highway-sprite.ilbm", BM_LOAD_PALETTE);
    ITER(i, 0, 7, sprite[i] = NewSpriteFromBitmap(24, title, 16 * i, 0));
    spritePal = title->palette;
    DeleteBitmap(title);
  }

  flash = TrackLookup(tracks, "flash");
}

static void UnLoad1() {
  DeletePalette(carLeft0->palette);
  DeleteBitmap(carLeft0);
  DeletePalette(carRight0->palette);
  DeleteBitmap(carRight0);
  DeletePalette(cityTop0->palette);
  DeleteBitmap(cityTop0);
  DeletePalette(cityBottom0->palette);
  DeleteBitmap(cityBottom0);
}

static void UnLoad2() {
  DeleteSprite(nullspr);
  ITER(i, 0, 7, DeleteSprite(sprite[i]));

  DeletePalette(carLeft1->palette);
  DeleteBitmap(carLeft1);
  DeletePalette(carRight1->palette);
  DeleteBitmap(carRight1);
  DeletePalette(cityTop1->palette);
  DeleteBitmap(cityTop1);
  DeletePalette(cityBottom1->palette);
  DeleteBitmap(cityBottom1);

  DeletePalette(laneBg->palette);
  DeleteBitmap(laneBg);
  DeletePalette(spritePal);
}

static void MakeCopperList(CopListT *cp) {
  CopInit(cp);
  CopMakeSprites(cp, sprptr, nullspr);
  CopLoadPal(cp, spritePal, 16);
  CopLoadPal(cp, spritePal, 20);
  CopLoadPal(cp, spritePal, 24);
  CopLoadPal(cp, spritePal, 28);

  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopShowPlayfield(cp, cityTop);
  CopWait(cp, Y(-18), 0);
  pal[0] = CopLoadColor(cp, 0, 15, 0);

  CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);

  {
    CopWait(cp, Y(LANEL_Y - 2), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
    pal[1] = CopLoadColor(cp, 0, 15, 0);
    CopMakePlayfield(cp, bplptr[0], lanes[0], DEPTH);
    CopMove16(cp, bpl1mod, 8);
    CopMove16(cp, bpl2mod, 8);

    CopWait(cp, Y(LANEL_Y), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);

    CopWait(cp, Y(LANEL_Y + LANE_H), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
  }

  {
    WORD y0 = LANEL_Y + LANE_H + 1;
    WORD y1 = LANER_Y - 2;

    while (y0 < y1) {
      CopWait(cp, Y(y0), 8);
      CopMove16(cp, bpldat[0], 0);
      y0++;
    }
  }

  {
    CopWait(cp, Y(LANER_Y - 1), 8);
    pal[2] = CopLoadColor(cp, 0, 15, 0);
    CopMakePlayfield(cp, bplptr[1], lanes[0], DEPTH);
    CopMove16(cp, bpl1mod, 8);
    CopMove16(cp, bpl2mod, 8);

    CopWait(cp, Y(LANER_Y), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
    CopWait(cp, Y(LANER_Y + LANE_H), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
  }

  {
    CopWait(cp, Y(LANER_Y + LANE_H + 1), 8);
    pal[3] = CopLoadColor(cp, 0, 15, 0);
    CopShowPlayfield(cp, cityBottom);
    CopWait(cp, Y(LANER_Y + LANE_H + 2), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
  }

  CopEnd(cp);

  ITER(i, 0, 7, CopInsSet32(sprptr[i], sprite[i]->data));
}

static WORD flash_env[] = {0, 4, 8, 12, 12, 10, 8, 6, 5, 4, 3, 2, 1, 0, 0, 0};

static void InterruptHandler() {
  if (frameFromStart < 16) {
    FadeIn(cityTop->palette, pal[0]);
    FadeIn(carLeft->palette, pal[1]);
    FadeIn(carRight->palette, pal[2]);
    FadeIn(cityBottom->palette, pal[3]);
  } else if (frameTillEnd < 16) {
    FadeOut(cityTop->palette, pal[0]);
    FadeOut(carLeft->palette, pal[1]);
    FadeOut(carRight->palette, pal[2]);
    FadeOut(cityBottom->palette, pal[3]);
  } else {
    WORD frame = ReadFrameCounter();
    WORD s = TrackValueGet(flash, frame);

    if (s > 0) {
      s = 16 - s;
      FadeWhite(cityTop->palette, pal[0], flash_env[s]);
      FadeWhite(carLeft->palette, pal[1], flash_env[s]);
      FadeWhite(carRight->palette, pal[2], flash_env[s]);
      FadeWhite(cityBottom->palette, pal[3], flash_env[s]);
    } else {
      WORD c = frame + 12;
      ContrastChange(cityTop->palette, pal[0], envelope[c % 24]);
      ContrastChange(cityBottom->palette, pal[3], envelope[c % 24]);
      if (frame >= MOD_POS_FRAME(0x0300)) {
        c += 12;
        ContrastChange(carLeft->palette, pal[1], envelope[c % 24] / 2);
        ContrastChange(carRight->palette, pal[2], envelope[c % 24] / 2);
      }
    }
  }
}

static void Init1() {
  static BitmapT recycled[2];

  lanes[0] = &recycled[0];
  lanes[1] = &recycled[1];

  InitSharedBitmap(lanes[0], LANE_W, LANE_H * 2, 4, screen[0]);
  InitSharedBitmap(lanes[1], LANE_W, LANE_H * 2, 4, screen[1]);

  BitmapMakeDisplayable(laneBg);
  BitmapMakeDisplayable(cityTop0);
  BitmapMakeDisplayable(cityBottom0);
  BitmapMakeDisplayable(carLeft0);
  BitmapMakeDisplayable(carRight0);

  cityTop = cityTop0;
  cityBottom = cityBottom0;
  carLeft = carLeft0;
  carRight = carRight0;

  carry = NewBitmap(HSIZE + 16, VSIZE, 2);

  cp = NewCopList(240);

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER;
}

static void Init2() {
  BitmapMakeDisplayable(laneBg);
  BitmapMakeDisplayable(cityTop1);
  BitmapMakeDisplayable(cityBottom1);
  BitmapMakeDisplayable(carLeft1);
  BitmapMakeDisplayable(carRight1);

  cityTop = cityTop1;
  cityBottom = cityBottom1;
  carLeft = carLeft1;
  carRight = carRight1;

  ITER(i, 0, 7, UpdateSpritePos(sprite[i], X(96 + 16 * i), Y(LANEL_Y + LANE_H + 4)));

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE;

  ITER(i, 0, 7, UpdateSpritePos(sprite[i], X(96 + 16 * i), Y(LANEL_Y + LANE_H + 4)));
}

static void Kill1() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;
}

static void Kill2() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE;

  DeleteBitmap(carry);
  DeleteCopList(cp);
}

static inline void CarInit(Car *car) {
  car->speed = random() & 15;
  car->x = 0;
  car->y = (random() & 3) * 10;
  car->active = TRUE;
  car->side = random() & 1;
}

static inline void CarMove(Car *car, WORD step) {
  car->x += (car->speed + 32) * step;
  if (car->x >= fx4i(LANE_W))
    car->active = FALSE;
}

/* Draw each active cars. */
static void DrawCars(WORD step) {
  Car *car = cars;
  Car *last = cars + CARS;

  do {
    if (car->active) {
      WORD x = (car->x + 7) / 16;

      if (car->side)
        BitmapAddSaturated(lanes[active], x, car->y + LANE_H, carRight, carry);
      else
        BitmapAddSaturated(lanes[active], LANE_W - x, car->y, carLeft, carry);

      CarMove(car, step);
    }
  } while (++car < last);
}

/* Add new car if there's a free slot. */
static inline void AddCar() {
  Car *car = cars;
  Car *last = cars + CARS;

  do {
    if (!car->active) {
      CarInit(car);
      break;
    }
  } while (++car < last);
}

static void AddCars() {
  static WORD iterCount = 0;

  iterCount += frameCount - lastFrameCount;
  while (iterCount > 10) {
    AddCar();
    iterCount -= 10;
  }
}

static void Render() {
  BitmapCopy(lanes[active], HSIZE, 0, laneBg);
  BitmapCopy(lanes[active], HSIZE, LANE_H, laneBg);

  AddCars();
  DrawCars(frameCount - lastFrameCount);

  WaitVBlank();
  {
    WORD i;

    for (i = 0; i < DEPTH; i++) {
      APTR bplpt = lanes[active]->planes[i] + 4;
      UWORD stride = lanes[active]->bytesPerRow;
      CopInsSet32(bplptr[0][i], bplpt);
      CopInsSet32(bplptr[1][i], bplpt + stride * LANE_H);
    }
  }
  active ^= 1;
}

EffectT Highway1 = { Load, UnLoad1, Init1, Kill1, Render, InterruptHandler };
EffectT Highway2 = { NULL, UnLoad2, Init2, Kill2, Render, InterruptHandler };
