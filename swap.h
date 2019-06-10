#ifndef _SWAP_
#define _SWAP_

#include "bitboards.h"

bitboard attacks_behind(struct board *board, int target,
			bitboard remove_pieces, int dir);
bitboard get_attacks(struct board *board, int target);
int checking_move(struct board *board, struct move *move);
int swap(struct board *board, int target, int target_pieceval,
	 int *source_sq, int *source_piece);
int swap_avoid(struct board *board, int target, int target_pieceval,
	       int *source_sq, int *source_piece, bitboard avoid_first);

#endif      //_SWAP_
