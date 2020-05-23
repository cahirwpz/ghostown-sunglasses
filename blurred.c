#include "sunglasses.h"
#include "bltop.h"
#include "coplist.h"
#include "memory.h"
#include "ilbm.h"
#include "2d.h"
#include "fx.h"
#include "circle.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5
#define SIZE 128

static UWORD active = 0;

static BitmapT *clip[2];
static BitmapT *equaliser;
static BitmapT *buffer;
static BitmapT *carry;
static PaletteT *palette[6];
static CopListT *cp;
static CopInsT *bplptr[2][DEPTH];
static CopInsT *pal[2];
static WORD palVer = -1;

static TrackT *flash;

static void Load() {
  equaliser = LoadILBMCustom("data/equaliser.ilbm", 0);
  clip[0] = LoadILBMCustom("data/blurred-a-clip.ilbm", 0);
  clip[1] = LoadILBMCustom("data/blurred-b-clip.ilbm", 0);

  palette[0] = LoadPalette("data/blurred-a-pal-1.ilbm");
  palette[1] = LoadPalette("data/blurred-a-pal-2.ilbm");
  palette[2] = LoadPalette("data/blurred-b-pal-1.ilbm");
  palette[3] = LoadPalette("data/blurred-b-pal-2.ilbm");
  palette[4] = LoadPalette("data/blurred-c-pal-1.ilbm");
  palette[5] = LoadPalette("data/blurred-c-pal-2.ilbm");

  flash = TrackLookup(tracks, "flash");
}

static void UnLoad() {
  DeleteBitmap(equaliser);
  DeleteBitmap(clip[0]);
  DeleteBitmap(clip[1]);
  DeletePalette(palette[0]);
  DeletePalette(palette[1]);
  DeletePalette(palette[2]);
  DeletePalette(palette[3]);
  DeletePalette(palette[4]);
  DeletePalette(palette[5]);
}

static WORD iterCount = 0;

static void MakeCopperList(CopListT *cp) {
  WORD i;

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopMakePlayfield(cp, bplptr[0], screen[active], DEPTH);
  CopWait(cp, Y(-18), 0);
  pal[0] = CopLoadColor(cp, 0, 31, 0);
  CopWait(cp, Y(127), 0);
  CopMove16(cp, dmacon, DMAF_RASTER);
  pal[1] = CopLoadColor(cp, 0, 31, 0);
  CopWait(cp, Y(128), 0);
  CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
  for (i = 0; i < DEPTH; i++)
    bplptr[1][i] = CopMove32(cp, bplpt[i], screen[active]->planes[i] - WIDTH / 16);
  CopEnd(cp);
}

static WORD flash_env[] = {0, 4, 8, 12, 12, 10, 8, 6, 5, 4, 3, 2, 1, 0, 0, 0};

static void InterruptHandler() {
  if (frameFromStart < 16) {
    FadeIn(palette[0 + palVer * 2], pal[0]);
    FadeIn(palette[1 + palVer * 2], pal[1]);
  } else if (frameTillEnd < 16) {
    FadeOut(palette[0 + palVer * 2], pal[0]);
    FadeOut(palette[1 + palVer * 2], pal[1]);
  } else {
    WORD s = TrackValueGet(flash, ReadFrameCounter());
    if (s > 0) {
      s = 16 - s;
      FadeWhite(palette[0 + palVer * 2], pal[0], flash_env[s]);
      FadeWhite(palette[1 + palVer * 2], pal[1], flash_env[s]);
    }
  }
}

