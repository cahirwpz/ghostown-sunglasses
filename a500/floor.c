#include "sunglasses.h"
#include "bltop.h"
#include "coplist.h"
#include "memory.h"
#include "fx.h"
#include "random.h"
#include "color.h"
#include "tga.h"
#include "ilbm.h"
#include "sprite.h"
#include "color.h"

#define WIDTH 320
#define HEIGHT 212
#define DEPTH 2
#define CPX 0x20

#define N 1024
#define SIZE 8
#define TILES (2 * SIZE)
#define TILESIZE (N / SIZE)

#define STRIDE ((SIZE * 2 + 1) * sizeof(CopInsT))

/* These are selected in such a way so line lenght
 * (HEIGHT - FAR_Y) is divisible by 8. */
#define NEAR_Z 102
#define FAR_Z 318
#define FAR_Y (HEIGHT * NEAR_Z / FAR_Z)
#define FAR_W (WIDTH * FAR_Z / 256)

static UWORD active = 0;

typedef struct {
  LONG delta;
  WORD x1, x2, y2;
  BYTE pad[6];
} LineDataT;

static LineDataT vert[N];
static WORD horiz[N];
static UBYTE *linePos[2][SIZE];
static UWORD *lineColor[2][SIZE];
static UWORD tileColumn[HEIGHT];
static PixmapT *texture;
static UWORD tileColor[TILES * TILES];
static UBYTE cycleStart[TILES * TILES];
static CopListT *cp[2];
static CopInsT *spr0dat[2][8];
static CopInsT *spr1ptr[2][8];
static SpriteT *headSpr[3][4];
static PaletteT *headPal[3];
static SpriteT *textSpr[3][3];
static PaletteT *textPal[3];
static SpriteT *nullspr;
static BitmapT *city;

static TrackT *floor_x, *floor_y;

static WORD activeSet = -1;

static void FloorPrecalc() {
  WORD i;

  for (i = 0; i < N; i++) {
    WORD x = (i - N / 2) * FAR_W / N;
    WORD far_x = WIDTH / 2 + (x * 256) / FAR_Z;
    WORD near_x = WIDTH / 2 + (x * 256) / NEAR_Z;
    WORD near_y = HEIGHT - 1;
    
    if (near_x < 0) {
      near_y = FAR_Y + (0 - far_x) * ((HEIGHT - 1) - FAR_Y) / (near_x - far_x);
      near_x = 0;
    }
    if (near_x > WIDTH - 1) {
      near_y = FAR_Y +  ((WIDTH - 1) - far_x) * ((HEIGHT - 1) - FAR_Y) / (near_x - far_x);
      near_x = WIDTH - 1;
    }
    
    vert[i].x1 = far_x;
    vert[i].x2 = near_x;
    vert[i].y2 = near_y;

    if (near_y != FAR_Y)
      vert[i].delta = ((X(near_x) / 2 - X(far_x) / 2) << 16) / (near_y - FAR_Y);
  }

  for (i = 0; i < N; i++) {
    WORD z = FAR_Z + ((NEAR_Z - FAR_Z) * i) / N;
    
    horiz[i] = HEIGHT * NEAR_Z / z;
  }
}

static void LoadSprite(const char *filename, SpriteT **sprite) {
  BitmapT *bitmap = LoadILBM(filename);
  WORD n = bitmap->width / 16;
  ITER(i, 0, n - 1, sprite[i] = NewSpriteFromBitmap(bitmap->height, bitmap, 16 * i, 0));
  DeletePalette(bitmap->palette);
  DeleteBitmap(bitmap);
}

