#ifndef _TIMECTRL_
#define _TIMECTRL_
#ifdef _MSC_VER
#include <windows.h>
#include <time.h>
#include "dos.h"
int gettimeofday(struct timeval * tp, void *tzp);
#endif
#include "bitboards.h"

/* The program supports three types of time controls:
   TIMECTRL_NEWTIME plays a certain number of moves, and then
   additional time is obtained.
   TIMECTRL_INC has a base time and in addition to that, it has a
   certain amount of time for each move, and the time that is not
   used up is accumulated for later use.
   TIMECTRL_NOINC has a base time, and no additional time per move,
   and it doesn't get additional time after a certain number of moves. */
#define TIMECTRL_NEWTIME 1
#define TIMECTRL_INC 2
#define TIMECTRL_NOINC 3

#define NORMAL_CLOCK_MODE 0
#define FIXED_CLOCK_MODE 1

void showtime(int engine_col);
int get_checkpoint_time(void);
void start_own_clock(void);
void start_opp_clock(void);
void stop_own_clock(void);
void stop_opp_clock(void);
void restore_time(void);
void level_completed(void);
void update_statistics_for_abort_in_tree(void);
int time_elapsed(int checkpoint);
void print_elapsed_time(int visited_nodes);
void update_gratis_time();
void set_nbr_moves(int);
void set_fixed_thinking_time(int);
void set_clock_mode(int);
int get_clock_mode(void);
void set_time_per_move(struct board *board, int movetype,
		       int just_out_of_book);
int time_is_up(int);

#endif         //_TIMECTRL_
