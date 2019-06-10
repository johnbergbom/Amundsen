/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "iterdeep.h"
#include <stdio.h>
#include <stdlib.h>
#include "misc.h"
#include "alfabeta.h"
#include "genmoves.h"
#include "timectrl.h"
#include "makemove.h"
#include "parse.h"
#include "hash.h"

/*------------------------------------
 | Variables for getting statistics. |
 ------------------------------------*/
int searches = 0;
int search_depth = 0;
int total_time_aborts = 0;
extern int visited_nodes;

/*-------------------------------------------------
 | Other assorted global variables that are used. |
 -------------------------------------------------*/
extern int abort_search;
extern int historymovestable[2][64][64];
extern int at_move;
extern int iteration_depth;
extern struct history_entry **historik;
extern int search_status;
extern bitboard square[64];
extern bitboard pawn_start[2];
extern int failed_cutoff_count[2][16][64];
extern int success_cutoff_count[2][16][64];

int nbr_fail_lows_at_curr_level;
int nbr_researches_at_curr_level;
int valmax = -INFTY;
int prev_valmax = -INFTY;
int recapture;
int post = 0; //if 1, then print output of the progress of the search

/* This function determines if an aspiration re-search has do be done, and if
   so, what the alpha/beta bounds should be. Altogether there are eight
   different states, where state zero means that no re-search is necessary. */
int next_state(int val, int *alpha, int *beta, int *org_alpha, int *org_beta,
	       int *state, int *re_searches) {
  if (*state == 1) {
    if (val <= *alpha) {
      *state = 2;
      if (search_status & NORMAL_SEARCH)
	printf("Failed low ");
      nbr_fail_lows_at_curr_level++;
    } else if (val >= *beta) {
      *state = 5;
      if (search_status & NORMAL_SEARCH)
	printf("Failed high ");
    } else
      *state = 0;
  } else if (*state == 2) {
    if (val <= *alpha) {
      *state = 3;
      if (search_status & NORMAL_SEARCH)
	printf("Failed low ");
      nbr_fail_lows_at_curr_level++;
    } else if (val >= *beta) {
      *state = 7;
      if (search_status & NORMAL_SEARCH)
	printf("Failed high ");
    } else
      *state = 0;
  } else if (*state == 3) {
    if (val >= *beta) {
      *state = 4;
      if (search_status & NORMAL_SEARCH)
	printf("Failed high ");
    } else
      *state = 0;
  } else if (*state == 4) {
    *state = 0;
  } else if (*state == 5) {
    if (val <= *alpha) {
      *state = 7;
      if (search_status & NORMAL_SEARCH)
	printf("Failed low ");
      nbr_fail_lows_at_curr_level++;
    } else if (val >= *beta) {
      *state = 6;
      if (search_status & NORMAL_SEARCH)
	printf("Failed high ");
    } else
      *state = 0;
  } else if (*state == 6) {
    if (val <= *alpha) {
      *state = 4;
      if (search_status & NORMAL_SEARCH)
	printf("Failed low ");
      nbr_fail_lows_at_curr_level++;
    } else
      *state = 0;
  } else if (*state == 7) {
    if (val <= *alpha) {
      if (search_status & NORMAL_SEARCH)
	printf("Failed low ");
      nbr_fail_lows_at_curr_level++;
      *state = 3;
    } else if (val >= *beta) {
      *state = 6;
      if (search_status & NORMAL_SEARCH)
	printf("Failed high ");
    } else
      *state = 0;
  }

  if (*state != 0) {
    if (search_status & NORMAL_SEARCH)
      printf("(state => %d)\n",*state);
    (*re_searches)++;
  }

  /* If there have been several re-searches the search window is made
     bigger than it would otherwise be. */
  if (*state == 0) {
    *org_alpha = *alpha = val - VALWINDOW - 15*(*re_searches);
    *org_beta = *beta = val + VALWINDOW + 15*(*re_searches);
  } else if (*state == 2) {
    *beta = *alpha + 1;
    *alpha = val - SECOND_ATTEMPT_VALWINDOW - 20*(*re_searches);
  } else if (*state == 3) {
    *alpha = -INFTY;
    //beta stays the same
  } else if (*state == 4) {
    *alpha = -INFTY;
    *beta = INFTY;
  } else if (*state == 5) {
    *alpha = *beta - 1;
    *beta = val + SECOND_ATTEMPT_VALWINDOW + 20*(*re_searches);
  } else if (*state == 6) {
    //alpha stays the same
    *beta = *beta = INFTY;
  } else if (*state == 7) {
    *alpha = minval(*alpha,*org_alpha);
    *beta = maxval(*beta,*org_beta);
  }
  return *state;
}

