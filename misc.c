/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "parse.h"
#include "genmoves.h"
#include "makemove.h"
#include "hash.h"
#ifndef _CONFIG_
#define _CONFIG_
#include "config.h"
#endif

extern bitboard square[64];
extern struct attack attack;
extern bitboard white_squares;
extern bitboard black_squares;
extern int failed_cutoff_count[2][16][64];
extern int success_cutoff_count[2][16][64];

void infolog(const char *text) {
  FILE *fp;
  const char *filename = INFOLOGFILE;

  fp = fopen(filename,"a");
  if (fp == NULL)
    fprintf(stderr,"Could not open %s\n",INFOLOGFILE);
  fprintf(fp,"%s\n",text);
  fclose(fp);
}

/* Prints without a newline at the end. */
void testsuitelog(const char *text) {
  FILE *fp;
  const char *filename = TESTSUITELOGFILE;

  fp = fopen(filename,"a");
  if (fp == NULL)
    fprintf(stderr,"Could not open %s\n",INFOLOGFILE);
  fprintf(fp,"%s",text);
  fclose(fp);
}

#ifdef AM_DEBUG
void debuglog(const char *text) {
  FILE *fp;
  const char *filename = DEBUGLOGFILE;

  fp = fopen(filename,"a");
  if (fp == NULL)
    fprintf(stderr,"Could not open %s\n",DEBUGLOGFILE);
  fprintf(fp,"%s\n",text);
  fclose(fp);
}
#endif

void resultlog(int status, int color, int engine_col) {
  FILE *fp;
  const char *filnamn = RESULTLOGFILE;
  char *str;
  
  str = (char *) malloc(100*sizeof(char));
  if (status == 2) {
    if (color == engine_col)
      sprintf(str,"%s lost.",PACKAGE_STRING);
    else
      sprintf(str,"%s won.",PACKAGE_STRING);
  } else if (status == 1) {
    sprintf(str,"1/2-1/2 {Stalemate}");
  } else if (status == 3) {
    sprintf(str,"1/2-1/2 {50 move rule}");
  } else if (status == 4) {
    sprintf(str,"1/2-1/2 {Insufficient material}");
  } else {
    sprintf(str,"1/2-1/2 {Draw by repetition}");
  }

  fp = fopen(filnamn,"a");
  if (fp == NULL)
    fprintf(stderr,"Could not open %s\n",RESULTLOGFILE);
  fprintf(fp,"%s\n",str);
  fclose(fp);
  free(str);
}

int draw_score() {
  return 0;
}

/* This function returns 1 if the move is legal, and 0 if it's illegal. */
int legal_move(struct move move_to_check, struct board *board) {
  struct moves moves[16];
  int movables, piece_nbr;
  bitboard target;
  struct move move;
  struct board newpos;

  /* Make a list of all the possible pseudo-moves in this position,
     and see if move_to_check is in the list. If it is, then make
     the move, and see if the move will get oneself into check.
     If one gets in check, then the move is illegal, otherwise it's legal. */

  /* This program only generates queen promotions, and no minor promotions.
     So if the opponent tries to make a minor promotion, there will be no
     match between move_to_check and any of the moves in the generated
     movelist. A way to solve that problem is to check if the opponent's
     move is a promotion. If it is, then we set move_to_check.type
     to QUEEN_PROMOTION_MOVE, and then we can have our match. */
  if (move_to_check.type & PROMOTION_MOVE) {
    move_to_check.type = move_to_check.type & (~PROMOTION_MOVE);
    move_to_check.type = move_to_check.type | QUEEN_PROMOTION_MOVE;
  }

  generate_moves(board,moves,&movables);

  move.old_castling_status[WHITE] = board->castling_status[WHITE];
  move.old_castling_status[BLACK] = board->castling_status[BLACK];
  move.old_passant = board->passant;
  move.old_moves_left_to_draw = board->moves_left_to_draw;
  piece_nbr = 0;
  while (piece_nbr < movables) {
    while (moves[piece_nbr].targets != 0) {
      target = getlsb(moves[piece_nbr].targets);
      move.from_square = (char) get_first_bitpos(moves[piece_nbr].source);
      move.to_square = (char) get_first_bitpos(target);
      move.piece = moves[piece_nbr].piece;
      move.type = get_movetype(board,&move);
      /* move.value is something we don't know about until after a search,
         so we don't compare the move.value */
      if ((move.from_square == move_to_check.from_square)
	  && (move.to_square == move_to_check.to_square)
	  && (move.piece == move_to_check.piece)
	  && (move.type == move_to_check.type)) {
	makemove(board,&move,0);
	//newpos.color_at_move = Color;  //make sure we check the own color
	if (!opp_in_check(board)) {
	  unmakemove(board,&move,0);
	  return 1;     //Now we know that the move is legal.
	} else {
	  unmakemove(board,&move,0);
	  return 0;     //illegal move
	}
      }
      moves[piece_nbr].targets &= ~target;
    }
    piece_nbr++;
  }
  /* If we get here, it means that move_to_check was not in the movelist,
     and is therefore illegal. */
  return 0;
}

