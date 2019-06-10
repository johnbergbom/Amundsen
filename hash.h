#ifndef _HASH_
#define _HASH_

#include "bitboards.h"

/* 18 bits in the transposition hash table is 262144 entries. */
#define REFUTATION_NBR_BITS 18
#define REFUTATION_NBR_ENTRIES (1<<REFUTATION_NBR_BITS)
#define TRANSP_NBR_BITS 21
#define TRANSP_NBR_ENTRIES (1<<TRANSP_NBR_BITS)
#define PAWN_HASH_SIZE (1<<13)

#define UNKNOWN 2000000
#define HASH_MOVE 2000001

#define UNKNOWN_NEW 0
#define TRANSP_MOVE 1
#define REF_MOVE 2
#define CUTOFF_VALUE 4

#define BEST_MOVE 1
#define EXACT 4
#define ALPHA 8
#define BETA 16
#define QUIESCENCE 32
#define MOVE_ONLY 64

/*---------------------------------------------------------------
 | A good position to use to test if the hashtables are working |
 | correctly, is the following position:                        |
 | 8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1 bm Kb1               |
 ---------------------------------------------------------------*/
struct refutation_entry {
  int64 lock;
  int value;
  char depth;
  unsigned char sequence;
  char status;
  char bestmove_fsquare;
  char bestmove_tsquare;
};

struct transposition_entry {
  int64 lock;
  int value;
  char depth;
  char status;
  char bestmove_fsquare;
  char bestmove_tsquare;
};

struct pawn_entry {
  int64 lock;
  int white_value;
  int black_value;
  bitboard passed_pawn[2];
  bitboard weak_pawn[2];
  bitboard pawn_hole[2];
  bitboard open_file;
  bitboard half_open_file[2]; //half_open_file[WHITE] => white has no pawns
  bitboard double_isolated_pawn[2];
  char center;
  char defects_q[2];
  char defects_k[2];
  char defects_e[2];
  char defects_d[2];
};

void turn_on_hash(void);
void turn_off_hash(void);
void init_hashtables(void);
int64 get_zobrist_key_for_board(struct board *board);
int64 get_zobrist_pawn_key_for_board(struct board *board);
void update_sequence(void);
int get_hashkey(unsigned long long int zobrist_key, long long int hash_size);
int probe_pawn_hash(struct board *board, struct pawn_entry *pawn_struct);
int probe_hash(int depth, struct board *board, struct move *transp_move,
	       struct move *refut_move, int *alpha, int *beta,
	       int *retval, int ply, int *cutoff_val, int *do_null, int pv);
int probe_refutation(int depth, struct board *board, struct move *move,
		     int *alpha, int *beta, int *retval, int ply,
		     int *cutoff_val, int pv);
void record_pawn_hash(struct board *board, struct pawn_entry *pawn_struct);
void record_refutation(int depth, struct board *board, struct move *move,
		       int flag, int ply, int value);
void record_transposition(int depth, struct board *board,
			  struct move *move, int is_best_move, int flag,
			  int ply, int value);

#endif      //_HASH_

