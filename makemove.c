/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "makemove.h"
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

extern bitboard square[64];
extern int64 CHANGE_COLOR;
extern int rotate0to90[64];
extern int rotate0toNE[64];
extern int rotate0toNW[64];
extern char promotion_piece[129];
extern int64 zobrist[6][2][64];
extern int64 zobrist_castling[2][5];
extern int64 zobrist_passant[64];
extern bitboard pawn_start[2];
extern bitboard pawn_passantrow[2];
extern int pieceval[6];
extern bitboard horiz_vert_cross[64];
extern bitboard rook_starting_square[2];
extern bitboard col_bitboard[8];
extern int square2col[64];

void make_nullmove(struct board *board) {
  board->color_at_move = 1 - board->color_at_move;
  board->zobrist_key ^= CHANGE_COLOR;
  board->moves_left_to_draw -= 1; //TODO: Seems like crafty sets this to 100
  (board->move_number)++;
  if (board->passant != 0) {
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
    board->passant = 0;
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
  }
}

void unmake_nullmove(struct board *board, char old_passant) {
  board->color_at_move = 1 - board->color_at_move;
  board->zobrist_key ^= CHANGE_COLOR;
  board->moves_left_to_draw += 1;
  (board->move_number)--;
  if (board->passant != old_passant) {
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
    board->passant = old_passant;
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
  }
}

void add_piece(struct board *board, int color, char piecetype, int sq) {
  board->piece[color][piecetype] |= square[sq];
  board->all_pieces[color] |= square[sq];
  board->rot90_pieces[color] |= square[rotate0to90[sq]];
  board->rotNE_pieces[color] |= square[rotate0toNE[sq]];
  board->rotNW_pieces[color] |= square[rotate0toNW[sq]];
  board->zobrist_key ^= zobrist[piecetype][color][sq];
  if (piecetype == PAWN) {
    board->zobrist_pawn_key ^= zobrist[PAWN][color][sq];
    board->material_pawns[color] += VAL_PAWN;
  } else {
    board->material_pieces[color] += pieceval[piecetype];
    board->nbr_pieces[color] += 1;
  }
  board->boardarr[sq] = (char) piecetype;
}

void remove_piece(struct board *board, int color, char piecetype, int sq) {
  board->piece[color][piecetype] &= ~(square[sq]);
  board->all_pieces[color] &= ~(square[sq]);
  board->rot90_pieces[color] &= ~(square[rotate0to90[sq]]);
  board->rotNE_pieces[color] &= ~(square[rotate0toNE[sq]]);
  board->rotNW_pieces[color] &= ~(square[rotate0toNW[sq]]);
  board->zobrist_key ^= zobrist[piecetype][color][sq];
  if (piecetype == PAWN) {
    board->zobrist_pawn_key ^= zobrist[PAWN][color][sq];
    board->material_pawns[color] -= VAL_PAWN;
  } else {
    board->material_pieces[color] -= pieceval[piecetype];
    board->nbr_pieces[color] -= 1;
  }
  board->boardarr[sq] = EMPTY;
}

