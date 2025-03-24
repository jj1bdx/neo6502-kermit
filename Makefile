# Makefile for neo6502-kermit.
# Copyright (C) 2025 Kenji Rikitake.
# Copyright (C) 1995, 2021,
#  Trustees of Columbia University in the City of New York.
#  All Rights Reserved.  See kermit.c for license.

LLVMPATH=${HOME}/bin/llvm-mos
CC = ${LLVMPATH}/bin/mos-neo6502-clang
INCLUDES = -I${LLVMPATH}/mos-platform/neo6502/include \
	-I${LLVMPATH}/mos-platform/common/include
# CFLAGS = -Os ${INCLUDES} -DNODEBUG
CFLAGS = -Os ${INCLUDES} -DDEBUG

OBJS = main.o kermit.o neoio.o
TARGET = kermit.neo

all: ${TARGET}
 
kermit.neo: $(OBJS)
	$(CC) $(CFLAGS) -o ${TARGET} $(OBJS)

#Dependencies

main.o: main.c cdefs.h debug.h kermit.h

kermit.o: kermit.c cdefs.h debug.h kermit.h

neoio.o: neoio.c cdefs.h debug.h kermit.h

#Targets

clean:
	rm -f $(OBJS) core

#End of Makefile
