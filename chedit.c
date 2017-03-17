/*
 * chedit - CHaracter EDITor, inspired by UCSD Pascal.
 *
 * "Abandon all hope, ye who enter here" -Dante
 *
 * Copyright (c) 1986,1994 David L. Parsons
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by David L. Parsons.  My name may not be used to endorse or
 * promote products derived from this software without specific
 * prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <curses.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#undef toupper

#include "psf.h"

#define gotoxy(x,y)	move(y,x)

#define	YES	1
#define	NO	0

#define	ESC	033

/* 
 * top line for help, edit, and charset windows
 */
#define	TOPY	4
#define	CHARY	0

/*
 * x position of display windows
 */
#define	CHARX	54
#define	EDITX	31

/*
 * pen states
 */
#define	NONE	0
#define	SET	1
#define	CLEAR	2
#define	XOR	3

#define DEPTH	32

/*
 * font information and editing information
 */
char	*fontname;
int	readonly=0;
unsigned char undo[DEPTH];
unsigned char font[256][DEPTH];
unsigned char curch=127;	/* character being edited */
int	ysize=16;		/* default to 8x16 characters */
int	curx=0;			/* x-position */
int	cury=0;			/* y-position */
int	touched=0;		/* font modified? */

/*
 * mask array for mapping xpos into the characters...
 */
int	pixelmask[8] = { 0200, 0100, 0040, 0020, 0010, 0004, 0002, 0001 };

char blurb[] = "CHEDIT V" VERSION " - Orc/" BUILD_DATE;
int (*pen)();
int pen_state;
int confirm(char *fmt, ...);
int dmsg(char *fmt, ...);

int rolll(), rollr(), rollu(), rolld(), invert(), flipr(), flipc();

