#include "sunglasses.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "bltop.h"
#include "2d.h"
#include "fx.h"
#include "color.h"
#include "random.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

#define PNUM 19

static UWORD active = 0;
static CopInsT *bplptr[5];

static BitmapT *background;
static PaletteT *palette[3], *tempPalette;
static CopListT *cp;
static CopInsT *pal[3];

static Area2D grt_area[2][PNUM];

typedef struct {
  WORD color;
  char *filename;
  BitmapT *bitmap;
  Point2D pos;
} GreetingT;

static GreetingT greeting[PNUM] = {
  { 0, "data/greet_ada.ilbm" },
  { 0, "data/greet_blacksun.ilbm" },
  { 1, "data/greet_dcs.ilbm" },
  { 0, "data/greet_dekadence.ilbm" },
  { 1, "data/greet_desire.ilbm" },
  { 0, "data/greet_dinx.ilbm" },
  { 1, "data/greet_elude.ilbm" },
  { 0, "data/greet_fd.ilbm" },
  { 1, "data/greet_floppy.ilbm" },
  { 0, "data/greet_lemon.ilbm" },
  { 1, "data/greet_loonies.ilbm" },
  { 1, "data/greet_moods.ilbm" },
  { 0, "data/greet_nah.ilbm" },
  { 0, "data/greet_rno.ilbm" },
  { 1, "data/greet_skarla.ilbm" },
  { 0, "data/greet_speccy.ilbm" },
  { 0, "data/greet_tulou.ilbm" },
  { 1, "data/greet_wanted.ilbm" },
  { 1, "data/greet_ycrew.ilbm" }
};

static void PositionGreetings() {
  GreetingT *grt = greeting;
  WORD y = HEIGHT + 200;
  WORD i;
  
  for (i = 0; i < PNUM; i++) {
    Point2D *pos = &grt->pos;
    BitmapT *src = grt->bitmap;

    pos->x = (i & 1) ? (WIDTH / 2 - 64) : (WIDTH / 2 + 64 - src->width);
    pos->y = y;

    y += src->height / 2 + (random() & 31) + 10;

    grt++;
  }
}

static void Load() {
  WORD i;

  for (i = 0; i < PNUM; i++)
    greeting[i].bitmap = LoadILBMCustom(greeting[i].filename, BM_KEEP_PACKED);

  background = LoadILBMCustom("data/neons.ilbm", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  palette[0] = background->palette;
  palette[1] = LoadPalette("data/greet_moods.ilbm");
  palette[2] = LoadPalette("data/greet_rno.ilbm");

  PositionGreetings();
}

static void UnLoad() {
  ITER(i, 0, PNUM - 1, DeleteBitmap(greeting[i].bitmap));
  DeleteBitmap(background);
  DeletePalette(palette[0]);
  DeletePalette(palette[1]);
  DeletePalette(palette[2]);
}

static void Prepare() {
  WORD i;

  BitmapUnpack(background, BM_DISPLAYABLE);

  for (i = 0; i < PNUM; i++)
    BitmapUnpack(greeting[i].bitmap, 0);
}

static void RotatePalette() {
  ColorT *src = palette[0]->colors;
  UBYTE *dst = (UBYTE *)&tempPalette->colors[1];
  LONG i = frameCount;
  WORD n = 15;

  while (--n >= 0) {
    UBYTE *c = (UBYTE *)&src[i++ & 15];
    *dst++ = *c++;
    *dst++ = *c++;
    *dst++ = *c++;
  }
}

static void InterruptHandler() {
  RotatePalette();

  if (frameFromStart < 32) {
    FadeBlack(tempPalette, pal[0], frameFromStart / 2);
    FadeBlack(palette[1], pal[1], frameFromStart / 2);
    FadeBlack(palette[2], pal[2], frameFromStart / 2);
  } else if (frameTillEnd < 32) {
    FadeBlack(tempPalette, pal[0], frameTillEnd / 2);
    FadeBlack(palette[1], pal[1], frameTillEnd / 2);
    FadeBlack(palette[2], pal[2], frameTillEnd / 2);
  } else {
    WORD frame1 = (frameFromStart / 2) % 24;
    WORD frame2 = (frame1 >= 12) ? (frame1 - 12) : (frame1 + 12);

    ContrastChange(tempPalette, pal[0], SIN(frameCount * 128) >> 9);
    FadeWhite(palette[1], pal[1], envelope[frame1] / 2);
    FadeWhite(palette[2], pal[2], envelope[frame2] / 2);
  }
}

static void Init() {
  WORD i;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  for (i = 0; i < PNUM; i++)
    BitmapMakeDisplayable(greeting[i].bitmap);

  BitmapCopy(screen[0], 0, 0, background);
  BitmapCopy(screen[1], 0, 0, background);

  BlitterClearSync(screen[0], 4);
  BlitterClearSync(screen[1], 4);

  tempPalette = CopyPalette(palette[0]);
  cp = NewCopList(100);

  CopInit(cp);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  pal[0] = CopLoadColor(cp, 0, 15, 0);
  pal[1] = CopLoadColor(cp, 16, 23, 0);
  pal[2] = CopLoadColor(cp, 24, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER;

  DeletePalette(tempPalette);
  DeleteCopList(cp);
}

void ClearCliparts() {
  Area2D *area = grt_area[active];
  BitmapT *dst = screen[active];
  WORD n = PNUM;

  while (--n >= 0) {
    WORD x = area->x;
    WORD y = area->y;
    WORD w = area->w;
    WORD h = area->h;

    if (h > 0) {
      if (h > 8) { y += h - 8; h = 8; }
      BlitterSetSync(dst, 4, x, y, w, h, 0);
      BitmapCopyArea(dst, x, y, background, x, y, w, h);
    }

    area++;
  }
}

static void DrawCliparts() {
  GreetingT *grt = greeting;
  Area2D *area = grt_area[active];
  BitmapT *dst = screen[active];
  WORD step = (frameCount - lastFrameCount) * 3;
  WORD n = PNUM;

  while (--n >= 0) {
    BitmapT *src = grt->bitmap;
    WORD dy = grt->pos.y;
    WORD sy = 0;
    WORD sh = src->height;

    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dy + sh >= HEIGHT) { sh = HEIGHT - dy; }

    if (sh > 0) {
      area->x = grt->pos.x;
      area->y = dy;
      area->w = src->width;
      area->h = sh;

      BitmapCopyArea(dst, grt->pos.x, dy, src, 0, sy, src->width, sh);
      BlitterSetSync(dst, 3, grt->pos.x, dy, src->width, sh, grt->color ? 0 : -1);
      BlitterSetSync(dst, 4, grt->pos.x, dy, src->width, sh, -1);
    } else {
      area->h = 0;
    }

    grt->pos.y -= step;
    grt++;
    area++;
  }
}

static void Render() {
  // LONG lines = ReadLineCounter();

  ClearCliparts();
  DrawCliparts();
  
  // Log("neons: %ld\n", ReadLineCounter() - lines);

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, CopInsSet32(bplptr[i], screen[active]->planes[i]));
  active ^= 1;
}

EffectT Neons = { Load, UnLoad, Init, Kill, Render, InterruptHandler, Prepare };
