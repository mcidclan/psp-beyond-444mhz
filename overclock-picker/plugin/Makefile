BINOUT = ./bin/
PATHSRC = ./src/

TARGET_NAME = opicker
TARGET = $(BINOUT)$(TARGET_NAME)

CC = psp-gcc
FIXUP = psp-fixup-imports

PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)
INCDIR = -I. -I$(PSPDEV)/psp/include -I$(PSPSDK)/include
LIBDIR = -L. -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib

PSPSDK = $(shell psp-config --pspsdk-path)
CFLAGS = -fsingle-precision-constant -Os -G0 -Wall $(INCDIR)
LDFLAGS = $(LIBDIR) -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -nostartfiles -Wl,-zmax-page-size=128

LIBS = -lpspdebug -lpspge -lpspdisplay -lpspctrl -lpsppower -lpsprtc -lpspsystemctrl_kernel

release: all

C_FILES = $(shell ls $(PATHSRC)*.c)
OBJS = $(notdir $(patsubst %.c, %.o, $(C_FILES)))
OBJS := $(sort $(OBJS:%.o=$(BINOUT)%.o))

FINAL_TARGET = $(TARGET).prx

all: $(FINAL_TARGET)

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@ && $(FIXUP) $@

%.prx: %.elf
	psp-prxgen $< $@

$(BINOUT)%.o: $(PATHSRC)%.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	-rm -f $(FINAL_TARGET) $(TARGET).elf $(OBJS)

rebuild: clean all