main(argc, argv)
char **argv;
{
    register c;
    register i;
    char *shell;
    char command[128];

    if (argc != 2) {
	fprintf(stderr, "usage: chedit font\n");
	exit(1);
    }

    if (!isatty(fileno(stdout))) {
	fprintf(stderr, "stdout is not a tty!");
	exit(1);
    }

    if ((fontname = strdup(argv[1])) == 0) {
	fprintf(stderr, "out of memory\n");
	exit(4);
    }

    beginwin();
    if (LINES < 24 || COLS < 80) {
	gotoxy(0, LINES-1);
	printw("Need at least a 24x80 screen for editing\n");
	goodbye(5);
    }

    if (!openfont(fontname)) {
	gotoxy(0,LINES-1);
	goodbye(2);
    }
    init();
    for (i=0; i<ysize; i++)
	undo[i] = font[curch][i];

    cursor(curx, cury, 1);
    while (1) {
	c = getany();
	cursor(curx, cury, 0);
	switch (isalpha(c) ? toupper(c) : c) {
	case 'Q':
		if (touched && !confirm("Discard changes"))
		    break;
		gotoxy(0,24);
		goodbye(0);
	case 'C':
	    if (confirm("Clear character")) {
		for (i=0; i<ysize;i++)
		    font[curch][i] = 0;
		updchar();
		touched=1;
	    }
	    break;
	
	case '=':
	    if (confirm("Copy into this character")) {
		dmsg("From ");
		if ((c = getedit()) != EOF) {
		    for (i=0; i<ysize; i++)
			font[curch][i] = font[c][i];
		    updchar();
		    touched=1;
		}
		clearmsg();
	    }
	    break;

	case 'U'-'@':
	case KEY_UNDO:
	    for (i=0; i<ysize; i++) {
		c = font[curch][i];
		font[curch][i] = undo[i];
		undo[i] = c;
	    }
	    updchar();
	    break;
 
 	case 'G':
	    dmsg("Get ");
	    if ((c = getedit()) != EOF) {
		curch = c;
		updchar();
		for (i=0; i<ysize; i++)
		    undo[i] = font[curch][i];
	    }
	    clearmsg();
	    break;

	case KEY_LEFT:
	case 'H': movecur(isupper(c), curx-1, cury  );	break;
	case 'B': movecur(isupper(c), curx-1, cury+1);	break;
	case KEY_DOWN:
	case 'J': movecur(isupper(c), curx,   cury+1);	break;
	case 'N': movecur(isupper(c), curx+1, cury+1);	break;
	case KEY_RIGHT:
	case 'L': movecur(isupper(c), curx+1, cury  );	break;
	case 'U': movecur(isupper(c), curx+1, cury-1);	break;
	case KEY_UP:
	case 'K': movecur(isupper(c), curx,   cury-1);	break;
	case 'Y': movecur(isupper(c), curx-1, cury-1);	break;
	case '<': rolll(curch); updchar();		break;
	case '>': rollr(curch); updchar();		break;
	case '{': rollu(curch); updchar();		break;
	case '}': rolld(curch); updchar();		break;
	case '/': invert(curch); updchar();		break;
	case KEY_F(1): charset(rolll);			break;
	case KEY_F(2): charset(rollr);			break;
	case KEY_F(3): charset(rollu);			break;
	case KEY_F(4): charset(rolld);			break;
	case KEY_F(5): charset(invert);			break;
	case KEY_F(6): charset(flipr);			break;
	case KEY_F(7): charset(flipc);			break;
	case KEY_F(9): flipr(curch); updchar();		break;
	case KEY_F(10):flipc(curch); updchar();		break;
	case 'D': penstate(CLEAR); offbit();		break;
	case 'A': penstate(SET); onbit();		break;
	case 'X': penstate(XOR); flipbit();		break;
	case ESC: penstate(NONE);			break;
	case 'S': slicer();				break;
	case 'T': slicec();				break;
	case 'V': visit(); clearmsg();			break;
	case 'R':
		if (touched && !confirm("Discard changes"))
		    break;
		dmsg("read (c/r = %s) ", fontname);
		if ((c = readline(command)) != ESC) {
		    if (c != 0 && openfont(command)) {
			gotoxy(1,2);
			for (i=0; fontname[i]; i++)
			    addch(' ');
			free(fontname);
			fontname = strdup(command);
			init();
			touched=0;
		    }
		}
		else
		    clearmsg();
		break;

	case 'W':
		if (writefont()) {
		    dmsg("font written");
		    touched=0;
		}
		break;

	case '!':
	    dmsg("!");
	    c = readline(command);
	    if (c != 0 && c != ESC) {
		endwin();
		if (system(command) >= 0) {
		    fprintf(stdout, "[more]");
		    fflush(stdout);
		    while (getchar() != '\n')
			;
		    beginwin();
		    init();
		}
		else
		    dmsg("Cannot fork off subprocess!");
	    }
	    else
		clearmsg();
	    break;
	}
	refresh();
	cursor(curx, cury, 1);
    }
}


charset(function)
int (*function)();
{
    register unsigned c;

    for (c = 0; c < 256; c++) {
	if (function)
	    (*function)(c);
	graphic(c);
    }
    updchar();
}


movecur(draw, newx, newy)
{
    if (newx >= 0 && newx <= 7 && newy >= 0 && newy < ysize) {
	curx = newx;
	cury = newy;
	if (draw && pen)
	    (*pen)();
    }
    else
	beep();
}


clearwin()
{
    clear();
}


readline(s)
register char *s;
{
    register c;
    register char *p = s;
    int x, y;

    leaveok(stdscr, FALSE);
    refresh();
    while ((c=getany()) != '\r' && c != '\n') {
	if (c == 0x10 || c == 0x7f || c == KEY_BACKSPACE) {
	    if (p > s) {
		getsyx(y, x);
		mvaddch(y, x-1, ' ');
		move(y, x-1);
		--p;
		refresh();
		continue;
	    }
	}
	else if (c == ESC)
	    return c;
	else if (c >= 32 && c <= '~') {
	    *p++ = c;
	    addch(c);
	    refresh();
	    continue;
	}
	beep();
    }
    *p = 0;
    return *s;
}


