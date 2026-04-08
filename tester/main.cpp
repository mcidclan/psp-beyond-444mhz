#include <psppower.h>
#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspgu.h>
#include "kcall.h"
#include "main.h"

#define u32 unsigned int

PSP_MODULE_INFO("expover-tester", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

#define DELAY_AFTER_CLOCK_CHANGE 300000

// Note:
// float a = (float)(THEORETICAL_FREQUENCY / BASE)
// PLL_DEN = (int)((255.0f / a) * PLL_RATIO)
// PLL_NUM = (int)(((float)(THEORETICAL_FREQUENCY * PLL_DEN)) / (PLL_BASE_FREQ * PLL_RATIO_VALUE))
// PLL_FREQ = PLL_BASE_FREQ * (PLL_NUM / PLL_DEN) * PLL_RATIO, with PLL_BASE_FREQ = 37 and PLL_RATIO = 1.0f

#define PLL_DEN                           20 /*17*/ /*18*/ 
#define PLL_MUL_MSB                       0x0124
#define PLL_BASE_FREQ                     37
#define PLL_RATIO_INDEX                   5
#define PLL_RATIO                         1.0f
//#define PLL_CUSTOM_FLAG                 27
#define DEFAULT_FREQUENCY                 333
#define OVERCLOCK_FREQUENCY_STEP          5

static const int THEORETICAL_FREQUENCY  = ((int)((255.0f / (float)PLL_DEN) * (float)PLL_BASE_FREQ)); // 471; // 555; // 524;

int switchOverclock = 0, stopped = 0;
int currFreq = 0, targetFreq = DEFAULT_FREQUENCY;

static int writeFrequency(u32 freq) {
  static char buf[16];
  
  int i = 0, j = 0;
  do {
    buf[i++] = '0' + (freq % 10);
    freq /= 10;
  } while (freq > 0);
  
  while (j < i / 2) {
    char tmp = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j++] = tmp;
  }
  
  SceUID fd = sceIoOpen("ms0:/overconfig.txt", PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if (fd >= 0) {
    buf[i++] = '\n';
    sceIoWrite(fd, buf, i);
    sceIoClose(fd);
  }
  return 0;
}

u32 ctrl = 0, mult = 0;
int _dump() {
  int intr, state;
  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);
  ctrl = hw(0xbc100068);
  mult = hw(0xbc1000fc);
  sync();
  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
  return 0;
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

#define updatePLLMultiplier(num, msb)               \
{                                                   \
  const u32 lsb = (num) << 8 | PLL_DEN;             \
  const u32 multiplier = (msb << 16) | lsb;         \
  hw(0xbc1000fc) = multiplier;                      \
  sync();                                           \
}

inline void adjustPLLMultiplier() {
  
  const u32 defaultNum = (u32)(((float)(DEFAULT_FREQUENCY * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));
  hw(0xbc1000fc) = (PLL_MUL_MSB << 16) | (defaultNum << 8) | PLL_DEN;
  settle();
}

inline void adjustPLLRatio() {
  
  u32 index = hw(0xbc100068) & 0x0f;
  sync();

  if (index != PLL_RATIO_INDEX) {
    
    //const u32 defaultNum = (u32)(((float)(DEFAULT_FREQUENCY * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));
    //hw(0xbc1000fc) = (PLL_MUL_MSB << 16) | (defaultNum << 8) | PLL_DEN;
    //sync();
    
    const int step = (index > PLL_RATIO_INDEX) ? -1 : 1;
    while (((step < 0) == (index > PLL_RATIO_INDEX)) || index == PLL_RATIO_INDEX) {
    //while ((step < 0 && index >= PLL_RATIO_INDEX) || (step > 0 && index <= PLL_RATIO_INDEX)) {
        
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

inline void adjustDomainRatios() {
  
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
  
  // hw(0xBC200008) = 511 << 16 | 511;
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

int _setOverclock() {
  
  sceKernelDelayThread(3000000);
  stopped = 0;

  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  currFreq = DEFAULT_FREQUENCY;
  
  adjustInitialFrequencies();
  
  const int freqStep = OVERCLOCK_FREQUENCY_STEP;
  int defaultFreq = DEFAULT_FREQUENCY;
  int theoreticalFreq = defaultFreq + freqStep;

  while ((theoreticalFreq <= THEORETICAL_FREQUENCY) &&
    (targetFreq == THEORETICAL_FREQUENCY)) {
    
    _dump();
    
    int intr, state;
    state = sceKernelSuspendDispatchThread();
    suspendCpuIntr(intr);

    u32 _num = (u32)(((float)(defaultFreq * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));
    const u32 num = (u32)(((float)(theoreticalFreq * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));
    
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

    scePowerTick(PSP_POWER_TICK_ALL);
    sceKernelDelayThread(9000000);
    writeFrequency(currFreq);
    sceKernelDelayThread(2000000);
    currFreq = defaultFreq;
  }
  return 0;
}

void _cancelOverclock() {
  
  u32 _num = (u32)(((float)(THEORETICAL_FREQUENCY * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));
  const unsigned int num = (u32)(((float)(DEFAULT_FREQUENCY * PLL_DEN)) / ((float)(PLL_BASE_FREQ * PLL_RATIO)));

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
  
  stopped = 1;
  _dump();
}

static inline void initOverclock() {
  sceKernelIcacheInvalidateAll();
  // unlockMemory();
  
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  dump();
  
  cancelOverclock();
  sceKernelDelayThread(DELAY_AFTER_CLOCK_CHANGE);
}

#define BUF_WIDTH   512
#define SCR_WIDTH   480
#define SCR_HEIGHT  272

#define DRAW_BUF_0 0
#define DRAW_BUF_1 0x88000
#define DEPTH_BUF  0x110000

struct Vertex {
  u32 color;
  u16 x, y, z;
} __attribute__((aligned(4)));

static unsigned int __attribute__((aligned(16))) list[1024] = {0};

void guInit() {
  sceGuInit();
  sceGuStart(GU_DIRECT, list);
  sceGuDrawBuffer(GU_PSM_8888, (void*)DRAW_BUF_0, BUF_WIDTH);
  sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)DRAW_BUF_1, BUF_WIDTH);
  sceGuDepthBuffer((void*)DEPTH_BUF, BUF_WIDTH);
  sceGuDisable(GU_DEPTH_TEST);
  sceGuEnable(GU_SCISSOR_TEST);
  sceGuScissor(0, 64, 480, 272 - 128);
  sceGuClearColor(0xff100808);
  sceGuDisplay(GU_TRUE);
  sceGuFinish();
  sceGuSync(0,0);
}

int thread() {
  
  SceCtrlData ctl;
  initOverclock();
  while (1) {
    
    sceCtrlPeekBufferPositive(&ctl, 1);
    
    if (switchOverclock == 1) {
      
      switchOverclock = 2;
      if (targetFreq == THEORETICAL_FREQUENCY) {
        setOverclock();
      }
      else {
        cancelOverclock();
      }
    }
    
    sceKernelDelayThread(100);
  }
}

int main() {
  
  pspDebugScreenInit();
  pspDebugScreenSetXY(1, 1);
  
  if (pspSdkLoadStartModule("./kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0) {
    pspDebugScreenPrintf("Can't load kcall prx");
    sceKernelExitGame();
    return 0;
  }
  
  guInit();
  
  int thid = sceKernelCreateThread("expover-thread",
    (int (*)(unsigned int, void*))((void*)thread), 0x14, 0x8000, PSP_THREAD_ATTR_VFPU, NULL);
  
  if (thid >= 0) {
    sceKernelStartThread(thid, 0, NULL);
  }
  
  pspDebugScreenInitEx(0x0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenEnableBackColor(1);

  int buffer = DRAW_BUF_0;
  pspDebugScreenSetOffset(buffer);

  u64 prev, now, fps = 0;
  const u64 res = sceRtcGetTickResolution();
  
  int dir = 1;
  int move = 0;
  SceCtrlData ctl;
  
  do {
    sceRtcGetCurrentTick(&prev);
    
    const u32 offset = (buffer == DRAW_BUF_0) ? DRAW_BUF_0 : DRAW_BUF_1;    
    sceCtrlPeekBufferPositive(&ctl, 1);
    
    if (!switchOverclock && (ctl.Buttons & PSP_CTRL_TRIANGLE)) {
      
      const int freq = targetFreq == DEFAULT_FREQUENCY ?
      THEORETICAL_FREQUENCY : DEFAULT_FREQUENCY;
      
      if (freq == THEORETICAL_FREQUENCY) {
        targetFreq = THEORETICAL_FREQUENCY;
      } else {
        targetFreq = DEFAULT_FREQUENCY;
      }
      switchOverclock = 1;
      sceKernelDelayThread(DELAY_AFTER_CLOCK_CHANGE);
    }
    else if(!(ctl.Buttons & PSP_CTRL_TRIANGLE) && switchOverclock == 2) {
      switchOverclock = 0;
    }
    
    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    pspDebugScreenSetOffset(offset);
    
    pspDebugScreenSetXY(40, 0);
    pspDebugScreenPrintf("Overclock Stress Tester v2.5");
    
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf(" FPS: %llu               \n", fps);
    pspDebugScreenPrintf(" Target freq: %u MHz     \n", targetFreq);
    pspDebugScreenPrintf(" Saved freq:  %u MHz     \n", currFreq);
    pspDebugScreenPrintf(" Test is %s            \n\n", stopped ? "STOPPED" : "RUNNING...");
    pspDebugScreenPrintf(" Ctrl: 0x%08x            \n", ctrl);
    pspDebugScreenPrintf(" Mult: 0x%08x            \n", mult);

    {
      Vertex* const vertices = (Vertex*)sceGuGetMemory(sizeof(Vertex) * 2);
      move += dir;
      if(move > 112) {
        dir = -1;
      } else if(move < -112) {
        dir = 1;
      }
      vertices[0].color = 0;
      vertices[0].x = 176 + move;
      vertices[0].y = 72;
      vertices[0].z = 0;
      vertices[1].color = 0xFF0000FF;
      vertices[1].x = 128 + 176 + move;
      vertices[1].y = 128 + 72;
      vertices[1].z = 0;
      sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, nullptr, vertices);
    }
    
    sceGuFinish();
    sceGuSync(0,0);
    
    // sceDisplayWaitVblankStart();
    buffer = (int)sceGuSwapBuffers();
    sceKernelDcacheWritebackInvalidateAll();
    
    sceRtcGetCurrentTick(&now);
    fps = res / (now - prev);
    
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  cancelOverclock();
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(1, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(300000);
  
  sceKernelExitGame();
  return 0;
}
