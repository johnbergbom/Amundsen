/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "swap.h"
//#include <stdio.h>
//#include <stdlib.h>
#include "parse.h"
#include "alfabeta.h"

extern struct attack attack;
extern int rotate0to90[64];
extern int rotate0toNE[64];
extern int ones[9];
extern int diagNE_start[64];
extern int diagNE_length[64];
extern int rotate0toNW[64];
extern int diagNW_start[64];
extern int diagNW_length[64];
extern bitboard square[64];
extern int direction[64][64];
extern int pieceval[6];
extern bitboard pawn_lastrow[2];
extern bitboard col_bitboard[8];
extern bitboard horiz_vert_cross[64];
extern bitboard nw_ne_cross[64];

/*--------------------------------------------------------------
 | This method returns the source square from where the target |
 | can be attacked from the direction specified, when the      |
 | specified pieces are removed from the board.                |
 --------------------------------------------------------------*/
bitboard attacks_behind(struct board *board, int target,
			bitboard remove_pieces, int dir) {
  bitboard temp;
  bitboard SLIDER, W_PIECES, B_PIECES, ray;
  bitboard w_rot_pieces, b_rot_pieces;
  int sq;

  ray = get_ray(target,dir);

  if (dir == -9 || dir == 9) { //NW-diagonal, up or down
    W_PIECES = board->piece[WHITE][QUEEN] | board->piece[WHITE][BISHOP];
    B_PIECES = board->piece[BLACK][QUEEN] | board->piece[BLACK][BISHOP];
    if ((W_PIECES & ray) == 0 && (B_PIECES & ray) == 0)
      return 0; //if no pieces found, then quick exit
    w_rot_pieces = board->rotNW_pieces[WHITE];
    b_rot_pieces = board->rotNW_pieces[BLACK];
    temp = ray & remove_pieces;
    while (temp) {
      sq = get_first_bitpos(temp);
      w_rot_pieces &= ~(square[rotate0toNW[sq]]);
      b_rot_pieces &= ~(square[rotate0toNW[sq]]);
      temp &= ~(square[sq]);
    }
    SLIDER =
      attack.sliderNW[target][(w_rot_pieces | b_rot_pieces)
			     >> (rotate0toNW[diagNW_start[target]])
			     & ones[diagNW_length[target]]];
  } else if (dir == -7 || dir == 7) { //NE-diagonal, up or down
    W_PIECES = board->piece[WHITE][QUEEN] | board->piece[WHITE][BISHOP];
    B_PIECES = board->piece[BLACK][QUEEN] | board->piece[BLACK][BISHOP];
    if ((W_PIECES & ray) == 0 && (B_PIECES & ray) == 0)
      return 0; //if no pieces found, then quick exit
    w_rot_pieces = board->rotNE_pieces[WHITE];
    b_rot_pieces = board->rotNE_pieces[BLACK];
    temp = ray & remove_pieces;
    while (temp) {
      sq = get_first_bitpos(temp);
      w_rot_pieces &= ~(square[rotate0toNE[sq]]);
      b_rot_pieces &= ~(square[rotate0toNE[sq]]);
      temp &= ~(square[sq]);
    }
    SLIDER =
      attack.sliderNE[target][(w_rot_pieces | b_rot_pieces)
			     >> (rotate0toNE[diagNE_start[target]])
			     & ones[diagNE_length[target]]];
  } else if (dir == -8 || dir == 8) { //vertical
    W_PIECES = board->piece[WHITE][QUEEN] | board->piece[WHITE][ROOK];
    B_PIECES = board->piece[BLACK][QUEEN] | board->piece[BLACK][ROOK];
    if ((W_PIECES & ray) == 0 && (B_PIECES & ray) == 0)
      return 0; //if no pieces found, then quick exit
    w_rot_pieces = board->rot90_pieces[WHITE];
    b_rot_pieces = board->rot90_pieces[BLACK];
    temp = ray & remove_pieces;
    while (temp) {
      sq = get_first_bitpos(temp);
      w_rot_pieces &= ~(square[rotate0to90[sq]]);
      b_rot_pieces &= ~(square[rotate0to90[sq]]);
      temp &= ~(square[sq]);
    }
    SLIDER =
      attack.vslider[target][((w_rot_pieces | b_rot_pieces)
			      >> (rotate0to90[target]/8)*8) & 255];
  } else if (dir == -1 || dir == 1) { //horizontal
    W_PIECES = board->piece[WHITE][QUEEN] | board->piece[WHITE][ROOK];
    B_PIECES = board->piece[BLACK][QUEEN] | board->piece[BLACK][ROOK];
    w_rot_pieces = board->all_pieces[WHITE] & ~remove_pieces;
    b_rot_pieces = board->all_pieces[BLACK] & ~remove_pieces;
    SLIDER =
      attack.hslider[target][((w_rot_pieces | b_rot_pieces)
			      >> ((target/8)*8)) & 255];
  } else
    return 0;

  return ((W_PIECES | B_PIECES) & ~remove_pieces) & (SLIDER & ray);
}

