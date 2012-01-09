/*
 * Lexmark 1000 Black & white driver
 *
 * Author: Jean-Francois Moine
 *	mailto:moinejf@free.fr
 *	http:moinejf.free.fr
 *
 * Last update: 2003/03/28
 *
 * Adapted from lm1100 by Tim Engler <tim@burntcouch.com>
 *	http://www.burntcouch.com/lexmark/
 *
 * License: GPL
 *
 * Generation:
 *	cc -O2 lx.c -o lx
 *
 * Usage:
 *	Create the script 'lp':
 *		!/bin/sh
 *		gs -q -sDEVICE=pbmraw -r288 \
 *		-dNOPAUSE -dSAFER -dBATCH \
 *		-sOutputFile=- \
 *		$1 | lx > /dev/lp0
 *	then call:
 *		lp <file>.ps
 *
 */
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_PBM

#define MAX_PAGE_WIDTH 2600
#define MAX_PAGE_HEIGHT 3471

#ifndef USE_PBM
/* bitcmyk */
//#define A4

#ifdef A4
#define PAGE_WIDTH 2480
#define PAGE_HEIGHT 3508
#else
/* US letter */
#define PAGE_WIDTH 2550
#define PAGE_HEIGHT 3300
#endif
#endif

//#define X_START 1092		// initial x position (lm1100)
#define X_START 1084		// initial x position
//#define Y_START 85		// initial y position
#define Y_START 100		// initial y position

static int brightness = 128;	// fixme: should an argument

static int page_width, page_height, max_page_height;
static int xshift;
static int error_cnt;
static int curr_x = -1;
static int curr_y = -1;
static int pbuflen, old_x;
static int move_head, new_y;

static unsigned char lines[56][MAX_PAGE_WIDTH];
static unsigned char print_buf[MAX_PAGE_WIDTH * 7];

/* brightness filter */
#define FILTER_HEIGHT 3
#define FILTER_WIDTH 5
#define DIVIDER 48
static int errorMatrix[FILTER_HEIGHT][FILTER_WIDTH] = {
	{1, 3, 5, 3, 1},
	{3, 5, 7, 5, 3},
	{5, 7, 0, 0, 0}
};
static signed short bbuf[FILTER_HEIGHT + 56][MAX_PAGE_WIDTH + FILTER_WIDTH];

static void print_line(int last);

/* adjust the brightness */
static void do_bright(int y)
{
	int x, err;

	if (error_cnt != 0)
		error_cnt--;
	for (x = 0; x < page_width; x++) {
		int porig, pnew;

		err = 0;
		if (error_cnt != 0) {
			int i, j;

			for (i = 0; i < FILTER_HEIGHT; i++) {
				for (j = 0; j < FILTER_WIDTH; j++) {
					err += bbuf[y + i][x + j]
						* errorMatrix[i][j];
				}
			}
			err /= DIVIDER;
		}
		porig = (lines[y][x] ? 0x4f : 0) + err;
		if (porig >= brightness / 2)
			pnew = brightness;
		else	pnew = 0;
		err = porig - pnew;
		if (err != 0) {
			lines[y][x] = pnew != 0;
			error_cnt = FILTER_HEIGHT - 1;
		}
		bbuf[FILTER_HEIGHT - 1 + y][FILTER_WIDTH / 2 + x] = err;
	}
}

