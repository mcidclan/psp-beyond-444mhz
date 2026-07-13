#ifndef H_OVERCLOCK_PLUGIN_MAIN
#define H_OVERCLOCK_PLUGIN_MAIN

#include "main.h"

#define u32 unsigned int

// multiplier numerators
static const u32 multipliers[] = {

  0xa2, //162 => 333
  0xab, //171 => 351,5
  0xb4, //180 => 370
  0xbd, //189 => 388,5
  0xc6, //198 => 407
  0xcf, //207 => 425,5
  0xd8, //216 => 444
  0xe1, //225 => 462,5
  0xea, //234 => 481
  0xf3, //243 => 499,5
  0xfc, //252 => 518
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

static inline void initOverclock(int* const delay) {
  
  sceKernelIcacheInvalidateAll();
  unlockMemory();
  
  *delay = 1;
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  checkNumerator();
}

#endif
