BINOUT = ./bin/
PATHSRC = ./
PATHOBJS = $(BINOUT)
TARGET = $(BINOUT)opicker

BUILD_PRX = 1

CPP_FILES = $(wildcard $(PATHSRC)*.cpp)
PATHFILES = $(CPP_FILES) kcall.S

OBJS = $(notdir $(patsubst %.cpp, %.o, $(patsubst %.S, %.o, $(PATHFILES))))
OBJS := $(sort $(OBJS:%.o=$(PATHOBJS)%.o))

CFLAGS = -Os -G0 -Wall -fno-pic -I./kernel/src \
         -Wextra -Werror

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti  -std=c++11
ASFLAGS = $(CFLAGS)

LIBS = -lpsppower -lpspgu -lpspge

EXTRA_CLEAN = kcall.S
PSP_EBOOT_SFO = $(BINOUT)PARAM.SFO
EXTRA_TARGETS = $(BINOUT)EBOOT.PBP
PSP_EBOOT = $(EXTRA_TARGETS)
PSP_EBOOT_TITLE = Overclock Picker
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

.PHONY: kernel
$(OBJS): kernel

kernel:
	$(MAKE) -C ./kernel

kcall.S: kernel
	@
  
$(PATHOBJS)kcall.o: kcall.S
	$(CXX) -o $@ -c $< $(ASFLAGS)

$(PATHOBJS)%.o: $(PATHSRC)%.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

clean:
	-rm -f $(TARGET).elf $(TARGET).prx $(OBJS) $(EXTRA_TARGETS) $(PSP_EBOOT_SFO) kcall.S
	$(MAKE) -C ./kernel clean
