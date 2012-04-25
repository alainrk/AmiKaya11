# Makefile di phase2

# Dichiarazione delle cartelle base
INCLUDE = ../../include
PHASE1PATHE = ../../phase1/e
PHASE2PATHE = ../e
PHASE1PATHSRC = ../../phase1/src
LIBPATH = /usr/local/lib/umps2
ELFPATH = /usr/local/include/umps2/umps
ELF32 = /usr/local/share/umps2

# Dichiarazione dei comandi base
CFLAGS = -Wall -I $(INCLUDE) -I $(PHASE2PATHE) -I $(PHASE1PATHE) -I $(PHASE1PATHSRC) -I $(LIBPATH) -I $(ELFPATH) -I $(ELF32) -c
CC = mipsel-linux-gcc

# Target principale ---------> AGGIUNGERE FILE DI PHASE2 DA QUI IN POI, .O .C ecc...
all: boot.o scheduler.o exceptions.o interrupts.o ssi.o manager.o p2test.0.2.o

boot.o: boot.c
	$(CC) $(CFLAGS) boot.c

scheduler.o: scheduler.c
	$(CC) $(CFLAGS) scheduler.c

exceptions.o: exceptions.c
	$(CC) $(CFLAGS) exceptions.c

interrupts.o: interrupts.c
	$(CC) $(CFLAGS) interrupts.c

ssi.o: ssi.c
	$(CC) $(CFLAGS) ssi.c

manager.o: manager.c
	$(CC) $(CFLAGS) manager.c

p2test.0.2.o: p2test.0.2.c
	$(CC) $(CFLAGS) p2test.0.2.c

# Pulizia dei file oggetto
clean:
	rm -f *.o
