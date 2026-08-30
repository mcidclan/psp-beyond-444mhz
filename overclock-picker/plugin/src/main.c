#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psputilsforkernel.h>
#include <psppower.h>
#include <psprtc.h>

PSP_MODULE_INFO("picker-plugin", 0x1006, 1, 1);
PSP_NO_CREATE_MAIN_THREAD();
PSP_HEAP_SIZE_KB(512);

#include "overclock.h"
#include "hook.h"

int thid, alive = 0;
int activated = 0;

static void cancelOverclock() {
  
  int i = overclockId;
  do {
    
    overclockId = i;
    setOverclock();
    sceKernelDelayThread(OC_MINIMAL_DELAY);
    i--;
  } while (i >= 0);

}

static inline int exitGameWithStatus() {
  cancelOverclock();
  return _exitGameWithStatus();
}
static inline void exitGame() {
  cancelOverclock();
  _exitGame();
}

#define SAFETY_MARGIN 2

static inline int displaySetFrameBuf(void *fbuf, int width, int format, int sync) {
  
  void *frame = (void*)(0x40000000 | (u32)fbuf);
  if (activated > 0) {
    int bytesPerPixel = 4;
    if (format != PSP_DISPLAY_PIXEL_FORMAT_8888) {
      bytesPerPixel = 2;
    }
    const int squareSize = 4;
    const int gap = 2;
    const int startX = 8;
    const int startY = 8;
    const int lastTableId = mulTableSize - 1;
    const int maxSquares = ((overclockMaxId <= lastTableId) ?
      overclockMaxId : lastTableId) + 1 - SAFETY_MARGIN;
    
    int i = 0;
    while (i < overclockId) {
      int squareX = startX + i * (squareSize + gap);
      int r = (i * 255) / (maxSquares - 1);
      int g = 255 - r;
      int b = 0;
      u32 color32 = 0xFF000000 | (b << 16) | (g << 8) | r;
      u16 color16 = (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      int dy = 0;
      while (dy < squareSize) {
        u8 *row = (u8 *)frame + ((startY + dy) * width * bytesPerPixel);
        int dx = 0;
        while (dx < squareSize) {
          if (bytesPerPixel == 4) {
            *(u32 *)(row + (squareX + dx) * 4) = color32;
          } else {
            *(u16 *)(row + (squareX + dx) * 2) = color16;
          }
          dx++;
        }
        dy++;
      }
      i++;
    }

    {
      const int border = 2;
      const int rectX0 = startX - border;
      const int rectX1 = startX + (maxSquares - 1) * (squareSize + gap) - gap + border;
      const int rectY0 = startY - border;
      const int rectY1 = startY + squareSize + border;

      u32 borderColor32 = 0xFF00FF00;
      u16 borderColor16 = 0x07E0;

      int y = rectY0;
      while (y <= rectY1) {
        int onHorizontalEdge = (y == rectY0) || (y == rectY1);
        u8 *row = (u8 *)frame + (y * width * bytesPerPixel);
        int x = rectX0;
        while (x <= rectX1) {
          int onVerticalEdge = (x == rectX0) || (x == rectX1);
          if (onHorizontalEdge || onVerticalEdge) {
            if (bytesPerPixel == 4) {
              *(u32 *)(row + x * 4) = borderColor32;
            } else {
              *(u16 *)(row + x * 2) = borderColor16;
            }
          }
          x++;
        }
        y++;
      }
    }
  }
  return _displaySetFrameBuf(fbuf, width, format, sync);
}

int switchOverclock() {
  
  //static int released = 1;
  const int lastTableId = mulTableSize - 1;
  
  int maxId = (overclockMaxId <= lastTableId) ? overclockMaxId : lastTableId;
  if (maxId >= SAFETY_MARGIN) {
    maxId -= SAFETY_MARGIN;
  }
  
  SceCtrlData ctl;
  sceCtrlPeekBufferPositive(&ctl, 1);
  
  activated =
    (ctl.Buttons & PSP_CTRL_LTRIGGER) &&
    (ctl.Buttons & PSP_CTRL_RTRIGGER);
    
  const int up = (ctl.Buttons & PSP_CTRL_VOLUP) != 0;
  const int down = (ctl.Buttons & PSP_CTRL_VOLDOWN) != 0;

  //const int upActive = up && released;
  //const int downActive = down && released;
  
  if (activated && (up || down) /*(upActive || downActive)*/) {
    
    //released = 0;
    
    if (up & (overclockId < maxId)) {
      overclockId += 1;
    } else if(down && (overclockId > 0)) {
      overclockId -= 1;
    }
  }
  
  /*
  if (!up && !down) {
    released = 1;
    return 1;
  }
  */
  
  return up || down;
}

int thread(SceSize args, void *argp) {
  
  sceKernelDelayThread(6000000);
  
  alive = 1;
  activated = 0;
  u64 lastTime = 0;
  u64 prev, now;
  
  int init = 0;
  int delay = 0;
  int width, format;
  void *frame = NULL;
  
  while (alive) {
    
    sceDisplayGetFrameBuf(&frame, &width, &format, 0);
    
    if (frame) {
      
      if (!init) {
        
        initOverclock(&delay);
        delay = 0;//10;
        init = 1;
      }

      sceRtcGetCurrentTick(&prev);
      
      if (delay == 0) {
        
        const int switched = switchOverclock();
        if (switched) {
          
          setOverclock();
          lastTime = sceKernelGetSystemTimeWide();
          delay = 10;
        }
      }
      if (delay > 0) {
        
        u64 currentTime = sceKernelGetSystemTimeWide();
        if (currentTime - lastTime >= 50000) {
          delay -= 1;
          lastTime = currentTime;
        }
      }
      
      sceRtcGetCurrentTick(&now);
    }
    sceKernelDelayThread(1000);
  }
  return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp) {
    
  _displaySetFrameBuf = hook("sceDisplay_Service", "sceDisplay", 0x289D82FE, (void*)displaySetFrameBuf);
  _exitGame = hook("sceLoadExec", "LoadExecForUser", 0x05572A5F, (void*)exitGame);
  _exitGameWithStatus = hook("sceLoadExec", "LoadExecForUser", 0x2AC9954B, (void*)exitGameWithStatus);

  thid = sceKernelCreateThread("over-picker-thread", thread, 0x18, 0x8000, PSP_THREAD_ATTR_VFPU, NULL);
  if (thid >= 0) {
    sceKernelStartThread(thid, 0, NULL);
  }
  return 0;
}

int module_stop(SceSize args, void *argp) {
  
  if (alive) {
    alive = 0;
    SceUInt timeout = 500000;
    int ret = sceKernelWaitThreadEnd(thid, &timeout);
    if (ret < 0) {
      sceKernelTerminateDeleteThread(thid);
    }
  }
  
  return 0;
}