openfont(name)
register char *name;
{
    register size;
    PSF_header hdr;
    char *bfr;
    int i, j;
    int handle;

    memset(font, 0, sizeof font);

    if ((handle = open(name, O_RDONLY)) < 0) {
	if (!confirm("Font %s does not exist; create it", name))
	    return 0;
	ysize = 16;
	return 1;
    }
    else {
	readonly = (access(name, W_OK) < 0);
	if (read(handle, &hdr, sizeof hdr) != sizeof hdr) {
	    dmsg("%s: %s", name, errno ? strerror(errno) : "read error");
	    return 0;
	}
	if (hdr.magic != PSF_MAGIC || hdr.mode != PSF_MODE) {
	    dmsg("Not a PSF font");
	    close(handle);
	    return 0;
	}
	if (hdr.size > LINES-(TOPY+4)) {
	    dmsg("Font too large for window (8x%d is max)", LINES-(TOPY+4));
	    close(handle);
	    return 0;
	}
	size = (ysize=hdr.size) * 256;

	if ((bfr = malloc(size)) == (char*)0) {
	    dmsg("OUT OF MEMORY");
	    close(handle);
	    return 0;
	}

	if (read(handle, bfr, size) != size) {
	    dmsg("Cannot read font");
	    free(bfr);
	    close(handle);
	    return 0;
	}

	for (i=0; i<256; i++)
	    for (j=0; j<ysize; j++)
		font[i][j] = bfr[(i*ysize)+j];
	free(bfr);
	close(handle);
    }
    return 1;
}

writefont()
{
    PSF_header hdr;
    char *bfr;
    int i, j;
    int handle;
    int size = ysize * 256;

    hdr.magic = PSF_MAGIC;
    hdr.mode  = PSF_MODE;
    hdr.size  = ysize;

    if ((bfr = malloc(size)) == (char*)0) {
	dmsg("OUT OF MEMORY");
	return 0;
    }

    if ((handle = open(fontname, O_WRONLY|O_CREAT, 0666)) < 0) {
	dmsg("%s: %s", fontname, errno ? strerror(errno) : "open error");
	return 0;
    }
    if (write(handle, &hdr, sizeof hdr) != sizeof hdr) {
	dmsg("%s: %s", fontname, errno ? strerror(errno) : "write error");
	close(handle);
	return 0;
    }

    for (i=0; i<256; i++)
	for (j=0; j<ysize; j++)
	    bfr[(i*ysize)+j] = font[i][j];

    if (write(handle, bfr, size) != size) {
	dmsg("%s: %s", fontname, errno ? strerror(errno) : "write error");
	free(bfr);
	close(handle);
	return 0;
    }
    close(handle);
    free(bfr);
    return 1;
}


init()
{
    register i;

    clearwin();
    mvaddstr(1, 1, blurb);
    mvaddstr(2, 1, fontname);
    if (readonly) addstr(" (readonly)");

    drawbox(0,    0,       EDITX+16, 2);	/* title bar */
    drawbox(0,    TOPY,    EDITX-2, 16);	/* help window */
    drawbox(EDITX,TOPY,    16,   ysize);	/* edit window */
    drawbox(CHARX,CHARY+18,16,       2);	/* mode window */

    mvaddstr(TOPY+1, 1, "G)et a character to edit");
    mvaddstr(TOPY+2, 1, "C)lear current char");
    mvaddstr(TOPY+3, 1, "A)dd a bit");
    mvaddstr(TOPY+4, 1, "D)elete a bit");
    mvaddstr(TOPY+5, 1, "X)or a bit");

    mvaddstr(TOPY+7, 1, "        y k u          Y K U");
    mvaddstr(TOPY+8, 1, "Moving: h   l  Drawing:H   L");
    mvaddstr(TOPY+9, 1, "        b j n          B J N");
    
    mvaddstr(TOPY+11,1, "R)ead in a font");
    mvaddstr(TOPY+12,1, "W)rite current font");
    mvaddstr(TOPY+13,1, "Q)uit chedit");

    mvaddstr(TOPY+15,1, "Colour set by A/D/X");
    mvaddstr(TOPY+16,1, "[ESC] turns off pen");
    pen_state = (-1);
    penstate(NONE);

    mvprintw(CHARY+20, CHARX+1, "8x%d font", ysize);
    
    fontwin();
    refresh();
}


fontwin()
{
    drawbox(CHARX, CHARY, 16, 16);	/* charset display window */
    charset((char *)NULL);
}