static void Load() {
  texture = LoadTGA("data/floor.tga", PM_RGB4, MEMF_PUBLIC);

  nullspr = NewSprite(0, FALSE);

  LoadSprite("data/credits-cahir.ilbm", headSpr[0]);
  LoadSprite("data/credits-jazzcat.ilbm", headSpr[1]);
  LoadSprite("data/credits-slayer.ilbm", headSpr[2]);

  LoadSprite("data/credits-cahir-txt.ilbm", textSpr[0]);
  LoadSprite("data/credits-jazzcat-txt.ilbm", textSpr[1]);
  LoadSprite("data/credits-slayer-txt.ilbm", textSpr[2]);

  headPal[0] = LoadPalette("data/credits-cahir.ilbm");
  headPal[1] = LoadPalette("data/credits-jazzcat.ilbm");
  headPal[2] = LoadPalette("data/credits-slayer.ilbm");

  textPal[0] = LoadPalette("data/credits-cahir-txt.ilbm");
  textPal[1] = LoadPalette("data/credits-jazzcat-txt.ilbm");
  textPal[2] = LoadPalette("data/credits-slayer-txt.ilbm");

  city = LoadILBMCustom("data/floor-city.ilbm", 0);

  FloorPrecalc();

  ITER(i, 0, 255, cycleStart[i] = random() & 63);

  floor_x = TrackLookup(tracks, "floor_x");
  floor_y = TrackLookup(tracks, "floor_y");
}

static void UnLoad() {
  WORD i;

  DeleteBitmap(city);
  DeletePixmap(texture);
  DeleteSprite(nullspr);

  for (i = 0; i < 3; i++) {
    DeletePalette(headPal[i]);
    DeletePalette(textPal[i]);
    ITER(j, 0, 3, DeleteSprite(headSpr[i][j]));
    ITER(j, 0, 2, DeleteSprite(textSpr[i][j]));
  }
}

static void MakeCopperList(CopListT *cp, WORD num) {
  CopInsT *ins;
  WORD i, j;

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopMakePlayfield(cp, NULL, screen[num], DEPTH);
  CopLoadColor(cp, 0, 3, 0);
  CopMakeSprites(cp, spr0dat[num], nullspr);
  CopLoadPal(cp, headPal[0], 16);

  for (i = 0; i < FAR_Y; i++) {
    WORD s = i * 15 / FAR_Y;

    CopWait(cp, Y(i), 0);
    CopSetRGB(cp, 3, ColorTransition(0, 0xADF, s));
  }

  CopMakeManualSprites(cp, spr1ptr[num], nullspr);
  CopLoadPal(cp, textPal[0], 16);
  CopLoadPal(cp, textPal[0], 20);
  CopLoadPal(cp, textPal[0], 24);
  CopLoadPal(cp, textPal[0], 28);

  for (i = FAR_Y; i < HEIGHT; i++) {
    CopWait(cp, Y(i), CPX);
    for (j = 0; j < SIZE; j++) {
      ins = CopWait(cp, Y(i), CPX);
      if (i == FAR_Y)
        linePos[num][j] = ((UBYTE*)ins) + 1;
      ins = CopSetRGB(cp, (j & 1) + 1, (j & 1) ? 0xff0 : 0x0ff);
      if (i == FAR_Y)
        lineColor[num][j] = ((UWORD*)ins);
    }
  }

  CopWait(cp, Y(i) & 255, 0);
  CopLoadColor(cp, 0, 3, 0);
  CopEnd(cp);
}

static __regargs void UpdateHeadPos(SpriteT **head, WORD x, WORD y) {
  CopInsT **spr0dat0 = spr0dat[0];
  CopInsT **spr0dat1 = spr0dat[1];
  WORD n = 4;

  while (--n >= 0) {
    SpriteT *spr = *head++;

    UpdateSpritePos(spr, x, y); x += 16;
    CopInsSet32(*spr0dat0++, spr->data);
    CopInsSet32(*spr0dat0++, spr->attached->data);
    CopInsSet32(*spr0dat1++, spr->data);
    CopInsSet32(*spr0dat1++, spr->attached->data);
  }
}

static __regargs void UpdateTextPos(SpriteT **text, WORD x, WORD y) {
  CopInsT **spr1ptr0 = spr1ptr[0];
  CopInsT **spr1ptr1 = spr1ptr[1];
  WORD n = 3;

  while (--n >= 0) {
    SpriteT *spr = *text++;
    CopInsT *ins0 = *spr1ptr0++;
    CopInsT *ins1 = *spr1ptr1++;

    UpdateSpritePos(spr, x, y); x += 16;
    CopInsSet16(ins0++, spr->data[0]);
    CopInsSet16(ins1++, spr->data[0]);
    CopInsSet16(ins0++, spr->data[1]);
    CopInsSet16(ins1++, spr->data[1]);
    CopInsSet32(ins0++, &spr->data[2]);
    CopInsSet32(ins1++, &spr->data[2]);
  }
}

