#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "myreadkey.h"

struct termios tsaved;

int rk_readkey(enum keys *k)
{
	if (k == NULL)
		return -1;

	char buf[10] = {0};
	read(0, buf, 10);
	
	switch (buf[0]) {
		case '\n':
		*k = ENTER;
		break;
		case '\t':
		*k = TAB;
		break;
		case 27:
		switch (buf[1]) {
			case 0:
			*k = ESC;
			return 0;
			default:
			switch (buf[2]) {
				case '1':
					switch (buf[3]) {
						case '5':
						*k = F5;
						return 0;
						case '7':
						*k = F6;
						return 0;
						default:
						break;
					}
				break;
				case 'A':
				*k = KEYUP;
				return 0;
				case 'D':
				*k = KEYLEFT;
				return 0;
				case 'B':
				*k = KEYDOWN;
				return 0;
				case 'C':
				*k = KEYRIGHT;
				return 0;
				default:
				break;
			}
		}
		break;
		case 's':
		*k = KEYS;
		break;
		case 'l':
		*k = KEYL;
		break;
		case 'i':
		*k = KEYI;
		break;
		case 'n':
		*k = KEYN;
		break;
		case 'y':
		*k = KEYY;
		break;
		default:
		*k = 0;
		break;
	}
	return 0;
}

int rk_mytermsave()
{	
	//if (tsaved == NULL)
		//return -1;
	
	if (tcgetattr(0, &tsaved) == 0)
		return 0;
	
	return -1;
}

int rk_mytermrestore()
{
	//if (tsaved == NULL)
		//return -1;
		
	const struct termios tnew = tsaved;
	
	if (tcsetattr(0, TCSADRAIN, &tnew) == 0)
		return 0;
	
	return -1;
}

int rk_mytermregime(int regime, int vtime, int vmin, int echo, int sigint) 
{
	struct termios tnew;
	
	tcgetattr(0, &tnew);
	if (regime == 1) {
		tnew.c_lflag |= ICANON;
		tcsetattr(0, TCSADRAIN, &tnew);
		return 0;
	}
	
	tnew.c_lflag &= (~ICANON);
	if (echo == 0)
		tnew.c_lflag &= (~ECHO);
	else
		tnew.c_lflag |= ECHO;
	
	if (sigint == 0)
		tnew.c_lflag &= (~ISIG);
	else
		tnew.c_lflag |= ISIG;
	
	tnew.c_cc[VTIME] = vtime;
	tnew.c_cc[VMIN] = vmin;
	tcsetattr(0, TCSADRAIN, &tnew);
	
	return 0;
}



