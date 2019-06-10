/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "hash.h"
#include "bitboards.h"
#include "parse.h"
#include "alfabeta.h"
#include <stdlib.h>
#include <stdio.h>
#include "makemove.h"
#include "misc.h"
#include "slump.h"

int64 CHANGE_COLOR;

struct refutation_entry *ref_table;
struct transposition_entry *transp_table;
struct pawn_entry *pawn_table;
int64 zobrist[6][2][64];
int64 zobrist_castling[2][5];
int64 zobrist_passant[64];
unsigned char sequence;
extern bitboard square[64];
extern int abort_search;

//Statistics for pawn search
int pawn_hits = 0;
int pawn_misses = 0;
int pawn_probes = 0;

/* In the postanalysis phase, if there is several best moves (with the same
   value), we pick the move that gives the best outcome after just 2 plies.
   The idea is to pick a move with a quick outcome rather than a gain farther
   away. In order to check that, we need to turn off probing of the
   transposition table, which is done by calling turn_off_hash(), which in
   turn sets the variable hash_probing to zero. */
int hash_probing = 1;

void turn_on_hash() {
  hash_probing = 1;
}

void turn_off_hash() {
  hash_probing = 0;
}

void init_hashtables() {
  int i, j, k;

  ref_table = (struct refutation_entry *) malloc(REFUTATION_NBR_ENTRIES*sizeof(struct refutation_entry));
  transp_table = (struct transposition_entry *) malloc(TRANSP_NBR_ENTRIES*sizeof(struct transposition_entry));
  pawn_table = (struct pawn_entry *) malloc(PAWN_HASH_SIZE*sizeof(struct pawn_entry));
  sequence = 255;
  update_sequence();
  turn_on_hash();
  for (i = 0; i < TRANSP_NBR_ENTRIES; i++) {
    //transp_table[i].repetitions = 0;
    transp_table[i].status = 0;
  }
  for (i = 0; i < REFUTATION_NBR_ENTRIES; i++) {
    ref_table[i].status = 0;
  }
  for (i = 0; i < 6; i++)
    for (j = 0; j < 2; j++)
      for (k = 0; k < 64; k++)
	zobrist[i][j][k] = rand64();
  zobrist_castling[WHITE][0] = rand64();
  zobrist_castling[WHITE][LONG_CASTLING_OK] = rand64();
  zobrist_castling[WHITE][SHORT_CASTLING_OK] = rand64();
  zobrist_castling[WHITE][LONG_CASTLING_OK | SHORT_CASTLING_OK] = rand64();
  zobrist_castling[WHITE][CASTLED] = rand64();
  zobrist_castling[BLACK][0] = rand64();
  zobrist_castling[BLACK][LONG_CASTLING_OK] = rand64();
  zobrist_castling[BLACK][SHORT_CASTLING_OK] = rand64();
  zobrist_castling[BLACK][LONG_CASTLING_OK | SHORT_CASTLING_OK] = rand64();
  zobrist_castling[BLACK][CASTLED] = rand64();
  for (i = 0; i < 64; i++)
    zobrist_passant[i] = rand64();
  CHANGE_COLOR = rand64();
  j = REFUTATION_NBR_ENTRIES*sizeof(struct refutation_entry);
  printf("size of refutation table = %d kb\n",j/1024);
  j = TRANSP_NBR_ENTRIES*sizeof(struct transposition_entry);
  printf("size of transposition table = %d kb\n",j/1024);
  j = PAWN_HASH_SIZE*sizeof(struct pawn_entry);
  printf("size of pawn hashtable = %d kb\n",j/1024);
}

int64 get_zobrist_key_for_board(struct board *board) {
  int64 zobrist_key = 0;
  bitboard pieces;
  int i, j, square_nbr;

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 6; j++) {
      pieces = board->piece[i][j];
      while (pieces != 0) {
	square_nbr = get_first_bitpos(pieces);
	zobrist_key ^= zobrist[j][i][square_nbr];
	pieces &= ~square[square_nbr];
      }
    }
  }
  zobrist_key ^= zobrist_castling[WHITE][(int)board->castling_status[WHITE]];
  zobrist_key ^= zobrist_castling[BLACK][(int)board->castling_status[BLACK]];
  zobrist_key ^= zobrist_passant[(int)board->passant];
  return zobrist_key;
}