static void InterruptHandler() {
  if (activeSet >= 0) {
    if (frameFromStart <= 32) {
      WORD x = frameFromStart * 2 - 64;
      UpdateHeadPos(headSpr[activeSet], X(x), Y(0));
      UpdateTextPos(textSpr[activeSet], X(x), Y(FAR_Y));
    } else if (frameTillEnd <= 32) {
      WORD x = frameTillEnd * 2 - 64;
      UpdateHeadPos(headSpr[activeSet], X(x), Y(0));
      UpdateTextPos(textSpr[activeSet], X(x), Y(FAR_Y));
    } else {
      UpdateHeadPos(headSpr[activeSet], X(0), Y(0));
      UpdateTextPos(textSpr[activeSet], X(0), Y(FAR_Y));
    }
  }
}

static void Prepare() {
  BitmapMakeDisplayable(city);
}

static void Init1() {
  WORD i;

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  for (i = 0; i < 2; i++) {
    BitmapClear(screen[i], DEPTH);

    BlitterSetSync(screen[i], 0, 0, 0, WIDTH, 36, -1);
    BitmapCopy(screen[i], 0, 36, city);
    BlitterSetSync(screen[i], 1, 0, 0, WIDTH, FAR_Y, -1);
  }

  cp[0] = NewCopList((HEIGHT - FAR_Y) * STRIDE / sizeof(CopInsT) + 500);
  cp[1] = NewCopList((HEIGHT - FAR_Y) * STRIDE / sizeof(CopInsT) + 500);

  ITER(j, 0, 1, MakeCopperList(cp[j], j));
  CopListActivate(cp[active]);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_SPRITE;
}

static void Init2() {
  activeSet = 0;
}

static void Init3() {
  activeSet = 1;
}

static void Init4() {
  activeSet = 2;
}

static void Kill() {
  custom->dmacon = DMAF_SPRITE | DMAF_RASTER | DMAF_BLITTER;

  DeleteCopList(cp[0]);
  DeleteCopList(cp[1]);
}

static __regargs void ClearLine(WORD k) {
  UBYTE *pos = linePos[active][k];
  UBYTE x = (k < 4 ? CPX : (X(WIDTH) >> 1)) | 1;
  WORD n = (HEIGHT - FAR_Y) / 8;

  while (--n >= 0) {
    pos[STRIDE * 0] = x;
    pos[STRIDE * 1] = x;
    pos[STRIDE * 2] = x;
    pos[STRIDE * 3] = x;
    pos[STRIDE * 4] = x;
    pos[STRIDE * 5] = x;
    pos[STRIDE * 6] = x;
    pos[STRIDE * 7] = x;
    pos += STRIDE * 8;
  }
}

static void ClearFloor() {
  BitmapT *buffer = screen[active];
  UWORD bltsize = ((buffer->height - FAR_Y) << 6) + (buffer->width >> 4);
  APTR bltpt = buffer->planes[0] + FAR_Y * WIDTH / 8;
  WORD i = SIZE / 2;

  WaitBlitter();
  custom->bltadat = 0;
  custom->bltdmod = 0;
  custom->bltcon0 = DEST;
  custom->bltcon1 = 0;
  custom->bltdpt = bltpt;
  custom->bltsize = bltsize;

  bltpt += buffer->bplSize;
  while (i < SIZE)
    ClearLine(i++);

  WaitBlitter();
  custom->bltdpt = bltpt;
  custom->bltsize = bltsize;
}

static inline void CopperLine(UBYTE *pos, WORD x1, WORD y2, LONG delta) {
  if (y2 > FAR_Y) {
    WORD n = y2 - FAR_Y + 1;
    LONG x = (X(x1) / 2) << 16;
    register UBYTE one asm("d7") = 1;

    while (--n >= 0) {
      *pos = swap16(x) | one;
      pos += STRIDE;
      x += delta;
    }
  }
}

static __regargs void DrawStripesCopper(WORD xo) {
  /* Color switching with copper. */
  WORD xi = (xo & (TILESIZE - 1));
  UBYTE **activeLinePos = linePos[active];
  WORD n = SIZE;

  while (--n >= 0) {
    LineDataT *l = &vert[xi & (N - 1)];
    CopperLine(*activeLinePos++, l->x1, l->y2, l->delta);
    xi += TILESIZE;
  }
}

