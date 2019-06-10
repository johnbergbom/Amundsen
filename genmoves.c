/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "genmoves.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bitboards.h"
#include "misc.h"
#include "parse.h"
#include "hash.h"
#include "makemove.h"
#include "alfabeta.h"

extern bitboard square[64];
extern bitboard pawn_start[2];
extern bitboard pawn_2nd_row[2];
extern bitboard pawn_passantrow[2];
extern struct attack attack;
extern bitboard pawn_lastrow[2];
extern int rotate0to90[64];
extern int rotate0toNE[64];
extern int ones[9];
extern int diagNE_start[64];
extern int diagNE_length[64];
extern int rotate0toNW[64];
extern int diagNW_start[64];
extern int diagNW_length[64];
extern int pieceval[6];
extern int historymovestable[2][64][64];
extern int direction[64][64];
extern int pawn_forward[2][64];
//extern int failed_cutoff_count[2][16][64];
//extern int success_cutoff_count[2][16][64];

bitboard generate_pawnmoves(struct board *board, int boardpos) {
  int64 targets;
  int forward_sq;

  forward_sq = pawn_forward[(int)Color][boardpos];
  targets = square[forward_sq] & ~(board->all_pieces[(int)Color]
				   | board->all_pieces[Oppcolor]);
  if (targets && (square[boardpos] & pawn_start[(int)Color])) {
    forward_sq = pawn_forward[(int)Color][forward_sq];
    targets |= square[forward_sq] & ~(board->all_pieces[(int)Color]
				      | board->all_pieces[Oppcolor]);
  }

  /* We also need to add the capture moves. */
  targets |= (attack.pawn[(int)Color][boardpos]
	      & board->all_pieces[(int)Oppcolor]);

  /* We also need to add the passant moves. */
  if (board->passant)
    targets |= (attack.pawn[(int)Color][boardpos]
		& square[(int)board->passant]);

  return targets;
}

int get_pawnmove_movetype(struct board *board, char boardpos, char target) {
  int type;

  /* Find out what type of move it was. */
  if (square[target] & board->all_pieces[Oppcolor])
    type = CAPTURE_MOVE;
  else if (attack.pawn[(int)Color][boardpos] & square[target]) {
    /* If the move is an attack to a square with no enemy piece
       occupying it. */
    type = CAPTURE_MOVE | PASSANT_MOVE;
  } else
    type = NORMAL_MOVE;
    
  /* If the pawn goes to the last row, then we set the move to be a
     queen promotion. Notice that we don't take care of different
     kinds of promotions. */
  if (square[target] & pawn_lastrow[(int)Color])
    type |= QUEEN_PROMOTION_MOVE;
  return type;
}

bitboard generate_kingmoves(struct board *board, int boardpos) {
  int64 targets;

  targets = attack.king[boardpos] & (~board->all_pieces[(int)Color]);

  /* Check castling. */
  if (Color == WHITE) {
    if (board->castling_status[WHITE] & SHORT_CASTLING_OK)
      /* Check if the squares between the king and the rook are empty. */
      if (((board->all_pieces[WHITE] | board->all_pieces[BLACK]) &
	   (square[61] | square[62])) == 0) {
	/* Make a final check to see if the king's square is in check,
	   or if the destination square is in check, or if the square in
	   between is in check. */
	if (!in_check(board)) {
	  board->piece[(int)Color][KING] = square[61];
	  if (!in_check(board)) {
	    board->piece[(int)Color][KING] = square[62];
	    if (!in_check(board))
	      targets = targets | square[62];
	  }
	  board->piece[(int)Color][KING] = square[60];
	}
      }
    if (board->castling_status[WHITE] & LONG_CASTLING_OK)
      /* Check if the squares between the king and the rook are empty. */
      if (((board->all_pieces[WHITE] | board->all_pieces[BLACK]) &
	   (square[57] | square[58] | square[59])) == 0) {
	/* Make a final check to see if the king's square is in check,
	   or if the destination square is in check, or if the square in
	   between is in check. */
	if (!in_check(board)) {
	  board->piece[(int)Color][KING] = square[59];
	  if (!in_check(board)) {
	    board->piece[(int)Color][KING] = square[58];
	    if (!in_check(board))
	      targets = targets | square[58];
	  }
	  board->piece[(int)Color][KING] = square[60];
	}
      }
  } else {    //color == BLACK
    if (board->castling_status[BLACK] & SHORT_CASTLING_OK)
      /* Check if the squares between the king and the rook are empty. */
      if (((board->all_pieces[WHITE] | board->all_pieces[BLACK]) &
	   (square[5] | square[6])) == 0) {
	/* Make a final check to see if the king's square is in check,
	   or if the destination square is in check, or if the square in
	   between is in check. */
	if (!in_check(board)) {
	  board->piece[(int)Color][KING] = square[5];
	  if (!in_check(board)) {
	    board->piece[(int)Color][KING] = square[6];
	    if (!in_check(board))
	      targets = targets | square[6];
	  }
	  board->piece[(int)Color][KING] = square[4];
	}
      }
    if (board->castling_status[BLACK] & LONG_CASTLING_OK)
      /* Check if the squares between the king and the rook are empty. */
      if (((board->all_pieces[WHITE] | board->all_pieces[BLACK]) &
	   (square[1] | square[2] | square[3])) == 0) {
	/* Make a final check to see if the king's square is in check,
	   or if the destination square is in check, or if the square in
	   between is in check. */
	if (!in_check(board)) {
	  board->piece[(int)Color][KING] = square[3];
	  if (!in_check(board)) {
	    board->piece[(int)Color][KING] = square[2];
	    if (!in_check(board))
	      targets = targets | square[2];
	  }
	  board->piece[(int)Color][KING] = square[4];
	}
      }
  }

  return targets;
}

int get_kingmove_movetype(struct board *board, char target) {
  /* Find out what type of move it was. */
  if (square[target] & board->all_pieces[Oppcolor])
    return CAPTURE_MOVE;
  else if ((Color == WHITE && (board->piece[(int)Color][KING] & square[60])
	    && ((target == 62) || (target == 58)))
	   || (Color == BLACK && (board->piece[(int)Color][KING] & square[4])
	       && ((target == 6) || (target == 2))))
    return CASTLING_MOVE;
  else
    return NORMAL_MOVE;
}

bitboard generate_knight_moves(struct board *board, int boardpos) {
  return attack.knight[boardpos] & (~board->all_pieces[(int)Color]);
}

/* This function generates moves that go horizontally along the board.
   It is needed for generating queen and rook moves. */
bitboard generate_horizontal_moves(struct board *board, int boardpos) {
  int occupancy;

  occupancy = ((board->all_pieces[WHITE]
		| board->all_pieces[BLACK]) >> ((boardpos/8)*8)) & 255;
  return attack.hslider[boardpos][occupancy]
    & (~board->all_pieces[(int)Color]);
}

/* This function returns the movetype of all pieces other than pawns
   and king. */
int get_othermove_movetype(struct board *board, char target) {
  /* Find out what type of move it was. */
  if (square[target] & board->all_pieces[Oppcolor])
    return CAPTURE_MOVE;
  else
    return NORMAL_MOVE;
}

/* This function generates moves that go vertically along the board.
   This function is needed for generating queen and rook moves. */
bitboard generate_vertical_moves(struct board *board, int boardpos) {
  int occupancy;

  /* We calculate the vertical moves (files) by using the rotated
     bitboard board.rot90_pieces, and getting the occupancy number of
     the file in question by reading off the occupancy of the rank in
     this rotated bitboard. Then we switch back to "unrotated" before
     we add the move to the movelist. */
  occupancy = ((board->rot90_pieces[WHITE]
		| board->rot90_pieces[BLACK])
	       >> (rotate0to90[boardpos]/8)*8) & 255;

  return attack.vslider[boardpos][occupancy]
    & (~board->all_pieces[(int)Color]);
}

/* This function generates moves that go diagonally along the board
   in the northeast direction (the a1-h8 diagonal). This function is
   needed for generating queen and bishop moves. */
bitboard generate_NEdiag_moves(struct board *board , int boardpos) {
  int occupancy;

  occupancy = (board->rotNE_pieces[WHITE] | board->rotNE_pieces[BLACK])
    >> (rotate0toNE[diagNE_start[boardpos]]) & ones[diagNE_length[boardpos]];
  return attack.sliderNE[boardpos][occupancy]
    & (~board->all_pieces[(int)Color]);
}

/* This function generates moves that go diagonally along the board
   in the northwest direction (the h1-a8 diagonal). This function is
   needed for generating queen and bishop moves. */