int64 get_zobrist_pawn_key_for_board(struct board *board) {
  int64 zobrist_key = 0;
  bitboard pieces;
  int i, square_nbr;

  for (i = 0; i < 2; i++) {
    pieces = board->piece[i][PAWN];
    while (pieces != 0) {
      square_nbr = get_first_bitpos(pieces);
      zobrist_key ^= zobrist[PAWN][i][square_nbr];
      pieces &= ~square[square_nbr];
    }
  }
  return zobrist_key;
}

void update_sequence() {
  int i;

  if (sequence < 255)
    sequence++;
  else {
    sequence = 1;
    for (i = 0; i < REFUTATION_NBR_ENTRIES; i++) {
      ref_table[i].lock = 0; //TODO: What good does this row do? Prob. nothing
      ref_table[i].sequence = 0;
    }
  }
}

/* The argument can't be a normal int64, because that could give us
   a negative hash key. So we have to use unsigned. */
int get_hashkey(unsigned long long int zobrist_key, long long int hash_size) {
  return (zobrist_key & (hash_size-1));
}

int probe_pawn_hash(struct board *board, struct pawn_entry *pawn_struct) {
  struct pawn_entry *entry;

  pawn_probes++;
  entry = &(pawn_table[get_hashkey((unsigned long long int) board->zobrist_pawn_key,(long long int) PAWN_HASH_SIZE)]);
  if (entry->lock == board->zobrist_pawn_key) {
    pawn_hits++;
    *pawn_struct = *entry;
    return 0;
  } else {
    pawn_misses++;
    return UNKNOWN;
  }
}

/*-------------------------------------------------------------------------
 | This function probes both the transposition- and the refutation table. |
 | Note that direct cutoffs are not allowed if we are in the PV. The      |
 | reason is that otherwise draw by repetition lines might go undetected. |
 | For a discussion on this, see:                                         |
 | http://www.talkchess.com/forum/viewtopic.php?topic_view=               |
 | threads&p=179317&t=20080                                               |
 -------------------------------------------------------------------------*/