static void Init1() {
  WORD i;

  BitmapMakeDisplayable(clip[0]);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_BLITHOG;

  for (i = 0; i < 2; i++) {
    BitmapClear(screen[i], DEPTH);

    /* Make the center of blurred shape use colors from range 16-31. */
    WaitBlitter();
    CircleEdge(screen[i], 4, SIZE / 2 + 16, SIZE / 2, SIZE / 4 - 1);
    BlitterFillSync(screen[i], 4);

    BitmapCopy(screen[i], WIDTH / 2, 0, clip[0]);
  }

  cp = NewCopList(200);
  carry = NewBitmap(SIZE, SIZE, 2);
  buffer = NewBitmap(SIZE, SIZE, 4);

  palVer = 0;

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Init2() {
  WORD i;

  BitmapMakeDisplayable(clip[1]);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_BLITHOG;

  for (i = 0; i < 2; i++) {
    BlitterSetSync(screen[i], 4, WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2, -1);
    BitmapCopy(screen[i], WIDTH / 2, 0, clip[1]);
  }

  palVer = 1;

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Init3() {
  BitmapMakeDisplayable(equaliser);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_BLITHOG;

  BitmapClearArea(screen[0], DEPTH, WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2);
  BitmapClearArea(screen[1], DEPTH, WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2);
  WaitBlitter();

  palVer = 2;

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG;

  DeleteCopList(cp);
  DeleteBitmap(carry);
  DeleteBitmap(buffer);
}

static void RotatingTriangle(WORD t, WORD phi, WORD size) {
  Point2D p[3];
  WORD i, j;

  /* Calculate vertices of a rotating triangle. */
  for (i = 0; i < 3; i++) {
    WORD k = SIN(t + phi) / 2;
    WORD x = SIN(k + i * (SIN_PI * 2 / 3));
    WORD y = COS(k + i * (SIN_PI * 2 / 3));

    p[i].x = normfx((WORD)size * (WORD)x) / 2 + SIZE / 2;
    p[i].y = normfx((WORD)size * (WORD)y) / 2 + SIZE / 2;
  }

  /* Create a bob with rotating triangle. */
  for (i = 0, j = 1; i < 3; i++, j = (i == 2 ? 0 : i + 1))
    BlitterLineSync(p[i].x, p[i].y, p[j].x, p[j].y);
}

static void DrawShape() {
  BlitterClearSync(carry, 0);

  WaitBlitter();
  BlitterLineSetup(carry, 0, LINE_EOR, LINE_ONEDOT);

  RotatingTriangle(iterCount * 16, 0, SIZE - 1);
  RotatingTriangle(iterCount * 16, SIN_PI * 2 / 3, SIZE - 1);
  RotatingTriangle(-iterCount * 16, SIN_PI * 2 / 3, SIZE / 2 - 1);

  BlitterFillSync(carry, 0);
}

static void Render() {
  if (iterCount & 1)
    BitmapDecSaturated(buffer, carry);

  iterCount++;

  DrawShape();
  BitmapIncSaturated(buffer, carry);

  BitmapCopy(screen[active], 16, 0, buffer);
}

static void Render1() {
  // LONG lines = ReadLineCounter();

  Render();

  // Log("blurred: %ld\n", ReadLineCounter() - lines);

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, {
    CopInsSet32(bplptr[0][i], screen[active]->planes[i]);
    CopInsSet32(bplptr[1][i], screen[active]->planes[i] - WIDTH / 16);
    });
  active ^= 1;
}

static void Render2() {
  WORD i;
  // LONG lines = ReadLineCounter();

  Render();

  for (i = 0; i < 4; i++) {
    WORD v = P61_visuctr[i];

    if (v > 25)
      v = 25;
    v *= 5;

    BitmapCopy(screen[active], 176 + 32 * i, 0, equaliser);
    BitmapClearArea(screen[active], DEPTH, 176 + 32 * i, 0, 32, v + 3);
  }

  // Log("blurred: %ld\n", ReadLineCounter() - lines);
  WaitBlitter();
  WaitVBlank();
  ITER(i, 0, DEPTH - 1, {
    CopInsSet32(bplptr[0][i], screen[active]->planes[i]);
    CopInsSet32(bplptr[1][i], screen[active]->planes[i] - WIDTH / 16);
    });
  active ^= 1;
}

EffectT BlurredShapes1 = { Load, NULL, Init1, NULL, Render1, InterruptHandler };
EffectT BlurredShapes2 = { NULL, NULL, Init2, NULL, Render1, InterruptHandler };
EffectT BlurredShapes3 = { NULL, UnLoad, Init3, Kill, Render2, InterruptHandler };