/* treat the next 56 non empty lines */
static int get56(void)
{
	int i, j, k;
	int line_width;
#ifdef USE_PBM
	unsigned char buf[MAX_PAGE_WIDTH / 8];

	if (new_y == 0) {

		/* get the PBM header */
		if (fgets(buf, sizeof buf, stdin) == 0)
			return -1;
		if (buf[0] != 'P' || buf[1] != '4') {
			fprintf(stderr, "Not a PBM file\n");
			exit(1);
		}
		fgets(buf, sizeof buf, stdin);	/* gs comment */
		fgets(buf, sizeof buf, stdin);	/* width height */
		sscanf(buf, "%d %d", &page_width, &page_height);
		if (page_width > MAX_PAGE_WIDTH) {
			fprintf(stderr, "Page too wide\n");
			exit(1);
		}
		max_page_height = page_height / 56 * 56;
#if 1
		xshift = 60;		// works with A4
#else
		xshift = (MAX_PAGE_WIDTH - page_width) / 2;
		xshift &= ~1;
#endif
	}
	line_width = (page_width + 7) / 8;

	/* search the next non empty line */
	for (;;) {
		if (fread(buf, 1, line_width, stdin) != line_width) {
			print_line(1);
			return -1;
		}

		for (j = 0; j < line_width; j++) {
			if (buf[j] != 0)
				break;
		}
		if (j < line_width)
			break;
		if (++new_y >= page_height) {
			print_line(1);
			new_y = 0;
			curr_x = -1;
			error_cnt = 0;
			return 0;
		}
	}

	/* shift the brightness buffer */
	i = curr_y + 56 - new_y + FILTER_HEIGHT-1;
	if (i <= 0 || curr_y < 0)
		memset(bbuf, 0, sizeof bbuf[0] * (FILTER_HEIGHT-1));
	else {
//fixme: test
if (i > FILTER_HEIGHT-1) {
  fprintf(stderr, "y error - curr_y:%d new_y:%d\n", curr_y, new_y);
  exit(1);
}
		memcpy(bbuf, bbuf[FILTER_HEIGHT-1 + 56 - i],
			 sizeof bbuf[0] * i);
		if (i != FILTER_HEIGHT-1)
			memset(bbuf[i], 0,
				sizeof bbuf[0] * (FILTER_HEIGHT-1 - i));
	}

	k = 0;
	for (j = 0; j < line_width; j++) {
		lines[0][k++] = (buf[j] & 0x80) != 0;
		lines[0][k++] = (buf[j] & 0x40) != 0;
		lines[0][k++] = (buf[j] & 0x20) != 0;
		lines[0][k++] = (buf[j] & 0x10) != 0;
		lines[0][k++] = (buf[j] & 0x08) != 0;
		lines[0][k++] = (buf[j] & 0x04) != 0;
		lines[0][k++] = (buf[j] & 0x02) != 0;
		lines[0][k++] = (buf[j] & 0x01) != 0;
	}
	do_bright(0);

	/* get the next 55 lines */
	for (i = 1; i < 56; i++) {
		if (i + new_y < page_height) {
			if (fread(buf, 1, line_width, stdin) != line_width) {
				fprintf(stderr, "Abnormal EOF\n");
				return -1;
			}
			k = 0;
			for (j = 0; j < line_width; j++) {
				lines[i][k++] = (buf[j] & 0x80) != 0;
				lines[i][k++] = (buf[j] & 0x40) != 0;
				lines[i][k++] = (buf[j] & 0x20) != 0;
				lines[i][k++] = (buf[j] & 0x10) != 0;
				lines[i][k++] = (buf[j] & 0x08) != 0;
				lines[i][k++] = (buf[j] & 0x04) != 0;
				lines[i][k++] = (buf[j] & 0x02) != 0;
				lines[i][k++] = (buf[j] & 0x01) != 0;
			}
		} else	memset(lines[i], 0, line_width * 8);

		do_bright(i);
	}
	print_line(0);
	new_y += 56;
	if (new_y >= page_height) {
		print_line(1);
		new_y = 0;
		curr_x = -1;
		error_cnt = 0;
	}
#else
/* bitcmyk */
//fixme: does not work
	unsigned char buf[MAX_PAGE_WIDTH/2];

	for (i = 0; i < 56; i++) {
		if (fread(buf, 1, page_width / 2, stdin) != page_width / 2)
			return -1;
		k = 0;
		for (j = 0; j < page_width / 2; j++) {
			lines[i][k++] = (buf[j] & 0xf0) != 0;
			lines[i][k++] = (buf[j] & 0x0f) != 0;
		}
		do_bright(i);
	}
#endif
	return 0;
}

/* advance the paper */
static void lex_advance(int last)
{
	int dy;
    static unsigned char seqe[] = {
	0x31,0x18,0x00,0x00,0x00,0x00,0x00,0x00,	// move head
	0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00};	// 'not sure' command
    static unsigned char seq[] = {
	0x23,0x80,0x00,0x00,0x00,0x00,0x00,0x00};

	if (curr_y < 0)
		dy = new_y + Y_START;
	else	dy = new_y - curr_y;
	if (last) {
		int i;

		i = (old_x + 90) / 2;
		seqe[2] = i >> 8;
		seqe[3] = i;
		fwrite(seqe, 1, sizeof seqe, stdout);
		dy = MAX_PAGE_HEIGHT - curr_y;
		curr_y = -1;
	} else	curr_y = new_y;
	dy *= 2;
	seq[0] = last ? 0x22 : 0x23;
	seq[2] = dy >> 8;
	seq[3] = dy;
	fwrite(seq, 1, sizeof seq, stdout);
	fflush(stdout);
}

