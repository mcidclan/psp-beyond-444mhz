#ifndef H_OVERCLOCK_PLUGIN_MAIN
#define H_OVERCLOCK_PLUGIN_MAIN

#include "main.h"

#define u32 unsigned int

// multiplier numerators
const u32 multipliers[] = {
  
  0xa2, // 162 => 333
  0xa4, // 164 => 337,1...
  0xa6, // 166 => 341,2...
  0xa8, // 168 => 345,3...
  0xaa, // 170 => 349,4...
  0xab, // 171 => 351,5
  
  0xb4, // 180 => 370
  0xb6, // 182 => 374,1...
  0xb8, // 184 => 378,2...
  0xba, // 186 => 382,3...
  0xbc, // 188 => 386,4...
  0xbd, // 189 => 388,5
  
  0xc6, // 198 => 407
  0xc8, // 200 => 411,1...
  0xca, // 202 => 415,2...
  0xcc, // 204 => 419,3...
  0xce, // 206 => 423,4...
  0xcf, // 207 => 425,5
  
  0xd8, // 216 => 444
  0xda, // 218 => 448,1...
  0xdc, // 220 => 452,2...
  0xde, // 222 => 456,3...
  0xe0, // 224 => 460,4...
  0xe1, // 225 => 462,5
  
  0xea, // 234 => 481
  0xec, // 236 => 485,1...
  0xee, // 238 => 489,2...
  0xf0, // 240 => 493,3...
  0xf2, // 242 => 497,4...
  0xf3, // 243 => 499,5
 
  0xfc,  // 252 => 518
  0xfe,  // 254 => 522,1...
  0x100, // 256 => 526,2...
  0x102, // 258 => 530,3...
  0x104, // 260 => 534,4...
  0x105, // 261 => 536,5
};

const int mulTableSize = sizeof(multipliers) / sizeof(u32);

#define DELAY_AFTER_CLOCK_CHANGE 300000
#define PLL_DEFAULT_RATIO        1.0f
#define PLL_DEFAULT_DEN          0x12
#define PLL_BASE_FREQ            37.0f
#define DEFAULT_FREQUENCY        333

#define pllReady()                                  \
{                                                   \
  do {                                              \
    delayPipeline();                                \
  } while (hw(0xbc100068) & 0x80);                  \
  sync();                                           \
}

static inline void adjustDomainRatios() {
  
  int intr, state;
  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);

  const u32 cpu = hw(0xbc200000);
  const u32 bus = hw(0xBC200004);
  sync();
  
  u32 cpuDen = cpu & 0x1ff;
  u32 cpuNum = (cpu >> 16) & 0x1ff;
  u32 busDen = bus & 0x1ff;
  u32 busNum = (bus >> 16) & 0x1ff;
  
  hw(0xbc200000) = (cpuNum << 16) | cpuDen;
  hw(0xBC200004) = (busNum << 16) | busDen;
  settle();
    
  const int step = 18;
  while ((cpuNum & cpuDen & busNum & busDen) != 0x1ff) {
    
    const u32 nextCpuNum = cpuNum + step;
    const u32 nextCpuDen = cpuDen + step;
    const u32 nextBusNum = busNum + step;
    const u32 nextBusDen = busDen + step;
    
    cpuNum = (nextCpuNum > 0x1ff) ? 0x1ff : nextCpuNum;
    cpuDen = (nextCpuDen > 0x1ff) ? 0x1ff : nextCpuDen;
    busNum = (nextBusNum > 0x1ff) ? 0x1ff : nextBusNum;
    busDen = (nextBusDen > 0x1ff) ? 0x1ff : nextBusDen;
    
    hw(0xbc200000) = (cpuNum << 16) | cpuDen;
    hw(0xBC200004) = (busNum << 16) | busDen;
    settle();
  }
  
  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
}

int overclockId = 0;
int ratioMode = 1;

static inline void checkNumerator() {
  
  const u32 num = (hw(0xbc1000fc) & 0xff00) >> 8;
  sync();
  
  int i = 0;
  while (i < mulTableSize) {
    
    if (multipliers[i] == num) {
      overclockId = i;
      break;
    }
    i++;
  }
}

static inline int setOverclock() {
  
  adjustDomainRatios();
  
  const int den = PLL_DEFAULT_DEN;
  
  int intr, state;
  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);

  hw(0xbc100068) = 0x85;
  sync();
  pllReady();
  settle();
  
  const u32 mul = multipliers[overclockId];
  hw(0xbc1000fc) = (hw(0xbc1000fc) & 0xffff0000) | (mul << 8) | den;
  sync();
  settle();
  
  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
    
  sceKernelDelayThread(100);
  return 0;
}

/*
static inline int readId() {
  
  char buf[16] = {0};
  SceUID fd = sceIoOpen("ms0:/opicker.id", PSP_O_RDONLY, 0777);
  if (fd >= 0) {
    
    sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
  } else {
    return -1;
  }
  
  u32 result = 0;
  for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) {
    result = result * 10 + (buf[i] - '0');
  }
  return result;
}
*/

static inline void initOverclock(int* const delay) {
  
  sceKernelIcacheInvalidateAll();
  unlockMemory();
  
  // readId();
  
  *delay = 1;
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  checkNumerator();
}

#endif
