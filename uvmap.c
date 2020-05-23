#include "sunglasses.h"
#include "bltop.h"
#include "coplist.h"
#include "memory.h"
#include "tga.h"
#include "print.h"
#include "file.h"

#define WIDTH 160
#define HEIGHT 100
#define DEPTH 5

static PixmapT *chunky[2];
static PixmapT *textureHi, *textureLo;
static PaletteT *palette;
static UWORD *uvmap;
static UWORD active = 0;
static CopInsT *bplptr[DEPTH];
static CopInsT *pal;
static CopListT *cp;
static PixmapT *texture[2];
static TrackT *pos_u, *pos_v;

extern APTR UVMapRenderTemplate[5];
#define UVMapRenderSize \
  (WIDTH * HEIGHT / 2 * sizeof(UVMapRenderTemplate) + 2)
void (*UVMapRender)(UBYTE *chunky asm("a0"),
                    UBYTE *textureHi asm("a1"),
                    UBYTE *textureLo asm("a2"));

static void PixmapScramble(PixmapT *image, PixmapT *imageHi, PixmapT *imageLo)
{
  ULONG *data = image->pixels;
  ULONG *hi = imageHi->pixels;
  ULONG *lo = imageLo->pixels;
  LONG size = image->width * image->height;
  LONG n = size / 4;
  register ULONG m1 asm("d6") = 0x0c0c0c0c;
  register ULONG m2 asm("d7") = 0x03030303;

  while (--n >= 0) {
    ULONG c = *data++;
    /* [0 0 0 0 a0 a1 a2 a3] => [a2 a3 0 0 a0 a1 0 0] */
    *hi++ = (c & m1) | ((c & m2) << 6);
    /* [0 0 0 0 a0 a1 a2 a3] => [ 0 0 a2 a3 0 0 a0 a1] */
    *lo++ = ((c & m1) >> 2) | ((c & m2) << 4);
  }

  /* Extra halves for cheap texture motion. */
  memcpy(((APTR)imageHi->pixels) + size, (APTR)imageHi->pixels, size);
  memcpy(((APTR)imageLo->pixels) + size, (APTR)imageLo->pixels, size);
}

static void MakeUVMapRenderCode() {
  UWORD *code = (APTR)UVMapRender;
  UWORD *tmpl = (APTR)UVMapRenderTemplate;
  UWORD *data = uvmap;
  WORD n = WIDTH * HEIGHT / 2;

  /* UVMap is pre-scrambled. */
  while (n--) {
    *code++ = tmpl[0];
    *code++ = *data++;
    *code++ = tmpl[2];
    *code++ = *data++;
    *code++ = tmpl[4];
  }

  *code++ = 0x4e75; /* return from subroutine instruction */
}

static void Load() {
  texture[0] = LoadTGA("data/texture-16-1.tga", PM_CMAP, MEMF_PUBLIC);
  texture[1] = LoadTGA("data/texture-16-2.tga", PM_CMAP, MEMF_PUBLIC);
  uvmap = ReadFile("data/uvmap.bin", MEMF_PUBLIC);

  pos_u = TrackLookup(tracks, "pos_u");
  pos_v = TrackLookup(tracks, "pos_v");
}

static void Prepare() {
  if (!UVMapRender) {
    UVMapRender = MemAlloc(UVMapRenderSize, MEMF_PUBLIC);
    MakeUVMapRenderCode();
    MemFreeAuto(uvmap);
    uvmap = NULL;
  }
}

static void UnLoad() {
  DeletePalette(texture[0]->palette);
  DeletePixmap(texture[0]);
  DeletePalette(texture[1]->palette);
  DeletePixmap(texture[1]);
  MemFree(UVMapRender, UVMapRenderSize);
}

static struct {
  WORD phase;
  WORD active;
} c2p = { 5, 0 };

