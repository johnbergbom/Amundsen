#ifndef _EVAL_
#define _EVAL_

#include "bitboards.h"

#define KRK 1
#define KQK 2
#define UNKNOWN_ENDGAME 3
#define PAWN_ENDGAME 4
#define KRPKR 5
#define KBNK 6
#define KPK 7

#define STRONG_PAWN 0
#define LITTLE_WEAK_PAWN 1
#define WEAK_PAWN 2
#define POSSIBLY_WEAK_PAWN 3

int eval_king_file(struct board *board, int file, int color);

//void eval_mobility(struct board *board, int *wval, int *bval);

/* This function returns a value for how good a position is. A high value
   means that the player at move is well off in the game. */
int eval(struct board *board, int alpha, int beta);

#endif    //_EVAL_
