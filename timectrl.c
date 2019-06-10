/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "timectrl.h"
#ifndef _CONFIG_
#define _CONFIG_
#include "config.h"
#endif
#ifdef _MSC_VER
#include "timeoday.c"
#else
#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#include <unistd.h>
#else
#include <sys/timeb.h>
#endif
#endif
#ifdef _MSC_VER
#define HAVE_GETTIMEOFDAY 1
#endif
#include "misc.h"
#include "parse.h"
#include "alfabeta.h"
#include <stdio.h>
#include <stdlib.h>

/* own_time and opp_time measure the clocks of the engine and the opponent
   respectively, measured in milliseconds. */
int own_time = 300000;  //default = 5 minutes
int opp_time = 300000;  //default = 5 minutes
int own_time_stored;
int own_checkpoint_time = -1;
int opp_checkpoint_time = -1;
int new_level_checkpoint;
int prev_level_total_time_spent;
int prev_estimate;

/* Default timectrl is TIMECTRL_NEWTIME, which is what xboard uses
   by default, when you don't run in ICS-mode. */
int timectrl = TIMECTRL_NEWTIME;
int moves_per_timecontrol = 40;
int base = 300000;  //default = 5 minutes
int increment = 0;
int time_per_move = 1000;
int absolute_time_limit;
int clock_mode = NORMAL_CLOCK_MODE;
int64 vain_thinking = 0;
int at_move;
int iteration_depth;
int total_nbr_moves = 0;
int fixed_thinking_time = 1000;
extern int nbr_researches_at_curr_level;
extern int nbr_fail_lows_at_curr_level;
extern int64 tot_visited_nodes;
extern int64 tot_elapsed_time;
extern int64 gratis_time;
int64 elapsed_time_in_curr_game = 0;
int64 elapsed_time_in_curr_game_stored;
int64 supposed_elapsed_time_in_curr_game = 0;
int64 supposed_elapsed_time_in_curr_game_stored;
extern int valmax;
extern int prev_valmax;
extern int recapture;
extern int search_status;

void showtime(int engine_col) {
  if (engine_col == WHITE)
    printf("White: %d:%d, Black: %d:%d\n",
	   ((own_time - time_elapsed(own_checkpoint_time))/1000)/60,
	   ((own_time - time_elapsed(own_checkpoint_time))/1000)%60,
	   ((opp_time - time_elapsed(opp_checkpoint_time))/1000)/60,
	   ((opp_time - time_elapsed(opp_checkpoint_time))/1000)%60);
  else
    printf("White: %d:%d, Black: %d:%d\n",
	   ((opp_time - time_elapsed(opp_checkpoint_time))/1000)/60,
	   ((opp_time - time_elapsed(opp_checkpoint_time))/1000)%60,
	   ((own_time - time_elapsed(own_checkpoint_time))/1000)/60,
	   ((own_time - time_elapsed(own_checkpoint_time))/1000)%60);
}

int get_checkpoint_time() {
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tval;
    if (gettimeofday(&tval,0) != 0) {
      perror("Error getting the time.");
      infolog("Error getting the time.");
      exit(1);
    } else
      return tval.tv_sec * 1000 + tval.tv_usec / 1000;
#else
    struct timeb tp;

    ftime(&tp);
    return tp.time * 1000 + tp.millitm;
#endif
}

void start_own_clock() {
  own_time_stored = own_time;
  supposed_elapsed_time_in_curr_game_stored =
    supposed_elapsed_time_in_curr_game;
  elapsed_time_in_curr_game_stored = elapsed_time_in_curr_game;
  own_checkpoint_time = get_checkpoint_time();
  new_level_checkpoint = own_checkpoint_time;
}

void start_opp_clock() {
  opp_checkpoint_time = get_checkpoint_time();
}

void stop_own_clock() {
  own_time = own_time - time_elapsed(own_checkpoint_time);
  own_checkpoint_time = -1;
}

void stop_opp_clock() {
  opp_time = opp_time - time_elapsed(opp_checkpoint_time);
  opp_checkpoint_time = -1;
}

