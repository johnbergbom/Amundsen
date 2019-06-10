#ifndef _DISPLAY_
#define _DISPLAY_

#include "bitboards.h"

#define INVERT printf("\033[7m")
#define NORMAL printf("\033[0m")

char getPieceLetter(int piece, int color);
void showboard(struct board *board);
void showfenboard(struct board *board);

#ifndef AM_DEBUG
void showsettings(struct board *board, int engine_col, int started);
#else
void showsettings(struct board *board, int engine_col,
		  int started, int gamelog);
#endif

void showhelp(int);

#endif      //_DISPLAY_
