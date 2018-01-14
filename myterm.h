#ifndef MYTERM_H
#define MYTERM_H

enum colors {
	BLACK,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	PURPLE,
	CEAN,
	WHITE
};

int mt_clrscr();
int mt_gotoXY(int x, int y);
int mt_setfgcolor(enum colors color);
int mt_setbgcolor(enum colors color);
int mt_getscreensize(int *rows, int *cols);

#endif
