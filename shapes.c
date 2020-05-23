#include "sunglasses.h"
#include "2d.h"
#include "bltop.h"
#include "coplist.h"
#include "fx.h"
#include "memory.h"
#include "ilbm.h"

#define WIDTH  320
#define HEIGHT 256
#define DEPTH  5

static PaletteT *palette[4];
static ShapeT *shapes[4];
static CopListT *cp;
static CopInsT *bplptr[DEPTH];
static CopInsT *pal;
static WORD plane, planeC;
static PaletteT *activePalette;

static void Load() {
  shapes[0] = LoadShape("data/boxes.2d");
  shapes[1] = LoadShape("data/night.2d");
  shapes[2] = LoadShape("data/star.2d");
  shapes[3] = LoadShape("data/glasses.2d");
  /* BUG: Some palette files thrash memory at loading! */
  palette[0] = LoadPalette("data/boxes-pal.ilbm");
  palette[1] = LoadPalette("data/night-pal.ilbm");
  palette[2] = LoadPalette("data/star-pal.ilbm");
  palette[3] = LoadPalette("data/glasses-pal.ilbm");
}

static void UnLoad() {
  DeleteShape(shapes[0]);
  DeleteShape(shapes[1]);
  DeleteShape(shapes[2]);
  DeleteShape(shapes[3]);
  DeletePalette(palette[0]);
  DeletePalette(palette[1]);
  DeletePalette(palette[2]);
  DeletePalette(palette[3]);
}

static void InterruptHandler() {
  if (frameFromStart < 16)
    FadeIn(activePalette, pal);
  if (frameTillEnd < 16)
    FadeOut(activePalette, pal);
}

static void Init() {
  plane = DEPTH - 1;
  planeC = 0;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;
  BitmapClear(screen[0], DEPTH);

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopMakePlayfield(cp, bplptr, screen[0], DEPTH);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Init1() {
  /* Set up clipping window. */
  ClipWin.minX = fx4i(0);
  ClipWin.maxX = fx4i(319);
  ClipWin.minY = fx4i(0);
  ClipWin.maxY = fx4i(255);

  cp = NewCopList(100);
  activePalette = palette[0];
  Init();
}

static void Init2() {
  activePalette = palette[1];
  Init();
}

static void Init3() {
  activePalette = palette[2];
  Init();
}

static void Init4() {
  activePalette = palette[3];
  Init();
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;
}

static void KillLast() {
  Kill();

  DeleteCopList(cp);
}

static Point2D tmpPoint[2][16];

static inline void DrawPolygon(Point2D *out, WORD n) {
  WORD *pos = (WORD *)out;
  WORD x1, y1, x2, y2;

  x1 = *pos++ >> 4;
  y1 = *pos++ >> 4;
  n--;

  while (--n >= 0) {
    x2 = *pos++ >> 4;
    y2 = *pos++ >> 4;
    BlitterLineSync(x1, y1, x2, y2);
    x1 = x2; y1 = y2;
  }
}

static __regargs void DrawShape(ShapeT *shape) {
  const PolygonT *polygon = shape->polygon;
  const Point2D *point = shape->viewPoint;
  const UBYTE *flags = shape->viewPointFlags;

  do {
    UBYTE clipFlags = 0;
    UBYTE outside = 0xff;

    {
      UWORD *vertex = shape->polygonVertex + polygon->index;
      Point2D *out = tmpPoint[0];
      WORD n = polygon->vertices;

      while (--n >= 0) {
        WORD k = *vertex++;
        clipFlags |= flags[k];
        outside &= flags[k];
        *out++ = point[k];
      }
    }

    if (!outside) {
      Point2D *out = tmpPoint[1];
      WORD n = ClipPolygon2D(tmpPoint[0], &out, polygon->vertices, clipFlags);
      DrawPolygon(out, n);
    }
  } while (++polygon < shape->polygon + shape->polygons);
}

static void Render(ShapeT *shape) {
  // LONG lines = ReadLineCounter();
  WORD i, a = frameCount * 64;
  Matrix2D t;

  BlitterClearSync(screen[0], plane);
  LoadIdentity2D(&t);
  Rotate2D(&t, frameCount * 8);
  Scale2D(&t, fx12f(1.0) + SIN(a) / 2, fx12f(1.0) + COS(a) / 2);
  Translate2D(&t, fx4i(WIDTH / 2), fx4i(HEIGHT / 2));
  Transform2D(&t, shape->viewPoint, shape->origPoint, shape->points);
  PointsInsideBox(shape->viewPoint, shape->viewPointFlags, shape->points);
  WaitBlitter();
  BlitterLineSetup(screen[0], plane, LINE_EOR, LINE_ONEDOT);
  DrawShape(shape);
  BlitterFillSync(screen[0], plane);
  WaitBlitter();
  // Log("shape: %ld\n", ReadLineCounter() - lines);
  WaitVBlank();

  for (i = 0; i < DEPTH; i++) {
    WORD j = (plane + i) % DEPTH;
    CopInsSet32(bplptr[i], screen[0]->planes[j]);
  }

  if (planeC & 1)
    plane = (plane + 1) % DEPTH;

  planeC ^= 1;
}

static void Render1() {
  Render(shapes[0]);
}

static void Render2() {
  Render(shapes[1]);
}

static void Render3() {
  Render(shapes[2]);
}

static void Render4() {
  Render(shapes[3]);
}

EffectT Shapes1 = { Load, NULL, Init1, Kill, Render1, InterruptHandler };
EffectT Shapes2 = { NULL, NULL, Init2, Kill, Render2, InterruptHandler };
EffectT Shapes3 = { NULL, NULL, Init3, Kill, Render3, InterruptHandler };
EffectT Shapes4 = { NULL, UnLoad, Init4, KillLast, Render4, InterruptHandler };