void restore_time() {
  own_time = own_time_stored;
  supposed_elapsed_time_in_curr_game =
    supposed_elapsed_time_in_curr_game_stored;
  elapsed_time_in_curr_game = elapsed_time_in_curr_game_stored;
}

void level_completed() {
  prev_estimate = INFTY;
  prev_level_total_time_spent = time_elapsed(own_checkpoint_time);
  new_level_checkpoint = get_checkpoint_time();
}

void update_statistics_for_abort_in_tree() {
  vain_thinking += time_elapsed(new_level_checkpoint);
}

int time_elapsed(int checkpoint) {
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tval;
    int currtime;

    if (checkpoint == -1)
      return 0;

    if (gettimeofday(&tval,0) != 0) {
      perror("Error with getting the time.");
      infolog("Error with getting the time.");
      exit(1);
    } else {
      currtime =  tval.tv_sec * 1000 + tval.tv_usec / 1000;
      return currtime - checkpoint;
    }
#else
    struct timeb tp;
    int currtime;

    if (checkpoint == -1)
      return 0;

    ftime(&tp);
    currtime = tp.time * 1000 + tp.millitm;
    return currtime - checkpoint;
#endif
}

void print_elapsed_time(int visited_nodes) {
  int milliseconds = time_elapsed(own_checkpoint_time);
  if (search_status & NORMAL_SEARCH) {
    printf("Elapsed time: ");
    printprettytime(milliseconds);
    printf("\n");
    printf("Search speed: ");
    printprettynodes(visited_nodes,milliseconds);
    printf("\n");
    tot_visited_nodes += visited_nodes;
    tot_elapsed_time += milliseconds;
  }
  elapsed_time_in_curr_game += milliseconds;
}

void update_gratis_time() {
  int millisec;
  millisec = time_elapsed(own_checkpoint_time);
  gratis_time += millisec;
  printf("Increasing time_per_move with ");
  printprettytime(millisec);
  printf(" (because predicted move was played).\n");
  time_per_move += millisec;
  absolute_time_limit = minval(own_time/2,6*time_per_move);
  supposed_elapsed_time_in_curr_game += millisec;
}

void set_nbr_moves(int m) {
  total_nbr_moves = m;
}

/* Call this function if the engine should get a specific number of
   milliseconds to think. Of course it can still stop earlier, if all
   the nodes for the specified depth have been searched. */
void set_fixed_thinking_time(int f) {
  fixed_thinking_time = f;
}

/* The mode can be NORMAL_CLOCK_MODE or FIXED_CLOCK_MODE. */
void set_clock_mode(int mode) {
  clock_mode = mode;
}

int get_clock_mode() {
  return clock_mode;
}

/* TODO #1: There can in reality be newtime and inc at the same time.
   TODO #2: When playing with higher timecontrols, there is often
   too much unused time in the end. Try to decrease the divisor to
   for example 35 if there is more than 5 minutes left. I.e. own_time/35.
   TODO #3: It seems like too litle time is used up if pondering is on.
   Possibly use own_time/30 instead of own_time/40 if pondering is on.
   (In fact it seems like pondering makes the program at most 5% better,
   and I think that's because of poor time handling for pondering)
   TODO #4: Check out Crafty's time handling regarding pondering. */
