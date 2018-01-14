#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "interface.h"
#include "mybigchars.h"

int main()
{
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	
	gameboard.x = 1;
	gameboard.y = 1;
	gameboard.w = 51;
	gameboard.h = 26;
	
	printBoard(gameboard);
	
	int x, y;
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	editBoard(&gameboard, &x, &y);
	return 0;
}
