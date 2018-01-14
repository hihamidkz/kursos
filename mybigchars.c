#include <stdio.h>
#include <unistd.h>

#include "mybigchars.h"

int bc_printA(char *str)
{
	if (str == NULL)
		return -1;
	
	printf("\E(0%s\E(B\n", str);
	return 0;
}

int bc_box(int x1, int y1, int x2, int y2)
{
	int i;
	int x = x1;
	
	mt_gotoXY(x1, y1);
	write(1, "\E(0l\E(B", 8);
	for (i = y1 + 1; i < y1 + y2; i++)
		write(1, "\E(0q\E(B", 8);
	write(1, "\E(0k\E(B", 8);
	for (i = x1; i < x1 + x2; i++) {
		x++;
		mt_gotoXY(x, y1);
		write(1, "\E(0x\E(B", 8);
	}
	mt_gotoXY(x1 + x2 + 1, y1);
	write(1, "\E(0m\E(B", 8);
	x = x1;
	for (i = x1; i < x1 + x2; i++) {
		x++;
		mt_gotoXY(x, y1 + y2);
		write(1, "\E(0x\E(B", 8);
	}
	mt_gotoXY(x1 + x2 + 1, y1 + 1);
	for (i = y1 + 1; i < y1 + y2; i++)
		write(1, "\E(0q\E(B", 8);
	write(1, "\E(0j\E(B", 8);
	
	return 0;
}

int bc_printbigchar(int *ch, int x, int y, enum colors color1, enum colors color2) {
	int i, j, s, z = 0;
	
	//mt_setfgcolor(color1);
	//mt_setbgcolor(color2);
	mt_gotoXY(x, y);
	
	for (i = 0; i < 8; i++) {
		z = 0;
		for (j = 7; j >= 0; j--) {
			mt_gotoXY(x + i, y + z * 2);
			z++;
			s = (ch[i / 4] >> ((i % 4) * 8 + j)) & 1;
			if (s == 1) {
				write(1, "\E(0aa\E(B", 8);
			} else {
				write(1, "\E(0  \E(B", 8);
		}
		}
	}
}

int bc_setbigcharpos (int *ch, int x, int y, int value) {
	if (ch == NULL)
		return -1;
	
	if (value > 1 || value < 0)
		return -1;
	
	if ((x / 4) % 2 == 0)
		ch[0] = (value << ((x % 4) * 8 + y)) | ch[0];
	else
		ch[1] = (value << ((x % 4) * 8 + y)) | ch[1];
	
	return 0;
}

int bc_getbigcharpos (int *ch, int x, int y, int *value) {
	if (ch == NULL || value == NULL)
		return -1;
	
	if ((x / 4) % 2 == 0)
		*value = ch[0] >> ((x % 4) * 8 + y) & 1;
	else
		*value = ch[1] >> ((x % 4) * 8 + y) & 1;
	
	return 0;
}

int bc_bigcharwrite (char *fd, int *ch, int count) {
	if (ch == NULL || fd == NULL)
		return -1;
		
	FILE *file = fopen(fd, "wb");
	fwrite(ch, sizeof(int), count, file);
	fclose(file);
	
	return 0;
}

int bc_bigcharread (char *fd, int *ch, int numofcount, int *count) {
	if (ch == NULL || fd == NULL)
		return -1;
	
	FILE *file = fopen(fd, "rb");
	fread(ch, sizeof(int), sizeof(ch) / sizeof(int), file);
	*count = sizeof(ch) / sizeof(int);
	fclose(file);
	
	return 0;
}