/* This function returns 0 if game is not ended, 1 if stalemate,
   2 if check mate, 3 if draw by the 50 move rule, 4 if
   insufficient material, and 5 if draw by repetition */
int game_ended(struct board *board) {
  struct moves moves[16];
  int movables, piece_nbr;
  bitboard target;
  struct move move;
  struct board newpos;

  /* First check for draws because of insufficient material on the board.
     Note that KNNK is not a draw according to FIDE rules! Although a
     mate cannot be forced it's still possible to mate if the enemy
     makes a mistake. (The FIDE rules say that "The game is drawn when
     a position is reached from which a checkmate cannot occur by any
     possible series of legal moves, even with the most unskilled play.")
     The only positions that are draws because of insufficient material
     are: KBKB(same colored bishops), KBK, KNK and KK. */
  if (board->nbr_pieces[WHITE] <= 1 && board->nbr_pieces[BLACK] <= 1
      && board->material_pawns[WHITE] == 0
      && board->material_pawns[BLACK] == 0) {

    if (board->piece[WHITE][BISHOP] && board->piece[BLACK][BISHOP]) {
      /* KBKB is a draw if the bishops are on the same color. */
      if (((board->piece[WHITE][BISHOP] & white_squares)
	   && (board->piece[BLACK][BISHOP] & white_squares))
	  || ((board->piece[WHITE][BISHOP] & black_squares)
	      && (board->piece[BLACK][BISHOP] & black_squares)))
	return 4; //KBKB, bishops on same colored squares
    } else if (board->nbr_pieces[WHITE] == 0
	       && board->material_pieces[BLACK] < (VAL_KING + VAL_ROOK)) {
      return 4; //KBK, KNK or KK
    } else if (board->nbr_pieces[BLACK] == 0
	       && board->material_pieces[WHITE] < (VAL_KING + VAL_ROOK)) {
      return 4; //KBK, KNK or KK
    }
  }

  /* Basically what we do in this function, is this:
     if (can make move which doesn't place him in check)
       return game is not over;
     else {
       if (in check now)
         return game is lost;
       else
         return stalemate;
     }
  */

  if (generate_moves(board,moves,&movables) != 0) {
    /* If we get here, it means that the last move by the opponent
       left him in check, which is illegal. */
    infolog("ERROR! ILLEGAL POSITION!");
    printf("ERROR! ILLEGAL POSITION!\n");
    exit(1);
  }

  /* Try to make all the moves, and see if the player is still in check. */
  piece_nbr = 0;
  move.old_castling_status[WHITE] = board->castling_status[WHITE];
  move.old_castling_status[BLACK] = board->castling_status[BLACK];
  move.old_passant = board->passant;
  move.old_moves_left_to_draw = board->moves_left_to_draw;
  while (piece_nbr < movables) {
    while (moves[piece_nbr].targets != 0) {
      target = getlsb(moves[piece_nbr].targets);
      move.from_square = (char) get_first_bitpos(moves[piece_nbr].source);
      move.to_square = (char) get_first_bitpos(target);
      move.piece = moves[piece_nbr].piece;
      move.type = get_movetype(board,&move);
      makemove(board,&move,0);
      //newpos.color_at_move = Color;  //make sure we check the own color
      if (!opp_in_check(board)) {
	unmakemove(board,&move,0);
	/* The player has a move that will not place him in check.
	   So the game is not over, unless the 50 move rule or
	   draw by repetition rule apply. */
	if (board->moves_left_to_draw <= 0)
	  return 3;
	else if (is_draw_by_repetition(board))
	  return 5;
	else
	  return 0;
      }
      unmakemove(board,&move,0);
      moves[piece_nbr].targets = moves[piece_nbr].targets & ~target;
    }
    piece_nbr++;
  }
  /* Every move places him in check. The game is over. */
  if (in_check(board))
    return 2;     //the game is lost
  else
    return 1;     //stalemate
}