bitboard generate_NWdiag_moves(struct board *board, int boardpos) {
  int occupancy;

  occupancy = (board->rotNW_pieces[WHITE] | board->rotNW_pieces[BLACK])
    >> (rotate0toNW[diagNW_start[boardpos]]) & ones[diagNW_length[boardpos]];
  return attack.sliderNW[boardpos][occupancy]
    & (~board->all_pieces[(int)Color]);
}

/*--------------------------------------------------------------------------
 | This function returns 1 if there are moves left, and 0 if there are no  |
 | moves left to generate. Moves are generated in the following order:     |
 | 1. Hash moves                                                           |
 | 2. If the king is in check the call get_check_evasion_moves()           |
 | else:                                                                   |
 | (generate captures using swap())                                        |
 | 3. Profitable captures                                                  |
 | 4. Killer moves                                                         |
 | (full move generation)                                                  |
 | 5. History moves                                                        |
 | 6. Promotion moves                                                      |
 | 7. Pawn pushes                                                          |
 | 8. Equal captures                                                       |
 | 9. Non captures                                                         |
 | 10. Bad captures                                                        |
 --------------------------------------------------------------------------*/
int get_next_move(struct board *board , struct moves *moves, int *movables,
		  int *hash_flag, struct move *refmove,
		  struct move *transpmove, struct killers *killers,
		  struct move *returnmove, int *order,
		  struct swap_entry *swap_list, int *swap_count,
		  int *killers_processed, struct hist_entry *hist_list,
		  int own_king_check, bitboard *spec_moves, int depth) {
  int i, j, kill_counter, val;
  bitboard target, targets;
  int max_swap_val, swap_number = 0, max_hist_val, hist_number = 0, max_val;
  bitboard pieces, pos;
  int s_square, s_piece, t_square;
  //zzz

  /*-----------------------------------
   | Extract the hashmove(s), if any. |
   -----------------------------------*/
  if (*order == 0) {
    if (*hash_flag & REF_MOVE) {
      /* If we have a hashmove we make sure that the hashmove is actually
	 a legal move (in case two different boards happen to hash to the
	 same key then the move is probably invalid). */
      if (generate_move(board,(int)refmove->from_square)
	  & square[refmove->to_square]) {
	returnmove->from_square = refmove->from_square;
	returnmove->to_square = refmove->to_square;
	returnmove->piece = board->boardarr[refmove->from_square];
	returnmove->type = get_movetype(board,returnmove);
	(*order)++;
	return 1;
      }
    }
    (*order)++;
  }

  if (*order == 1) {
    if ((*hash_flag & TRANSP_MOVE) && (*hash_flag & REF_MOVE)
	&& refmove->from_square == transpmove->from_square
	&& refmove->to_square == transpmove->to_square) {
      //do nothing, this hashmove has already been tried.
    } else if (*hash_flag & TRANSP_MOVE) {
      /* If we have a hashmove we make sure that the hashmove is actually
	 a legal move (in case two different boards happen to hash to the
	 same key the move is probably invalid). */
      if (generate_move(board,(int)transpmove->from_square)
	  & square[transpmove->to_square]) {
	returnmove->from_square = transpmove->from_square;
	returnmove->to_square = transpmove->to_square;
	returnmove->piece = board->boardarr[transpmove->from_square];
	returnmove->type = get_movetype(board,returnmove);
	(*order)++;
	return 1;
      }
    }
    (*order)++;
  }

  if (own_king_check) {
    /* Make sure that the check evasion function doesn't return any move
       that has already been tried as a hash move. */
    *order -= 2;
    do {
      val = get_check_evasion_moves(board,returnmove,swap_list,
				    order,swap_count,spec_moves);
    } while (val &&
	     (((*hash_flag & REF_MOVE)
	      && refmove->from_square == returnmove->from_square
	      && refmove->to_square == returnmove->to_square)
	     ||
	      ((*hash_flag & TRANSP_MOVE)
	      && transpmove->from_square == returnmove->from_square
	      && transpmove->to_square == returnmove->to_square)));
    *order += 2;
    return val;
  }

  /*--------------------------------------------------------------------------
   | Instead of generating all the moves at once using the generate_moves    |
   | function, we go through all the enemy pieces and call swap for each     |
   | of them. Don't check if the king can be taken (=>start counting from 4) |
   | since in the alphabeta function we make sure that the opponent king     |
   | can't be taken.                                                         |
   --------------------------------------------------------------------------*/
  if (*order == 2) {
    for (i = 4; i >= 0; i--) {
      pieces = board->piece[Oppcolor][i];
      while (pieces != 0) {
	pos = getlsb(pieces);
	/* Initialize the old from squares to zero, and in case any hash
	   moves have been made to this target square put it in the old
	   from squares list. */
	swap_list[*swap_count].old_fsquares = 0;
	if ((*hash_flag & REF_MOVE) && (square[refmove->to_square] == pos)) {
	  swap_list[*swap_count].old_fsquares |= square[refmove->from_square];
	}
	if ((*hash_flag & TRANSP_MOVE) && (square[transpmove->to_square] == pos)) {
	  swap_list[*swap_count].old_fsquares |= square[transpmove->from_square];
	}

	val = swap_avoid(board,get_first_bitpos(pos),pieceval[i],&s_square,&s_piece,swap_list[*swap_count].old_fsquares);
	if (val != -INFTY) {
	  swap_list[*swap_count].fsquare = square[s_square];
	  swap_list[*swap_count].tsquare = pos;
	  swap_list[*swap_count].piece = s_piece;
	  swap_list[*swap_count].swap_val = val;
	  (*swap_count)++;
	} //else {
	  //not necessary to set fsquare to zero since
	  //swap_count isn't updated
	  //swap_list[*swap_count].fsquare = 0;
	//}
	pieces &= ~pos;
      }
    }
    (*order)++;
  }

  /*-----------------------------------------------------------------------
   | Try moves having swap value greater than zero (profitable captures). |
   | In the higher part of the tree order the equal valued swap moves     |
   | according to the history value.                                      |
   -----------------------------------------------------------------------*/
  if (*order == 3) {
    max_swap_val = -INFTY;
    if (depth > 3) {
      max_hist_val = -INFTY;

      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare) {
	  if (swap_list[i].swap_val > max_swap_val) {
	    max_swap_val = swap_list[i].swap_val;
	    swap_number = i;
	  } else if (swap_list[i].swap_val == max_swap_val
		     && (val = historymovestable
			 [(int)Color]
			 [get_first_bitpos(swap_list[swap_number].fsquare)]
			 [get_first_bitpos(swap_list[swap_number].tsquare)])
		     > max_hist_val) {
	    max_swap_val = swap_list[i].swap_val;
	    swap_number = i;
	    max_hist_val = val;
	  }
	}
      }
    } else {
      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare && swap_list[i].swap_val > max_swap_val) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	}
      }
    }

    /* If we found a move with value > 0 then return it. */
    if (max_swap_val > 0) {
      returnmove->piece = (char) swap_list[swap_number].piece;
      returnmove->from_square = get_first_bitpos(swap_list[swap_number].fsquare);
      returnmove->to_square = get_first_bitpos(swap_list[swap_number].tsquare);
      returnmove->type = get_movetype(board,returnmove);
      swap_list[swap_number].old_fsquares |= square[returnmove->from_square];

      /* Try to see if we can put another move into this slot. */
      val = swap_avoid(board,(int)returnmove->to_square,pieceval[board->boardarr[returnmove->to_square]],&s_square,&s_piece,swap_list[swap_number].old_fsquares);
      if (val != -INFTY) {
	swap_list[swap_number].fsquare = square[s_square];
	swap_list[swap_number].piece = s_piece;
	swap_list[swap_number].swap_val = val;
      } else {
	swap_list[swap_number].fsquare = 0;
      }
      return 1;
    }

    (*order)++;
  }

  /*---------------------------------------------------------------
   | Promotion moves without capture (the promotion captures will |
   | have been taken care of already, since they will always be   |
   | considered good captures atomatically).                      |
   ---------------------------------------------------------------*/
  if (*order == 4) {
    *spec_moves = board->piece[Color][PAWN] & pawn_start[Oppcolor];
    (*order)++;
  }
  if (*order == 5) {
    int forward_sq;
    while (*spec_moves) {
      s_square = get_first_bitpos(*spec_moves);
      *spec_moves &= ~(square[s_square]);
      forward_sq = pawn_forward[Color][s_square];
      target = square[forward_sq]
	& ~(board->all_pieces[Color] | board->all_pieces[Oppcolor]);
      if ((*hash_flag & REF_MOVE) && refmove->from_square == s_square
	  && refmove->to_square == get_first_bitpos(target))
	continue;
      if ((*hash_flag & TRANSP_MOVE) && transpmove->from_square == s_square
	  && transpmove->to_square == get_first_bitpos(target))
	continue;
      if (target) {
	returnmove->from_square = (char) s_square;
	returnmove->to_square = (char) get_first_bitpos(target);
	returnmove->piece = PAWN;
	returnmove->type = get_movetype(board,returnmove);
	
	//moves[i].targets &= ~(square[returnmove->to_square]);
	if (*spec_moves == 0)
	  (*order)++;
	return 1;
      }
    }
    (*order)++;
  }
  /*if (*order == 4) {
    *spec_moves = board->piece[Color][PAWN] & pawn_start[Oppcolor];
    (*order) += 2;
    }*/

  /*----------------
   | Killer moves. |
   ----------------*/
  if (*order == 6) {
    for (kill_counter = *killers_processed; kill_counter < NBR_KILLERS; kill_counter++) {
      int skip = 0;
      /* First check if this killer already has been tried, and if so skip
	 this one and continue with the next one. This is done to cover
	 the case where two identical killers are in the killer list.
	 Kind of ugly, but there is no other way to check it. */
      for (i = 0; i < kill_counter; i++) {
	if (killers[i].fsquare & killers[kill_counter].fsquare
	    && killers[i].tsquare & killers[kill_counter].tsquare) {
	  skip = 1;
	}
      }

      /* Promotion moves have already been tried, so don't try those again. */
      if ((killers[kill_counter].fsquare & pawn_start[Oppcolor])
	  & board->piece[Color][PAWN])
	skip = 1;

      if (!skip) {
	int generate = 1;
	/* Go through the swap list and see if the move is there. */
	for (i = ((*swap_count)-1); i >= 0; i--) {
	  if (swap_list[i].tsquare == killers[kill_counter].tsquare) {
	    if (swap_list[i].fsquare == killers[kill_counter].fsquare) {
	      returnmove->piece = (char) swap_list[i].piece;
	      returnmove->from_square = get_first_bitpos(swap_list[i].fsquare);
	      returnmove->to_square = get_first_bitpos(swap_list[i].tsquare);
	      returnmove->type = get_movetype(board,returnmove);
	      swap_list[i].old_fsquares |= square[returnmove->from_square];
	      
	      /* Try to see if we can put another move into this slot. */
	      val = swap_avoid(board,returnmove->to_square,pieceval[board->boardarr[returnmove->to_square]],&s_square,&s_piece,swap_list[i].old_fsquares);
	      if (val != -INFTY) {
		swap_list[i].fsquare = square[s_square];
		swap_list[i].piece = s_piece;
		swap_list[i].swap_val = val;
	      } else {
		swap_list[i].fsquare = 0;
	      }
	      (*killers_processed)++;
	      return 1;
	    } else if (swap_list[i].old_fsquares & killers[kill_counter].fsquare) {
	      /* The killer move is in the swap list and has already been
		 tried, so we don't need to bother searching any more. */
	      generate = 0;
	      break;
	    } else {
	      /* Check if it is a real move. */
	      if (generate_move(board,get_first_bitpos(killers[kill_counter].fsquare)) & killers[kill_counter].tsquare) {
		/* Since the move wasn't in the old_fsquares list, we know that
		   it hasn't been tried before (possible hashmoves have been
		   tried already, and they are in the old_fsquares list. So
		   we don't need to check if it's a hashmove. */
		returnmove->from_square = get_first_bitpos(killers[kill_counter].fsquare);
		returnmove->to_square = get_first_bitpos(killers[kill_counter].tsquare);
		returnmove->piece = board->boardarr[returnmove->from_square];
		returnmove->type = get_movetype(board,returnmove);
		swap_list[i].old_fsquares |= square[returnmove->from_square];
		(*killers_processed)++;
		return 1;
	      } else {
		generate = 0;
	      }
	    }
	  }
	}
	
	/* If the move wasn't in the swap list, then check if it is a
	   real move. */
	if (generate) {
	  /* If we get here we don't know if it was a hash move or not, so
	     we need to check that. */
	  if (((*hash_flag & REF_MOVE) && (square[refmove->from_square] == killers[kill_counter].fsquare) && (square[refmove->to_square] == killers[kill_counter].tsquare)) || ((*hash_flag & TRANSP_MOVE) && (square[transpmove->from_square] == killers[kill_counter].fsquare) && (square[transpmove->to_square] == killers[kill_counter].tsquare))) {
	    //do nothing, this move was a hash move that has already been tried
	  } else if (generate_move(board,get_first_bitpos(killers[kill_counter].fsquare)) & killers[kill_counter].tsquare) {
	    returnmove->from_square = get_first_bitpos(killers[kill_counter].fsquare);
	    returnmove->to_square = get_first_bitpos(killers[kill_counter].tsquare);
	    returnmove->piece = board->boardarr[returnmove->from_square];
	    returnmove->type = get_movetype(board,returnmove);
	    (*killers_processed)++;
	    return 1;
	  }
	}
      }
      (*killers_processed)++;
    }
    (*order)++;
  }

  /*--------------------------------------------------------------
   | Now we have tried all good moves, and a cutoff still hasn't |
   | occurred, so now we need to do a full move generation.      |
   --------------------------------------------------------------*/
  if (*order == 7) {
    generate_moves(board,moves,movables);

    /* From this full move list we need to remove the hash moves that have
       already been tried. The tried moves from the swap list don't need
       to be removed since all capture moves are taken from the swap list,
       and the non capture moves are taken from the moves-movelist. */
    for (i = 0; i < *movables; i++) {
      /* Remove the hashmoves. */
      if ((*hash_flag & REF_MOVE)
	  && square[refmove->from_square] & moves[i].source)
	moves[i].targets &= ~(square[refmove->to_square]);
      if ((*hash_flag & TRANSP_MOVE)
	  && square[transpmove->from_square] & moves[i].source)
	moves[i].targets &= ~(square[transpmove->to_square]);

      /* Remove the killer moves. */
      for (j = 0; j < NBR_KILLERS; j++) {
	if (killers[j].fsquare & moves[i].source) {
	  moves[i].targets &= ~(killers[j].tsquare);
	}
      }

      /* Remove the promotion moves. */
      if (moves[i].piece == PAWN && moves[i].source & pawn_start[Oppcolor])
	moves[i].targets = 0;
    }

    (*order)++;
  }

  /*---------------------------------------------------------------
   | Extract the NBR_HISTMOVES best history moves and put them in |
   | the history list.                                            |
   ---------------------------------------------------------------*/
  if (*order == 8) {
    int worst_pos, worst_val;
    /* Start by zeroing the list. */
    for (i = 0; i < NBR_HISTMOVES; i++) {
      hist_list[i].hist_val = -1;
      hist_list[i].untried = 1;
    }
    worst_pos = 0;
    worst_val = -1;

    for (i = 0; i < *movables; i++) {
      pieces = moves[i].targets;
      s_square = get_first_bitpos(moves[i].source);
      while (pieces != 0) {
	target = getlsb(pieces);
	val = historymovestable[(int)Color][s_square][get_first_bitpos(target)];
	/* If the current move is better than the worst move in the
	   history list, then replace the worst move in the list and
	   find the new worst move. */
	if (val > worst_val) {
	  hist_list[worst_pos].fsquare = square[s_square];
	  hist_list[worst_pos].tsquare = target;
	  hist_list[worst_pos].hist_val = val;
	  //We also save movelist pos to make it easy to find later
	  hist_list[worst_pos].movelist_pos = i;
	  worst_val = INFTY;
	  for (j = 0; j < NBR_HISTMOVES; j++) {
	    if (hist_list[j].hist_val < worst_val) {
	      worst_val = hist_list[j].hist_val;
	      worst_pos = j;
	    }
	  }
	}
	pieces &= ~target;
      }
    }
    (*order)++;
  }

  /*-------------------------
   | Try the history moves. |
   -------------------------*/
  while (*order == 9) {
    max_hist_val = -INFTY;
    /* Extract the best history move from the history list. */
    for (i = 0; i < NBR_HISTMOVES; i++) {
      if (hist_list[i].untried && hist_list[i].hist_val > max_hist_val) {
	max_hist_val = hist_list[i].hist_val;
	hist_number = i;
      }
    }
    /* Don't try history moves that have zero cutoffs.*/
    if (max_hist_val > 0) {
      if ((board->all_pieces[Oppcolor] & hist_list[hist_number].tsquare)==0) {
	/* Ok, not a capture move so then we don't need to care about
	   updating the swap list. */
	returnmove->from_square = get_first_bitpos(hist_list[hist_number].fsquare);
	returnmove->to_square = get_first_bitpos(hist_list[hist_number].tsquare);
	returnmove->piece = (char) moves[hist_list[hist_number].movelist_pos].piece;
	returnmove->type = get_movetype(board,returnmove);
	moves[hist_list[hist_number].movelist_pos].targets
	  &= ~(hist_list[hist_number].tsquare); //remove from movelist
	hist_list[hist_number].untried = 0;
	return 1;
      } else {
	/* Ok, a capture move so then we don't need to care about
	   updating the move list, but the swap list needs to be updated. */
	for (i = ((*swap_count)-1); i >= 0; i--) {
	  if (swap_list[i].tsquare == hist_list[hist_number].tsquare) {
	    if (swap_list[i].fsquare == hist_list[hist_number].fsquare) {
	      returnmove->piece = (char) swap_list[i].piece;
	      returnmove->from_square = get_first_bitpos(swap_list[i].fsquare);
	      returnmove->to_square = get_first_bitpos(swap_list[i].tsquare);
	      returnmove->type = get_movetype(board,returnmove);
	      swap_list[i].old_fsquares |= square[returnmove->from_square];
	      
	      /* Try to see if we can put another move into this slot. */
	      val = swap_avoid(board,(int)returnmove->to_square,pieceval[board->boardarr[returnmove->to_square]],&s_square,&s_piece,swap_list[i].old_fsquares);
	      if (val != -INFTY) {
		swap_list[i].fsquare = square[s_square];
		swap_list[i].piece = s_piece;
		swap_list[i].swap_val = val;
	      } else {
		swap_list[i].fsquare = 0;
	      }
	      hist_list[hist_number].untried = 0;
	      return 1;
	    } else if (swap_list[i].old_fsquares & hist_list[hist_number].fsquare) {
	      /* The history move is in the swap list and has already been
		 tried. */
	      hist_list[hist_number].untried = 0;
	      break;
	    } else {
	      /* Since the move wasn't in the old_fsquares list, we know that
		 it hasn't been tried before (possible hashmoves have been
		 tried already, and they are in the old_fsquares list. So
		 we don't need to check if it's a hashmove. */
	      returnmove->from_square = get_first_bitpos(hist_list[hist_number].fsquare);
	      returnmove->to_square = get_first_bitpos(hist_list[hist_number].tsquare);
	      returnmove->piece =
		(char) moves[hist_list[hist_number].movelist_pos].piece;
	      returnmove->type = get_movetype(board,returnmove);
	      swap_list[i].old_fsquares |= square[returnmove->from_square];
	      hist_list[hist_number].untried = 0;
	      return 1;
	    }
	  }
	}
	//if (hist_list[hist_number].untried == 1) {
	  /* We get here if the move wasn't found in the swap list. That can
	     happen for example if the only possible (pseudo) legal capture
	     move was a king taking a defended piece. Such a move won't be
	     put in the swap_list, but it can still be in the history list.
	     In this case mark this history move as tried. */
	  //hist_list[hist_number].untried = 0;
	//}
      }
    } else {
      (*order)++;
    }
  }

  /*---------------------------------------------------------------
   | Pawn pushes for pawns on the 6th rank. For the upper part of |
   | the tree order these moves based on their history values.    |
   | However this history checking is so slow that it's skipped   |
   | in the lower parts of the tree.                              |
   ---------------------------------------------------------------*/
  if (*order == 10) {
    if (depth > 3) {
      max_hist_val = -INFTY;
      /* Extract the best history move from the history list. */
      for (i = 0; i < *movables; i++) {
	if (moves[i].piece == PAWN
	    && moves[i].source & pawn_2nd_row[Oppcolor]) {
	  targets = moves[i].targets & ~(board->all_pieces[Oppcolor]);
	  while (targets) {
	    t_square = get_first_bitpos(targets);
	    s_square = (char) get_first_bitpos(moves[i].source);
	    val = historymovestable[(int)Color][s_square][t_square];
	    if (val > max_hist_val) {
	      returnmove->from_square = s_square;
	      returnmove->to_square = t_square;
	      returnmove->piece = (char) moves[i].piece;
	      /*showboard(board);
	      printf("s = %d\n",s_square);
	      printf("t = %d\n",t_square);
	      exit(1);*/
	      j = i;
	      max_hist_val = val;
	    }
	    targets &= ~square[t_square];
	  }
	}
      }

      if (max_hist_val > -INFTY) {
	returnmove->type = get_movetype(board,returnmove);
	moves[j].targets &= ~(square[returnmove->to_square]);
	return 1;
      }
      (*order)++;
    } else {
      for (i = 0; i < *movables; i++) {
	if (moves[i].piece == PAWN && moves[i].source & pawn_2nd_row[Oppcolor]
	    && moves[i].targets & ~(board->all_pieces[Oppcolor])) {
	  returnmove->from_square = (char) get_first_bitpos(moves[i].source);
	  returnmove->to_square = (char) get_first_bitpos(moves[i].targets & ~(board->all_pieces[Oppcolor]));
	  returnmove->piece = (char) moves[i].piece;
	  returnmove->type = get_movetype(board,returnmove);
	  moves[i].targets &= ~(square[returnmove->to_square]);
	  if (i == (*movables - 1) && moves[i].targets == 0)
	    (*order)++;
	  return 1;
	}
      }
      (*order)++;
    }

  }

  /*--------------------------------------------------------------
   | Try moves having swap value equal to zero (equal captures). |
   | For the upper part of the tree order these moves based on   |
   | their history values. However this history checking is so   |
   | slow that it's skipped in the lower parts of the tree.      |
   --------------------------------------------------------------*/
  if (*order == 11) {
    max_swap_val = -INFTY;
    if (depth > 3) {
      max_hist_val = -INFTY;
      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare && swap_list[i].swap_val == 0
	    && (val = historymovestable
		[(int)Color]
		[get_first_bitpos(swap_list[swap_number].fsquare)]
		[get_first_bitpos(swap_list[swap_number].tsquare)])
	    > max_hist_val) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	  max_hist_val = val;
	}
      }
    } else {
      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare && swap_list[i].swap_val > max_swap_val) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	}
      }
    }
      
    /* If we found a move with value >= 0 then return it. */
    if (max_swap_val >= 0) {
      returnmove->piece = (char) swap_list[swap_number].piece;
      returnmove->from_square =
	(char) get_first_bitpos(swap_list[swap_number].fsquare);
      returnmove->to_square =
	(char) get_first_bitpos(swap_list[swap_number].tsquare);
      returnmove->type = get_movetype(board,returnmove);
      swap_list[swap_number].old_fsquares |= square[returnmove->from_square];
      
      /* Try to see if we can put another move into this slot. */
      val = swap_avoid(board,(int)returnmove->to_square,
		       pieceval[board->boardarr[returnmove->to_square]],
		       &s_square,&s_piece,swap_list[swap_number].old_fsquares);
      if (val != -INFTY) {
	swap_list[swap_number].fsquare = square[s_square];
	swap_list[swap_number].piece = s_piece;
	swap_list[swap_number].swap_val = val;
      } else {
	swap_list[swap_number].fsquare = 0;
      }
      return 1;
    }
    (*order)++;
  }

  /*-----------------------------------------------------------------
   | Non-captures. For the upper part of the tree order these moves |
   | based on their history values. However this history checking   |
   | is so slow that it's skipped in the lower parts of the tree.   |
   -----------------------------------------------------------------*/
  if (*order == 12) {
    //zzz
    if (depth > 3) {
      max_hist_val = -INFTY;
      /* Extract the best history move from the history list. */
      for (i = 0; i < *movables; i++) {
	targets = moves[i].targets & ~(board->all_pieces[Oppcolor]);
	while (targets) {
	  t_square = get_first_bitpos(targets);
	  s_square = (char) get_first_bitpos(moves[i].source);
	  val = historymovestable[(int)Color][s_square][t_square];
	  if (val > max_hist_val) {
	    returnmove->from_square = s_square;
	    returnmove->to_square = t_square;
	    returnmove->piece = (char) moves[i].piece;
	    j = i;
	    max_hist_val = val;
	  }
	  targets &= ~square[t_square];
	}
      }

      if (max_hist_val > -INFTY) {
	returnmove->type = get_movetype(board,returnmove);
	moves[j].targets &= ~(square[returnmove->to_square]);
	return 1;
      }
      (*order)++;
    } else {
      for (i = 0; i < *movables; i++) {
	if (moves[i].targets & ~(board->all_pieces[Oppcolor])) {
	  returnmove->from_square = (char) get_first_bitpos(moves[i].source);
	  returnmove->to_square =
	    (char) get_first_bitpos(moves[i].targets
				    & ~(board->all_pieces[Oppcolor]));
	  returnmove->piece = (char) moves[i].piece;
	  returnmove->type = get_movetype(board,returnmove);
	  moves[i].targets &= ~(square[returnmove->to_square]);
	  if (i == (*movables - 1) && moves[i].targets == 0)
	    (*order)++;
	  return 1;
	}
      }
      (*order)++;
    }
  }

  /*---------------------------------------------------------------------
   | Try the remaining swap moves (the only remaining ones have a value |
   | below zero, and are thus bad captures). In the higher parts of the |
   | tree order equal valued swap moves according to the history value. |
   ---------------------------------------------------------------------*/
  if (*order == 13) {
    max_swap_val = -INFTY;
    if (depth > 3) {
      max_hist_val = -INFTY;

      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare) {
	  if (swap_list[i].swap_val > max_swap_val) {
	    max_swap_val = swap_list[i].swap_val;
	    swap_number = i;
	  } else if (swap_list[i].swap_val == max_swap_val
		     && (val = historymovestable
			 [(int)Color]
			 [get_first_bitpos(swap_list[swap_number].fsquare)]
			 [get_first_bitpos(swap_list[swap_number].tsquare)])
		     > max_hist_val) {
	    max_swap_val = swap_list[i].swap_val;
	    swap_number = i;
	    max_hist_val = val;
	  }
	}
      }
    } else {
      /* Extract the highest valued move from the swap list. */
      for (i = ((*swap_count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare && swap_list[i].swap_val > max_swap_val) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	}
      }
    }

    /* If we found a move with value > -INFTY then return it. */
    if (max_swap_val > -INFTY) {
      returnmove->piece = (char) swap_list[swap_number].piece;
      returnmove->from_square = (char) get_first_bitpos(swap_list[swap_number].fsquare);
      returnmove->to_square = (char) get_first_bitpos(swap_list[swap_number].tsquare);
      returnmove->type = get_movetype(board,returnmove);
      swap_list[swap_number].old_fsquares |= square[returnmove->from_square];
      
      /* Try to see if we can put another move into this slot. */
      val = swap_avoid(board,(int)returnmove->to_square,pieceval[board->boardarr[returnmove->to_square]],&s_square,&s_piece,swap_list[swap_number].old_fsquares);
      if (val != -INFTY) {
	swap_list[swap_number].fsquare = square[s_square];
	swap_list[swap_number].piece = s_piece;
	swap_list[swap_number].swap_val = val;
      } else {
	swap_list[swap_number].fsquare = 0;
      }
      return 1;
    }
    
    (*order)++;
  }

  return 0;
}

