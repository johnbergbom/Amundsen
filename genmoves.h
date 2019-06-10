#ifndef _GENMOVES_
#define _GENMOVES_

#include "bitboards.h"

/*------------------------------------------------------------------------
 | The constant QUIESCENCE_FUTIL_MARGIN is used in the quiescence search |
 | to determine which moves should be classified as unpromising and      |
 | therefore pruned. Only try the move if the following condition holds: |
 | stat_pos_value + exp_mat_gain + QUIESCENCE_FUTIL_MARGIN > alpha       |
 ------------------------------------------------------------------------*/
#define QUIESCENCE_FUTIL_MARGIN 50

bitboard generate_pawnmoves(struct board *board, int boardpos);
bitboard generate_kingmoves(struct board *board, int boardpos);
bitboard generate_knight_moves(struct board *board, int boardpos);
bitboard generate_horizontal_moves(struct board *board, int boardpos);
bitboard generate_vertical_moves(struct board *board, int boardpos);
bitboard generate_NEdiag_moves(struct board *board, int boardpos);
bitboard generate_NWdiag_moves(struct board *board, int boardpos);

int get_movetype(struct board *board, struct move *move);

/* For move generation we use the moves struct. The source represents the
   square that a piece will move from. The targets represent all the
   squares the piece can move to. Piece tells what kind of piece we are
   dealing with. */
struct moves {
  bitboard source;
  bitboard targets;
  int piece;
};

#define NBR_KILLERS 1

struct killers {
  bitboard fsquare;
  bitboard tsquare;
  int value;
};

struct swap_entry {
  bitboard fsquare;        //from square
  bitboard tsquare;        //to square
  int piece;               //which kind of piece made the move?
  int swap_val;            //value returned by the swap function
  int untried;             //set to one for untried moves
  bitboard old_fsquares;   //here old fsquare attempts are saved
};

#define NBR_HISTMOVES 5

struct hist_entry {
  bitboard fsquare;   //from square
  bitboard tsquare;   //to square
  int hist_val;       //value returned by the history moves table
  int untried;        //set to one for untried moves
  int movelist_pos;   //save the movelist pos to make it easier to find later
};

int get_check_evasion_moves(struct board *board, struct move *returnmove,
			    struct swap_entry *swap_list, int *order,
			    int *count, bitboard *spec_moves);
int get_next_quiet_move(struct board *board, struct move *returnmove,
			struct swap_entry *swap_list, int *order, int *count,
			int own_king_check, bitboard *spec_moves,
			int stat_val, int alpha);
int get_next_move(struct board *board , struct moves *moves, int *movables,
		  int *hash_flag, struct move *refmove,
		  struct move *transpmove, struct killers *killers,
		  struct move *returnmove, int *order,
		  struct swap_entry *swap_list, int *swap_count,
		  int *killers_processed, struct hist_entry *hist_list,
		  int own_king_check, bitboard *spec_moves, int depth);
int get_next_root_move(struct board *board, struct move_list_entry *movelist,
		       int mcount, int *searched_list);
int generate_moves(struct board *board, struct moves *moves, int *movables);
bitboard generate_move(struct board *board, int from_square);


int get_pawnmove_movetype(struct board *board, char boardpos, char target);
int get_kingmove_movetype(struct board *board, char target);
int get_othermove_movetype(struct board *board, char target);

#endif      //_GENMOVES_

