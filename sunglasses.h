#ifndef __SUNGLASSES_H__
#define __SUNGLASSES_H__

#include "p61/p61.h"
#include "coplist.h"
#include "gfx.h"
#include "timeline.h"
#include "sync.h"

#ifndef X
#define X(x) ((x) + 0x81)
#endif

#ifndef Y
#define Y(y) ((y) + 0x2c)
#endif

extern BitmapT *screen[2];
extern TrackT **tracks;

__regargs void FadeBlack(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeWhite(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeIntensify(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void ContrastChange(PaletteT *pal, CopInsT *ins, WORD step);

static inline void FadeIn(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameFromStart);
}

static inline void FadeOut(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameTillEnd);
}

extern UBYTE envelope[24];

#endif