penstate(state)
{
    int onbit(), offbit(), flipbit();
    char *msg;

    if (state != pen_state) {
	switch (pen_state=state) {
	default: pen = NULL;	msg = "        ";	break;
	case SET: pen = onbit;	msg = "adding  ";	break;
	case CLEAR: pen=offbit;	msg = "deleting";	break;
	case XOR: pen=flipbit;	msg = "xorring ";	break;
	}
	gotoxy(CHARX+1, CHARY+19);
	dstring(msg);
	clearmsg();
    }
}


drawbox(x0, y0, dx, dy)
{
    register x;
    register y;

    for (x=0; x<=dx; x++) {
	mvaddch(y0,      x0+x, ACS_HLINE);
	mvaddch(y0+dy+1, x0+x, ACS_HLINE);
    }
    for (y=0; y<=dy; y++) {
	mvaddch(y0+y, x0,      ACS_VLINE);
	mvaddch(y0+y, x0+dx+1, ACS_VLINE);
    }
    mvaddch(y0,      x0,      ACS_ULCORNER);
    mvaddch(y0+dy+1, x0,      ACS_LLCORNER);
    mvaddch(y0,      x0+dx+1, ACS_URCORNER);
    mvaddch(y0+dy+1, x0+dx+1, ACS_LRCORNER);
}


updchar()
{
    register y;

    for (y=0; y<ysize; y++)
	updcolumn(y);
    penstate(NONE);
    graphic(curch);
    updsample(curch);
}


updsample()
{
    register y;

    for (y=0; y<ysize; y++)
	blockat(CHARX+1+y, CHARY+20, font[curch]);
}


updcolumn(y)
{
    register x;

    for (x=0; x<8; x++)
	if (font[curch][y] & pixelmask[x])
	    dot(x,y);
	else
	    clearbit(x,y);
}


onbit()
{
    font[curch][cury] |= pixelmask[curx];
    updcolumn(cury);
    updsample(curch);
    graphic(curch);
    touched=1;
}


offbit()
{
    font[curch][cury] &= ~pixelmask[curx];
    updcolumn(cury);
    updsample(curch);
    graphic(curch);
    touched=1;
}


flipbit()
{
    font[curch][cury] ^= pixelmask[curx];
    updcolumn(cury);
    updsample(curch);
    graphic(curch);
    touched=1;
}


getany()
{
    return getch();
}


highlight(c, on)
register c;
{
    int x = c%16,
	y = c/16;
    int mode = (c & 0x80) ? A_STANDOUT : A_NORMAL;

    attrset(on ? A_REVERSE : mode);
    mvaddch(CHARY+1+y, CHARX+1+x, (char)mvinch(CHARY+1+y, CHARX+1+x));
    attrset(A_NORMAL);
}


/*
 * get a char to edit.  You can type in anything < 128; alt-digits selects
 * an ascii number; the arrow keys will move a little cursor around to pick
 * a character to edit; c/r selects the character.
 */
getedit()
{
    register c;
    int ret;
    register thisch=curch;
    register limit = 256;

    leaveok(stdscr, FALSE);
    while (1) {
	highlight(thisch, 1);
	c = getch();
	highlight(thisch, 0);

	switch (c) {
	case KEY_LEFT:  if (thisch>0)		/* leftarrow */
			    --thisch;
			continue;
	case KEY_UP:    if (thisch>=16)		/* uparrow */
			    thisch -= 16;
			continue;
	case KEY_RIGHT: if (thisch < limit-1)	/* rightarrow */
			    ++thisch;
			continue;
	case KEY_DOWN: if (thisch < limit-16)	/* downarrow */
			    thisch += 16;
			continue;
	case '\n':
	case '\r':	ret = thisch;
			goto done;
	case ESC:	ret = EOF;
			goto done;
	default:	if (isprint(c)) {
			    ret = c;
			    goto done;
			}
	}
    }
done:
    leaveok(stdscr, TRUE);
    return ret;
} /* getedit */


getupper()
{
    return toupper(getany());
}


clearmsg()
{
    gotoxy(0,LINES-2);
    clrtoeol();
    refresh();
}