void unmakemove(struct board *board, struct move *move, int depth) {
  int color, oppcolor;

  color = Oppcolor;
  oppcolor = Color;

  board->color_at_move = color;
  board->zobrist_key ^= CHANGE_COLOR;

  /*----------------------------------------------
   | First take care of castling specific stuff. |
   ----------------------------------------------*/
  if (move->piece == KING) {
    if (move->type & CASTLING_MOVE) {
      /* Update castling status and move the rook. */
      char rookfsquare, rooktsquare;
      board->zobrist_key ^= zobrist_castling[color][CASTLED];
      board->castling_status[color] = move->old_castling_status[color];
      if (move->old_castling_status[color] & LONG_CASTLING_OK)
	board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
      if (move->old_castling_status[color] & SHORT_CASTLING_OK)
	board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];

      if (move->to_square == 6 || move->to_square == 62) { //short castling
	rookfsquare = move->to_square - 1;
	rooktsquare = move->to_square + 1;
      } else { //long castling
	rookfsquare = move->to_square + 1;
	rooktsquare = move->to_square - 2;
      }
      remove_piece(board,color,ROOK,rookfsquare);
      add_piece(board,color,ROOK,rooktsquare);
    } else {
      /* Update the castling status. */
      if (move->old_castling_status[color] != CASTLED
	  && move->old_castling_status[color] != 0) {
	if ((move->old_castling_status[color] & LONG_CASTLING_OK)
	    != (board->castling_status[color] & LONG_CASTLING_OK))
	  board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
	if ((move->old_castling_status[color] & SHORT_CASTLING_OK)
	    != (board->castling_status[color] & SHORT_CASTLING_OK))
	  board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];
      }
      board->castling_status[color] = move->old_castling_status[color];
    }
  } else if ((move->piece == ROOK
	      && move->old_castling_status[color] != CASTLED
	      && move->old_castling_status[color] != 0)
	     && (rook_starting_square[color] & square[move->from_square])) {
    /* Update the castling status. */
    if ((move->from_square == 0 || move->from_square == 56)
	&& (move->old_castling_status[color] & LONG_CASTLING_OK)) {
      board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
      board->castling_status[color] |= LONG_CASTLING_OK;
    } else if ((move->from_square == 7 || move->from_square == 63)
	&& (move->old_castling_status[color] & SHORT_CASTLING_OK)) {
      board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];
      board->castling_status[color] |= SHORT_CASTLING_OK;
    }
  }
  if ((move->type & CAPTURE_MOVE) && (move->old_to_square == ROOK)
      && (rook_starting_square[oppcolor] & square[move->to_square])) {
    if (move->old_castling_status[oppcolor] != CASTLED
	&& move->old_castling_status[oppcolor] != 0) {
      /* If opponent rook was taken, then update opponent's castling rights. */
      if ((move->to_square == 0 || move->to_square == 56)
	  && (move->old_castling_status[oppcolor] & LONG_CASTLING_OK)) {
	board->zobrist_key ^= zobrist_castling[oppcolor][LONG_CASTLING_OK];
      } else if ((move->to_square == 7 || move->to_square == 63)
		 && (move->old_castling_status[oppcolor] & SHORT_CASTLING_OK)) {
	board->zobrist_key ^= zobrist_castling[oppcolor][SHORT_CASTLING_OK];
      }
      board->castling_status[oppcolor] = move->old_castling_status[oppcolor];
    }
  }

  /*--------------------------------------
   | Then take care of moving the piece. |
   --------------------------------------*/
  if (move->type & PROMOTION_MOVE) {
    remove_piece(board,color,promotion_piece
		 [move->type & PROMOTION_MOVE],move->to_square);
  } else {
    remove_piece(board,color,move->piece,move->to_square);
  }
  add_piece(board,color,move->piece,move->from_square);

  /*-----------------------------------------------------------
   | Update assorted status information.                      |
   -----------------------------------------------------------*/
  (board->move_number)--;
  if (board->passant != move->old_passant) {
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
    board->passant = move->old_passant;
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
  }
  if (move->piece == PAWN) {
    board->moves_left_to_draw = move->old_moves_left_to_draw;
    if (move->type & PROMOTION_MOVE || move->type & CAPTURE_MOVE)
      board->captures[color] -= depth;
  } else if (move->type & CAPTURE_MOVE) {
    board->captures[color] -= depth;
    board->moves_left_to_draw = move->old_moves_left_to_draw;
  } else {
    ++(board->moves_left_to_draw);
  }

  /*---------------------------------------------------
   | Finally take care of the removed piece (if any). |
   ---------------------------------------------------*/
  if (move->type & CAPTURE_MOVE) {
    /* Pawns taken with enpassant are removed differently
       than other captures. */
    if (move->type & PASSANT_MOVE) {
      int opp_pawn =
	get_first_bitpos(horiz_vert_cross[move->from_square]
			 & col_bitboard[square2col[move->to_square]]);
      add_piece(board,oppcolor,PAWN,opp_pawn);
    } else {
      if (move->old_to_square < PAWN || move->old_to_square > QUEEN) {
	infolog("Error, move->old_to_square is faulty.");
	printf("Error, move->old_to_square = %d\n",move->old_to_square);
	showboard(board);
	printmove(move);
	printf("consistent_board = %d\n",consistent_board(board));
	exit(1);
      }
      add_piece(board,oppcolor,move->old_to_square,move->to_square);
    }
  }
}