static void ChunkyToPlanar() {
  BitmapT *dst = screen[c2p.active];
  PixmapT *src = chunky[c2p.active];

  switch (c2p.phase) {
    case 0:
      /* Initialize chunky to planar. */
      custom->bltamod = 2;
      custom->bltbmod = 2;
      custom->bltdmod = 0;
      custom->bltcdat = 0xf0f0;
      custom->bltafwm = -1;
      custom->bltalwm = -1;

      /* Swap 4x2, pass 1. */
      custom->bltapt = src->pixels;
      custom->bltbpt = src->pixels + 2;
      custom->bltdpt = dst->planes[0];

      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
      custom->bltcon1 = 4 << BSHIFTSHIFT;
      custom->bltsize = 1;
      break;

    case 1:
      custom->bltsize = 1 | (976 << 6);
      break;

    case 2:
      /* Swap 4x2, pass 2. */
      // custom->bltapt = src->pixels + WIDTH * HEIGHT / 2;
      // custom->bltbpt = src->pixels + WIDTH * HEIGHT / 2 + 2;
      custom->bltdpt = dst->planes[2] + WIDTH * HEIGHT / 4;

      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (4 << ASHIFTSHIFT);
      custom->bltcon1 = BLITREVERSE;
      custom->bltsize = 1;
      break;

    case 3:
      custom->bltsize = 1 | (977 << 6);
      break;

    case 4:
      CopInsSet32(bplptr[0], dst->planes[0]);
      CopInsSet32(bplptr[1], dst->planes[0]);
      CopInsSet32(bplptr[2], dst->planes[2]);
      CopInsSet32(bplptr[3], dst->planes[2]);
      CopInsSet32(bplptr[4], dst->planes[4]);
      break;

    default:
      return;
  }

  c2p.phase++;
}

static void InterruptHandler() {
  if (custom->intreqr & INTF_VERTB) {
    if (frameFromStart < 16)
      FadeIn(palette, pal);
    if (frameTillEnd < 16)
      FadeOut(palette, pal);
  }

  if (custom->intreqr & INTF_BLIT)
    ChunkyToPlanar();
}

static void MakeCopperList(CopListT *cp) {
  WORD i;

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(28), WIDTH * 2, HEIGHT * 2);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  CopLoadColor(cp, 0, 15, 0);
  pal = CopLoadColor(cp, 16, 31, 0);
  for (i = 0; i < HEIGHT * 2; i++) {
    CopWaitMask(cp, Y(i + 28), 0, 0xff, 0);
    CopMove16(cp, bplcon1, (i & 1) ? 0x0021 : 0x0010);
    CopMove16(cp, bpl1mod, (i & 1) ? -40 : 0);
    CopMove16(cp, bpl2mod, (i & 1) ? -40 : 0);
  }
  CopEnd(cp);
}

static void Init1() {
  static PixmapT recycled[2];

  chunky[0] = &recycled[0];
  chunky[1] = &recycled[1];

  InitSharedPixmap(chunky[0], WIDTH, HEIGHT, PM_GRAY4, screen[0]->planes[1]);
  InitSharedPixmap(chunky[1], WIDTH, HEIGHT, PM_GRAY4, screen[1]->planes[1]);

  textureHi = NewPixmap(128, 256, PM_CMAP, MEMF_PUBLIC);
  textureLo = NewPixmap(128, 256, PM_CMAP, MEMF_PUBLIC);

  PixmapScramble(texture[0], textureHi, textureLo);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  BitmapClear(screen[0], DEPTH);
  BitmapClear(screen[1], DEPTH);

  memset(screen[0]->planes[4], 0x55, WIDTH * HEIGHT * 4 / 8);
  memset(screen[1]->planes[4], 0x55, WIDTH * HEIGHT * 4 / 8);

  palette = texture[0]->palette;

  cp = NewCopList(900);
  MakeCopperList(cp);
  CopListActivate(cp);

  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
  custom->intena = INTF_SETCLR | INTF_BLIT;
}

static void Kill1() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;
  custom->intena = INTF_BLIT;
}

static void Init2() {
  PixmapScramble(texture[1], textureHi, textureLo);

  palette = texture[1]->palette;

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER;
  custom->intena = INTF_SETCLR | INTF_BLIT;
}

static void Kill2() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;
  custom->intena = INTF_BLIT;

  DeleteCopList(cp);
  DeletePixmap(textureHi);
  DeletePixmap(textureLo);
}

static void Render() {
  WORD u = TrackValueGet(pos_u, frameCount);
  WORD v = TrackValueGet(pos_v, frameCount);
  WORD offset = ((u & 127) << 7) | (v & 127);

  {
    UBYTE *txtHi = textureHi->pixels + offset;
    UBYTE *txtLo = textureLo->pixels + offset;

    (*UVMapRender)(chunky[active]->pixels, txtHi, txtLo);
  }

  c2p.phase = 0;
  c2p.active = active;
  ChunkyToPlanar();
  active ^= 1;
}

EffectT Distortions1 = { Load, NULL, Init1, Kill1, Render, InterruptHandler, Prepare };
EffectT Distortions2 = { NULL, UnLoad, Init2, Kill2, Render, InterruptHandler, Prepare };