/*-------------------------------------------------------------------------
 | This function returns pseudo-legal check evasion moves. The move order |
 | is quite good, but not perfect:                                        |
 | 1.) Moves attacking the king attacker (if only one attacker)           |
 | 2.) Passant captures                                                   |
 | 3.) King move with capture                                             |
 | 4.) King move without capture                                          |
 | 5.) Move pieces between the attacker and the king                      |
 -------------------------------------------------------------------------*/
int get_check_evasion_moves(struct board *board, struct move *returnmove,
			    struct swap_entry *swap_list, int *order,
			    int *count, bitboard *spec_moves) {
  int i, j, val;
  bitboard pieces, pos, ray;
  int s_square, s_piece;
  int max_swap_val, swap_number=0;
  struct board tempboard;

  /*----------------------
   | Load the swap list. |
   ----------------------*/
  if (*order == 0) {
    pieces = get_attacks(board,get_first_bitpos((board)->piece[(int)Color][KING]))
      & ((board)->all_pieces[Oppcolor]);
    if (bitcount(pieces) == 1) {
      /* Only one piece attacking the king, load the swap list with
	 attacks to this square. */
      if (pieces & board->piece[(int)Oppcolor][PAWN]) {
	val = pieceval[PAWN];
      } else if (pieces & board->piece[(int)Oppcolor][KNIGHT]) {
	val = pieceval[KNIGHT];
      } else if (pieces & board->piece[(int)Oppcolor][BISHOP]) {
	val = pieceval[BISHOP];
      } else if (pieces & board->piece[(int)Oppcolor][ROOK]) {
	val = pieceval[ROOK];
      } else {
	val = pieceval[QUEEN];
      }
      pos = getlsb(pieces);
      val = swap_avoid(board,get_first_bitpos(pieces),val,&s_square,&s_piece,
		      board->piece[(int)Color][KING]);
      if (val != -INFTY && val != KINGTAKEN) {
	swap_list[0].fsquare = square[s_square];
	swap_list[0].tsquare = pos;
	swap_list[0].piece = s_piece;
	swap_list[0].swap_val = val;
	swap_list[0].old_fsquares = 0;
	*count = 1;
      } else {
	/* No own pieces could capture the king attacker. Set the swap list
	   count to zero. */
	*count = 0;
      }
    } else {
      /* Several attackers, can only get away by moving the king. Set the
	 swap list count to zero. */
      *count = 0;
    }
    (*order)++;
  }

  /*------------------------------------
   | Try to attack the checking piece. |
   ------------------------------------*/
  if (*order == 1) {
    /* Get the next move from the swap list (if any). */
    if (*count > 0 && swap_list[0].fsquare)
      max_swap_val = swap_list[0].swap_val;
    else
      max_swap_val = -INFTY;

    /* If we found a move then return it. */
    if (max_swap_val > -INFTY) {
      returnmove->piece = (char) swap_list[0].piece;
      returnmove->from_square = (char) get_first_bitpos(swap_list[0].fsquare);
      returnmove->to_square = (char) get_first_bitpos(swap_list[0].tsquare);
      returnmove->type = get_movetype(board,returnmove);
      swap_list[0].old_fsquares |= square[returnmove->from_square];
	
      /* Ok, see if there are other pieces that can also take the
	 same target piece, and if so add it to the same
	 slot (in swap_list). */
      pieces = (get_attacks(board,(int)returnmove->to_square)
		& (board->all_pieces[(int)Color] & ~(board->piece[(int)Color][KING])))
	& ~(swap_list[0].old_fsquares);
      if (pieces != 0) {
	if (pieces & board->piece[(int)Color][PAWN]) {
	  swap_list[0].piece = PAWN;
	} else if (pieces & board->piece[(int)Color][KNIGHT]) {
	  swap_list[0].piece = KNIGHT;
	} else if (pieces & board->piece[(int)Color][BISHOP]) {
	  swap_list[0].piece = BISHOP;
	} else if (pieces & board->piece[(int)Color][ROOK]) {
	  swap_list[0].piece = ROOK;
	} else {
	  swap_list[0].piece = QUEEN;
	}
	swap_list[0].fsquare =
	  getlsb(pieces & board->piece[(int)Color][swap_list[0].piece]);
	/* Don't bother to call swap, but rather give it a fixed value. */
	swap_list[0].swap_val = 10;
      } else {
	swap_list[0].fsquare = 0;
      }
	
      return 1;
    } else {
      (*order)++;
    }
  }

  /*-------------------------------------------
   | Try passant capture (in case it exists). |
   -------------------------------------------*/
  if (*order == 2) {
    if (board->passant) {
      *spec_moves = (attack.pawn[(int)Oppcolor][(int)board->passant])
	& board->piece[(int)Color][PAWN];
      (*order)++;
    } else
      *order += 3;
  }

  /* There can be a maximum of two passant moves. */
  while (*order == 3 || *order == 4) {
    (*order)++;
    if (*spec_moves) {
      returnmove->from_square = (char) get_first_bitpos(*spec_moves);
      returnmove->piece = PAWN;
      returnmove->to_square = board->passant;
      returnmove->type = get_movetype(board,returnmove);
      *spec_moves &= ~(square[returnmove->from_square]);
      return 1;
    }
  }

  /*-------------------------------------------------
   | Then get moves where the king moves to safety. |
   -------------------------------------------------*/
  if (*order == 5) {
    *spec_moves =
      generate_kingmoves(board,get_first_bitpos(board->piece[(int)Color][KING]));
    if (*count > 0)  //remove old moves if any
      *spec_moves &= ~(swap_list[0].old_fsquares);
    (*order)++;
  }

  /* Move the king away by capturing an enemy piece. */
  if (*order == 6) {
    if (*spec_moves & board->all_pieces[Oppcolor]) {
      returnmove->from_square = (char) get_first_bitpos(board->piece[(int)Color][KING]);
      returnmove->piece = KING;
      if (*spec_moves & board->piece[Oppcolor][QUEEN]) {
	returnmove->to_square =
	  (char) get_first_bitpos(*spec_moves & board->piece[Oppcolor][QUEEN]);
      } else if (*spec_moves & board->piece[Oppcolor][ROOK]) {
	returnmove->to_square =
	  (char) get_first_bitpos(*spec_moves & board->piece[Oppcolor][ROOK]);
      } else if (*spec_moves & board->piece[Oppcolor][BISHOP]) {
	returnmove->to_square =
	  (char) get_first_bitpos(*spec_moves & board->piece[Oppcolor][BISHOP]);
      } else if (*spec_moves & board->piece[Oppcolor][KNIGHT]) {
	returnmove->to_square =
	  (char) get_first_bitpos(*spec_moves & board->piece[Oppcolor][KNIGHT]);
      } else {
	returnmove->to_square =
	  (char) get_first_bitpos(*spec_moves & board->piece[Oppcolor][PAWN]);
      }
      returnmove->type = get_movetype(board,returnmove);
      *spec_moves &= ~(square[returnmove->to_square]);
      if ((*spec_moves & board->all_pieces[Oppcolor]) == 0)
	(*order)++;
      return 1;
    }
    (*order)++;
  }

  /*-------------------------------------------------------------
   | Try the remaining king moves (only non-captures are left). |
   -------------------------------------------------------------*/
  if (*order == 7) {
    if (*spec_moves != 0) {
      returnmove->from_square = (char) get_first_bitpos(board->piece[(int)Color][KING]);
      returnmove->piece = KING;
      returnmove->to_square = (char) get_first_bitpos(*spec_moves);
      returnmove->type = get_movetype(board,returnmove);
      *spec_moves &= ~(square[returnmove->to_square]);
      if (*spec_moves == 0)
	(*order)++;
      return 1;
    }
    (*order)++;
  }

  /*-----------------------------------------------------------------------
   | Try moves where a piece protects the king by moving between the king |
   | and the attacker (this only works if there is only one attacker, and |
   | that attacker isn't a knight).                                       |
   -----------------------------------------------------------------------*/
  if (*order == 8) {
    pieces = get_attacks(board,get_first_bitpos((board)->piece[(int)Color][KING]))
      & ((board)->all_pieces[Oppcolor]);
    if (bitcount(pieces) == 1
	&& !(board->piece[Oppcolor][KNIGHT] & pieces)) {
      /* Only one piece attacking the king (not knight), load the swap
	 list with attacks to this square. First create a ray of squares
	 that are between the king and attacker. */
      int king_sq, attacker_sq;
      *count = 0;
      king_sq = get_first_bitpos(board->piece[(int)Color][KING]);
      attacker_sq = get_first_bitpos(pieces);
      ray = get_ray(king_sq,direction[king_sq][attacker_sq])
	& get_ray(attacker_sq,direction[attacker_sq][king_sq]);
      while (ray != 0) {
	pieces = 0;

	/* Add the (non-capturing) pawn moves since they aren't returned
	   by get_attacks. */
	if (Color == WHITE) {
	  if (board->piece[(int)Color][PAWN] & (getlsb(ray) << 8)) {
	    pieces = (getlsb(ray) << 8);
	  } else if (((getlsb(ray) << 16) & pawn_start[WHITE])
		     && (~(board->all_pieces[WHITE]
			   | board->all_pieces[BLACK])
			 & (getlsb(ray) << 8))
		     && (board->piece[(int)Color][PAWN] & (getlsb(ray) << 16))) {
	    pieces = (getlsb(ray) << 16);
	  }
	} else {
	  if (board->piece[(int)Color][PAWN] & (getlsb(ray) >> 8)) {
	    pieces = (getlsb(ray) >> 8);
	  } else if (((getlsb(ray) >> 16) & pawn_start[BLACK])
		     && (~(board->all_pieces[WHITE]
			   | board->all_pieces[BLACK])
			 & (getlsb(ray) >> 8))
		     && (board->piece[(int)Color][PAWN] & (getlsb(ray) >> 16))) {
	    pieces = (getlsb(ray) >> 16);
	  }
	}

	/* Non-capture pawn moves can't defend anything, so let's do those
	   move's first. */
	if (pieces != 0) {
	  /* Let's do a trick here in order to get the correct swap value.
	     Remove the pawn from the board, switch colors and let the
	     opponent attack this square. (We can't simply call swap like
	     we normally do since a non-capturing pawn move won't be handled
	     by swap, and also the target square is empty in this case.) */
	  i = get_first_bitpos(pieces);
	  board->piece[(int)Color][PAWN] &= ~pieces; //won't be more than one
	  board->rot90_pieces[(int)Color] &= ~(square[rotate0to90[i]]);
	  board->color_at_move = Oppcolor;
	  val = -swap(board,get_first_bitpos(ray),pieceval[PAWN],
			  &s_square,&s_piece);
	  board->color_at_move = Oppcolor;
	  board->piece[(int)Color][PAWN] |= pieces;
	  board->rot90_pieces[(int)Color] |= square[rotate0to90[i]];

	  /* val will never be INFTY since the pawn is placed between the
	     king and the attacking piece. */
	  if (val != -KINGTAKEN) {
	    swap_list[*count].fsquare = pieces;
	    swap_list[*count].tsquare = getlsb(ray);
	    swap_list[*count].piece = PAWN;
	    swap_list[*count].swap_val = val;
	    swap_list[*count].old_fsquares = 0;
	    (*count)++;
	  }
	} else {
	  val = -KINGTAKEN;
	}

	if (val == -KINGTAKEN) { //get here if no pawn move (or illegal)
	  /* Get the attacks that can be made to the square. Note that the
	     get_attacks()-function returns attacks, and since the square
	     is empty there cannot be any attacking pawn moves, so remove
	     those. Also remove the king moves since those have been tried
	     already. */
	  pieces = get_attacks(board,get_first_bitpos(ray))
	    & ~(board->piece[(int)Color][PAWN]
		| board->piece[(int)Color][KING]
		| board->all_pieces[Oppcolor]);

	  if (pieces != 0) {
	    do {
	      /* Do the same trick as above to get the right swap value. Note
		 that now it's not enough to just change the board->piece and
		 board->rot90_pieces since now "from-behind-moves" can come
		 from any direction. */
	      if (pieces & board->piece[(int)Color][PAWN]) {
		j = PAWN;
	      } else if (pieces & board->piece[(int)Color][KNIGHT]) {
		j = KNIGHT;
	      } else if (pieces & board->piece[(int)Color][BISHOP]) {
		j = BISHOP;
	      } else if (pieces & board->piece[(int)Color][ROOK]) {
		j = ROOK;
	      } else {
		j = QUEEN;
	      } //else {
		//No need to check king moves since they have already been
		//tried, and were therefore removed from the pieces bitboard
	        //above.
	      //}
	      i = get_first_bitpos(pieces & board->piece[(int)Color][j]);
	      tempboard = *board;
	      board->piece[(int)Color][j] &= ~(square[i]);
	      board->all_pieces[(int)Color] &= ~(square[i]);
	      board->rot90_pieces[(int)Color] &= ~(square[rotate0to90[i]]);
	      board->rotNE_pieces[(int)Color] &= ~(square[rotate0toNE[i]]);
	      board->rotNW_pieces[(int)Color] &= ~(square[rotate0toNW[i]]);
	      board->color_at_move = Oppcolor;
	      val = -swap(board,get_first_bitpos(ray),pieceval[j],
			      &s_square,&s_piece);
	      board->color_at_move = Oppcolor;
	      board->piece[(int)Color][j] |= square[i];
	      board->all_pieces[(int)Color] |= square[i];
	      board->rot90_pieces[(int)Color] |= square[rotate0to90[i]];
	      board->rotNE_pieces[(int)Color] |= square[rotate0toNE[i]];
	      board->rotNW_pieces[(int)Color] |= square[rotate0toNW[i]];
	      pieces &= ~(square[i]);
	    } while (val==-KINGTAKEN && pieces!=0); //val is never INFTY here
	    if (val != -KINGTAKEN) {
	      swap_list[*count].fsquare = square[i];
	      swap_list[*count].tsquare = getlsb(ray);
	      swap_list[*count].piece = j;
	      swap_list[*count].swap_val = val;
	      swap_list[*count].old_fsquares = 0;
	      (*count)++;
	    }
	  }
	}
	ray &= ~(getlsb(ray));
      }
    } else {
      /* Several attackers so it doesn't work to put something in between.
	 Set the swap list count to zero. */
      *count = 0;
    }
    (*order)++;
  }

  /*---------------------------
   | Try the remaining moves. |
   ---------------------------*/
  if (*order == 9) {
    max_swap_val = -INFTY;
    /* Extract the highest valued move from the swap list. */
    for (i = ((*count)-1); i >= 0; i--) {
      if (swap_list[i].fsquare && swap_list[i].swap_val > max_swap_val) {
	max_swap_val = swap_list[i].swap_val;
	swap_number = i;
      }
    }
      
    /* If we found a move with value > -INFTY then return it. */
    if (max_swap_val > -INFTY) {
      returnmove->piece = swap_list[swap_number].piece;
      returnmove->from_square = (char) get_first_bitpos(swap_list[swap_number].fsquare);
      returnmove->to_square = (char) get_first_bitpos(swap_list[swap_number].tsquare);
      returnmove->type = get_movetype(board,returnmove);
      swap_list[swap_number].old_fsquares |= square[returnmove->from_square];
	
      /* Try to see if we can put another move into this slot.
	 At this point we won't bother about calling swap,
	 or if it's an illegal move. Illegal moves will anyway
	 be caught later, so nothing to worry about. And we don't
	 need to take care of non-capturing pawn moves either
	 since we made sure that those were tried first.

	 Note that the get_attacks()-function returns attacks,
	 and since the square is empty there cannot be any
	 attacking pawn moves, so remove those. Also remove the
	 king moves since those have been tried already. */
      pieces = get_attacks(board,(int)returnmove->to_square)
	& ~(board->piece[(int)Color][PAWN]
	    | board->piece[(int)Color][KING]
	    | board->all_pieces[Oppcolor]
	    | swap_list[swap_number].old_fsquares);
      swap_list[swap_number].fsquare = getlsb(pieces);
      if (pieces != 0) {
	swap_list[swap_number].swap_val = -500; //make sure it's tried late
	if (swap_list[swap_number].fsquare & board->piece[(int)Color][PAWN]) {
	  swap_list[swap_number].piece = PAWN;
	} else if (swap_list[swap_number].fsquare
		   & board->piece[(int)Color][KNIGHT]) {
	  swap_list[swap_number].piece = KNIGHT;
	} else if (swap_list[swap_number].fsquare
		   & board->piece[(int)Color][BISHOP]) {
	  swap_list[swap_number].piece = BISHOP;
	} else if (swap_list[swap_number].fsquare
		   & board->piece[(int)Color][ROOK]) {
	  swap_list[swap_number].piece = ROOK;
	} else {
	  swap_list[swap_number].piece = QUEEN;
	}
      }
      return 1;
    }
      
    (*order)++;
  }

  return 0;
}