static void lex_init(void)
{
    static unsigned char seq[] = {
	0x1b,0x2a,0x6d,0x00,0x00,0x00,0x00,0x00,
	0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	// 'not sure' command
	0x30,0x80,0xe0,0x02,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	fwrite(seq, 1, sizeof seq, stdout);
	old_x = -499;		// magic!
	move_head = 1;
}

/* output a printer line */
static void lex_line(int new_x, int last)
{
	int i;
    static unsigned char seqm[] = {
	0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00};	// 'move head' command
    static unsigned char seq[] = {
	0x40,0x24,0x00,0x00,0x00,0x00,0x00,0x00,	// 'read color' command
	0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	i = pbuflen;
	seq[2] = i >> 8;			// pkts
	seq[3] = i;

	/* move the head if needed, and compute pkts2 */
	if (move_head) {
		int dx;

		dx = curr_x - old_x;
		if (dx < 456) {			// ??
			if (dx < 90) {
				dx = 90 - dx;
				seqm[1] = 0x18;
			} else if (dx < 162) {	// ??
				seqm[1] = 0x08;
				dx = 338 - dx;
			} else {
				seqm[1] = 0x00;
				dx = 456 - dx;
			}
			dx /= 2;
			seqm[2] = dx >> 8;
			seqm[3] = dx;
			fwrite(seqm, 1, sizeof seqm, stdout);
			i += 500;
		} else	i += dx;
	} else	i += 510;
	move_head = 1;
	if (!last && new_y - curr_y <= 56
	    && old_x > 0) {			// ?? test
		if (curr_x + pbuflen < new_x - 10)
			i = new_x - curr_x + 500;
		move_head = 0;
	}

	i = (i - 162) / 2;			// !!
	seq[4] = i >> 8;			// pkts2
	seq[5] = i;

	i = (curr_x + X_START) * 2;
	seq[6] = i >> 8;			// printer start
	seq[7] = i;

	old_x = curr_x + pbuflen;		// here is the head
	i = old_x - new_x;
	if (!move_head) {
		seq[9] = 0x10;			// unk
		if (i < -10)
			i = -10;
		i = (i + 100) / 2;
		seq[10] = i >> 8;		// reverse
		seq[11] = i;
	} else {
		seq[9] = 0x00;
		seq[10] = 0x00;
		seq[11] = 0x00;
	}
	i = pbuflen * 7;
	seq[19] = i >> 8;			// data length
	seq[20] = i;
	fwrite(seq, 1, sizeof seq, stdout);
	fwrite(print_buf, 1, i, stdout);
//	fflush(stdout);
}

/* build a printer line */
static void print_line(int last)
{
	int x, y, bx, endx, x0;

	/* reinitialize the printer if a new page */
	if (curr_y < 0)
		lex_init();

	/* search the starting x offset */
	y = 0;
	x = -1;
	if (!last) {
		for (x0 = 0; x0 < page_width; x0++) {
			for (y = 0; y < 56; y += 2) {
				if (lines[y][x0] != 0)
					break;
			}
			if (y < 56)
				break;
		}
		x0 += 9;
		for (x = 0; x < x0; x++) {
			for (y = 1; y < 56; y += 2) {
				if (lines[y][x] != 0)
					break;
			}
			if (y < 56)
				break;
		}
		x &= ~1;
	}

	/* output the previous line if any */
	if (curr_x >= 0)
		lex_line(x + xshift, last);
	curr_x = x + xshift;

	/* advance the paper */
	lex_advance(last);

	if (last)
		return;

	/* search the last x offset */
	for (x0 = page_width; --x0 > x + 9;) {	/* left jet */
		for (y = 0; y < 56; y += 2) {
			if (lines[y][x0] != 0)
				break;
		}
		if (y < 56)
			break;
	}
	x0 += 9;
	for (endx = page_width; --endx > x0;) {	/* right jet */
		for (y = 1; y < 56; y += 2) {
			if (lines[y][endx] != 0)
				break;
		}
		if (y < 56)
			break;
	}
	endx += 2;
	endx &= ~1;
//fixme: test
fprintf(stderr, "y:%d x:%d-%d\n",
	curr_y, curr_x, endx + xshift);

	/* build the printer line */
	bx = 0;
	for (; x < endx; x++) {
		int b, by;
		unsigned char bit, c;

		y = 0;
		b = 0;
		for (by = 0; by < 56 / 8; by++) {
			c = 0;
			for (bit = 0x80;
			     bit != 0;
			     bit >>= 1) {
				if (b) {
					if (x < endx - 9 && lines[y][x] != 0)
						c |= bit;
				} else {
					if (x >= 9 && lines[y][x - 9] != 0)
						c |= bit;
				}
				b = 1 - b;
				y++;
			}
			print_buf[bx++] = c;
		}
	}
	pbuflen = bx / 7;
}

/* main */
int main(int argc, char *argv[])
{
#ifndef USE_PBM
	page_width = PAGE_WIDTH;
	page_height = PAGE_HEIGHT;
	max_page_height = PAGE_HEIGHT / 56 * 56;
#endif
	for (;;) {
		if (get56() < 0)
			break;
	}
	return 0;
}
