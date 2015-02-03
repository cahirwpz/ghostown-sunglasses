#include "hardware.h"
#include "memory.h"
#include "file.h"
#include "reader.h"
#include "color.h"
#include "interrupts.h"

#include "sunglasses.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

/* For command line parsing. */
extern char *__commandline;
extern int __commandlen;

/* Resources shared among all effects. */
BitmapT *screen[2];
TrackT **tracks;

/* Private resources. */
static APTR module;
static APTR samples;

/* References to all effects. */
extern EffectT Intro1;
extern EffectT Intro2;
extern EffectT Intro3;
extern EffectT Intro4;
extern EffectT Punk1;
extern EffectT Punk2;
extern EffectT Punk3;
extern EffectT Punk4;
extern EffectT Theme;
extern EffectT ThemeNeons;
extern EffectT BlurredShapes1;
extern EffectT BlurredShapes2;
extern EffectT BlurredShapes3;
extern EffectT Neons;
extern EffectT DanceFloor1;
extern EffectT DanceFloor2;
extern EffectT DanceFloor3;
extern EffectT DanceFloor4;
extern EffectT Highway1;
extern EffectT Highway2;
extern EffectT Metaballs1;
extern EffectT Metaballs2;
extern EffectT Plotter1;
extern EffectT Plotter2;
extern EffectT Shapes1;
extern EffectT Shapes2;
extern EffectT Shapes3;
extern EffectT Shapes4;
extern EffectT Distortions1;
extern EffectT Distortions2;
extern EffectT WhoFarted;
extern EffectT EndScroll;

static TimelineItemT timeline[] = {
  _TL(0x0001, 0x0010, &Intro1),
  _TL(0x0010, 0x0020, &Intro2),
  _TL(0x0020, 0x0030, &Intro3),
  _TL(0x0030, 0x0100, &Intro4),
  _TL(0x0106, 0x0116, &Punk1),
  _TL(0x0116, 0x0126, &Punk2),
  _TL(0x0126, 0x0136, &Punk3),
  _TL(0x0136, 0x0200, &Punk4),
  _TL(0x0200, 0x0300, &Highway1),
  _TL(0x0300, 0x0400, &Highway2),
  _TL(0x0400, 0x0420, &BlurredShapes1),
  _TL(0x0420, 0x0500, &BlurredShapes2),
  _TL(0x0500, 0x0600, &BlurredShapes3),
  _TL(0x0600, 0x0620, &Plotter1),
  _TL(0x0620, 0x0700, &Plotter2),
  _TL(0x0700, 0x0710, &Shapes1),
  _TL(0x0710, 0x0720, &Shapes2),
  _TL(0x0720, 0x0730, &Shapes3),
  _TL(0x0730, 0x0800, &Shapes4),
  _TL(0x0800, 0x0900, &Metaballs1),
  _TL(0x0900, 0x0A00, &Metaballs2),
  _TL(0x0A00, 0x0B00, &Theme),
  _TL(0x0B00, 0x0C00, &ThemeNeons),
  _TL(0x0C00, 0x0D30, &Neons),
  _TL(0x0D30, 0x0E00, &WhoFarted),
  _TL(0x0E00, 0x0E1C, &DanceFloor1),
  _TL(0x0E1C, 0x0F00, &DanceFloor2),
  _TL(0x0F00, 0x0F1C, &DanceFloor3),
  _TL(0x0F1C, 0x1000, &DanceFloor4),
  _TL(0x1000, 0x1100, &Distortions1),
  _TL(0x1100, 0x1200, &Distortions2),
  _TL(0x1200, 0x183f, &EndScroll)
};

#define TIMELINE_LEN (sizeof(timeline) / sizeof(TimelineItemT))

#ifdef START_POS
static WORD SetModulePosFromCmdline() {
  UWORD len = __commandlen;
  char *cmdline = __builtin_alloca(len);
  WORD pos = 0;

  memcpy(cmdline, __commandline, len--);
  cmdline[len] = '\0';

  if (ReadNumber(&cmdline, &pos)) {
    WaitVBlank();
    P61_SetPosition(pos);
  }

  return pos * 64 * FRAMES_PER_ROW;
}
#endif

