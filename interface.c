#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "myterm.h"
#include "mybigchars.h"
#include "interface.h"
#include "myreadkey.h"

int check_win(struct board b)
{
	if (b.cells[0] == b.cells[1] && b.cells[0] == b.cells[2] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[0] == b.cells[3] && b.cells[0] == b.cells[6] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[0] == b.cells[4] && b.cells[0] == b.cells[8] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[1] == b.cells[4] && b.cells[1] == b.cells[7] && b.cells[1] != 0)
		return b.cells[1];
	else if (b.cells[2] == b.cells[5] && b.cells[2] == b.cells[8] && b.cells[2] != 0)
		return b.cells[2];
	else if (b.cells[2] == b.cells[4] && b.cells[2] == b.cells[6] && b.cells[2] != 0)
		return b.cells[2];
	else if (b.cells[3] == b.cells[4] && b.cells[3] == b.cells[5] && b.cells[3] != 0)
		return b.cells[3];
	else if (b.cells[6] == b.cells[7] && b.cells[6] == b.cells[8] && b.cells[6] != 0)
		return b.cells[6];
		
	return -1;
}

int save_game(struct board gameboard, int move)
{
	time_t timer = time(NULL);
	char *time = ctime(&timer);
	char *temp;
	strcpy(temp, time);
	
	FILE *sf = fopen(strcat(time, ".sf"), "wb");
	FILE *sg = fopen("savegames.txt", "a");
	
	fwrite(&gameboard, sizeof(gameboard), 1, sf);
	fwrite(&move, sizeof(int), 1, sf);
	fprintf(sg, "%s", temp);
	fclose(sf);
}

void printBoard(struct board gameboard)
{
	mt_clrscr();
	bc_box(gameboard.x, gameboard.y, gameboard.h, gameboard.w);
	
	int x, y;
	int ch[2];
	
	for (x = 0; x < 3; x++) {
		for (y = 0; y < 3; y++) {
			int idx = x * 3 + y;
			if (gameboard.cells[idx] == 0)
				continue;
			else if (gameboard.cells[idx] == TIC) {
				ch[0] = 405029505;
				ch[1] = 2168595480;
				bc_printbigchar(ch, gameboard.x + x * 9 + 1, gameboard.y + y * 17 + 1, 0, 0);
			} else if (gameboard.cells[idx] == TAC) {
				ch[0] = 2172748158;
				ch[1] = 2122416513;
				bc_printbigchar(ch, gameboard.x + x * 9 + 1, gameboard.y + y * 17 + 1, 0, 0);
			}
		}
	}
}

void setBoardPos(struct board *b, int x, int y, enum tPosSign val)
{
	int idx = x * 3 + y;
	int ch[2];
	
	b->cells[idx] = val;
	
	if (val == TAC) {
		ch[0] = 2172748158;
		ch[1] = 2122416513;
	} else {
		ch[0] = 405029505;
		ch[1] = 2168595480;
	}
	bc_printbigchar(ch, b->x + x * 9 + 1, b->y + y * 17 + 1, 0, 0);
}

void getBoardPos(struct board b, int x, int y, enum tPosSign *val)
{
	int idx = x * 3 + y;
	
	*val = b.cells[idx];
}

int editBoard(struct board *b, int *x, int *y, int *line)
{
	int set = -1;
	
	enum keys k;
	enum tPosSign val;
	rk_mytermsave();
	rk_mytermregime(0, 0, 1, 0, 1);
	
	mt_gotoXY(*line, 1);
	printf("Please, press F5 or F6 to make a move, TAB to switch to chat, ESC to exit.\n");
	while (1) {
		rk_readkey(&k);
		if (k == F5) {
			val = TIC;
			break;
		}
		else if (k == F6) {
			val = TAC;
			break;
		} else if (k == TAB) {
			rk_mytermrestore();
			return 1;
		} else if (k == ESC) {
			rk_mytermrestore();
			return 2; 
		} else {
			mt_gotoXY(*line, 1);
			write(1, "\E[M", 3);
			printf("Please, press F5 or F6 to make a move, TAB to switch to chat, ESC to exit.\n");
		}
	}
	rk_mytermrestore();
	
	srand(time(NULL));
	
	while (set == -1) {
		*x = rand() % 3;
		*y = rand() % 3;
		int idx = *x * 3 + *y;
		if (b->cells[idx] == 0) {
			b->cells[idx] = val;
			setBoardPos(b, *x, *y, val);
			set = 1;
			break;
		}
	}
	
	*line += 1;
	return 0;
}

int chat(char *buf)
{
	rk_mytermsave();
	rk_mytermregime(0, 0, 1, 0, 1);
}