/*-------------------------------------------------------------------------
 | This function returns moves to try in the quiescence search. If there  |
 | are moves left 1 is returned, else 0 is returned. If the king is in    |
 | check then the get_check_evasion_moves()-function is called. Otherwise |
 | move oves are generated in the following order:                        |
 | 1. Promotion with capture                                              |
 | 2. Promotion without capture                                           |
 | 3. Remaining captures that are good enough to be likely to raise alpha |
 -------------------------------------------------------------------------*/
int get_next_quiet_move(struct board *board, struct move *returnmove,
			struct swap_entry *swap_list, int *order, int *count,
			int own_king_check, bitboard *spec_moves,
			int stat_val, int alpha) {
  int i, val;
  bitboard pieces, pos;
  int s_square, s_piece;
  int max_swap_val, swap_number=0;

  if (own_king_check) {
    return get_check_evasion_moves(board,returnmove,swap_list,
				   order,count,spec_moves);
  } else {

    /*-----------------------------------------------------------------------
     | Instead of generating all the moves at once using the generate_moves |
     | function, we go through all the enemy pieces and call swap for each  |
     | of them. Don't check if the king can be taken (=>start counting      |
     | from 4) since in the quiescence function we make sure that the       |
     | opponent king can't be taken.                                        |
     -----------------------------------------------------------------------*/
    if (*order == 0) {
      for (i = 4; i >= 0; i--) {
	pieces = board->piece[Oppcolor][i];
	while (pieces != 0) {
	  pos = getlsb(pieces);
          /* At the same time as we are building the swap list, lets save
             information about what pieces are on given squares.*/
          //opp_pieces[get_first_bitpos(pos)] = i;

	  val = swap(board,get_first_bitpos(pos),pieceval[i],&s_square,&s_piece);
	  if (val > 0) {
	    swap_list[*count].fsquare = square[s_square];
	    swap_list[*count].tsquare = pos;
	    swap_list[*count].piece = s_piece;
	    swap_list[*count].swap_val = val;
	    swap_list[*count].old_fsquares = 0;
	    (*count)++;
	  }
	  pieces &= ~pos;
	}
      }
      *order = 1;
    }

    /*---------------------------------------
     | Extract promotion captures (if any). |
     ---------------------------------------*/
    if (*order == 1) {
      max_swap_val = -INFTY;
      for (i = ((*count)-1); i >= 0; i--) {
	if (swap_list[i].piece == PAWN
	    && (swap_list[i].fsquare & pawn_start[Oppcolor])) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	}
      }
      
      /* If we found a move then return it. */
      if (max_swap_val > 0) {
	returnmove->piece = (char) swap_list[swap_number].piece;
	returnmove->from_square = (char) get_first_bitpos(swap_list[swap_number].fsquare);
	returnmove->to_square = (char) get_first_bitpos(swap_list[swap_number].tsquare);
	returnmove->type = get_movetype(board,returnmove);
	swap_list[swap_number].fsquare = 0;
	return 1;
      } else {
	(*order)++;
      }
    }

    /*------------------------------------------------------
     | Extract promotions that don't capture other pieces. |
     ------------------------------------------------------*/
    if (*order == 2) {
      *spec_moves = board->piece[(int)Color][PAWN] & pawn_start[Oppcolor];
      (*order)++;
    }
    if (*order == 3) {
      if (Color == WHITE) {
	while (*spec_moves) {
	  pos = (getlsb(*spec_moves) >> 8) &
	    ~(board->all_pieces[WHITE] | board->all_pieces[BLACK]);
	  if (pos) {
	    returnmove->piece = PAWN;
	    returnmove->from_square = (char) get_first_bitpos(*spec_moves);
	    returnmove->to_square = (char) get_first_bitpos(pos);
	    returnmove->type = get_movetype(board,returnmove);
	    *spec_moves &= ~(getlsb(*spec_moves));
	    return 1;
	  }
	  *spec_moves &= ~(getlsb(*spec_moves));
	}
	(*order)++;
      } else {
	while (*spec_moves) {
	  pos = (getlsb(*spec_moves) << 8) &
	    ~(board->all_pieces[WHITE] | board->all_pieces[BLACK]);
	  if (pos) {
	    returnmove->piece = PAWN;
	    returnmove->from_square = (char) get_first_bitpos(*spec_moves);
	    returnmove->to_square = (char) get_first_bitpos(pos);
	    returnmove->type = get_movetype(board,returnmove);
	    *spec_moves &= ~(getlsb(*spec_moves));
	    return 1;
	  }
	  *spec_moves &= ~(getlsb(*spec_moves));
	}
	(*order)++;
      }
    }

    /*-------------------------------------------
     | Remaining captures that are good enough. |
     -------------------------------------------*/
    if (*order == 4) {
      /* Extract the highest valued move from the swap list. */
      max_swap_val = -INFTY;
      for (i = ((*count)-1); i >= 0; i--) {
	if (swap_list[i].fsquare && swap_list[i].swap_val > max_swap_val) {
	  max_swap_val = swap_list[i].swap_val;
	  swap_number = i;
	}
      }
    
      /* Here we do a selective futility pruning which means that
	 the move is tried only if it's likely to raise alpha. */
      if (stat_val + max_swap_val + QUIESCENCE_FUTIL_MARGIN > alpha) {
	returnmove->piece = swap_list[swap_number].piece;
	returnmove->from_square = (char) get_first_bitpos(swap_list[swap_number].fsquare);
	returnmove->to_square = (char) get_first_bitpos(swap_list[swap_number].tsquare);
	returnmove->type = get_movetype(board,returnmove);
	swap_list[swap_number].old_fsquares |= square[returnmove->from_square];
	  
	/* Ok, see if there are other pieces that can also take the
	   same target piece, and if so add it to the same slot
	   (in swap_list). */
	pieces = (get_attacks(board,(int)returnmove->to_square)
		  & board->all_pieces[(int)Color])
	  & ~(swap_list[swap_number].old_fsquares);
	if (pieces != 0) {
	  if (pieces & board->piece[(int)Color][PAWN]) {
	    swap_list[swap_number].piece = PAWN;
	  } else if (pieces & board->piece[(int)Color][KNIGHT]) {
	    swap_list[swap_number].piece = KNIGHT;
	  } else if (pieces & board->piece[(int)Color][BISHOP]) {
	    swap_list[swap_number].piece = BISHOP;
	  } else if (pieces & board->piece[(int)Color][ROOK]) {
	    swap_list[swap_number].piece = ROOK;
	  } else if (pieces & board->piece[(int)Color][QUEEN]) {
	    swap_list[swap_number].piece = QUEEN;
	  } else {
	    swap_list[swap_number].piece = KING;
	  }
	  swap_list[swap_number].fsquare = getlsb(pieces & board->piece[(int)Color][swap_list[swap_number].piece]);
          swap_list[swap_number].swap_val = swap_avoid(board,(int)returnmove->to_square,pieceval[board->boardarr[returnmove->to_square]],&s_square,&s_piece,swap_list[swap_number].old_fsquares);
	  swap_list[swap_number].fsquare = square[s_square];
	  swap_list[swap_number].piece = s_piece;
	} else {
          swap_list[swap_number].fsquare = 0;
        }
	return 1;
      }
    }
  }

  return 0;
}

