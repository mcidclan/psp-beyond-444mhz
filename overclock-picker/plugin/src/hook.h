#ifndef H_OVERCLOCK_PLUGIN_HOOK
#define H_OVERCLOCK_PLUGIN_HOOK

u32 sctrlHENFindFunction(const char *szMod, const char *szLib, u32 nid);
void sctrlHENPatchSyscall(unsigned int* addr, void *newaddr);

static int (*_displaySetFrameBuf)(void*, int, int, int);
static void (*_exitGame)(void);
static int (*_exitGameWithStatus)(void);

static inline void* hook(char* mod, char* lib, u32 nid, void* hf) {
  unsigned int* const f = (unsigned int*)sctrlHENFindFunction(mod, lib, nid);
  if (f) {
    sctrlHENPatchSyscall(f, hf);
    return f;
  }
  return NULL;
}

#endif
