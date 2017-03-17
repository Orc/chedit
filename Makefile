#
# makefile for chedit
#
CFLAGS= -s -O -DLINUX -DNCURSES
LIBES=-lcurses

CONTENTS=chedit.1 chedit.c chedit.lsm psf.h Makefile README README.ATARIST \
	Announce

chedit: chedit.o
	cc $(CFLAGS) -o chedit chedit.o $(LIBES)

chedit.o: chedit.c psf.h VERSION
	cc $(CFLAGS) -DVERSION="\"`cat VERSION`\"" -DBUILD_DATE="\"`date +'%d %B %Y'`\"" -c chedit.c

tarball: chedit-`cat VERSION`.tar.gz

clean:
	rm -f *.o chedit

chedit-`cat VERSION`.tar.gz: $(CONTENTS) chedit VERSION
	tar czvf chedit-`cat VERSION`.tar.gz $(CONTENTS)

