#ifndef MYREADKEY_H
#define MYREADKEY_H

enum keys {
	ESC = 1,
	F5, F6,
	KEYR, KEYT, KEYI,
	KEYS, KEYL, 
	KEYUP, KEYDOWN, KEYLEFT, KEYRIGHT,
	ENTER, TAB, KEYY, KEYN
};

int rk_readkey(enum keys *k);
int rk_mytermsave();
int rk_mytermrestore();
int rk_mytermregime(int regime, int vtime, int vmin, int echo, int sigint);

#endif
