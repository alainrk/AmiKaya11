# Declaration of base directories
INCLUDE = ./include
PHASE1PATHE = ./phase1/e
PHASE2PATHE = ./phase2/e
PHASE1PATHSRC = ./phase1/src
PHASE2PATHSRC = ./phase2/src
LIBPATH = /usr/local/lib/umps2
ELFPATH = /usr/local/include/umps2/umps
ELF32 = /usr/local/share/umps2

# Declaration of base commands
CFLAGS = -Wall -I $(INCLUDE) -I $(PHASE1PATHE) -I $(PHASE2PATHE) -I $(ELFPATH) -I $(ELF32) -c
LDFLAGS =  -T
CC = mipsel-linux-gcc
LD = mipsel-linux-ld

# Main target
all: kernel.core.umps

kernel.core.umps: kernel
	umps2-elf2umps -k kernel

# Kernel linking
kernel: phase1dir phase2dir $(LIBPATH)/crtso.o $(LIBPATH)/libumps.o
	$(LD) $(LDFLAGS) 	$(ELF32)/elf32ltsmip.h.umpscore.x \
				$(LIBPATH)/crtso.o \
				$(LIBPATH)/libumps.o \
				$(PHASE1PATHSRC)/msg.o \
				$(PHASE1PATHSRC)/tcb.o \
				$(PHASE2PATHSRC)/boot.o \
				$(PHASE2PATHSRC)/scheduler.o \
				$(PHASE2PATHSRC)/exceptions.o \
				$(PHASE2PATHSRC)/interrupts.o \
				$(PHASE2PATHSRC)/ssi.o \
				$(PHASE2PATHSRC)/manager.o \
				$(PHASE2PATHSRC)/p2test.0.2.o \
				-o kernel

# Phase1 test, put p1test object in the kernel linking files

# Phase1 sources
phase1dir:
	cd $(PHASE1PATHSRC) && make all

# Phase2 sources
phase2dir:
	cd $(PHASE2PATHSRC) && make all

# Clean created files
clean:
	rm -f *.o kernel
	rm -f kernel.*.umps
	cd $(PHASE1PATHSRC) && make clean
	cd $(PHASE2PATHSRC) && make clean