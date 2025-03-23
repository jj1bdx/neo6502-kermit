#Makefile for embedded Kermit.
#
# Copyright (C) 1995, 2021,
#  Trustees of Columbia University in the City of New York.
#  All Rights Reserved.  See kermit.c for license.

CC = ~/bin/llvm-mos/bin/mos-neo6502-clang
INCLUDE  = ~/bin/llvm-mos/mos-platform/neo6502/include
CFLAGS = -Os
OBJS= main.o kermit.o neoio.o

#Dependencies

main.o: main.c cdefs.h debug.h kermit.h

kermit.o: kermit.c cdefs.h debug.h kermit.h

neoio.o: neoio.c cdefs.h debug.h kermit.h

#Targets

clean:
	rm -f $(OBJS) core
 
all: $(ALL)

ek: $(OBJS)
	$(CC) $(CFLAGS) -o ek $(OBJS)

#End of Makefile
