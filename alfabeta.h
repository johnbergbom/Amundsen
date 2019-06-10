#ifndef _ALFABETA_
#define _ALFABETA_

#include "misc.h"
#include "bitboards.h"

#define INFTY 1000000
#define KINGTAKEN 500000
#define MAXNODE 1
#define MINNODE 2

int maxval(int a, int b);
int minval(int a, int b);

/*-----------------------------------------------------
 | The following constants are used in alphabeta() to |
 | determine which moves should be pruned as futile.  |
 -----------------------------------------------------*/
#define FUTILITY_PRUNING_MARGIN 400
#define EXT_FUTILITY_PRUNING_MARGIN 700
#define RAZOR_MARGIN 1000

int compare_boards(struct board *board1, struct board *board2);

int perft(struct board *board, int alpha, int beta, int depth, int ply);

int quiescence(struct board *board, int alpha, int beta, int ply, int depth);

int material_gain(struct board *board, struct move *move);

int alphabeta(struct board *board, int alpha, int beta, int depth, int null_ok,
	      int ply, int threat, int white_checks, int black_checks, int pv,
	      struct move *best_move);

#endif       //_ALFABETA_

