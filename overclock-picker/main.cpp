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

PSP_MODULE_INFO("overclock-picker", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

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

float frequency = 0.0f;

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
     
/*     
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
  
  frequency = PLL_BASE_FREQ * (((float)mul) / ((float)0x12)) * ratio;
  
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

int thread() {
  
  static int up = 1;
  
  const int lastTableId = mulTableSize - 1;
  
  SceCtrlData ctl;
  initOverclock();
  
  while (1) {
    
    sceCtrlPeekBufferPositive(&ctl, 1);
    
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
    
    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    pspDebugScreenSetOffset(offset);
    
    pspDebugScreenSetXY(40, 0);
    pspDebugScreenPrintf("Overclock Picker v1.0");
    
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf(" FPS: %llu               \n", fps);
    pspDebugScreenPrintf(" freq: %.0f MHz            \n", frequency);
    //pspDebugScreenPrintf(" Ctrl: 0x%08x            \n", ctrl);
    //pspDebugScreenPrintf(" Mult: 0x%08x            \n", mult);
  
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