/*--------------------------------------------------------------------
 | This method returns all the source squares from where the target  |
 | can be attacked. (It returns BOTH the attacks of white and black) |
 | Note that passant captures are NOT returned. If we have for       |
 | example a white pawn on a4, and then call get_attacks(board,24)   |
 | we won't get a4 in the returned bitboard, since pawns can only    |
 | capture sideways and forward. Note that passant moves aren't      |
 | returned either.
 --------------------------------------------------------------------*/
bitboard get_attacks(struct board *board, int target) {
  bitboard HSLIDER =
    attack.hslider[target][((board->all_pieces[WHITE]
			     | board->all_pieces[BLACK])
			    >> ((target/8)*8)) & 255];
  bitboard VSLIDER =
    attack.vslider[target][((board->rot90_pieces[WHITE]
			     | board->rot90_pieces[BLACK])
			    >> (rotate0to90[target]/8)*8) & 255];
  bitboard NESLIDER =
    attack.sliderNE[target][(board->rotNE_pieces[WHITE]
			     | board->rotNE_pieces[BLACK])
			   >> (rotate0toNE[diagNE_start[target]])
			   & ones[diagNE_length[target]]];
  bitboard NWSLIDER =
    attack.sliderNW[target][(board->rotNW_pieces[WHITE]
			     | board->rotNW_pieces[BLACK])
			   >> (rotate0toNW[diagNW_start[target]])
			   & ones[diagNW_length[target]]];

  bitboard KINGS = board->piece[WHITE][KING] | board->piece[BLACK][KING];
  bitboard QUEENS = board->piece[WHITE][QUEEN] | board->piece[BLACK][QUEEN];
  bitboard ROOKS = board->piece[WHITE][ROOK] | board->piece[BLACK][ROOK];
  bitboard BISHOPS = board->piece[WHITE][BISHOP] | board->piece[BLACK][BISHOP];
  bitboard KNIGHTS = board->piece[WHITE][KNIGHT] | board->piece[BLACK][KNIGHT];

  return
    (((QUEENS | ROOKS) & (HSLIDER | VSLIDER))
     | ((QUEENS | BISHOPS) & (NESLIDER | NWSLIDER))
     | (KNIGHTS & attack.knight[target])
     | (KINGS & attack.king[target])
     | (attack.pawn[WHITE][target] & board->piece[BLACK][PAWN])
     | (attack.pawn[BLACK][target] & board->piece[WHITE][PAWN]));
}

/*-------------------------------------------------------------------------
 | This function returns 1 if the move will check the opponent's king,    |
 | otherwise it returns 0. Note: This function only checks if the move    |
 | in question is a checking move, but it doesn't check if the opponent   |
 | is in check already before this move is made. That needs to be checked |
 | elsewhere.                                                             |
 -------------------------------------------------------------------------*/
