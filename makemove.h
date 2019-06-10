#ifndef _MAKEMOVE_
#define _MAKEMOVE_

#include "misc.h"
#include "bitboards.h"

void make_nullmove(struct board *board);
void unmake_nullmove(struct board *board, char old_passant);
void add_piece(struct board *board, int color, char piecetype, int sq);
void remove_piece(struct board *board, int color, char piecetype, int sq);
void unmakemove(struct board *board, struct move *move, int depth);
void makemove(struct board *board, struct move *move, int depth);

#endif      //_MAKEMOVE_