/* Prints out a move on the screen. */
void printmove(struct move *move) {
  char *dragstr;

  dragstr = (char *) malloc(20*sizeof(char));
  move2str(move,dragstr);
  printf("move %s\n",dragstr);
  free(dragstr);
}

/* Prints a bitboard nicely. */
void printbitboard(bitboard brd) {
  int i;

  for (i = 0; i < 64; i++) {
    if (brd & square[i])
      printf("1");
    else
      printf("0");
    if (i == 55 || i == 47 || i == 39 || i == 31
	|| i == 23 || i == 15 || i == 7)
      printf("\n");
  }
  printf("\n");
}

/* Prints a char nicely (msb-lsb). */
void printchar(unsigned char lg) {
  int i;
  bitboard brd = (bitboard) lg;

  for (i = 7; i >= 0; i--) {
    if (brd & square[i])
      printf("1");
    else
      printf("0");
  }
  printf("\n");
}

/* This function returns the ordereth root of number. */
double nroot(int order, double number) {
  double x = 10;
  int i;

  for (i = 0; i < 10; i++) {
    x = (double) ((double)1/order)*((double)(order-1)*x + (double)(number/pow(x,order-1)));
  }
  return x;
}

/* This function takes a time value in unit millisecond,
   and prints it nicely. */
void printprettytime(int milliseconds) {
  if (milliseconds < 1000)
    printf("%dms",milliseconds);
  else if (milliseconds < 10000)
    printf("%3.2fs",(double)milliseconds/1000);
  else if (milliseconds < 60000)
    printf("%4.2fs",(double)milliseconds/1000);
  else if (milliseconds < 600000)
    printf("%3.2fm",(double)milliseconds/60000);
  else if (milliseconds < 3600000)
    printf("%4.2fm",(double)milliseconds/60000);
  else
    printf("%3.2fh",(double)milliseconds/3600000);
}

/* This function takes a value in unit nodes and total time
   int milliseconds and prints it nicely. */
void printprettynodes(int64 nodes, int milliseconds) {
  int64 nps;

  if (milliseconds == 0) {
    printf("inf");
    return;
  }
  nps = (int64)((double)(nodes)/((double)milliseconds/1000));
  if (nps < 1000)
    printf("%dNPS",nps);
  else if (nps < 10000)
    printf("%3.2fkNPS",(double)nps/1000);
  else if (nps < 100000)
    printf("%4.2fkNPS",(double)nps/1000);
  else if (nps < 1000000)
    printf("%5.2fkNPS",(double)nps/1000);
  else if (nps < 10000000)
    printf("%3.2fmegaNPS",(double)nps/1000000);
  else
    printf("%4.2fmegaNPS",(double)nps/1000000);
}

/* Zero the success/failure count for non-captures
   and non-promotions*/
void zero_success_failure_count() {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 16; j++)
      for (k = 0; k < 64; k++) {
	success_cutoff_count[i][j][k] = 0;
	failed_cutoff_count[i][j][k] = 0;
      }
}

/* This function is for debugging purposes, and it checks if the board
   is in a consistent state. */