dmsg(char *fmt, ...)
{
    va_list ptr;
    char bfr[200];

    gotoxy(0, LINES-2);
    va_start(ptr, fmt);
    vsprintf(bfr, fmt, ptr);
    va_end(ptr);
    printw("%s", bfr);
    clrtoeol();
    refresh();
}


confirm(char *fmt, ...)
{
    va_list ptr;
    register c;
    char bfr[200];

    gotoxy(0, LINES-2);
    va_start(ptr, fmt);
    vsprintf(bfr, fmt, ptr);
    va_end(ptr);
    printw("%s?", bfr);
    clrtoeol();
    refresh();
    c = getupper();
    clearmsg();
    return (c == 'Y');
}


dstring(s)
char *s;
{
    addstr(s);
}


graphic(c)
register c;
{
    int x = c%16,
	y = c/16;

    if (c & 0x80)
	attron(A_STANDOUT);
    if (c == curch)
	attron(A_REVERSE);
    c &= 0x7f;
    mvaddch(CHARY+1+y, CHARX+1+x, (isprint(c) && c != 0x7f) ? c : '.');
    attroff(A_STANDOUT|A_REVERSE);
}


doublesq(x,y, lhs, rhs)
char lhs, rhs;
{
    mvaddch(TOPY+1+y, EDITX+1+x+x, lhs); addch(rhs);
}


dot(x,y)
{
    doublesq(x, y, '[', ']');
}


halfdot(x,y)
{
    attrset(A_STANDOUT);
    doublesq(x, y, '[', ']');
    attrset(A_NORMAL);
}


shadow(c)
{
    register x, y;

    for (y=0; y<ysize; y++)
	for (x=0; x<8; x++)
	    if (font[c][y] & pixelmask[x])
		halfdot(x,y);
}


cursor(x,y,on)
{
    attrset(on ? A_REVERSE : A_NORMAL);
    mvaddch(TOPY+1+y, EDITX+1+x+x, (char)mvinch(TOPY+1+y, EDITX+1+x+x));
    mvaddch(TOPY+1+y, EDITX+2+x+x, (char)mvinch(TOPY+1+y, EDITX+2+x+x));
    attrset(A_NORMAL);
}


clearbit(x,y)
{
    doublesq(x, y, ' ', ' ');
}


blockat(x, y, mask)
char *mask;
{
}


flipr(c)
unsigned c;
{
    register carry, i;

    for (i=0; i<8; i++) {
	carry = font[c][i];
	font[c][i] = font[c][ysize-i];
	font[c][ysize-i] = carry;
    }
    touched=1;
}


flipc(c)
unsigned c;
{
    register carry, i, j;

    for (i=0; i<ysize; i++) {
	carry = 0;
	for (j=0; j<8; j++)
	    if (font[c][i] & pixelmask[j])
		carry |= pixelmask[7-j];
	font[c][i] = carry;
    }
    touched=1;
}


invert(c)
unsigned c;
{
    register i;

    for (i=0; i<ysize;i++)
	font[c][i] = ~font[c][i];
    touched=1;
}


rolll(c)
{
    register carry, i;

    for (i=0; i<ysize;i++) {
	carry = font[c][i] & 0x80;
	font[c][i] <<= 1;
	if (carry)
	    font[c][i] |= 0x01;
    }
    touched=1;
}


rollr(c)
{
    register carry, i;

    for (i=0; i<ysize;i++) {
	carry = font[c][i] & 0x01;
	font[c][i] >>= 1;
	if (carry)
	    font[c][i] |= 0x80;
    }
    touched=1;
}


rollu(c)
{
    register carry, i;

    carry = font[c][0];
    for (i=1; i<ysize;i++)
	font[c][i-1] = font[c][i];
    font[c][ysize-1] = carry;
    touched=1;
}


rolld(c)
{
    register carry, i;

    carry = font[c][ysize-1];
    for (i=ysize-2; i>=0; --i)
	font[c][i+1] = font[c][i];
    font[c][0] = carry;
    touched=1;
}


