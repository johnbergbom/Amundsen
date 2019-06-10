#ifndef _MISC_
#define _MISC_

#include "bitboards.h"
#include "swap.h"

#define INFOLOGFILE "log.info"
#define DEBUGLOGFILE "log.debug"
#define RESULTLOGFILE "log.result"
#define TESTSUITELOGFILE "log.testsuite"

#define PAWN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define QUEEN 4
#define KING 5

#define EMPTY 7

#define VAL_PAWN 100
#define VAL_KNIGHT 300
#define VAL_BISHOP 300
#define VAL_ROOK 500
#define VAL_QUEEN 900
#define VAL_KING 10000

void infolog(const char *text);

void testsuitelog(const char *text);

void resultlog(int status, int color, int engine_col);

#ifdef AM_DEBUG
void debuglog(const char *text);
#endif

int draw_score(void);

/* This function returns 1 if the move is legal, and 0 if it's illegal. */
int legal_move(struct move move_to_check, struct board *board);

/* This function returns 0 if game is not ended, 1 if stalemate,
   2 if check mate, 3 if draw by the 50 move rule, 4 if
   insufficient material, and 5 if draw by repetition */
int game_ended(struct board *board);

/* Prints out a move on the screen. */
void printmove(struct move *move);

/* Prints a bitboard nicely. */
void printbitboard(bitboard brd);

/* Prints a char nicely (msb-lsb). */
void printchar(unsigned char lg);

void printprettytime(int milliseconds);
void printprettynodes(int64 nodes, int milliseconds);
void zero_success_failure_count();

/*--------------------------------------------
 | Define assorted macros.                   |
 --------------------------------------------*/
#define Color (board->color_at_move)
#define Oppcolor (1-board->color_at_move)

/* This macro returns a non zero value if the king of the color at move
   is in check. Otherwise zero. */
#define in_check(board)     ((get_attacks(board,get_first_bitpos((board)->piece[(int)((board)->color_at_move)][KING])) & ((board)->all_pieces[1-(board)->color_at_move])) != 0)

/* This macro returns a non zero value if the king of the color not at move
   is in check. Otherwise zero. */
#define opp_in_check(board)     ((get_attacks(board,get_first_bitpos((board)->piece[1-(board)->color_at_move][KING])) & ((board)->all_pieces[(int)((board)->color_at_move)])) != 0)

/* This macro returns a non zero value if a given square is under attack
   by the opponent. Otherwise zero. */
#define sq_under_attack_by_opp(board,sq)     ((get_attacks(board,sq) & ((board)->all_pieces[1-(board)->color_at_move])) != 0)

double nroot(int order, double number);
int consistent_board(struct board *board);

#endif      //_MISC_

