#include <psppower.h>
#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspgu.h>
#include "kcall.h"
#include "main.h"
#include "octable.h"

#define u32 unsigned int

PSP_MODULE_INFO("overclock-picker", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

const int mulTableSize = sizeof(multipliers) / sizeof(u32);

float frequency = 0.0f;

#define DELAY_AFTER_CLOCK_CHANGE 300000

#define PLL_DEFAULT_RATIO        1.0f
#define PLL_BASE_FREQ            37.0f
#define DEFAULT_FREQUENCY        333


#define pllReady()                                  \
{                                                   \
  do {                                              \
    delayPipeline();                                \
  } while (hw(0xbc100068) & 0x80);                  \
  sync();                                           \
}

int switchOverclock = 0, stopped = 0;
int currFreq = 0, targetFreq = DEFAULT_FREQUENCY;

static int writeId(u32 id) {
  
  static char buf[16];
  
  int i = 0, j = 0;
  do {
    buf[i++] = '0' + (id % 10);
    id /= 10;
  } while (id > 0);
  
  while (j < i / 2) {
    char tmp = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j++] = tmp;
  }
  
  SceUID fd = sceIoOpen("ms0:/opicker.id", PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if (fd >= 0) {
    
    buf[i++] = '\n';
    sceIoWrite(fd, buf, i);
    sceIoClose(fd);
  }
  return 0;
}

/*
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
*/

inline void adjustDomainRatios() {
  
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

int _setOverclock() {
  
  adjustDomainRatios();
  
  const int den = PLL_DEFAULT_DEN;
  const float ratio = PLL_DEFAULT_RATIO;
  
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
  
  frequency = PLL_BASE_FREQ * (((float)mul) / ((float)PLL_DEFAULT_DEN)) * ratio;
  return 0;
}

void _cancelOverclock() {
  
  // todo:
  // _dump();
}

static inline void initOverclock() {
  
  unlockMemory();
  
  scePowerSetClockFrequency(DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY/2);
  cancelOverclock();
  sceKernelDelayThread(DELAY_AFTER_CLOCK_CHANGE);
  
  setOverclock();
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
  sceGuScissor(0, 68, 480, 272 - 136);
  sceGuClearColor(0xff100808);
  sceGuDisplay(GU_TRUE);
  sceGuFinish();
  sceGuSync(0,0);
}

const int lastTableId = mulTableSize - 1;

int autoMode = 0;
int autoFirst = 1;

int autoThread() {

  SceCtrlData ctl;
  while (1) {

    sceCtrlPeekBufferPositive(&ctl, 1);
    if (autoMode) {
      
      if ((overclockId < lastTableId)) {
        
        if(!autoFirst) {
          
          overclockId++;
        }
        setOverclock();
        scePowerTick(PSP_POWER_TICK_ALL);
        sceKernelDelayThread(1000000);
        writeId((u32)overclockId);
        sceKernelDelayThread(2000000);
        autoFirst = 0;
      }
    }
    
    sceKernelDelayThread(10);
  }
}

int thread() {
  
  int up = 1;
  int autoUp = 1;
  
  SceCtrlData ctl;
  initOverclock();
  
  while (1) {
    
    sceCtrlPeekBufferPositive(&ctl, 1);
    
    if ((ctl.Buttons & PSP_CTRL_CIRCLE) && autoUp) {
      
      autoMode = (~autoMode) & 1;
      autoUp = 0;
    } else if(!(ctl.Buttons & PSP_CTRL_CIRCLE)) {
      autoUp = 1;
    }
    

    if(!autoMode) {
      
      autoFirst = 1;
      if ((ctl.Buttons & PSP_CTRL_TRIANGLE) && up) {
        
        if (overclockId < lastTableId) {
          overclockId += 1;
          up = 0;
        }
      }

      if ((ctl.Buttons & PSP_CTRL_CROSS) && up) {
        
        if (overclockId > 0) {
          overclockId -= 1;
          up = 0;
        }
      }
      
      if (!up) {
        setOverclock();
      }

      if (!(ctl.Buttons & PSP_CTRL_TRIANGLE) && !(ctl.Buttons & PSP_CTRL_CROSS)) {
        up = 1;
      }
    }
    
    sceKernelDelayThread(10);
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

  {
    int thid = sceKernelCreateThread("ocpicker-thread",
      (int (*)(unsigned int, void*))((void*)thread), 0x14, 0x8000, PSP_THREAD_ATTR_VFPU, NULL);
    
    if (thid >= 0) {
      sceKernelStartThread(thid, 0, NULL);
    }
  }

  {
    int thid = sceKernelCreateThread("ocpicker-thread-auto",
      (int (*)(unsigned int, void*))((void*)autoThread), 0x15, 0x8000, PSP_THREAD_ATTR_VFPU, NULL);
    
    if (thid >= 0) {
      sceKernelStartThread(thid, 0, NULL);
    }
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
    
    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    pspDebugScreenSetOffset(offset);
    
    pspDebugScreenSetXY(42, 32);
    pspDebugScreenPrintf("Overclock Picker Test v1.1");
    
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf(" FPS: %llu               \n", fps);
    pspDebugScreenPrintf(" freq: %.0f MHz            ", frequency);
    
    //pspDebugScreenPrintf(" v: 0x%08x            \n", autoMode);
    //pspDebugScreenPrintf(" Ctrl: 0x%08x            \n", ctrl);
    //pspDebugScreenPrintf(" Mult: 0x%08x            \n", mult);
  
    pspDebugScreenSetXY(0, 4);
    if(!autoMode) {
      
      pspDebugScreenPrintf(" MANUAL mode:                                     \n");
      pspDebugScreenPrintf(" - Press TRIANGLE or CROSS to change the frequency\n");
      pspDebugScreenPrintf(" - Press CIRCLE to switch to AUTO mode            \n");
    }
    else {
      pspDebugScreenPrintf(" AUTO mode:                                       \n");
      pspDebugScreenPrintf("                                                  \n");
      pspDebugScreenPrintf(" - Press CIRCLE to switch to MANUAL mode          \n");
    }
    
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
    
    buffer = (int)sceGuSwapBuffers();
    sceKernelDcacheWritebackInvalidateAll();
    
    sceRtcGetCurrentTick(&now);
    fps = res / (now - prev);
    
  } while (!(ctl.Buttons & PSP_CTRL_HOME));
  
  cancelOverclock();
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(1, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(300000);
  
  sceKernelExitGame();
  return 0;
}