/* Returns the search status. */
int iterative_deepening_aspiration_search(struct board *board,
					  struct move_list_entry *movelist,
					  int mcount, int max_depth,
					  int movetype, int *searched_depth,
					  int *last_iter_nbr_nodes,
					  int *pre_pre_last_iter_nbr_nodes,
					  int just_out_of_book) {
  int alpha = -INFTY, beta = INFTY;
  int i, j, k;
  int nbr_moves = 0;
  int depth;
  //int attempt_nbr;
  char *str;
  int org_alpha, org_beta;
  int *searched_list;
  int best;
  int fail;
  int state = 1;
  int re_searches = 0;
  int legal_moves;
  struct move_list_entry *temp_movelist;
  struct move the_move;
  //int best_move_sought_deeper; //doesn't need initialization
  int nbr_nodes_tmp;
  struct move best_move_below;
  int second_last_iter_nbr_nodes;
  int own_king_check;
  int check_move;
  int pawn_push;
  int likely_good_move;
  int possibly_good_move;
  int fail_count;

  *last_iter_nbr_nodes = *pre_pre_last_iter_nbr_nodes
    = second_last_iter_nbr_nodes = 0;

  searched_list = (int *) malloc(mcount*sizeof(int));

  /* If a search or re-search takes too long we want to quit the search
     before arriving at a value within the alpha/beta bounds. This is
     done by using a temporary movelist, which is copied to the real
     movelist only after a search within the bounds. Thereby we know that
     the real movelist will always contain usable moves, so it can be
     returned to the caller at any time (i.e. when time is up) */
  temp_movelist = (struct move_list_entry *)
    malloc(mcount * sizeof(struct move_list_entry));
  for (i = 0; i < mcount; i++)
    temp_movelist[i] = movelist[i];

  /*----------------------------------------------------------
   | Divide the history moves table entries by two. Inactive |
   | counters then tend to go towards zero.                  |
   ----------------------------------------------------------*/
  for (i = 0; i < 2; i++)
    for (j = 0; j < 64; j++)
      for (k = 0; k < 64; k++)
	//historymovestable[i][j][k] >>= 1;
	historymovestable[i][j][k] = 0;

  /* Go through the moves in the movelist and play them by calling
     alphabeta. Go deeper and deeper iteratively. Start by searching
     to depth 2. If we only search to depth 1 it won't be discovered
     that one's king is left in check. */
  iteration_depth = depth = 2;
  //attempt_nbr = 1;
  org_alpha = alpha;
  org_beta = beta;
  abort_search = 0;
  valmax = -INFTY;
  set_time_per_move(board,movetype,just_out_of_book);
  set_nbr_moves(mcount);
  nbr_researches_at_curr_level = nbr_fail_lows_at_curr_level = 0;
  if (movetype == MOVETYPE_NORMAL)
    search_status = NORMAL_SEARCH;
  else if (movetype == MOVETYPE_FIND_MOVE_TO_PONDER)
    search_status = FINDING_MOVE_TO_PONDER;
  else //movetype == MOVETYPE_PONDER
    search_status = PONDERING;

  the_move.old_castling_status[WHITE] = board->castling_status[WHITE];
  the_move.old_castling_status[BLACK] = board->castling_status[BLACK];
  the_move.old_passant = board->passant;
  the_move.old_moves_left_to_draw = board->moves_left_to_draw;
  own_king_check = in_check(board);

  while (1) {
    prev_valmax = valmax;
    /* valmax has to be cleared (=-INFTY) for each new turn */
    valmax = -INFTY;
    //best_move_sought_deeper = 0;
    /* Unset the searched moves before each new turn. */
    for (i = 0; i < mcount; i++) {
      searched_list[i] = 0;
    }

    /*----------------------------------------------
     | Determine if this is a recapture situation. |
     ----------------------------------------------*/
    if (depth > 2) {
      recapture = 0;
      if ((*historik)[board->move_number-1].move.type & CAPTURE_MOVE) {
	best = get_next_root_move(board,temp_movelist,mcount,searched_list);
	if (movelist[best].value != KINGTAKEN) {
	  int second_best;
	  searched_list[best] = 1;
	  second_best =
	    get_next_root_move(board,temp_movelist,mcount,searched_list);
	  if (movelist[second_best].value != KINGTAKEN) {
	    int val1, val2;
	    val1 = movelist[best].value;
	    val2 = movelist[second_best].value;
	    if (val1 > val2 + 200
		&& (get_first_bitpos(movelist[best].tsquare) ==
		    (*historik)[board->move_number-1].move.to_square
		    || movelist[best].value < VAL_PAWN))
	      recapture = 1;
	  }
	  searched_list[best] = 0;
	}
      }
      if (recapture && search_status & NORMAL_SEARCH) {
	printf("recapture\n");
	infolog("recapture");
      }
    }
    fail = 0;
    legal_moves = 0;
    at_move = 0;
    nbr_nodes_tmp = visited_nodes;
    for (nbr_moves = 0; nbr_moves < mcount && !fail; nbr_moves++) {
      best = get_next_root_move(board,temp_movelist,mcount,searched_list);
      if (best == -1) {
	infolog("BEST == -1");
	printf("BEST == -1\n");
	exit(1);
      }

      the_move.from_square = (char) get_first_bitpos(temp_movelist[best].fsquare);
      the_move.to_square = (char) get_first_bitpos(temp_movelist[best].tsquare);
      the_move.piece = (char) temp_movelist[best].piece;
      the_move.type = (char) temp_movelist[best].type;
      check_move = checking_move(board,&the_move);

      /*---------------------------------------------------
       | Check if this move is likely to be good based on |
       | simple cutoff statistics.                        |
       ---------------------------------------------------*/
      {
	int succ;
	succ = success_cutoff_count[(int)Color]
	  [board->boardarr[the_move.from_square]][the_move.to_square];
	fail_count = failed_cutoff_count[(int)Color]
	  [board->boardarr[the_move.from_square]][the_move.to_square];
	if (succ > fail_count) {
	  likely_good_move = 1;
	  possibly_good_move = 1;
	} else if (succ == fail_count) {
	  likely_good_move = 0;
	  possibly_good_move = 1;
	} else {
	  likely_good_move = 0;
	  possibly_good_move = 0;
	}
      }

      if ((the_move.piece == PAWN)
	  && (square[the_move.to_square] & (pawn_start[WHITE] | pawn_start[BLACK])))
	pawn_push = 1;
      else
	pawn_push = 0;
      makemove(board,&the_move,depth);
      if (legal_moves == 0) {
	temp_movelist[best].value =
	  -alphabeta(board,-beta,-alpha,depth-1,1,1,0,0,0,1,&best_move_below);
      } else {
	if (!own_king_check
	    && depth > 2
	    && !(the_move.type & CAPTURE_MOVE)
	    && !(the_move.type & PROMOTION_MOVE)
	    && !pawn_push
	    && !check_move
	    && legal_moves >= 14
	    && !possibly_good_move
	    && fail_count >= 10) {
	  temp_movelist[best].value =
	    -alphabeta(board,-alpha-1,-alpha,depth-2,1,1,0,0,0,0,
		       &best_move_below);
	} else {
	  temp_movelist[best].value = alpha + 1; //hack to make sure full depth search is done
	}
	if (temp_movelist[best].value > alpha
	    && temp_movelist[best].value != -KINGTAKEN) {
	  temp_movelist[best].value =
	    -alphabeta(board,-alpha-1,-alpha,depth-1,1,1,0,0,0,0,
		       &best_move_below);
	  if (temp_movelist[best].value > alpha
	      && temp_movelist[best].value != -KINGTAKEN)
	    temp_movelist[best].value =
	      -alphabeta(board,-beta,-alpha,depth-1,1,1,0,0,0,1,
			 &best_move_below);
	}
      }
      unmakemove(board,&the_move,depth);
      if (abort_search)
	fail = 1;
      else {
	if (temp_movelist[best].value == -KINGTAKEN)
	  temp_movelist[best].value = KINGTAKEN;
	if (temp_movelist[best].value != KINGTAKEN) {
	  /*if (!abort_search && temp_movelist[best].value > valmax) {
	    best_move_sought_deeper = 1;
	    }*/
	  valmax = maxval(temp_movelist[best].value,valmax);
	  /* Set alpha to maxval(alpha,valmax-1) rather than maxval(alpha,valmax).
	     Otherwise the following could happen: Lets say that the first
	     move gave a value of 5, and then alpha would be set to 5. Then
	     for the next move the search at a one ply deeper level gets a
	     5 for one of its moves. That would abort the search and give
	     a 5 for the root node's move number two. But it is in fact
	     possible that move number two is much worse than that. And if
	     move number one and number two get the same score, then it's
	     possible that the postanalysis function picks number two. */
	  alpha = maxval(alpha,valmax-1);
	  legal_moves++;
	  at_move++;
	}
	searched_list[best] = 1;
	if (valmax >= beta)
	  fail = 1;
	/*else {
	  /* Use every move that has been fully examined, even if we run out of
	     time before all moves have been searched for this ply. However if
	     we ran out of time during the search of _this_ particular move,
	     then of course this move can't be used. /
	  if (!abort_search) {
	    movelist[best] = temp_movelist[best];
	    /* In order to favor moves searched to a higher depth, add
	       depth*5 to the value. Note: If we have a mate value, then
	       don't add anything, since the mate values describe how
	       many moves are left to a mate. /
	    if (movelist[best].value != KINGTAKEN
		&& movelist[best].value > (-INFTY + 150)
		&& movelist[best].value < (INFTY - 150))
	      movelist[best].value += depth*5;
	  }
	}*/
      }
    }
    fail = 0;
    set_nbr_moves(legal_moves);

    if (!abort_search && next_state(valmax,&alpha,&beta,&org_alpha,&org_beta,
				    &state,&re_searches) == 0) {
      level_completed();
      if (second_last_iter_nbr_nodes != 0)
	*pre_pre_last_iter_nbr_nodes = second_last_iter_nbr_nodes;
      if (*last_iter_nbr_nodes != 0)
	second_last_iter_nbr_nodes = *last_iter_nbr_nodes;
      *last_iter_nbr_nodes = visited_nodes - nbr_nodes_tmp;
      iteration_depth++;
      nbr_researches_at_curr_level = nbr_fail_lows_at_curr_level = 0;
      if (search_status & NORMAL_SEARCH)
	printf("depth = %d\n",depth);
      if (post) {   //then send output to xboard
	printf("%d %d 0 0 ",depth,valmax);
	str = (char *) malloc(100*sizeof(char));
	for (i = 0; i < mcount; i++) {
	  if (temp_movelist[i].value == valmax) {
	    the_move.from_square = (char) get_first_bitpos(temp_movelist[i].fsquare);
	    the_move.to_square = (char) get_first_bitpos(temp_movelist[i].tsquare);
	    the_move.piece = (char) temp_movelist[i].piece;
	    the_move.type = (char) temp_movelist[i].type;
	    move2str(&the_move,str);
	    printf("%s ",str);
	  }
	}
	free(str);
	printf("\n");
      }
      /* Copy the temporary movelist into the list that will be returned. */
      for (i = 0; i < mcount; i++) {
	movelist[i] = temp_movelist[i];
      }

      if (time_is_up(0)) {
	//total_time_aborts++;
	break;
      }

      /*--------------------------------------------------------------------
	| If there will be a draw, or if we find a quick mate, or if there |
	| is just one legal move, or if we have reached max depth, then    |
	| stop searching.                                                  |
	-------------------------------------------------------------------*/
      if ((board->moves_left_to_draw - depth <= 0 && valmax == 0)
	  || ((valmax <= (-INFTY + 150)) || (valmax >= (INFTY - 150)))
	  || (legal_moves < 2) || (depth >= max_depth))
	break;
      depth++;
      //attempt_nbr = 1;
      state = 1;
    } else {
      nbr_researches_at_curr_level++;
    }

    if (abort_search) {
      //if (!best_move_sought_deeper)
	depth--;   //since we didn't finish this search
      if (search_status & NORMAL_SEARCH) {
	printf("Search aborted.\n");
	total_time_aborts++;
	update_statistics_for_abort_in_tree();
      }
      break;
    }
  }
  print_elapsed_time(visited_nodes);
  update_sequence();

  if (search_status & NORMAL_SEARCH) {
    searches++;
    search_depth += depth;
  }

  /* These are only used in the search, and they should be cleared before
     next search. */
  board->captures[WHITE] = 0;
  board->captures[BLACK] = 0;

  abort_search = 0;
  if (search_status & NORMAL_SEARCH)
    printf("Ply = %d\n",depth);

  free(temp_movelist);
  free(searched_list);

  *searched_depth = depth;

  if (!(search_status & NO_SEARCH) && !(search_status & PONDERING))
    search_status |= BEST_MOVE_FOUND;

  return search_status;
}
