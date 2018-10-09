#
#	Makefile for 2INC0 Condition Variables
#
#	(c) TUe 2010-2018, Nephthijs Jan Vanicetoek
#

BINARIES = prodcons

CC = gcc
CFLAGS = -Wall -Wextra -ggdb -c
LDLIBS = -lpthread

all:	$(BINARIES)

clean:
	rm -f *.o $(BINARIES)

prodcons: prodcons.o

prodcons.o: prodcons.c prodcons.h