void set_time_per_move(struct board *board, int movetype,
		       int just_out_of_book) {
  prev_level_total_time_spent = 0;
  prev_estimate = INFTY;
  int old_supp = supposed_elapsed_time_in_curr_game;

  if (clock_mode == NORMAL_CLOCK_MODE) {
    if (timectrl == TIMECTRL_NEWTIME) {
      /* Make sure we have some extra time left at the end by
	 adding +1 in the denominator. */
      time_per_move = (int) ((double) base / moves_per_timecontrol);
      absolute_time_limit = minval(own_time/2,6*time_per_move);

      if (movetype != MOVETYPE_FIND_MOVE_TO_PONDER) {
	/* If one has more time left than one's opponent, then it's okay
	   to think longer, or else if one has less time left than the
	   opponent, then decrease the time allowed for thinking. */
	if (own_time < opp_time)
	  time_per_move -= own_time/120;
	else if (own_time > 2*opp_time && own_time > 4*1000)
	  time_per_move += own_time/50;
	else if (own_time > opp_time)
	  time_per_move += own_time/120;
	
	/* Adjust the time in case too little time has been used. */
	supposed_elapsed_time_in_curr_game = time_per_move*(1+board->move_number/2);
	if (old_supp > elapsed_time_in_curr_game) {
	  /* If an opening book was used for the first X moves, then we don't
	     want to use up all the "bonus time" at the first move, but rather
	     spread it out at several moves. If we are less than 10 seconds
	     "ahead", then use up all of it in one shot. */
	  if (old_supp > elapsed_time_in_curr_game + 10000) {
	    printf("Increasing with %d\n",(int) ((double)(old_supp - elapsed_time_in_curr_game)/5));
	    time_per_move += (int) ((double)(old_supp - elapsed_time_in_curr_game)/5);
	  } else {
	    printf("increasing with %d\n",old_supp - elapsed_time_in_curr_game);
	    time_per_move += (old_supp - elapsed_time_in_curr_game);
	  }
	}
      }
    } else if (timectrl == TIMECTRL_INC) {
      /* If more than 10 seconds left, then use the whole increment
	 + on fortieth of the remaining time. Else just use the increment.
	 Also make sure that amundsen doesn't loose right away if the
	 increment is very large, and the initial time is low. */
      time_per_move = increment + own_time/40;
      absolute_time_limit = minval(own_time/2,6*time_per_move);

      if (movetype != MOVETYPE_FIND_MOVE_TO_PONDER) {
	/* If one has more time left than one's opponent, then it's okay
	   to think longer, or else if one has less time left than the
	   opponent, then decrease the time allowed for thinking. */
	if (own_time < opp_time)
	  time_per_move -= own_time/120;
	else if (own_time > 2*opp_time && own_time > 4*1000)
	  time_per_move += own_time/50;
	else if (own_time > opp_time)
	  time_per_move += own_time/120;
	
	/* Adjust the time in case too little time has been used. */
	supposed_elapsed_time_in_curr_game += time_per_move;
	if (old_supp > elapsed_time_in_curr_game) {
	  printf("increasing with %d\n",old_supp - elapsed_time_in_curr_game);
	  time_per_move += (old_supp - elapsed_time_in_curr_game);
	}
      }
    } else {    //timectrl == TIMECTRL_NOINC
      /*-----------------------------------------------------------------
       | In the beginning of the game use a little over 1/40th of the   |
       | total remaining thinking time, and as the game makes progress, |
       | use a gradually bigger and bigger part of the remaining total  |
       | search time, until the end when almost 1/20th is used.         |
       -----------------------------------------------------------------*/
      time_per_move = own_time/40;
      absolute_time_limit = minval(own_time/2,6*time_per_move);

      if (movetype != MOVETYPE_FIND_MOVE_TO_PONDER) {
	/* If one has more time left than one's opponent, then it's okay
	   to think longer, or else if one has less time left than the
	   opponent, then decrease the time allowed for thinking. */
	if (own_time < opp_time)
	  time_per_move -= own_time/120;
	else if (own_time > 2*opp_time && own_time > 4*1000)
	  time_per_move += own_time/50;
	else if (own_time > opp_time)
	  time_per_move += own_time/120;
      }
    }

    /*-------------------------------------------------------------------
     | If we just got out of the book and we're not just trying to find |
     | a move to ponder, then increase the search time.                 |
     -------------------------------------------------------------------*/
    if (just_out_of_book && movetype != MOVETYPE_FIND_MOVE_TO_PONDER) {
      //printf("old time_per_move = %d\n",time_per_move);
      //printf("old absolute_time_limit = %d\n",absolute_time_limit);
      time_per_move = (int)((float) time_per_move * 1.5);
      absolute_time_limit = (int)((float) absolute_time_limit * 1.5);
      //printf("new time_per_move = %d\n",time_per_move);
      //printf("new absolute_time_limit = %d\n",absolute_time_limit);
      printf("Just got out of book, increasing search time.\n");
      infolog("Just got out of book, increasing search time.");
    }

  } else {    //clock_mode == FIXED_CLOCK_MODE
    time_per_move = fixed_thinking_time;
    absolute_time_limit = minval(own_time/2,6*time_per_move);
  }

  if (movetype == MOVETYPE_NORMAL) {
    printf("time per move: ");
  } else if (movetype == MOVETYPE_FIND_MOVE_TO_PONDER) {
    time_per_move = maxval(time_per_move/10,1);
    absolute_time_limit = maxval(absolute_time_limit/10,1);
    printf("time for finding move to ponder: ");
  } else if (movetype == MOVETYPE_PONDER) {
    printf("time to ponder move: ");
  }
  printprettytime(time_per_move);
  printf("\n");
}