static __regargs void DrawStripes(WORD xo, WORD kxo) {
  LineDataT first = { 0, 0, WIDTH - 1, FAR_Y };
  LineDataT *l0, *l1;
  WORD k;

  xo &= TILESIZE - 1;

  /* Even stripes. */
  WaitBlitter();
  BlitterLineSetup(screen[active], 0, LINE_EOR, LINE_ONEDOT);

  l0 = &first;
  k = SIZE;

  while (--k >= 0) {
    WORD xi = xo + k * TILESIZE;
    WORD col = kxo + k;

    if (col & 1) {
      xi += 8;
      if (xi > N - 1)
        xi = N - 1;
    } else {
      xi -= 8;
      if (xi < 0)
        xi = 0;
    }

    l1 = &vert[xi & (N - 1)];

    BlitterLineSync(l1->x1, FAR_Y, l1->x2, l1->y2);

    if ((l0->x2 == WIDTH - 1) && (col & 1))
      BlitterLineSync(WIDTH - 1, l0->y2, WIDTH - 1, l1->y2);

    l0 = l1;
  }

  /* Odd stripes. */
  WaitBlitter();
  BlitterLineSetup(screen[active], 1, LINE_EOR, LINE_ONEDOT);

  l0 = &first;
  k = SIZE;

  while (--k >= 0) {
    WORD xi = xo + k * TILESIZE;
    WORD col = kxo + k;

    if (col & 1) {
      xi -= 8;
      if (xi < 0)
        xi = 0;
    } else {
      xi += 8;
      if (xi > N - 1)
        xi = N - 1;
    }

    l1 = &vert[xi & (N - 1)];

    BlitterLineSync(l1->x1, FAR_Y, l1->x2, l1->y2);

    if ((l0->x2 == WIDTH - 1) && (col & 1) == 0)
      BlitterLineSync(WIDTH - 1, l0->y2, WIDTH - 1, l1->y2);

    l0 = l1;
  }
}

__regargs static void AssignColorToTileColumn(WORD k, WORD kxo);

static void FillStripes(WORD kxo) {
  BitmapT *buffer = screen[active];
  UWORD bltsize = ((buffer->height - FAR_Y) << 6) + (buffer->width >> 4);
  APTR bltpt = buffer->planes[0] + buffer->bplSize - 1;

  WaitBlitter();
  custom->bltamod = 0;
  custom->bltdmod = 0;
  custom->bltcon0 = (SRCA | DEST) | A_TO_D;
  custom->bltcon1 = BLITREVERSE | FILL_OR;
  custom->bltafwm = -1;
  custom->bltalwm = -1;
  custom->bltapt = bltpt;
  custom->bltdpt = bltpt;
  custom->bltsize = bltsize;

  bltpt += buffer->bplSize;

  {
    WORD i = 0;

    while (i < SIZE)
      AssignColorToTileColumn(i++, kxo);
  }

  WaitBlitter();
  custom->bltapt = bltpt;
  custom->bltdpt = bltpt;
  custom->bltsize = bltsize;
}

static void HorizontalStripes(WORD yo) {
  BitmapT *buffer = screen[active];
  WORD yi = yo & (TILESIZE - 1);
  WORD k, y1, y2;

  for (k = 0; k <= SIZE; k++, yi += TILESIZE) {
    y1 = horiz[(yi < N) ? yi : N - 1];
    y2 = horiz[(yi + 16 < N) ? yi + 16 : N - 1];

    if (y1 != y2) {
      UWORD bltsize = ((y2 - y1) << 6) | (WIDTH >> 4);
      APTR bltpt = buffer->planes[0] + y1 * WIDTH / 8;

      WaitBlitter();
      custom->bltadat = 0;
      custom->bltdmod = 0;
      custom->bltcon0 = DEST;
      custom->bltcon1 = 0;
      custom->bltdpt = bltpt;
      custom->bltsize = bltsize;

      bltpt += buffer->bplSize;

      WaitBlitter();
      custom->bltdpt = bltpt;
      custom->bltsize = bltsize;
    }
  }
}