void makemove(struct board *board, struct move *move, int depth) {
  int color, oppcolor;
  char new_passant;

  color = Color;
  oppcolor = Oppcolor;
  board->color_at_move = oppcolor;
  board->zobrist_key ^= CHANGE_COLOR;

  /*----------------------------------------------
   | First take care of castling specific stuff. |
   ----------------------------------------------*/
  if (move->piece == KING) {
    if (move->type & CASTLING_MOVE) {
      /* Update castling status and move the rook. */
      char rookfsquare, rooktsquare;
      if (board->castling_status[color] & LONG_CASTLING_OK)
	board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
      if (board->castling_status[color] & SHORT_CASTLING_OK)
	board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];
      board->zobrist_key ^= zobrist_castling[color][CASTLED];
      board->castling_status[color] = CASTLED;
      
      if (move->to_square == 6 || move->to_square == 62) { //short castling
	rookfsquare = move->to_square + 1;
	rooktsquare = move->to_square - 1;
      } else { //long castling
	rookfsquare = move->to_square - 2;
	rooktsquare = move->to_square + 1;
      }
      remove_piece(board,color,ROOK,rookfsquare);
      add_piece(board,color,ROOK,rooktsquare);
    } else {
      /* Update the castling status. */
      if (board->castling_status[color] != CASTLED
	  && board->castling_status[color] != 0) {

	if (board->castling_status[color] & LONG_CASTLING_OK)
	  board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
	if (board->castling_status[color] & SHORT_CASTLING_OK)
	  board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];
	board->castling_status[color] = 0;
      }
    }
  } else if (move->piece == ROOK && board->castling_status[color] != CASTLED
	     && board->castling_status[color] != 0
	     && (rook_starting_square[color] & square[move->from_square])) {
    /* Update the castling status. */
    if ((move->from_square == 0 || move->from_square == 56)
	&& (board->castling_status[color] & LONG_CASTLING_OK)) {
      board->zobrist_key ^= zobrist_castling[color][LONG_CASTLING_OK];
      board->castling_status[color] &= ~LONG_CASTLING_OK;
    } else if ((move->from_square == 7 || move->from_square == 63)
	&& (board->castling_status[color] & SHORT_CASTLING_OK)) {
      board->zobrist_key ^= zobrist_castling[color][SHORT_CASTLING_OK];
      board->castling_status[color] &= ~SHORT_CASTLING_OK;
    }
  }
  if ((move->type & CAPTURE_MOVE)
      && (board->piece[oppcolor][ROOK] & square[move->to_square])
      && (rook_starting_square[oppcolor] & square[move->to_square])) {
    if (board->castling_status[oppcolor] != CASTLED
	&& board->castling_status[oppcolor] != 0) {
      /* If opponent rook is taken, then update opponent's castling rights. */
      if ((move->to_square == 0 || move->to_square == 56)
	  && (board->castling_status[oppcolor] & LONG_CASTLING_OK)) {
	board->zobrist_key ^= zobrist_castling[oppcolor][LONG_CASTLING_OK];
	board->castling_status[oppcolor] &= ~LONG_CASTLING_OK;
      } else if ((move->to_square == 7 || move->to_square == 63)
		 && (board->castling_status[oppcolor] & SHORT_CASTLING_OK)) {
	board->zobrist_key ^= zobrist_castling[oppcolor][SHORT_CASTLING_OK];
	board->castling_status[oppcolor] &= ~SHORT_CASTLING_OK;
      }
    }
  }

  /*------------------------------------------------
   | Then take care of the removed piece (if any). |
   ------------------------------------------------*/
  if (move->type & CAPTURE_MOVE) {
    /* Pawns taken with enpassant are removed differently
       than other captures. */
    if (move->type & PASSANT_MOVE) {
      int opp_pawn =
	get_first_bitpos(horiz_vert_cross[move->from_square]
			 & col_bitboard[square2col[move->to_square]]);
      remove_piece(board,oppcolor,PAWN,opp_pawn);
      move->old_to_square = EMPTY;
    } else {
      move->old_to_square = board->boardarr[move->to_square];
      remove_piece(board,oppcolor,
		   board->boardarr[move->to_square],move->to_square);
    }
  } else {
    move->old_to_square = EMPTY;
  }

  /*-----------------------------------------------------------
   | Update assorted status information.                      |
   |                                                          |
   | Note: According to the official FIDE rules (see          |
   | http://www.fide.com/official/handbook.asp?level=EE101)   |
   | the moves_left_to_draw counter should not be zeroed for  |
   | castling moves. However for 3 position repetitions the   |
   | castling rights matter (see section 9.2 and 9.3b).       |
   | As far as passant goes, changed passant rights should be |
   | reflected in the hashkey (which in turn affects the      |
   | 3 position repetition rule), but should not affect the   |
   | moves_left_to_draw. (Esim. if white played a2a4 from the |
   | starting position, and black answers with g8f6, then the |
   | knight move shouldn't set the moves_left_to_draw to 100  |
   | although the knight move changed the passant rights to   |
   | zero.)                                                   |
   -----------------------------------------------------------*/
  (board->move_number)++;
  new_passant = 0;
  if (move->piece == PAWN) {
    board->moves_left_to_draw = 100;
    if (move->type & PROMOTION_MOVE || move->type & CAPTURE_MOVE)
      board->captures[color] += depth;
    if ((square[move->from_square] & pawn_start[color])
	&& (square[move->to_square] & pawn_passantrow[color])) {
      if (color == WHITE)
	new_passant = move->from_square-8;
      else
	new_passant = move->from_square+8;
    }
  } else if (move->type & CAPTURE_MOVE) {
    board->captures[color] += depth;
    board->moves_left_to_draw = 100;
  } else {
    --(board->moves_left_to_draw);
  }
  if (new_passant != board->passant) {
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
    board->passant = new_passant;
    board->zobrist_key ^= zobrist_passant[(int)board->passant];
  }

  /*-----------------------------------------
   | Finally take care of moving the piece. |
   -----------------------------------------*/
  remove_piece(board,color,move->piece,move->from_square);
  if (move->type & PROMOTION_MOVE) {
    add_piece(board,color,promotion_piece
	      [move->type & PROMOTION_MOVE],move->to_square);
  } else {
    add_piece(board,color,move->piece,move->to_square);
  }
}

