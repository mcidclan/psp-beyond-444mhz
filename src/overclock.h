#ifndef H_OVERCLOCK_PLUGIN_MAIN
#define H_OVERCLOCK_PLUGIN_MAIN

#include "main.h"

// m-c/d 2026, for more information on this project see:
// https://github.com/mcidclan/psp-undocumented-sorcery/tree/main/experimental-overclock

#define PLL_DEN                               20 /*17*/ /*18*/
#define PLL_MUL_MSB                           0x0124
#define PLL_RATIO_INDEX                       5
#define PLL_BASE_FREQ                         37
//#define PLL_CUSTOM_FLAG                     27
#define DEFAULT_FREQUENCY                     333
#define OVERCLOCK_FREQUENCY_STEP              5  /*PLL_BASE_FREQ / 2*/

static const int MAX_THEORETICAL_FREQUENCY  = ((int)((255.0f / (float)PLL_DEN) * (float)PLL_BASE_FREQ)); //555; //471; //524;
static int THEORETICAL_FREQUENCY            = MAX_THEORETICAL_FREQUENCY;

#define updatePLLMultiplier(num, msb)               \
{                                                   \
  const u32 lsb = (num) << 8 | PLL_DEN;             \
  const u32 multiplier = (msb << 16) | lsb;         \
  hw(0xbc1000fc) = multiplier;                      \
  sync();                                           \
}

#define updatePLLControl()                          \
{                                                   \
  if (!(hw(0xbc100068) & PLL_RATIO_INDEX)) {        \
    hw(0xbc100068) = 0x80 | PLL_RATIO_INDEX;        \
    /*hw(0xbc100068) &= 0xfffffff0;*/               \
    /*hw(0xbc100068) |= (0x80 | PLL_RATIO_INDEX);*/ \
    sync();                                         \
    do {                                            \
      delayPipeline();                              \
    } while (hw(0xbc100068) & 0x80);                \
    sync();                                         \
  }                                                 \
}

static inline void adjustPLLMultiplier() {
  
  const u32 defaultNum = (u32)(((float)(DEFAULT_FREQUENCY * PLL_DEN)) / ((float)PLL_BASE_FREQ));
  hw(0xbc1000fc) = (PLL_MUL_MSB << 16) | (defaultNum << 8) | PLL_DEN;
  settle();
}

static inline void adjustPLLRatio() {
  
  u32 index = hw(0xbc100068) & 0x0f;
  sync();

  if (index != PLL_RATIO_INDEX) {
    
    const int step = (index > PLL_RATIO_INDEX) ? -1 : 1;
    while (((step < 0) == (index > PLL_RATIO_INDEX)) || index == PLL_RATIO_INDEX) {
        
      hw(0xbc100068) = 0x80 | index;
      sync();
      
      do {
        delayPipeline();
      } while ((hw(0xbc100068) & 0x80));
      settle();
      
      index += step;
    }
  }
  
}

static inline void adjustDomainRatios() {
  
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
}

void adjustInitialFrequencies() {
  
  sceKernelDelayThread(100);

  int intr, state;
  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);

  adjustPLLMultiplier();
  adjustPLLRatio();
  adjustDomainRatios();

  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
}



static inline void setOverclock() {
  
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  adjustInitialFrequencies();
  
  int defaultFreq = DEFAULT_FREQUENCY;
  const int freqStep = OVERCLOCK_FREQUENCY_STEP;
  int theoreticalFreq = defaultFreq + freqStep;
  
  while (theoreticalFreq <= THEORETICAL_FREQUENCY) {
    
    int intr, state;
    state = sceKernelSuspendDispatchThread();
    suspendCpuIntr(intr);
    
    // clearTags();
    
    u32 _num = (u32)(((float)(defaultFreq * PLL_DEN)) / ((float)PLL_BASE_FREQ));
    const u32 num = (u32)(((float)(theoreticalFreq * PLL_DEN)) / ((float)PLL_BASE_FREQ));
    
    updatePLLControl();
    
    //const u32 msb = PLL_MUL_MSB | (1 << (PLL_CUSTOM_FLAG - 16));
    while (_num <= num) {
      updatePLLMultiplier(_num, PLL_MUL_MSB);
      _num++;
    }
    settle();
    
    defaultFreq += freqStep;
    theoreticalFreq = defaultFreq + freqStep;
    
    resumeCpuIntr(intr);
    sceKernelResumeDispatchThread(state);
  
    // scePowerTick(PSP_POWER_TICK_ALL);
    sceKernelDelayThread(100);
  }
}

static inline int cancelOverclock() {
  
  u32 _num = (u32)(((float)(THEORETICAL_FREQUENCY * PLL_DEN)) / ((float)PLL_BASE_FREQ));
  const u32 num = (u32)(((float)(DEFAULT_FREQUENCY * PLL_DEN)) / ((float)PLL_BASE_FREQ));
  
  int intr, state;
  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);
  
  const u32 pllCtl = hw(0xbc100068) & 0x0f;
  const u32 pllMul = hw(0xbc1000fc) & 0xffff;
  sync();
  
  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
  
  const float n = (float)((pllMul & 0xff00) >> 8);
  const float d = (float)((pllMul & 0x00ff));
  const float m = (d > 0.0f) ? (n / d) : 9.0f;
  const int overclocked = ((pllCtl & PLL_RATIO_INDEX) && (m > 9.0f)) ? 1 : 0;
  sceKernelDelayThread(1000);

  //const u32 pllMul = hw(0xbc1000fc); sync();
  //const int overclocked = pllMul & (1 << PLL_CUSTOM_FLAG);
  
  if (overclocked) {
    state = sceKernelSuspendDispatchThread();
    suspendCpuIntr(intr);
    
    updatePLLControl();

    while (_num >= num) {
      updatePLLMultiplier(_num, PLL_MUL_MSB);
      _num--;
    }
    settle();
    
    resumeCpuIntr(intr);
    sceKernelResumeDispatchThread(state);
  }
  
  return overclocked;
}

static inline int readFreqConfig() {
  char buf[16] = {0};
  SceUID fd = sceIoOpen("ms0:/overconfig.txt", PSP_O_RDONLY, 0777);
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

static inline void initOverclock(int* const delay) {
  sceKernelIcacheInvalidateAll();
  unlockMemory();
  
  const int freq = readFreqConfig();
  if (freq > DEFAULT_FREQUENCY && freq <= MAX_THEORETICAL_FREQUENCY) {
    THEORETICAL_FREQUENCY = freq;
  }
  
  *delay = 1;
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  cancelOverclock();
}

#endif
