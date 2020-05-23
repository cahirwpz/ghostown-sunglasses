#include <proto/exec.h>

#include "common.h"
#include "memory.h"
#include "print.h"

#define CHIPSIZE (445 * 1024)
#define FASTSIZE (305 * 1024)

typedef struct MemHeader MemHeaderT;
typedef struct MemChunk MemChunkT;

static MemHeaderT *chip, *fast;

__regargs static MemHeaderT *MemPoolAlloc(ULONG byteSize, ULONG attributes) {
  MemHeaderT *mem = AllocMem(byteSize + sizeof(struct MemHeader), attributes);
  char *const memName = (attributes & MEMF_CHIP) ? "chip" : "public";

  if (mem) {
    /* Set up first chunk in the freelist */
    MemChunkT *mc = (APTR)mem + sizeof(MemHeaderT);
    mc->mc_Next  = NULL;
    mc->mc_Bytes = byteSize;

    mem->mh_Node.ln_Type = NT_MEMORY;
    mem->mh_Node.ln_Name = memName;
    mem->mh_First = mc;
    mem->mh_Lower = (APTR)mc;
    mem->mh_Upper = (APTR)mc + byteSize;
    mem->mh_Free  = byteSize;
    return mem;
  }

  Print("Failed to allocate %ld bytes of %s memory in one chunk!\n",
        byteSize, memName);
  return mem;
}

BOOL MemInit() {
  if ((chip = MemPoolAlloc(CHIPSIZE, MEMF_CHIP))) {
    if ((fast = MemPoolAlloc(FASTSIZE, MEMF_PUBLIC)))
      return TRUE;
    FreeMem(chip, CHIPSIZE + sizeof(MemHeaderT));
  }
  return FALSE;
}

void MemKill() {
  FreeMem(chip, CHIPSIZE + sizeof(MemHeaderT));
  FreeMem(fast, FASTSIZE + sizeof(MemHeaderT));
}

__regargs static void MemDebug(MemHeaderT *mem) {
  MemChunkT *mc = mem->mh_First;

  Log("Free %s memory map:\n", mem->mh_Node.ln_Name);
  while (mc) {
    Log("$%08lx : %ld\n", (LONG)mc, (LONG)mc->mc_Bytes);
    mc = mc->mc_Next;
  }
  Log("Total free %s memory: %ld\n", mem->mh_Node.ln_Name, mem->mh_Free);
}

LONG MemAvail(ULONG attributes asm("d1")) {
  return (attributes & MEMF_CHIP) ? chip->mh_Free : fast->mh_Free;
}

LONG MemUsed(ULONG attributes asm("d1")) {
  return (attributes & MEMF_CHIP) ? 
    (CHIPSIZE - chip->mh_Free) : (FASTSIZE - fast->mh_Free);
}

static __attribute__((noreturn)) 
  void MemPanic(ULONG byteSize asm("d0"), ULONG attributes asm("d1")) 
{
  MemHeaderT *mem = (attributes & MEMF_CHIP) ? chip : fast;

  Log("Failed to allocate %ld bytes of %s memory.\n",
      byteSize, mem->mh_Node.ln_Name);

  //if (byteSize < mem->mh_Free)
  MemDebug(mem);
  MemKill();
  exit();
}

APTR MemAlloc(ULONG byteSize asm("d0"), ULONG attributes asm("d1")) {
  APTR ptr = Allocate((attributes & MEMF_CHIP) ? chip : fast, byteSize);

  if (!ptr)
    MemPanic(byteSize, attributes);

  if (attributes & MEMF_CLEAR)
    memset(ptr, 0, byteSize);

  return ptr;
}

void MemFree(APTR memoryBlock asm("a1"), ULONG byteSize asm("d0")) {
  if (memoryBlock) {
    if (memoryBlock >= chip->mh_Lower && memoryBlock <= chip->mh_Upper)
      Deallocate(chip, memoryBlock, byteSize);
    else if (memoryBlock >= fast->mh_Lower && memoryBlock <= fast->mh_Upper)
      Deallocate(fast, memoryBlock, byteSize);
  }
}

typedef struct {
  ULONG size;
  UBYTE data[0];
} MemBlockT;

APTR MemAllocAuto(ULONG byteSize asm("d0"), ULONG attributes asm("d1")) {
  MemBlockT *mb = MemAlloc(byteSize + sizeof(MemBlockT), attributes);
  mb->size = byteSize;
  return mb->data;
}

void MemFreeAuto(APTR memoryBlock asm("a1")) {
  if (memoryBlock) {
    MemBlockT *mb = (MemBlockT *)((APTR)memoryBlock - sizeof(MemBlockT));
    MemFree(mb, mb->size + sizeof(MemBlockT));
  }
}
