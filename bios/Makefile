CROSS_COMPILE	?= /media/zeus/zPc/_FunyLinuxHack/mips-mti-linux-gnu/2017.10-05/bin/mips-mti-linux-gnu-
CC				= $(CROSS_COMPILE)gcc
LD				= $(CROSS_COMPILE)gcc
OBJCOPY			= $(CROSS_COMPILE)objcopy

OPT				= -Os

SRCS			= entry.S

FLAGS			= $(OPT) -mabi=32 -EL -mips1 -mno-mips16 -mno-long-calls  -nostdlib
CCFLAGS			= $(FLAGS) -fomit-frame-pointer -ffunction-sections -fdata-sections
LDFLAGS			= $(FLAGS) -Wl,--gc-sections  -Wl,-T loader.lkr

INCS			= $(wildcard *.h)
OBJS			= $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))

all:	loader.bin


%.bin: %.elf Makefile
	$(OBJCOPY) $< $@ -O binary -j.text

%.elf: $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c $(INCS) Makefile
	$(CC) -o $@ -c $< $(CCFLAGS)

%.o: %.S $(INCS) Makefile
	$(CC) -o $@ -c $< $(CCFLAGS)
