CFLAGS=-O3 -Wall -pedantic -Iuxn -g -std=c89
OBJ=uxnseq.o uxn/uxn.o

default: uxnseq uxnasm

uxnseq: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ -lsndkit

uxnasm: uxn/uxnasm.c
	$(CC) $(CFALGS) $< -o $@

clean:
	$(RM) uxnseq $(OBJ) uxnasm