int consistent_board(struct board *board) {
  int i, j, k, returnval = 1;

  if (board->passant) {
    if (board->passant / 8 != 2 && board->passant / 8 != 5) {
      printf("passant on a faulty rank (passant = %d)\n",board->passant);
      returnval = 0;
    }
    if (board->boardarr[board->passant-8] != PAWN
	&& board->boardarr[board->passant+8] != PAWN) {
      printf("No pawn causing passant found.\n");
      returnval = 0;
    }
  }

  if (bitcount(board->piece[WHITE][KING]) != 1) {
    printf("nbr white kings: %d\n",bitcount(board->piece[WHITE][KING]));
    returnval = 0;
  }
  if (bitcount(board->piece[WHITE][PAWN]) > 8) {
    printf("nbr white pawns: %d\n",bitcount(board->piece[WHITE][PAWN]));
    returnval = 0;
  }
  if (bitcount(board->piece[WHITE][ROOK]) > 2) {
    printf("nbr white rooks: %d\n",bitcount(board->piece[WHITE][ROOK]));
    returnval = 0;
  }
  if (bitcount(board->piece[WHITE][BISHOP]) > 2) {
    printf("nbr white bishops: %d\n",bitcount(board->piece[WHITE][BISHOP]));
    returnval = 0;
  }
  if (bitcount(board->piece[WHITE][KNIGHT]) > 2) {
    printf("nbr white knights: %d\n",bitcount(board->piece[WHITE][KNIGHT]));
    returnval = 0;
  }
  if (bitcount(board->piece[WHITE][QUEEN]) > 9) {
    printf("nbr white queens: %d\n",bitcount(board->piece[WHITE][QUEEN]));
    returnval = 0;
  }


  if (bitcount(board->piece[BLACK][KING]) != 1) {
    printf("nbr black kings: %d\n",bitcount(board->piece[BLACK][KING]));
    returnval = 0;
  }
  if (bitcount(board->piece[BLACK][PAWN]) > 8) {
    printf("nbr black pawns: %d\n",bitcount(board->piece[BLACK][PAWN]));
    returnval = 0;
  }
  if (bitcount(board->piece[BLACK][ROOK]) > 2) {
    printf("nbr black rooks: %d\n",bitcount(board->piece[BLACK][ROOK]));
    returnval = 0;
  }
  if (bitcount(board->piece[BLACK][BISHOP]) > 2) {
    printf("nbr black bishops: %d\n",bitcount(board->piece[BLACK][BISHOP]));
    returnval = 0;
  }
  if (bitcount(board->piece[BLACK][KNIGHT]) > 2) {
    printf("nbr black knights: %d\n",bitcount(board->piece[BLACK][KNIGHT]));
    returnval = 0;
  }
  if (bitcount(board->piece[BLACK][QUEEN]) > 9) {
    printf("nbr black queens: %d\n",bitcount(board->piece[BLACK][QUEEN]));
    returnval = 0;
  }

  for (i = 0; i < 64; i++) {
    if (board->boardarr[i] == EMPTY) {
      for (j = 0; j < 2; j++)
	for (k = 0; k < 6; k++)
	  if (board->piece[j][k] & square[i]) {
	    printf("boardarr.EMPTY.piece: i = %d, j = %d, k = %d\n",i,j,k);
	    returnval = 0;
	  }
      for (j = 0; j < 2; j++)
	if (board->all_pieces[j] & square[i]) {
	  printf("boardarr.EMPTY.all_pieces: i = %d, j = %d\n",i,j);
	  returnval = 0;
	}
    } else if (board->boardarr[i] == PAWN) {
      if (!(board->piece[WHITE][PAWN] & square[i])
	  && !(board->piece[BLACK][PAWN] & square[i])) {
	printf("boardarr.PAWN.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.PAWN.all_pieces: i = %d\n",i);
	returnval = 0;
      }
      if (i < 8 || i > 55) {
	printf("Pawn found on square %d\n",i);
	returnval = 0;
      }
    } else if (board->boardarr[i] == KNIGHT) {
      if (!(board->piece[WHITE][KNIGHT] & square[i])
	  && !(board->piece[BLACK][KNIGHT] & square[i])) {
	printf("boardarr.KNIGHT.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.KNIGHT.all_pieces: i = %d\n",i);
	returnval = 0;
      }
    } else if (board->boardarr[i] == BISHOP) {
      if (!(board->piece[WHITE][BISHOP] & square[i])
	  && !(board->piece[BLACK][BISHOP] & square[i])) {
	printf("boardarr.BISHOP.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.BISHOP.all_pieces: i = %d\n",i);
	returnval = 0;
      }
    } else if (board->boardarr[i] == ROOK) {
      if (!(board->piece[WHITE][ROOK] & square[i])
	  && !(board->piece[BLACK][ROOK] & square[i])) {
	printf("boardarr.ROOK.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.ROOK.all_pieces: i = %d\n",i);
	returnval = 0;
      }
    } else if (board->boardarr[i] == QUEEN) {
      if (!(board->piece[WHITE][QUEEN] & square[i])
	  && !(board->piece[BLACK][QUEEN] & square[i])) {
	printf("boardarr.QUEEN.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.QUEEN.all_pieces: i = %d\n",i);
	returnval = 0;
      }
    } else if (board->boardarr[i] == KING) {
      if (!(board->piece[WHITE][KING] & square[i])
	  && !(board->piece[BLACK][KING] & square[i])) {
	printf("boardarr.KING.piece: i = %d\n",i);
	returnval = 0;
      }
      if (!(board->all_pieces[WHITE] & square[i])
	  && !(board->all_pieces[BLACK] & square[i])) {
	printf("boardarr.KING.all_pieces: i = %d\n",i);
	returnval = 0;
      }
    } else {
      printf("boardarr.unknown_value = %d (i = %d)\n",board->boardarr[i],i);
      returnval = 0;
    }
  }
  return returnval;
}
