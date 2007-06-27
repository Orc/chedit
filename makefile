#
# makefile for chedit
#
CFLAGS= -s -O -DLINUX -DNCURSES
LIBES=-lcurses
CONTENTS=chedit.1 chedit.c chedit.lsm psf.h Makefile README README.ATARIST \
	Announce

chedit: chedit.o
	cc $(CFLAGS) -o chedit chedit.o $(LIBES)

chedit.o: chedit.c
	cc $(CFLAGS) -c chedit.c

tarball: chedit.tar.gz

clean:
	rm -f *.o chedit

chedit.tar.gz: $(CONTENTS) chedit
	tar czvf chedit.tar.gz $(CONTENTS)