int checking_move(struct board *board, struct move *move) {
  int to_square = move->to_square;
  int from_square = move->from_square;
  bitboard opp_king = board->piece[Oppcolor][KING];
  bitboard HSLIDER, VSLIDER, NWSLIDER, NESLIDER;
  int dir;

  /* First check if the piece making the move can take the opposite king. */
  if (move->piece == KING) {
    if (attack.king[to_square] & opp_king)
      return 1;
    if (move->type & CASTLING_MOVE) {
      int rook_square = get_first_bitpos(board->piece[Color][KING]) + (from_square%8 < to_square%8 ? 1 : -1);
      HSLIDER =
	attack.hslider[rook_square][(((board->all_pieces[WHITE]
				       | board->all_pieces[BLACK]
				       | square[rook_square])&~(square[move->from_square]))
				     >> ((rook_square/8)*8)) & 255];
      if (HSLIDER & opp_king)
	return 1;
      VSLIDER =
	attack.vslider[rook_square][(((board->rot90_pieces[WHITE]
				       | board->rot90_pieces[BLACK]
				       | square[rotate0to90[rook_square]]))
				     >> (rotate0to90[rook_square]/8)*8) & 255];
      if (VSLIDER & opp_king)
	return 1;
    }
  } else if (move->piece == KNIGHT || (move->piece == PAWN && (move->type & KNIGHT_PROMOTION_MOVE))) {
    if (attack.knight[to_square] & opp_king)
      return 1;
  } else if (move->piece == PAWN && !(move->type & PROMOTION_MOVE)) {
    if (attack.pawn[Color][to_square] & opp_king)
      return 1;
  } else if ((move->piece == QUEEN || move->piece == ROOK)
	     && (horiz_vert_cross[to_square] & opp_king)) {
    HSLIDER =
      attack.hslider[to_square][((board->all_pieces[WHITE]
				  | board->all_pieces[BLACK]
				  | square[move->to_square])
				 >> ((to_square/8)*8)) & 255];
    if (HSLIDER & opp_king)
      return 1;
    VSLIDER =
      attack.vslider[to_square][((board->rot90_pieces[WHITE]
				  | board->rot90_pieces[BLACK]
				  | square[rotate0to90[to_square]])
				 >> (rotate0to90[to_square]/8)*8) & 255];
    if (VSLIDER & opp_king)
      return 1;
  } else if ((move->piece == QUEEN || move->piece == BISHOP)
	     && (nw_ne_cross[to_square] & opp_king)) {
    NESLIDER =
      attack.sliderNE[to_square][(board->rotNE_pieces[WHITE]
				  | board->rotNE_pieces[BLACK]
				  | square[rotate0toNE[to_square]])
				 >> (rotate0toNE[diagNE_start[to_square]])
				 & ones[diagNE_length[to_square]]];
    if (NESLIDER & opp_king)
      return 1;
    NWSLIDER =
      attack.sliderNW[to_square][(board->rotNW_pieces[WHITE]
				  | board->rotNW_pieces[BLACK]
				  | square[rotate0toNW[to_square]])
				 >> (rotate0toNW[diagNW_start[to_square]])
				 & ones[diagNW_length[to_square]]];
    if (NWSLIDER & opp_king)
      return 1;
  } else if ((move->piece == PAWN && (move->type & (QUEEN_PROMOTION_MOVE
						    | ROOK_PROMOTION_MOVE)))
	     && (horiz_vert_cross[to_square] & opp_king)) {
    HSLIDER =
      attack.hslider[to_square][(((board->all_pieces[WHITE]
				   | board->all_pieces[BLACK]
				   | square[move->to_square]) & ~(square[move->from_square]))
				 >> ((to_square/8)*8)) & 255];
    if (HSLIDER & opp_king)
      return 1;
    VSLIDER =
      attack.vslider[to_square][(((board->rot90_pieces[WHITE]
				   | board->rot90_pieces[BLACK]
				   | square[rotate0to90[to_square]])
				 & ~(square[rotate0to90[from_square]]))
				 >> (rotate0to90[to_square]/8)*8) & 255];
    if (VSLIDER & opp_king)
      return 1;
  } else if ((move->piece == PAWN && (move->type & (QUEEN_PROMOTION_MOVE
						    | BISHOP_PROMOTION_MOVE)))
	     && (nw_ne_cross[to_square] & opp_king)) {
    NESLIDER =
      attack.sliderNE[to_square][((board->rotNE_pieces[WHITE]
				   | board->rotNE_pieces[BLACK]
				   | square[rotate0toNE[to_square]])
				  & ~(square[rotate0toNE[from_square]]))
				 >> (rotate0toNE[diagNE_start[to_square]])
				 & ones[diagNE_length[to_square]]];
    if (NESLIDER & opp_king)
      return 1;
    NWSLIDER =
      attack.sliderNW[to_square][((board->rotNW_pieces[WHITE]
				   | board->rotNW_pieces[BLACK]
				   | square[rotate0toNW[to_square]])
				  & ~(square[rotate0toNW[from_square]]))
				 >> (rotate0toNW[diagNW_start[to_square]])
				 & ones[diagNW_length[to_square]]];
    if (NWSLIDER & opp_king)
      return 1;
  }

  /* Here we need to check if the from_square and to_square and the
     opponent king are lined up on a ray. Otherwise the following could
     happen: White rook on e1, white pawn on e2, black king on e8, and
     white makes the move e2e4. If we now would call attacks_behind()
     and remove the pawn, then we would falsely get the result that this
     move is a checking move. (If from_square, to_square and opponent
     king are lined up on a ray, then it will have been discovered above
     already if this is truly a checking move.) */
  dir = direction[from_square][get_first_bitpos(opp_king)];
  if (dir != 0) //from_square and opp king on same ray
    if (get_ray(get_first_bitpos(opp_king),-dir) & square[move->to_square])
      return 0; //to_square is on the same ray

  /* Then check if there are any discovered checks. */
  if (move->type & PASSANT_MOVE) {
    /* No discovered checks if to_square and opp king are on the same ray. */
    bitboard opp_pawn;
    dir = direction[to_square][get_first_bitpos(opp_king)];
    if (dir == 8 || dir == -8)
      return 0;

    opp_pawn = horiz_vert_cross[from_square] & col_bitboard[to_square%8];
    /* If neither from_square and the opponent king nor the taken pawn
       and the opponent king are lined up on the same ray, then there
       cannot be any discovered checks. */
    dir = direction[from_square][get_first_bitpos(opp_king)];
    if (dir == 0)
      dir = direction[get_first_bitpos(opp_pawn)][get_first_bitpos(opp_king)];
    if (dir == 0)
      return 0;
    if (attacks_behind(board,get_first_bitpos(opp_king),
		       square[move->from_square] | opp_pawn,-dir)
	& board->all_pieces[Color])
      return 1;
  } else if ((horiz_vert_cross[from_square] & opp_king)
	     || (nw_ne_cross[from_square] & opp_king)) {
    if (attacks_behind(board,get_first_bitpos(opp_king),square[move->from_square],-dir)
	& board->all_pieces[Color])
      return 1;
  }

  return 0;
}

