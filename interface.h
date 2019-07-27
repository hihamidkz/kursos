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
	int filled_cells;
};

void printBoard(struct board);
void setBoardPos(struct board *, int, int, enum tPosSign);
int editBoard(struct board *b, int *pos, enum tPosSign val, int *line, int sockfd);
int check_win(struct board);
int exit_game(int sockfd);
int send_message(int sockfd, int *line, int my_number);

#endif