int probe_hash(int depth, struct board *board, struct move *transp_move,
	       struct move *refut_move, int *alpha, int *beta,
	       int *retval, int ply, int *cutoff_val, int *do_null, int pv) {
  struct transposition_entry *entry;
  int ref_flag, transp_flag = UNKNOWN_NEW;
  int R_adapt;
  int draw_history;

  if (hash_probing) {

    /*---------------------------------------------------------------------
     | Try probing the transposition table first, and if that one returns |
     | a hit good enough to cause a cutoff, then we don't need to probe   |
     | the refutation table at all.                                       |
     ---------------------------------------------------------------------*/
    entry = &(transp_table[get_hashkey((unsigned long long int)
				       board->zobrist_key,(long long int)
				       TRANSP_NBR_ENTRIES)]);
    if (entry->lock == board->zobrist_key) {
      /*-------------------------------------------
       | Check if it's any use trying null moves. |
       -------------------------------------------*/
      if (depth <= 6 || (depth <= 8 && (board->nbr_pieces[WHITE] < 3
					|| board->nbr_pieces[BLACK] < 3))) {
	R_adapt = 2;
      } else {
	R_adapt = 3;
      }
      if (((entry->status & ALPHA) || (entry->status & EXACT))
	  && depth - R_adapt <= entry->depth && entry->value < *beta)
	*do_null = 0;

      /*---------------------------------------------------------------------
       | Only take the hash value if it's at the same depth, or deeper than |
       | the current depth. Also don't use the score if we are in the PV in |
       | order to not miss any draw by repetition lines.                    |
       ---------------------------------------------------------------------*/
      if (!pv && entry->depth >= ((char)depth)) {
	/* If entry value is draw_score, then don't extract the value, since
	   the value came from a 3-fold repetition or a 50-move rule draw,
	   and those scores are path dependent (note that the eval()-function
	   never returns the score 0). */
	if (!(entry->status & MOVE_ONLY) && entry->value != draw_score()) {
	  if (entry->status & EXACT) {
	    if (entry->value <= (-INFTY + 150)) {
	      *cutoff_val = entry->value + ply;
	      return CUTOFF_VALUE;
	    } else if (entry->value >= (INFTY - 150)) {
	      *cutoff_val = entry->value - ply;
	      return CUTOFF_VALUE;
	    } else {
	      *cutoff_val = entry->value;
	      return CUTOFF_VALUE;
	    }
	  }
	  if (entry->status & ALPHA) {
	    if (entry->value <= (*alpha)) {
	      if (entry->value <= (-INFTY + 150)) {
		*cutoff_val = entry->value + ply;
		return CUTOFF_VALUE;
	      } else if (entry->value >= (INFTY - 150)) {
		*cutoff_val = entry->value - ply;
		return CUTOFF_VALUE;
	      } else {
		*cutoff_val = entry->value;
		return CUTOFF_VALUE;
	      }
	    } else {
	      if (entry->value <= (-INFTY + 150)) {
		*beta = minval(*beta,entry->value+ply);
	      } else if (entry->value >= (INFTY - 150)) {
		*beta = minval(*beta,entry->value-ply);
	      } else {
		*beta = minval(*beta,entry->value);
	      }
	    }
	  }
	  if (entry->status & BETA) {
	    if (entry->value >= (*beta)) {
	      if (entry->value <= (-INFTY + 150)) {
		*cutoff_val = entry->value + ply;
		return CUTOFF_VALUE;
	      } else if (entry->value >= (INFTY - 150)) {
		*cutoff_val = entry->value - ply;
		return CUTOFF_VALUE;
	      } else {
		*cutoff_val = entry->value;
		return CUTOFF_VALUE;
	      }
	    } else {
	      if (entry->value <= (-INFTY + 150)) {
		*retval = maxval(*retval,entry->value+ply);
		*alpha = maxval(*alpha,entry->value+ply);
	      } else if (entry->value >= (INFTY - 150)) {
		*retval = maxval(*retval,entry->value-ply);
		*alpha = maxval(*alpha,entry->value-ply);
	      } else {
		*retval = maxval(*retval,entry->value);
		*alpha = maxval(*alpha,entry->value);
	      }
	    }
	  }
	}
      }

      ref_flag = probe_refutation(depth,board,refut_move,alpha,
				  beta,retval,ply,cutoff_val,pv);
      if (ref_flag == CUTOFF_VALUE) {
	return CUTOFF_VALUE;
      } else {
	/* If the value from the refutation table wasn't good enough to cause
	   a cutoff, then let's at least return a best move from the
	   transposition table (if any). */
	if (ref_flag & REF_MOVE)
	  transp_flag |= REF_MOVE;
	if (entry->status & BEST_MOVE) {
	  (*transp_move).from_square = entry->bestmove_fsquare;
	  (*transp_move).to_square = entry->bestmove_tsquare;
	  transp_flag |= TRANSP_MOVE;
	}
	return transp_flag;
      }
    } else {
      /* Transposition hash miss, check the refutation table. */
      return probe_refutation(depth,board,refut_move,alpha,
			      beta,retval,ply,cutoff_val,pv);
    }
  }
  return UNKNOWN_NEW;
}

/* TODO: Could we check also in the refutation table if it's
   any use trying null moves? */
int probe_refutation(int depth, struct board *board, struct move *move,
			 int *alpha, int *beta, int *retval, int ply,
			 int *cutoff_val, int pv) {
  struct refutation_entry *entry;

  if (hash_probing) {
    entry = &(ref_table[get_hashkey((unsigned long long int)
				    board->zobrist_key,(long long int)
				    REFUTATION_NBR_ENTRIES)]);
    if (entry->lock == board->zobrist_key) {
      /*---------------------------------------------------------------------
       | Only take the hash value if it's at the same depth, or deeper than |
       | the current depth. Also don't use the score if we are in the PV in |
       | order to not miss any draw by repetition lines.                    |
       ---------------------------------------------------------------------*/
      if (!pv && entry->depth >= ((char)depth)) {
	/* If entry value is draw_score, then don't extract the value, since
	   the value came from a 3-fold repetition or a 50-move rule draw,
	   and those scores are path dependent (note that the eval()-function
	   never returns the score 0). */
	if (!(entry->status & MOVE_ONLY) && entry->value != draw_score()) {
	  if (entry->status & BETA) {
	    if (entry->value >= (*beta)) {
	      //if (!pv) {
		if (entry->value <= (-INFTY + 150)) {
		  *cutoff_val = entry->value + ply;
		  return CUTOFF_VALUE;
		} else if (entry->value >= (INFTY - 150)) {
		  *cutoff_val = entry->value - ply;
		  return CUTOFF_VALUE;
		} else {
		  *cutoff_val = entry->value;
		  return CUTOFF_VALUE;
		}
		//}
	    } else {
	      if (entry->value <= (-INFTY + 150)) {
		*retval = maxval(*retval,entry->value + ply);
		*alpha = maxval(*alpha,*retval);
	      } else if (entry->value >= (INFTY - 150)) {
		*retval = maxval(*retval,entry->value - ply);
		*alpha = maxval(*alpha,*retval);
	      } else {
		*retval = maxval(*retval,entry->value);
		*alpha = maxval(*alpha,*retval);
	      }
	    }
	  }
	}
      }
      if (entry->status == 0)
	return UNKNOWN_NEW;
      (*move).from_square = entry->bestmove_fsquare;
      (*move).to_square = entry->bestmove_tsquare;
      return REF_MOVE;
    }
  }
  return UNKNOWN_NEW;
}

