#include "myterm.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int mt_clrscr()
{
	write(0, "\E[H\E[J", 6);
	//printf("\E[H\E[J");
	
	return 0;
}

int mt_gotoXY(int x, int y)
{
	int rows, cols;
	
	mt_getscreensize(&rows, &cols);
	
	if (y > cols || x > rows || x < 0 || y < 0)
		return -1;
	
	char buf[10] = {0};
	sprintf(buf, "\E[%d;%dH", x, y);
	//printf("%s", buf);
	write(0, buf, sizeof(buf));
	
	return 0;
}

int mt_getscreensize(int *rows, int *cols)
{
	if (rows == NULL || cols == NULL)
		return -1;
		
	struct winsize wins;
	int fd = open("/dev/tty", O_RDWR);
	if (fd < 0)
		return -1;

	if (ioctl(fd, TIOCGWINSZ, &wins) < 0)
		return -1;
	
	*cols = wins.ws_col;
	*rows = wins.ws_row;
	close(fd);
	return 0;
}
	
int mt_setfgcolor(enum colors color)
{
	printf("\E[3%dm", color);
	
	return 0;
}

int mt_setbgcolor(enum colors color)
{
	printf("\E[4%dm", color);
	
	return 0;
}