BOOL DemoLoad() {
  if (!(tracks = LoadTrackList("data/sunglasses.sync")))
    return FALSE;

  module = ReadFile("data/jazzcat-sunglasses.p61", MEMF_PUBLIC);
  samples = ReadFile("data/jazzcat-sunglasses.smp", MEMF_CHIP);

  screen[0] = NewBitmap(WIDTH, HEIGHT, DEPTH);
  screen[1] = NewBitmap(WIDTH, HEIGHT, DEPTH);

  LoadEffects(timeline, TIMELINE_LEN);

  Log("Chip memory used   : %ld bytes.\n", MemUsed(MEMF_CHIP));
  Log("Public memory used : %ld bytes.\n", MemUsed(MEMF_PUBLIC));

  return TRUE;
}

static __interrupt_handler void IntLevel3Handler() {
  asm volatile("" ::: "d0", "d1", "a0", "a1");

  if (currentInterruptHandler)
    currentInterruptHandler();

  if (custom->intreqr & INTF_VERTB)
    P61_Music();

  custom->intreq = INTF_LEVEL3;
  custom->intreq = INTF_LEVEL3;
}

void DemoRun() {
  WORD pos = 0;

  /* Wait one second. */
  custom->color[0] = 0;
  {
    LONG start = ReadFrameCounter();
    while (ReadFrameCounter() - start < 50);
  }

  P61_Init(module, samples, NULL);
#ifdef START_POS
  pos = SetModulePosFromCmdline();
#endif

  InterruptVector->IntLevel3 = IntLevel3Handler;
  custom->intena = INTF_SETCLR | INTF_VERTB;

  P61_ControlBlock.Play = 1;
  WaitVBlank();

  RunEffects(timeline, TIMELINE_LEN, pos);

  P61_ControlBlock.Play = 0;
  P61_End();

  custom->intena = INTF_VERTB;
}

void DemoUnLoad() {
  UnLoadEffects(timeline, TIMELINE_LEN);

  DeleteBitmap(screen[0]);
  DeleteBitmap(screen[1]);

  MemFreeAuto(module);
  MemFreeAuto(samples);

  DeleteTrackList(tracks);
}

__regargs void FadeBlack(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    r = (r & 0xf0) | step;
    g = (g & 0xf0) | step;
    b = (b & 0xf0) | step;
    
    CopInsSet16(ins++, (colortab[r] << 4) | colortab[g] | (colortab[b] >> 4));
  }
}

__regargs void FadeWhite(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    r = ((r & 0xf0) << 4) | 0xf0 | step;
    g = ((g & 0xf0) << 4) | 0xf0 | step;
    b = ((b & 0xf0) << 4) | 0xf0 | step;

    CopInsSet16(ins++, (colortab[r] << 4) | colortab[g] | (colortab[b] >> 4));
  }
}

__regargs void FadeIntensify(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    CopInsSet16(ins++, ColorIncreaseContrastRGB(r, g, b, step));
  }
}

__regargs void ContrastChange(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step >= 0) {
    if (step > 15)
      step = 15;

    while (--n >= 0) {
      WORD r = *c++;
      WORD g = *c++;
      WORD b = *c++;

      CopInsSet16(ins++, ColorIncreaseContrastRGB(r, g, b, step));
    }
  } else {
    step = -step;
    if (step > 15)
      step = 15;

    while (--n >= 0) {
      WORD r = *c++;
      WORD g = *c++;
      WORD b = *c++;

      CopInsSet16(ins++, ColorDecreaseContrastRGB(r, g, b, step));
    }
  }
}

UBYTE envelope[24] = {
  0, 0, 1, 1, 2, 3, 5, 8, 11, 13, 14, 15,
  15, 14, 13, 11, 8, 5, 3, 2, 1, 1, 0, 0 
};