/*-------------------------------------------------------------------
 | This method returns the number of the best move to execute next. |
 | It uses the value of the previous iteration to determine which   |
 | move is best.                                                    |
 -------------------------------------------------------------------*/
int get_next_root_move(struct board *board, struct move_list_entry *movelist,
		       int mcount, int *searched_list) {
  int i, best = 0;

  /* Find out a starting value for best. */
  while (searched_list[best] == 1 && best < mcount) {
    best++;
  }
  if (best >= mcount)
    return -1;
      
  /* Pick the best move next. */
  for (i = best + 1; i < mcount; i++) {
    if (movelist[i].value > movelist[best].value && searched_list[i] == 0)
      best = i;
  }
  if (best >= mcount)
    return -1;
  return best;
}

int get_movetype(struct board *board, struct move *move) {
  if (move->piece == PAWN)
    return get_pawnmove_movetype(board,move->from_square,move->to_square);
  else if (move->piece == KING)
    return get_kingmove_movetype(board,move->to_square);
  else
    return get_othermove_movetype(board,move->to_square);
}

/*---------------------------------------------------------------------
 | This function returns all pseudo-legal moves in a given position.  |
 | Pseudo-legal move generation means that no checking is done to see |
 | if the move places it's own king in check. That has to be checked  |
 | elsewhere. The function returns 0 on no error, and -1 on error,    |
 | which is if the opposite king can be taken. There are at most 16   |
 | pieces of one color, so struct moves *moves needs to have room for |
 | 16 entries, and no more.                                           |
 ---------------------------------------------------------------------*/
