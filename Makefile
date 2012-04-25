# Makefile per AmiKaya11

# Dichiarazione delle cartelle base
INCLUDE = ./include
PHASE1PATHE = ./phase1/e
PHASE2PATHE = ./phase2/e
PHASE1PATHSRC = ./phase1/src
PHASE2PATHSRC = ./phase2/src
LIBPATH = /usr/local/lib/umps2
ELFPATH = /usr/local/include/umps2/umps
ELF32 = /usr/local/share/umps2

# Dichiarazione dei comandi base
CFLAGS = -Wall -I $(INCLUDE) -I $(PHASE1PATHE) -I $(PHASE2PATHE) -I $(ELFPATH) -I $(ELF32) -c
LDFLAGS =  -T
CC = mipsel-linux-gcc
LD = mipsel-linux-ld

# Target principale
all: kernel.core.umps

kernel.core.umps: kernel
	umps2-elf2umps -k kernel

# Linking del kernel
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

# Test phase1, mettere oggetto p1test nei file per il linking del kernel

# Sorgenti di phase1
phase1dir:
	cd $(PHASE1PATHSRC) && make all

# Sorgenti di phase2
phase2dir:
	cd $(PHASE2PATHSRC) && make all

# Pulizia dei file creati
clean:
	rm -f *.o kernel
	rm -f kernel.*.umps
	cd $(PHASE1PATHSRC) && make clean
	cd $(PHASE2PATHSRC) && make clean