draw_hline(y, on)
{
    register x;

    attrset(on ? (A_STANDOUT|A_REVERSE) : A_NORMAL);

    for (x=0; x<8; x++) {
	mvaddch(TOPY+1+y, EDITX+1+x+x, (char)mvinch(TOPY+1+y, EDITX+1+x+x));
	mvaddch(TOPY+1+y, EDITX+2+x+x, (char)mvinch(TOPY+1+y, EDITX+2+x+x));
    }

    attrset(A_NORMAL);
}


slicer()
{
    register i, c;
    register y;

    dmsg("Slice row: J,K move slice, ESC aborts, D)elete, I)insert");
    draw_hline(y=cury, YES);
    while ((c=getupper()) != ESC && c != 'D' && c != 'I') {
	draw_hline(y, NO);
	if ((c == 'J' || c == KEY_DOWN) && y < ysize-1)
	    ++y;
	else if ((c == 'K' || c == KEY_UP) && y > 0)
	    --y;
	draw_hline(y, YES);
    }
    draw_hline(y, NO);
    switch (c) {
    case 'D':
	while (y < ysize-1) {
	    font[curch][y] = font[curch][y+1];
	    ++y;
	}
	font[curch][ysize-1] = 0;
	updchar();
	touched=1;
	break;
    case 'I':
	for (i=ysize-2;i>=y;--i)
	    font[curch][i+1] = font[curch][i];
	updchar();
	touched=1;
	break;
    }
    clearmsg();
}


draw_vline(x, on)
{
    register y;

    attrset(on ? (A_STANDOUT|A_REVERSE) : A_NORMAL);

    for (y=0; y<ysize; y++)
	mvaddch(TOPY+1+y, EDITX+1+x+x, (char)mvinch(TOPY+1+y, EDITX+1+x+x));

    attrset(A_NORMAL);
}


slicec()
{
    static unsigned char bitmask[] = { 0xff, 0x7f, 0x3f, 0x1f,
				       0x0f, 0x07, 0x03, 0x01,
				       0x00  };
    register i, c;
    register x;
    register unsigned rhs;

    dmsg("Slice column: H,L move slice, ESC aborts, D)elete, I)insert");
    draw_vline(x=curx, YES);
    while ((c=getupper()) != ESC && c != 'D' && c != 'I') {
	draw_vline(x, NO);
	if ((c == 'L' || c == KEY_RIGHT) && x < 7)
	    ++x;
	else if ((c == 'H' || c == KEY_LEFT) && x > 0)
	    --x;
	draw_vline(x, YES);
    }
    draw_vline(x, NO);
    clearmsg();
    if (c == ESC)
	return;
    for (i=0; i<ysize; i++) {
	if (c == 'D') {
	    rhs = font[curch][i] & bitmask[x+1];
	    font[curch][i] &= ~bitmask[x];
	    rhs <<= 1;
	}
	else {
	    rhs = font[curch][i] & bitmask[x];
	    font[curch][i] &= ~bitmask[x+1];
	    rhs >>= 1;
	}
	font[curch][i] |= rhs;
    }
    touched=1;
    updchar();
}


visit()
{
    register i;
    register unsigned c;
    char ask;

    dmsg("Visit ");
    if ((c = getedit()) == EOF)
	return;
    shadow(c);
    dmsg("O)r, X)or, N)and, A)nd, Q)uit, ESC");
    while ((ask = getupper()) != ESC && strchr("OXNAQ", ask) == 0)
	;
    if (ask != ESC) {
	for (i=0; i<ysize; i++)
	    updcolumn(i);
    }
    if (ask != ESC && ask != 'Q') {
	for (i=0; i<ysize; i++)
	    switch (ask) {
	    case 'O': font[curch][i] |= font[c][i];	break;
	    case 'A': font[curch][i] &= font[c][i];	break;
	    case 'X': font[curch][i] ^= font[c][i];	break;
	    case 'N': font[curch][i] &= ~font[c][i];	break;
	    }
	updchar();
	touched=1;
    }
}


beginwin()
{
    if (!initscr())
	exit(9);
    raw();
    noecho();
    clear();
    leaveok(stdscr, TRUE);
    keypad(stdscr, TRUE);
}


goodbye(code)
{
    printw("[Press any key to exit]");getupper();
    endwin();
    exit(code);
}