int generate_moves(struct board *board, struct moves *moves, int *movables) {
  int piecetype, boardpos, forward_sq, occupancy, color, opp_color;
  bitboard pieces, targets=0, all_pieces, own_color_pieces, opp_color_pieces;
  bitboard empty_squares, all_rotNE_pieces, all_rotNW_pieces;
  bitboard all_rot90_pieces;

  *movables = 0;
  color = Color;
  opp_color = 1 - color;
  own_color_pieces = board->all_pieces[color];
  opp_color_pieces = board->all_pieces[opp_color];
  all_rotNE_pieces = board->rotNE_pieces[WHITE] | board->rotNE_pieces[BLACK];
  all_rotNW_pieces = board->rotNW_pieces[WHITE] | board->rotNW_pieces[BLACK];
  all_rot90_pieces = board->rot90_pieces[WHITE] | board->rot90_pieces[BLACK];
  all_pieces = own_color_pieces | opp_color_pieces;
  empty_squares = ~all_pieces;

  for (piecetype = 0; piecetype < 6; piecetype++) {
    pieces = board->piece[color][piecetype];
    /* Go through all the pieces for the given piece type. */
    while (pieces != 0) {
      boardpos = get_first_bitpos(pieces);
      if (piecetype == PAWN) {
	forward_sq = pawn_forward[color][boardpos];
	targets = square[forward_sq] & empty_squares;
	if (targets && (square[boardpos] & pawn_start[color])) {
	  forward_sq = pawn_forward[color][forward_sq];
	  targets |= (square[forward_sq] & empty_squares);
	}
	targets |= (attack.pawn[color][boardpos] & opp_color_pieces);
	if (board->passant)
	  targets |= (attack.pawn[color][boardpos]
		      & square[(int)board->passant]);
	targets &= ~own_color_pieces;
      } else if (piecetype == KNIGHT) {
	targets = (attack.knight[boardpos] & ~own_color_pieces);
      } else if (piecetype == BISHOP) {
	occupancy = all_rotNE_pieces >> (rotate0toNE[diagNE_start[boardpos]])
	  & ones[diagNE_length[boardpos]];
	targets = attack.sliderNE[boardpos][occupancy];
	occupancy = all_rotNW_pieces >> (rotate0toNW[diagNW_start[boardpos]])
	  & ones[diagNW_length[boardpos]];
	targets |= attack.sliderNW[boardpos][occupancy];
	targets &= ~own_color_pieces;
      } else if (piecetype == ROOK) {
	occupancy = (all_pieces >> ((boardpos/8)*8)) & 255;
	targets = attack.hslider[boardpos][occupancy];
	occupancy = (all_rot90_pieces >> (rotate0to90[boardpos]/8)*8) & 255;
	targets |= attack.vslider[boardpos][occupancy];
	targets &= ~own_color_pieces;
      } else if (piecetype == QUEEN) {
	occupancy = (all_pieces >> ((boardpos/8)*8)) & 255;
	targets = attack.hslider[boardpos][occupancy];
	occupancy = (all_rot90_pieces >> (rotate0to90[boardpos]/8)*8) & 255;
	targets |= attack.vslider[boardpos][occupancy];
	occupancy = all_rotNE_pieces >> (rotate0toNE[diagNE_start[boardpos]])
	  & ones[diagNE_length[boardpos]];
	targets |= attack.sliderNE[boardpos][occupancy];
	occupancy = all_rotNW_pieces >> (rotate0toNW[diagNW_start[boardpos]])
	  & ones[diagNW_length[boardpos]];
	targets |= attack.sliderNW[boardpos][occupancy];
	targets &= ~own_color_pieces;
      } else if (piecetype == KING) {
	targets = attack.king[boardpos] & ~own_color_pieces;

	/* Check castling. */
	if (color == WHITE) {
	  if (board->castling_status[WHITE] & SHORT_CASTLING_OK)
	    if ((all_pieces & (square[61] | square[62])) == 0) {
	      /* Make a final check to see if the king's square is in check,
		 or if the destination square is in check, or if the square in
		 between is in check.

		 TODO: This checking examination can be done at a later stage,
		 and thereby save time. */
	      if (!in_check(board)) {
		if (!sq_under_attack_by_opp(board,61)) {
		  if (!sq_under_attack_by_opp(board,62))
		    targets |= square[62];
		}
	      }
	    }
	  if (board->castling_status[WHITE] & LONG_CASTLING_OK)
	    if ((all_pieces & (square[57] | square[58] | square[59])) == 0) {
	      if (!in_check(board)) {
		if (!sq_under_attack_by_opp(board,59)) {
		  if (!sq_under_attack_by_opp(board,58))
		    targets |= square[58];
		}
	      }
	    }
	} else {    //color == BLACK
	  if (board->castling_status[BLACK] & SHORT_CASTLING_OK)
	    if ((all_pieces & (square[5] | square[6])) == 0) {
	      if (!in_check(board)) {
		if (!sq_under_attack_by_opp(board,5)) {
		  if (!sq_under_attack_by_opp(board,6))
		    targets |= square[6];
		}
	      }
	    }
	  if (board->castling_status[BLACK] & LONG_CASTLING_OK)
	    if ((all_pieces & (square[1] | square[2] | square[3])) == 0) {
	      if (!in_check(board)) {
		if (!sq_under_attack_by_opp(board,3)) {
		  if (!sq_under_attack_by_opp(board,2))
		    targets |= square[2];
		}
	      }
	    }
	}
      }

      if (targets & board->piece[opp_color][KING])
	return -1;

      /* Only add this piece if it can go somewhere. */
      if (targets != 0) {
	moves[*movables].targets = targets;
	moves[*movables].source = square[boardpos];
	moves[*movables].piece = piecetype;
	(*movables)++;
      }
      pieces = pieces & ~square[boardpos];
    }
  }

  return 0;
}