/*----------------------------------------------------------------------
 | Check if we have to stop thinking (return 1), or if we can continue |
 | (return 0). If there is _very_ little time left, then concentrate   |
 | on not loosing on time, otherwise think for approximately the       |
 | allocated amount of time. Be more willing to abort the search if    |
 | a whole level of search has been completed, and be more restrictive |
 | to abort if the search is in the tree.                              |
 ----------------------------------------------------------------------*/
int safe_to_quit(int in_tree) {
  int total_time_spent_on_move;
  if (search_status == FINDING_MOVE_TO_PONDER)
    total_time_spent_on_move = time_elapsed(opp_checkpoint_time);
  else
    total_time_spent_on_move = time_elapsed(own_checkpoint_time);

  /*-----------------------------------------------------------------
   | Ok, if we get here then we have used up the time limit. Now we |
   | need to decide if it's safe to stop the search or not. If we   |
   | aren't in the tree (=all moves searched through) or we haven't |
   | finished searching the first move AND we haven't gotten 2 fail |
   | lows at the root, then it's safe to quit.                      |
   |                                                                |
   | TODO: Check out what crafty's following condition means:       |
   | if (shared->root_value == shared->root_alpha                   |
   -----------------------------------------------------------------*/
  if ((!in_tree || at_move == 0) && nbr_fail_lows_at_curr_level < 2)
    return 1;

  /*---------------------------------------------------------------------
   | We have at least one moved fully searched at the root for the      |
   | current iteration. If this move isn't significantly worse than     |
   | the best move from the previous iteration, then it's safe to quit. |
   | If we are way ahead, then this margin is increased.                |
   ---------------------------------------------------------------------*/
  if ((valmax >= prev_valmax - 24 && nbr_fail_lows_at_curr_level < 2)
      || (valmax > 350 && valmax >= prev_valmax - 50)) {
    if (total_time_spent_on_move > time_per_move*2)
      return 1;
    else
      return ((at_move == 0 || !in_tree) ? 1 : 0);
  }

  /*----------------------------------------------------------------
   | We have problems at the root. Increase the search time. If we |
   | are still quite a bit ahead and the drop isn't so bad, then   |
   | quit if we aren't in the tree.                                |
   ----------------------------------------------------------------*/
  if (total_time_spent_on_move < time_per_move*2.5
      && total_time_spent_on_move + 5000 < own_time)
    return 0;
  if ((valmax >= prev_valmax - 67 && nbr_fail_lows_at_curr_level < 2)
      || valmax > 550)
    return ((at_move == 0 || !in_tree) ? 1 : 0);
  

  /*-----------------------------------------------------------------
   | We are having very serious troubles at the root. Increase the  |
   | search time a lot. (Remember that loosing material is the same |
   | thing as loosing the game)                                     |
  ------------------------------------------------------------------*/
  if (total_time_spent_on_move < time_per_move*7
      && total_time_spent_on_move + 5000 < own_time)
    return 0;
  return 1;

}


