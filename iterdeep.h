#ifndef _ITERDEEP_
#define _ITERDEEP_

#include "bitboards.h"
#include "misc.h"

#define VALWINDOW (VAL_PAWN/2)
#define SECOND_ATTEMPT_VALWINDOW (VAL_PAWN)
#define MAX_ITERATIONS 40

int next_state(int val, int *alpha, int *beta, int *org_alpha, int *org_beta,
	       int *state, int *re_searches);
int iterative_deepening_aspiration_search(struct board *board,
					  struct move_list_entry *movelist,
					  int mcount, int max_depth,
					  int movetype, int *searched_depth,
					  int *last_iter_nbr_nodes,
					  int *pre_pre_last_iter_nbr_nodes,
					  int just_out_of_book);

#endif        //_ITERDEEP_

