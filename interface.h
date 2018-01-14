#ifndef INTERFACE_H
#define INTERFACE_H

enum tPosSign {
	TIC = 1,
	TAC
};

struct board {
	int x, y; // Top-Left corner coordinates
	int w, h; // Width and height
	int cells[9];	
};

void printBoard(struct board);
void setBoardPos(struct board *, int, int, enum tPosSign);
int editBoard(struct board *, int *, int *, int *);
int check_win(struct board);
int save_game(struct board, int);

#endif
