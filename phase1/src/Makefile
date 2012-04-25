# Makefile di phase1

# Dichiarazione delle cartelle base
INCLUDE = ../../include
PHASE1PATHE = ../e
ELFPATH = /usr/local/include/umps2/umps
ELF32 = /usr/local/share/umps2

# Dichiarazione dei comandi base
CFLAGS = -Wall -I $(INCLUDE) -I $(PHASE1PATHE) -I $(ELFPATH) -I $(ELF32) -c
CC = mipsel-linux-gcc

# Target principale
all: tcb.o msg.o
# Per testare phase1, commentare la riga precedente e decommentare le sottostanti
#all: tcb.o msg.o p1test.0.2.1.o

tcb.o: tcb.c
	$(CC) $(CFLAGS) tcb.c

msg.o: msg.c
	$(CC) $(CFLAGS) msg.c

#p1test.0.2.1.o: p1test.0.2.1.c
#	$(CC) $(CFLAGS) p1test.0.2.1.c

# Pulizia dei file oggetto
clean:
	rm -f *.o