/*----------------------------------------------------------------------
 | This function returns all pseudo-legal moves in a given position    |
 | for a piece on a certain square. Pseudo-legal move generation means |
 | that no checking is done to see if the move places it's own king    |
 | in check. That has to be checked elsewhere.                         |
 ----------------------------------------------------------------------*/
bitboard generate_move(struct board *board, int from_square) {
  char piecetype;

  if (from_square < 0) {
    return 0;
  }

  if (square[from_square] & board->all_pieces[(int)Color]) {
    piecetype = board->boardarr[from_square];
    if (piecetype == PAWN) {
      return generate_pawnmoves(board,from_square);
    } else if (piecetype == KNIGHT) {
      return generate_knight_moves(board,from_square);
    } else if (piecetype == BISHOP) {
      return generate_NEdiag_moves(board,from_square)
	| generate_NWdiag_moves(board,from_square);
    } else if (piecetype == ROOK) {
      return generate_horizontal_moves(board,from_square)
	| generate_vertical_moves(board,from_square);
    } else if (piecetype == QUEEN) {
      return generate_horizontal_moves(board,from_square)
	| generate_vertical_moves(board,from_square)
	| generate_NEdiag_moves(board,from_square)
	| generate_NWdiag_moves(board,from_square);
    } else if (piecetype == KING) {
      return generate_kingmoves(board,from_square);
    }
  }
  return 0;
}
