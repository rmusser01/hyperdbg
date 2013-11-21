#-*-makefile-*-

.SUFFIXES=.c .o .S

KVERSION= $(shell uname -r)
KDIR= /lib/modules/$(KVERSION)/build

hdbg-src:= hyperdbg
core-src:= core
i386-src:= $(core-src)/i386
libudis86-src:= libudis86

DEFINE += -DHVM_ARCH_BITS=32 -DENABLE_HYPERDBG \
	-DGUEST_LINUX -DVIDEO_DEFAULT_RESOLUTION_X=1024 -DVIDEO_DEFAULT_RESOLUTION_Y=768 -DENABLE_EPT -DHYPERDBG_VERSION=$(shell date +%Y%m%d)

INCLUDE += -I$(src)/$(hdbg-src) -I$(src)/$(core-src) -I$(src)/$(i386-src) -I$(src)/$(libudis86-src)
EXTRA_CFLAGS += $(DEFINE) $(INCLUDE) -fno-omit-frame-pointer

# DBG += CONFIG_DEBUG_SECTION_MISMATCH=y

core-objs:= $(core-src)/pill_linux.o $(core-src)/pill_common.o \
	    $(core-src)/comio.o $(core-src)/idt.o $(core-src)/x86.o $(core-src)/vmmstring.o $(core-src)/events.o \
	    $(core-src)/common.o $(core-src)/vmhandlers.o $(core-src)/vmx.o $(core-src)/mmu.o $(core-src)/snprintf.o \
	    $(core-src)/process.o $(core-src)/network.o $(core-src)/vt.o $(core-src)/linux.o  $(core-src)/ept.o

i386-objs:= $(i386-src)/io-asm.o $(i386-src)/common-asm.o $(i386-src)/reg-asm.o $(i386-src)/vmx-asm.o

hyperdbg-objs:= $(hdbg-src)/gui.o $(hdbg-src)/font_256.o  $(hdbg-src)/hyperdbg_cmd.o $(hdbg-src)/hyperdbg_guest.o \
	        $(hdbg-src)/hyperdbg_host.o $(hdbg-src)/hyperdbg_print.o $(hdbg-src)/keyboard.o $(hdbg-src)/pager.o $(hdbg-src)/pci.o \
	        $(hdbg-src)/scancode.o $(hdbg-src)/sw_bp.o $(hdbg-src)/syms.o $(hdbg-src)/symsearch.o \
	        $(hdbg-src)/video.o $(hdbg-src)/xpvideo.o

libudis86-objs:= $(libudis86-src)/decode.o $(libudis86-src)/input.o $(libudis86-src)/itab.o $(libudis86-src)/syn-att.o \
	         $(libudis86-src)/syn.o $(libudis86-src)/syn-intel.o $(libudis86-src)/udis86.o

obj-m := hcore.o
hcore-objs := $(core-objs) $(i386-objs) $(hyperdbg-objs) $(libudis86-objs)

all: asm-offset.h
	make $(DBG) -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	-rm -f $(core-src)/asm-offset.s $(core-src)/asm-offset.h

asm-offset.h: asm-offset.s
	@(echo "/*"; \
	echo " * DO NOT MODIFY."; \
	echo " *"; \
	echo " * This file was auto-generated from $<"; \
	echo " *"; \
	echo " */"; \
	echo " "; \
	echo "#ifndef __TEST_H__"; \
	echo "#define __TEST_H__"; \
	echo " "; \
	sed -ne "/^->/{s:^->\([^ ]*\) [\$$#]*\([^ ]*\) \(.*\):#define \1 \2 /* \3 */:; s:->::; p;}"; \
	echo " "; \
	echo "#endif") < $(core-src)/$< >$(core-src)/$@


asm-offset.s: $(core-src)/asm-offset.c
	$(CC) $(DEFINE) -S -o $(core-src)/$@  $<