/*------------------------------------------------------------------
 | This function returns the value of exhaustive exchanges on the  |
 | target square. If the return value is positive, then it's good  |
 | for the player at move to make this exchange. The source_sq and |
 | source_piece arguments return the source square and piece type  |
 | of the piece to make the first exchange. If no moves could be   |
 | done, then -INFTY is returned.                                  |
 ------------------------------------------------------------------*/
int swap(struct board *board, int target, int target_pieceval,
	 int *source_sq, int *source_piece) {
  int capturelist[32];
  bitboard attacks_from;
  bitboard moved_pieces = 0;
  int counter = 0;
  int sq, s_piece = 0, dir;
  int color = board->color_at_move;
  int old_val;

  attacks_from = get_attacks(board,target);

  if (!(attacks_from & board->all_pieces[color]))
    return -INFTY;  //if no attacks by color at move, then exit
  else if (target_pieceval == VAL_KING)
    return KINGTAKEN;  //if king can be taken

  old_val = target_pieceval;
  while (attacks_from) {
    /* If the previous capture was done by the king, and we get here, it
       means that there are captures left, and since the king can never be
       taken, the previous move was actually not legal, so then we abort. */
    if (s_piece == KING)
      break;

    if (attacks_from & board->piece[color][PAWN]) {
      s_piece = PAWN;
      sq = get_first_bitpos(board->piece[color][PAWN] & attacks_from);
    } else if (attacks_from & board->piece[color][KNIGHT]) {
      s_piece = KNIGHT;
      sq = get_first_bitpos(board->piece[color][KNIGHT] & attacks_from);
    } else if (attacks_from & board->piece[color][BISHOP]) {
      s_piece = BISHOP;
      sq = get_first_bitpos(board->piece[color][BISHOP] & attacks_from);
    } else if (attacks_from & board->piece[color][ROOK]) {
      s_piece = ROOK;
      sq = get_first_bitpos(board->piece[color][ROOK] & attacks_from);
    } else if (attacks_from & board->piece[color][QUEEN]) {
      s_piece = QUEEN;
      sq = get_first_bitpos(board->piece[color][QUEEN] & attacks_from);
    } else if (attacks_from & board->piece[color][KING]) {
      s_piece = KING;
      sq = get_first_bitpos(board->piece[color][KING]);
    } else
      break;  //for example if black had 6 attacking pieces and white had 4

    /* If the capture was not done by a knight, then let's peek behind
       the piece to find uncovered attacks. */
    if (s_piece != KNIGHT) {
      moved_pieces |= square[sq];
      dir = direction[sq][target];
      attacks_from |= attacks_behind(board,target,moved_pieces,-dir);
    }

    if (counter == 0) {
      capturelist[counter] = old_val;
      *source_piece = s_piece;
      *source_sq = sq;
    } else
      capturelist[counter] = -capturelist[counter-1] + old_val;
    old_val = pieceval[s_piece];
    attacks_from &= ~(square[sq]);
    color = 1-color;
    counter++;
  }

  if (!counter)
    return -INFTY;

  while (--counter) {
    if (capturelist[counter] > -capturelist[counter-1])
      capturelist[counter-1] = -capturelist[counter];
  }

  return capturelist[0];
}

