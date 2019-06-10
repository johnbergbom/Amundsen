/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "display.h"
#include <stdio.h>
#include "bitboards.h"
//#include "misc.h"
#include "parse.h"
#include "alfabeta.h"

extern bitboard square[64];

char getPieceLetter(int piece, int color) {
  char ret_char = '?';
  switch(piece) {
    case PAWN : ret_char = (color == WHITE ? 'P' : 'p');
                break;
    case KNIGHT : ret_char = (color == WHITE ? 'N' : 'n');
                  break;
    case BISHOP : ret_char = (color == WHITE ? 'B' : 'b');
                  break;
    case ROOK : ret_char = (color == WHITE ? 'R' : 'r');
                break;
    case QUEEN : ret_char = (color == WHITE ? 'Q' : 'q');
                 break;
    case KING : ret_char = (color == WHITE ? 'K' : 'k');
                break;
    case EMPTY : ret_char = ' ';
                 break;
  }
  return ret_char;
}

void showboard(struct board *board) {
  int rad = 0, kol = 0;
  int temp[64];
  int i, square_nbr;
  int64 pieces;
  int color;

  for (kol = 0; kol < 64; kol++)
    temp[kol] = EMPTY;

  for (i = 0; i < 6; i++) {
    pieces = board->piece[WHITE][i];
    while (pieces != 0) {
      square_nbr = get_first_bitpos(pieces);
      temp[square_nbr] = i;
      pieces = pieces & ~square[square_nbr];
    }
  }

  for (i = 0; i < 6; i++) {
    pieces = board->piece[BLACK][i];
    while (pieces != 0) {
      square_nbr = get_first_bitpos(pieces);
      temp[square_nbr] = i;
      pieces = pieces & ~square[square_nbr];
    }
  }

  printf("  ---------------------------------\n");
  for (rad = 0; rad < 8; rad++) {
    printf("%d ",8 - rad);
    for (kol = 0; kol < 8; kol++) {
      printf("|");
      if (board->piece[WHITE][temp[rad*8+kol]] & square[rad*8+kol])
	color = WHITE;
      else
	color = BLACK;
      printf(" %c ",getPieceLetter(temp[rad*8 + kol],color));
      //NORMAL;
    }
    printf("|\n");
    if (rad < 7)
      printf("  |---+---+---+---+---+---+---+---|\n");
  }
  printf("  ---------------------------------\n");
  printf("    A   B   C   D   E   F   G   H\n");
}

/* TODO: This function displays empty squares the wrong way. */
void showfenboard(struct board *board) {
  int rad = 0, kol = 0;
  int i;
  int color;

  for (rad = 0; rad < 8; rad++) {
    for (kol = 0; kol < 8; kol++) {
      if (board->all_pieces[WHITE] & square[rad*8+kol])
	color = WHITE;
      else
	color = BLACK;
      printf("%c",getPieceLetter(board->boardarr[rad*8 + kol],color));
    }
    if (rad < 7)
      printf("/");
  }
  printf(" ");
  if (Color == WHITE)
    printf("w");
  else
    printf("b");
  printf(" ");
  if (!(board->castling_status[WHITE]
	& (LONG_CASTLING_OK | SHORT_CASTLING_OK))
      && !(board->castling_status[BLACK]
	   & (LONG_CASTLING_OK | SHORT_CASTLING_OK)))
    printf("-");
  else {
    if (board->castling_status[WHITE] & SHORT_CASTLING_OK)
      printf("K");
    if (board->castling_status[WHITE] & LONG_CASTLING_OK)
      printf("Q");
    if (board->castling_status[BLACK] & SHORT_CASTLING_OK)
      printf("k");
    if (board->castling_status[BLACK] & LONG_CASTLING_OK)
      printf("q");
  }
  printf(" ");
  if (board->passant == 0)
    printf("-");
  else
    printf("%c%d",('a'-(board->passant%8)),board->passant/8);
  printf(" ");
  printf("%d",100-board->moves_left_to_draw);
  printf(" ");
  printf("%d",board->move_number/2+1);
  printf("\n");
}

#ifndef AM_DEBUG
void showsettings(struct board *board, int engine_col, int started) {
#else
void showsettings(struct board *board, int engine_col, int started, int gamelog) {
#endif
  printf("White     : ");
  if (engine_col == WHITE)
    printf("engine\n");
  else
    printf("human\n");
  printf("Black     : ");
  if (engine_col == BLACK)
    printf("engine\n");
  else
    printf("human\n");
  printf("Whose turn: ");
  if (board->color_at_move == WHITE)
    printf("white\n");
  else
    printf("black\n");
#ifdef AM_DEBUG
  if (gamelog)
    printf("Games are logged.");
  else
    printf("Games are not logged.");
#endif
  if (!started)
    printf("\nGame not started.\n");
}

void showhelp(int mode) {
#ifdef AM_DEBUG
  if (mode == DEBUG_MODE) {
    printf("\nCommand:    Meaning:\n\n");
    printf("show         Draw the board\n");
    printf("gamelog      Turn on logging of games\n");
    printf("nogamelog    Turn off logging of games\n");
    printf("set          Show settings\n");
    printf("?            Show help\n");
    printf("help         Show help\n");
    printf("debexit      Exit from debugging mode\n\n");
  } else
#endif
  {
    printf("\nCommand:                 Meaning:\n\n");
    printf("show                      Draw the board\n");
    printf("start                     Start a new game\n");
    printf("hist                      Show move history\n");
    printf("set                       Show settings\n");
    printf("testsuite filename.epd    Play a testsuite\n");
    printf("perft depth               Run perft to a specified depth\n");
    printf("play [-d depth] [-t time] Let the engine play a move from the current position\n");
    printf("setboard FEN              Set up a position according to a FEN string.\n");
    printf("enginewhite               Computer plays white\n");
    printf("humanwhite                Human plays white (default)\n");
    printf("engineblack               Computer plays black (default)\n");
    printf("humanblack                Human plays black\n");
#ifdef AM_DEBUG
    printf("debug                     Enter debug mode\n\n");
#endif
    printf("xboard                    Enter xboard mode\n\n");
    
    printf("?                         Show help\n");
    printf("help                      Show help\n");
    printf("quit                      Quit the program\n\n");
    
    printf("If you make a move, it should be written in the form\n");
    printf("e2e4 for normal moves, and a7a8[q|r|b|n] for pawn promotions.\n");
  }
}