__regargs static void CalculateTileColumns(WORD yo, WORD kyo) {
  WORD k;
  WORD y0 = FAR_Y;
  WORD yi = (yo & (TILESIZE - 1)) + 16;
  UWORD *tileCol = &tileColumn[y0];

  for (k = 0; k <= SIZE && y0 < HEIGHT; k++, yi += TILESIZE) {
    WORD column = ((k + kyo) & (TILES - 1)) * 2;
    WORD y1 = (yi < N) ? horiz[yi] : HEIGHT;
    WORD n = y1 - y0;

    while (--n >= 0) 
      *tileCol++ = column;

    y0 = y1;
  }
}

#define STRIDE2 (STRIDE / sizeof(WORD))

__regargs static void AssignColorToTileColumn(WORD k, WORD kxo) {
  UWORD *color = lineColor[active][k];
  UWORD column = (k + kxo) & (TILES - 1);
  APTR textureRow = &tileColor[column * TILES];
  WORD *tileCol = &tileColumn[FAR_Y];
  WORD n = (HEIGHT - FAR_Y) >> 3;
  UWORD reg = 2 * (column & 1) + 0x182;
  WORD c;

  while (--n >= 0) {
    color[0 + STRIDE2 * 0] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 0] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 1] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 1] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 2] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 2] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 3] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 3] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 4] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 4] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 5] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 5] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 6] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 6] = *(UWORD *)(textureRow + c);
    color[0 + STRIDE2 * 7] = reg; c = *tileCol++;
    color[1 + STRIDE2 * 7] = *(UWORD *)(textureRow + c);
    color += 8 * STRIDE2;
  }
}

static void ControlTileColors() {
  UWORD *src = texture->pixels, *dst = tileColor;
  UBYTE *cycle = cycleStart;
  WORD n = TILES * TILES;

  while (--n >= 0) {
    UWORD f = (frameCount / 2 + *cycle++) & 63;
    UWORD c = *src++;
    WORD r, g, b;

    if (f < 16) {
      r = ((c >> 4) & 0x0f0) | f;
      g = (c & 0x0f0) | f;
      b = ((c << 4) & 0x0f0) | f;
    } else if (f < 32) {
      r = (c & 0xf00) | 0x0f0 | (f - 16);
      g = ((c << 4) & 0xf00) | 0x0f0 | (f - 16);
      b = ((c << 8) & 0xf00) | 0x0f0 | (f - 16);
    } else if (f < 48) {
      r = 0xf00 | ((c >> 4) & 0x0f0) | (f - 32);
      g = 0xf00 | (c & 0x0f0) | (f - 32);
      b = 0xf00 | ((c << 4) & 0x0f0) | (f - 32);
    } else {
      r = (c & 0xf00) | (f - 48);
      g = ((c << 4) & 0xf00) | (f - 48);
      b = ((c << 8) & 0xf00) | (f - 48);
    }

    *dst++ = (colortab[r] << 4) | colortab[g] | (colortab[b] >> 4);
  }
}

static void Render() {
  //LONG lines = ReadLineCounter();

  ControlTileColors();
  {
    WORD xo = (N / 4) + TrackValueGet(floor_x, frameCount);
    WORD yo = (N / 4) + TrackValueGet(floor_y, frameCount);
    WORD kxo = 7 - xo * SIZE / N;
    WORD kyo = 7 - yo * SIZE / N;

    ClearFloor();
    DrawStripesCopper(xo);
    DrawStripes(xo, kxo);
    CalculateTileColumns(yo, kyo);
    HorizontalStripes(yo);
    FillStripes(kxo);
  }

  //Log("floor: %ld\n", ReadLineCounter() - lines);

  CopListRun(cp[active]);
  WaitVBlank();
  active ^= 1;
}

EffectT DanceFloor1 = { Load, NULL, Init1, NULL, Render, InterruptHandler, Prepare };
EffectT DanceFloor2 = { NULL, NULL, Init2, NULL, Render, InterruptHandler, Prepare };
EffectT DanceFloor3 = { NULL, NULL, Init3, NULL, Render, InterruptHandler, Prepare };
EffectT DanceFloor4 = { NULL, UnLoad, Init4, Kill, Render, InterruptHandler, Prepare };