void record_pawn_hash(struct board *board, struct pawn_entry *pawn_struct) {
  struct pawn_entry *entry;

  entry = &(pawn_table[get_hashkey((unsigned long long int) board->zobrist_pawn_key,(long long int) PAWN_HASH_SIZE)]);
  pawn_struct->lock = board->zobrist_pawn_key;
  *entry = *pawn_struct;
}

void record_refutation(int depth, struct board *board, struct move *move,
		       int flag, int ply, int value) {
  struct refutation_entry *entry;

  /*--------------------------------------------------------
   | Don't record the hashvalue if it's an aborted search. |
   --------------------------------------------------------*/
  if (hash_probing && !abort_search) {
    entry = &(ref_table[get_hashkey((unsigned long long int)
				    board->zobrist_key,(long long int)
				    REFUTATION_NBR_ENTRIES)]);
    /*----------------------------------------------------------
     | Only replace if same depth or deeper, or if the element |
     | pertains to an ancient search or if status == 0         |
     | TODO: PV nodes should probably always be saved!         |
     ----------------------------------------------------------*/
    if ((((char)depth) >= entry->depth) || (sequence > entry->sequence)
	|| (entry->status == 0)) {
      entry->lock = board->zobrist_key;
      entry->bestmove_fsquare = move->from_square;
      entry->bestmove_tsquare = move->to_square;
      entry->status = 0;
      entry->status |= (char) flag;
      /*-------------------------------------------------------------------
       | If it's a mating score, then don't store the mating value in the |
       | hash directly, since the exact value depends on which depth the  |
       | mate was found. Rather subtract the ply and then add the ply     |
       | when the entry is later retrieved from the hash (possibly from   |
       | a different search depth).                                       |
       -------------------------------------------------------------------*/
      if (value <= (-INFTY + 150)) {
	entry->value = value - ply;
      } else if (value >= (INFTY - 150)) {
	entry->value = value + ply;
      } else
	entry->value = value;
      entry->depth = (char) depth;
      entry->sequence = sequence;
    }
  }
}

void record_transposition(int depth, struct board *board,
			  struct move *move, int is_best_move, int flag,
			  int ply, int value) {
  struct transposition_entry *entry;

  /*--------------------------------------------------------
   | Don't record the hashvalue if it's an aborted search. |
   --------------------------------------------------------*/
  if (hash_probing && !abort_search) {
    entry = &(transp_table[get_hashkey((unsigned long long int)
				       board->zobrist_key,(long long int)
				       TRANSP_NBR_ENTRIES)]);
    /*------------------
     | Always replace. |
     ------------------*/
    entry->lock = board->zobrist_key;
    if (is_best_move) {
      entry->status = BEST_MOVE;
      entry->bestmove_fsquare = move->from_square;
      entry->bestmove_tsquare = move->to_square;
    } else
      entry->status = 0;
    entry->status |= (char) flag;
    if (flag == 0)
      entry->status = 0;
    /*-------------------------------------------------------------------
     | If it's a mating score, then don't store the mating value in the |
     | hash directly, since the exact value depends on which depth the  |
     | mate was found. Rather subtract the ply and then add the ply     |
     | when the entry is later retrieved from the hash (possibly from   |
     | a different search depth).                                       |
     --------------- ---------------------------------------------------*/
    if (value <= (-INFTY + 150)) {
      entry->value = value - ply;
    } else if (value >= (INFTY - 150)) {
      entry->value = value + ply;
    } else
      entry->value = value;
    entry->depth = (char) depth;
  }
}