/*------------------------------------------------------------------
 | This function returns the value of exhaustive exchanges on the  |
 | target square. If the return value is positive, then it's good  |
 | for the player at move to make this exchange. The source_sq and |
 | source_piece arguments return the source square and piece type  |
 | of the piece to make the first exchange.                        |
 |                                                                 |
 | The difference between this function and the swap()-function is |
 | that this function avoids moves given in the avoid_first     |
 | parameter (as the first move).                                  |
 ------------------------------------------------------------------*/
int swap_avoid(struct board *board, int target, int target_pieceval,
	       int *source_sq, int *source_piece, bitboard avoid_first) {
  int capturelist[32];
  bitboard attacks_from;
  bitboard moved_pieces = 0;
  int counter = 0;
  int sq, s_piece = 0, dir;
  int color = board->color_at_move;
  int old_val;

  attacks_from = get_attacks(board,target) & ~avoid_first;

  if (!(attacks_from & board->all_pieces[color]))
    return -INFTY;  //if no attacks by color at move, then exit
  else if (target_pieceval == VAL_KING)
    return KINGTAKEN;  //if king can be taken

  old_val = target_pieceval;
  while (attacks_from) {
    /* If the previous capture was done by the king, and we get here, it
       means that there are captures left, and since the king can never be
       taken, the previous move was actually not legal, so then we abort. */
    if (s_piece == KING)
      break;

    if (attacks_from & board->piece[color][PAWN]) {
      s_piece = PAWN;
      sq = get_first_bitpos(board->piece[color][PAWN] & attacks_from);
    } else if (attacks_from & board->piece[color][KNIGHT]) {
      s_piece = KNIGHT;
      sq = get_first_bitpos(board->piece[color][KNIGHT] & attacks_from);
    } else if (attacks_from & board->piece[color][BISHOP]) {
      s_piece = BISHOP;
      sq = get_first_bitpos(board->piece[color][BISHOP] & attacks_from);
    } else if (attacks_from & board->piece[color][ROOK]) {
      s_piece = ROOK;
      sq = get_first_bitpos(board->piece[color][ROOK] & attacks_from);
    } else if (attacks_from & board->piece[color][QUEEN]) {
      s_piece = QUEEN;
      sq = get_first_bitpos(board->piece[color][QUEEN] & attacks_from);
    } else if (attacks_from & board->piece[color][KING]) {
      s_piece = KING;
      sq = get_first_bitpos(board->piece[color][KING]);
    } else
      break;  //for example if black had 6 attacking pieces and white had 4

    /* If the capture was not done by a knight, then let's peek behind
       the piece to find uncovered attacks. */
    if (s_piece != KNIGHT) {
      moved_pieces |= square[sq];
      dir = direction[sq][target];
      attacks_from |= attacks_behind(board,target,moved_pieces,-dir);
    }

    if (counter == 0) {
      capturelist[counter] = old_val;
      *source_piece = s_piece;
      *source_sq = sq;
      attacks_from |= get_attacks(board,target);
    } else
      capturelist[counter] = -capturelist[counter-1] + old_val;
    old_val = pieceval[s_piece];
    attacks_from &= ~(square[sq]);
    color = 1-color;
    counter++;
  }

  if (!counter)
    return -INFTY;

  while (--counter) {
    if (capturelist[counter] > -capturelist[counter-1])
      capturelist[counter-1] = -capturelist[counter];
  }

  return capturelist[0];
}