/* This function returns 1 if the engine has to stop thinking, and
   0 if it can continue. Basically this function tries to be restrictive
   with aborting a search in the tree, and more willing to abort if a
   whole level of search has been completed. */
int time_is_up(int in_tree) {
  int time_elapsed_since_last_level, estimated_time_to_finish;
  int total_time_spent_on_move;
  double percentage_done;
  double time_percentage_done;

  time_elapsed_since_last_level = time_elapsed(new_level_checkpoint);
  if (search_status == FINDING_MOVE_TO_PONDER)
    total_time_spent_on_move = time_elapsed(opp_checkpoint_time);
  else
    total_time_spent_on_move = time_elapsed(own_checkpoint_time);
  percentage_done = (double) at_move / total_nbr_moves;
  time_percentage_done = (double) total_time_spent_on_move / time_per_move;

  /*----------------------------------------------
   | Never abort the search if we are pondering. |
   ----------------------------------------------*/
  if (search_status == PONDERING)
    return 0;

  /*------------------------------------------
   | Are we in a mood where a fixed number   |
   | of seconds should be used for thinking? |
   ------------------------------------------*/
  if (clock_mode == FIXED_CLOCK_MODE) {
    if (total_time_spent_on_move > time_per_move) {
      return 1;
    } else
      return 0;
  }

  /*---------------------------------------------------------------------
   | If we have less than one second left on the clock, then just       |
   | concentrate on not losing on time, and therefore we abort the      |
   | search right away if we have searched for at least one millisecond |
   | and we aren't in the tree.                                         |
  ----------------------------------------------------------------------*/
  if (own_time < 1000 && !in_tree && total_time_spent_on_move > 0)
    return 1;

  if (total_time_spent_on_move <= 1) //this one probably isn't necessary
    return 0;

  /*----------------------------------------------
   | Never abort iteration depth is less than 3. |
   ----------------------------------------------*/
  if (iteration_depth < 3)
    return 0;

  /*--------------------------------------------------------------
   | Abort if we have exceeded the absolute maximum search time. |
   --------------------------------------------------------------*/
  if (total_time_spent_on_move > absolute_time_limit)
    return 1;

  /*-----------------------------------------------------------------------
   | If this is an easy move then abort even if we have searched for less |
   | than the stipulated time.                                            |
   | Definition of easy move:                                             |
   | Capture of the piece moved by the opponent's previous move           |
   | && This capture is the best move                                     |
   | && This capture is more than two pawn's value better                 |
   |    than the second best move                                         |
   | && The value of this best move is less than 100 (one pawn)           |
   -----------------------------------------------------------------------*/
  if (recapture && time_per_move > 700 && time_percentage_done >= 0.33)
    return 1;

  /*------------------------------------------------------------
   | Attempt to estimate the time it will take to finish this  |
   | whole iteration. The estimation will be more accurate the |
   | more moves that have been searched through at the root.   |
   ------------------------------------------------------------*/
  if (at_move == 0) {
    //The average branching factor is around four, so that's a good guess
    estimated_time_to_finish = prev_level_total_time_spent*4;
  } else {
    estimated_time_to_finish = time_elapsed_since_last_level
      * (1/percentage_done) * (nbr_researches_at_curr_level + 1)
      - time_elapsed_since_last_level;

    /*---------------------------------------------------------------
     | Correct the estimated time to finish since it will otherwise |
     | go up as the search makes progress within one root move.     |
     ---------------------------------------------------------------*/
    if (nbr_researches_at_curr_level == 0)
      estimated_time_to_finish =
	minval(estimated_time_to_finish,prev_estimate);
  }
  prev_estimate = estimated_time_to_finish;

  if ((total_time_spent_on_move + estimated_time_to_finish)
      < time_per_move*0.50)
    return 0;
  else if (!in_tree)
    return 1;
  else if (((total_time_spent_on_move + estimated_time_to_finish)
	    < time_per_move*2)
	   || (percentage_done > 0.90))
    return 0;
  else
    return safe_to_quit(in_tree);

}
