#
# makefile for chedit
#
CFLAGS= -O -DATARIST

chedit: \bin\chedit.ttp
\bin\chedit.ttp: chedit.o
	cc $(CFLAGS) -o \bin\chedit chedit.o

chedit.o: chedit.c
	cc $(CFLAGS) -c chedit.c
