/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "alfabeta.h"
#include "iterdeep.h"
#include "makemove.h"
#include "genmoves.h"
#include "display.h"
#include "timectrl.h"
#include "eval.h"
#include "hash.h"
#include "misc.h"
#include "slump.h"
#include "swap.h"
#include "threads.h"
#ifndef _CONFIG_
#define _CONFIG_
#include "config.h"
#endif

int mode = NORMAL_MODE;
int engine_color = BLACK;
int pondering_in_use = 1;  //by default pondering is turned on
int max_depth = MAX_ITERATIONS;
struct move guessed_opp_move;
struct board stored_board;

/* We have one list for keeping track of the moves played in the game,
   and then (for cpu cache efficiency reasons) another more compact list
   for keeping track of repetitions on the board. */
struct history_entry **historik;
int hlistsize = 100;
struct repetition_entry **repetition_list;
int rlistsize = 100;

//double branch_factor_sum = 0;
//double branch_factor_count = 0;
int64 nodes_last = 0;
int64 nodes_pre_pre_last = 0;
int failed_predictions = 0;
int successful_predictions = 0;

#ifdef AM_DEBUG
int gamelog = 0;
#endif

//extern int endgame;
extern int probes;
extern int sc_hits;
extern int m_hits;
extern int b_hits;
extern int an_hits;
extern int misses;
int tot_probes = 0, tot_hits = 0;
int64 tot_visited_nodes = 0;
int64 tot_elapsed_time = 0;
int64 gratis_time = 0;
extern int64 elapsed_time_in_curr_game;
extern int64 supposed_elapsed_time_in_curr_game;
extern int visited_nodes;
extern int fpruned_moves;
extern int node_count_at_last_check;
extern int quiescence_nodes;
extern int pawn_probes;
extern int pawn_hits;
extern int pawn_misses;
extern int presearch_pieceval[2];
extern int presearch_pawnval[2];
extern int rotate90to0[64];
extern int rotateNEto0[64];
extern int rotateNWto0[64];

extern struct killers **killers;
extern int cutoffs;
extern int cutoffs_first;
extern int nullmoves_tried;
extern int nullmove_cutoffs;
extern int lmr_reductions;
extern int lmr_reductions_tried;
extern int searches;
extern int search_depth;
extern struct refutation_entry *ref_table;
extern struct transposition_entry *transp_table;
extern struct pawn_entry *pawn_table;
extern bitboard square[64];
extern struct attack attack;
extern bitboard pawn_lastrow[2];
extern int own_time;
extern int opp_time;
extern int timectrl;
extern int moves_per_timecontrol;
extern int base;
extern int increment;
extern int post;
extern int64 vain_thinking;
extern int abort_search;
extern int total_time_aborts;
extern int dist_to_square[64][64];
extern int dist_to_side[64];
extern int historymovestable[2][64][64];

char *input_from_thread;
int pending_input;

/* This function reallocates the size of a history_entry list in the memory. */
int change_historylist_size(struct history_entry **list, int new_size) {
  struct history_entry *new_list;

  if((new_list = (struct history_entry *)
      realloc(*list,(new_size + 1)*sizeof(struct history_entry))) == NULL) {
    perror("Realloc failed");
    infolog("Realloc failed");
    exit(1);
  } else
    *list = new_list;
  
  return new_size;
}

/* This function reallocates the size of a repetition_entry list
   in the memory. */
int change_repetitionlist_size(struct repetition_entry **list, int new_size) {
  struct repetition_entry *new_list;

  if((new_list = (struct repetition_entry *)
      realloc(*list,(new_size + 1)*sizeof(struct repetition_entry))) == NULL) {
    perror("Realloc failed");
    infolog("Realloc failed");
    exit(1);
  } else
    *list = new_list;
  
  return new_size;
}

void move2history(struct board *board, struct move *move, int nullmove) {
  if (board->move_number > hlistsize - max_depth - 20) {
    infolog("Increasing size of game history.");
    hlistsize = change_historylist_size(historik,hlistsize
					+ max_depth + 20);
  }

  /* TODO: This nullmove stuff could be removed from here since it's
     no longer used in the tree. */
  if (nullmove) {
    (*historik)[board->move_number].nullmove = 1;
  } else {
    (*historik)[board->move_number].nullmove = 0;
    (*historik)[board->move_number].move.from_square = move->from_square;
    (*historik)[board->move_number].move.to_square = move->to_square;
    (*historik)[board->move_number].move.piece = move->piece;
    (*historik)[board->move_number].move.type = move->type;
    (*historik)[board->move_number].move.old_to_square = move->old_to_square;
    (*historik)[board->move_number].move.old_castling_status[0] =
      move->old_castling_status[0];
    (*historik)[board->move_number].move.old_castling_status[1] =
      move->old_castling_status[1];
    (*historik)[board->move_number].move.old_passant = move->old_passant;
    (*historik)[board->move_number].move.old_moves_left_to_draw =
      move->old_moves_left_to_draw;
  }
  (*historik)[board->move_number].board = *board;
}

/*------------------------------------------------------------------
 | Add the current board to the list, and check if it has occurred |
 | twice before in the tree. If so return 1, otherwise 0.          |
 ------------------------------------------------------------------*/
int is_draw_by_repetition(struct board *board) {
  int i, rep_count = 1;
  char left_to_draw_spr = board->moves_left_to_draw;

  /* Add the current entry, and increase the list size if necessary.*/
  if (board->move_number > rlistsize - MAX_ITERATIONS - 20) {
    infolog("Increasing sizeof repetition list.");
    rlistsize = change_repetitionlist_size(repetition_list,rlistsize
					   + MAX_ITERATIONS + 20);
  }
  (*repetition_list)[board->move_number].moves_left_to_draw =
    board->moves_left_to_draw;
  (*repetition_list)[board->move_number].zobrist_key = board->zobrist_key;

  /* Check if there are three identical positions in the tree. */
  for (i = board->move_number - 1; i >= 0; i--) {
    if ((*repetition_list)[i].moves_left_to_draw < left_to_draw_spr)
      break; //stop searching if we hit a non-repetitive move
    if ((*repetition_list)[i].zobrist_key == board->zobrist_key)
      rep_count++;
    if (rep_count >= 3)
      return 1;
    left_to_draw_spr = (*repetition_list)[i].moves_left_to_draw;
  }
  return 0;
}

void preanalysis(struct board *brd, struct move_list_entry **movelist, int *mcount, int pondering) {
  int movables = 0, piece_nbr = 0, nbr_moves = 0, i;
  struct moves moves[16];
  bitboard typetargets, target;
  int pawns[2];
  struct move move;

  sc_hits = 0;
  m_hits = 0;
  b_hits = 0;
  an_hits = 0;
  misses = 0;
  probes = 0;
  /* Make sure node_count_at_last_check is zeroed at the
     same time as visited_nodes. */
  visited_nodes = node_count_at_last_check = fpruned_moves = 0;
  quiescence_nodes = 0;
  pawn_probes = 0;
  pawn_hits = 0;
  pawn_misses = 0;

  //if (brd->moves_left_to_draw <= 20)
  //clear_hashtable(); //50 move rule fix

  for (i = 0; i < 2; i++) {
    pawns[i] = bitcount(brd->piece[i][PAWN]);
  }

  /* Check the material values. */
  for (i = 0; i < 2; i++) {
    presearch_pawnval[i] = bitcount(brd->piece[i][PAWN])*VAL_PAWN;
    presearch_pieceval[i] = bitcount(brd->piece[i][KNIGHT])*VAL_KNIGHT
      + bitcount(brd->piece[i][BISHOP])*VAL_BISHOP
      + bitcount(brd->piece[i][ROOK])*VAL_ROOK
      + bitcount(brd->piece[i][QUEEN])*VAL_QUEEN
      + bitcount(brd->piece[i][KING])*VAL_KING;
  }

  /* A board position should be in a legal state upon calling this
     function, so we should never be able to take the opponents king. */
  if (generate_moves(brd,moves,&movables) != 0) {
    infolog("ERROR!!! ILLEGAL MOVES SHOULD NOT BE FOUND HERE!");
    printf("ERROR!!! ILLEGAL MOVES SHOULD NOT BE FOUND HERE!\n");
    exit(1);
  }

  /* Count the number of possible moves, so we know how to initialize
     the movelist. */
  for (i = 0; i < movables; i++)
    nbr_moves += bitcount(moves[i].targets);
  *movelist = (struct move_list_entry *) malloc(nbr_moves*sizeof(struct move_list_entry));

  /* Go through all movable pieces and add them to the movelist. */
  for (piece_nbr = 0; piece_nbr < movables; piece_nbr++) {
      typetargets = moves[piece_nbr].targets;
      /* Go through the targets one at a time. */
      while (typetargets != 0) {
        target = getlsb(typetargets);
        (*movelist)[*mcount].fsquare = moves[piece_nbr].source;
        (*movelist)[*mcount].tsquare = target;
        (*movelist)[*mcount].piece = moves[piece_nbr].piece;
	move.from_square = get_first_bitpos((*movelist)[*mcount].fsquare);
	move.to_square = get_first_bitpos((*movelist)[*mcount].tsquare);
	move.piece = (*movelist)[*mcount].piece;
        (*movelist)[*mcount].type = get_movetype(brd,&move);
	(*movelist)[*mcount].value = 0; //so we can check if != KINGTAKEN
        (*mcount)++;
        typetargets = typetargets & ~target;
      }
  }
  return;
}

int postanalysis(struct board *board, struct move_list_entry **movelist,
		 int mcount, struct move *returnmove, int to_search_depth,
		 int pondering, int last_iter_nbr_nodes,
		 int pre_pre_last_iter_nbr_nodes) {
  struct move_list_entry *validmoves;
  struct move_list_entry *goodmoves;
  struct move_list_entry *bestmoves;
  int i, j, b, bestvalue;
  char *str;
  struct move the_move;
  int returnval;
  //double b_factor;
  int temp = mcount;
  struct move best_move_below;

  str = (char *) malloc(500*sizeof(char));

  /* Update statistics if not pondering. */
  tot_probes += probes;
  tot_hits += sc_hits + m_hits + b_hits + an_hits;
  //b_factor = nroot(to_search_depth,(visited_nodes-quiescence_nodes));

  /* Update the branching factor (in some cases we'll search only one ply,
     and then we cannot calculate the EBF, so therefore we make sure that
     both node counters have a non-zero value). */
  if (last_iter_nbr_nodes != 0 && pre_pre_last_iter_nbr_nodes != 0) {
    //b_factor = (double) last_iter_nbr_nodes / second_last_iter_nbr_nodes;
    //printf("b_factor = %4.2f\n",(double)b_factor);
    //printf("last_iter_nbr_nodes = %d, second_last_iter_nbr_nodes = %d\n",last_iter_nbr_nodes,second_last_iter_nbr_nodes);
    //exit(1);
    //branch_factor_sum += b_factor;
    //branch_factor_count++;
    nodes_last += last_iter_nbr_nodes;
    nodes_pre_pre_last += pre_pre_last_iter_nbr_nodes;
  }

  /* Print statistics if not pondering. */
  if (!pondering) {
    printf("Visited nodes            : tot=%d, q=%d\n",
	   visited_nodes,quiescence_nodes);
    printf("Futility pruned moves    : %d\n",fpruned_moves);
    printf("\n");
    printf("Average search speed     : abs=");
    printprettynodes(tot_visited_nodes,tot_elapsed_time);
    if (pondering_in_use) {
      printf(", eff=");
      printprettynodes(tot_visited_nodes,tot_elapsed_time-gratis_time);
    }
    printf("\n");
  }
  if (!pondering) {
    printf("Hash rate                : norm=%4.2f\%, pawn=%4.2f\%\n",(double) 100*tot_hits/tot_probes,(double)(((double)100*pawn_hits)/((double)pawn_probes)));
    printf("Cutoff rate              : %4.2f\%\n",(double)100*cutoffs_first/cutoffs);
    printf("Nullmove rate            : %4.2f\%\n",(double)100*nullmove_cutoffs/nullmoves_tried);
    printf("Late move reduction rate : %4.2f\%\n",(double)100*lmr_reductions/lmr_reductions_tried);
    printf("Avg. depth               : %4.2f\n",(double)search_depth/searches);
    if (pondering_in_use)
      printf("Predictions              : %4.2f (%d/%d)\n",
	     (double)successful_predictions/(failed_predictions+successful_predictions),successful_predictions,(failed_predictions+successful_predictions));
  }
  if (!pondering) {
    //printf("Eff. branching factor    : %4.2f\n",(double)branch_factor_sum/branch_factor_count);
    printf("Eff. branching factor    : %4.2f\n",(double)nroot(2,(double)nodes_last/nodes_pre_pre_last));
    //printf("branch_factor_sum = %4.2f, branch_factor_count = %4.2f\n",branch_factor_sum,branch_factor_count);
    printf("Vain thinking            : ");
    printprettytime(vain_thinking);
    printf("\n");
    if (pondering_in_use) {
      printf("Well used pondering time : ");
      printprettytime(gratis_time);
      printf("\n");
    }
    printf("Aborts                   : %4.2f\%\n",(double)100*total_time_aborts/searches);
  }

  /* Remove all invalid moves from the list. (The invalid ones have
     value == KINGTAKEN, those are the moves that checks one's own king) */
  validmoves = (struct move_list_entry *) malloc(mcount * sizeof(struct move_list_entry));
  j = 0;
  for (i = 0; i < mcount; i++)
    if ((*movelist)[i].value != KINGTAKEN)
      validmoves[j++] = (*movelist)[i];

  mcount = j;

  the_move.old_castling_status[WHITE] = board->castling_status[WHITE];
  the_move.old_castling_status[BLACK] = board->castling_status[BLACK];
  the_move.old_passant = board->passant;
  the_move.old_moves_left_to_draw = board->moves_left_to_draw;
  turn_off_hash();
  if (mcount > 0) {
    /* Put the best moves in the array goodmoves, and chose a random best move.
       (Provided there are several moves which are equally good.) */
    goodmoves = (struct move_list_entry *) malloc(mcount * sizeof(struct move_list_entry));
    bestvalue = validmoves[0].value;
    b = 0;
    for (i = 0; i < mcount; i++) {
      if (validmoves[i].value >= bestvalue) {
	if (validmoves[i].value > bestvalue)
	  b = 0;
	bestvalue = validmoves[i].value;
	goodmoves[b++] = validmoves[i];
      }
    }
    /* Now we have an array of good moves. If there are two or more moves
       in the goodmoves-array, we will take the one that gives the maximum
       "profit" after two moves. */
    if (b > 1) {
      abort_search = 0;
      for (i = 0; i < b; i++) {
	the_move.from_square = (char) get_first_bitpos(goodmoves[i].fsquare);
	the_move.to_square = (char) get_first_bitpos(goodmoves[i].tsquare);
	the_move.piece = (char) goodmoves[i].piece;
	the_move.type = (char) goodmoves[i].type;
	makemove(board,&the_move,2);
	goodmoves[i].value = -alphabeta(board,-INFTY,INFTY,1,1,1,0,0,0,1,
					&best_move_below);
	unmakemove(board,&the_move,2);
      }
      bestvalue = goodmoves[0].value;
      mcount = b;
      b = 0;
      bestmoves = (struct move_list_entry *) malloc(mcount * sizeof(struct move_list_entry));
      for (i = 0; i < mcount; i++) {
	if (goodmoves[i].value >= bestvalue) {
	  if (goodmoves[i].value > bestvalue)
	    b = 0;
	  bestvalue = goodmoves[i].value;
	  bestmoves[b++] = goodmoves[i];
	}
      }
      returnval = get_random_number(b-1);
      returnmove->from_square = (char) get_first_bitpos(bestmoves[returnval].fsquare);
      returnmove->to_square = (char) get_first_bitpos(bestmoves[returnval].tsquare);
      returnmove->piece = (char) bestmoves[returnval].piece;
      returnmove->type = (char) bestmoves[returnval].type;
      returnval = bestmoves[returnval].value;
      free(bestmoves);
    } else {
      returnmove->from_square = (char) get_first_bitpos(goodmoves[0].fsquare);
      returnmove->to_square = (char) get_first_bitpos(goodmoves[0].tsquare);
      returnmove->piece = (char) goodmoves[0].piece;
      returnmove->type = (char) goodmoves[0].type;
      returnval = goodmoves[0].value;
    }
    free(goodmoves);
  } else {
    printf("ERROR, NO LEGAL MOVE FOUND!\n");
    infolog("ERROR, NO LEGAL MOVE FOUND!");
    exit(1);
  }
  free(validmoves);
  free(*movelist);
  free(str);
  turn_on_hash();
  if (!pondering)
    printf("Value = %d\n",returnval);
  return returnval;
}

int bookmove(int *in_book, struct board *board, struct move *move) {
  int which_row = 0;
  int possible_rows[BOOKSIZE];
  int i, j;
  FILE *fp;
  const char *filename = "openings.bok";
  char *line;
  char *movestr;
  char *themove;
  struct move the_move;

  fp = fopen(filename,"r");
  if (fp == NULL) {
    infolog("Could not open book file.");
    fprintf(stderr,"Could not open book file.\n");
    *in_book = 0;
  } else {
    line = (char *) calloc(200,sizeof(char));
    movestr = (char *) malloc(20*sizeof(char));
    for (i = 0; i < BOOKSIZE; i++) {
      fscanf(fp,"%s\n",line);
      possible_rows[i] = 1;
      for (j = 0; j < (board->move_number); j++) {
	the_move.from_square = (*historik)[j].move.from_square;
	the_move.to_square = (*historik)[j].move.to_square;
	the_move.piece = (*historik)[j].move.piece;
	the_move.type = (*historik)[j].move.type;
	move2str(&the_move,movestr);
	if (!(strlen(line) > (board->move_number)*4 && movestr[0] == line[j*4 + 0]
	      && movestr[1] == line[j*4 + 1] && movestr[2] == line[j*4 + 2]
	      && movestr[3] == line[j*4 + 3])) {
	  possible_rows[i] = 0;
	  break;
	}
      }
    }
    free(movestr);
    fclose(fp);

    /* Count how many possible rows we had, and then pick one of them
       by random. */
    j = 0;
    for (i = 0; i < BOOKSIZE; i++)
      if (possible_rows[i] == 1)
	j++;
    if (j == 0) {
      *in_book = 0;
    } else {
      j = get_random_number(j-1);
      for (i = 0; i < BOOKSIZE; i++) {
	if (possible_rows[i] == 1) {
	  if (j == 0) {
	    which_row = i;
	    break;
	  }
	  j--;
	}
      }
      fp = fopen(filename,"r");
      for (i = 0; i <= which_row; i++)
	fscanf(fp,"%s\n",line);
      fclose(fp);
      themove = (char *) malloc(5*sizeof(char));
      themove[0] = line[(board->move_number)*4 + 0];
      themove[1] = line[(board->move_number)*4 + 1];
      themove[2] = line[(board->move_number)*4 + 2];
      themove[3] = line[(board->move_number)*4 + 3];
      themove[4] = '\0';
      str2move(themove,board,move);
      free(themove);
    }
    free(line);
  }
  return *in_book;
}

void play_move(struct board **board, struct move *move,
	       const char *prefix, int *started) {
  char *str, *str2;

  str = (char *) malloc(100*sizeof(char));
  str2 = (char *) malloc(100*sizeof(char));

  move2history(*board,move,0);
  move2str(move,str);
  makemove(*board,move,0);
  is_draw_by_repetition(*board); //Put the new board in the repetition list.

  if (mode != XBOARD_MODE) {
    showboard(*board);
    showtime(1 - (*board)->color_at_move);
    sprintf(str2,"%s: move %s",prefix,str);
    infolog(str2);
    printf("%s\n",str2);
  } else {
    if ((*board)->color_at_move != engine_color && *started)
      printf("move %s\n",str);
    sprintf(str2,"%s: move %s",prefix,str);
    infolog(str2);
    printf("%s\n",str2);
  }
#ifdef AM_DEBUG
  if (gamelog) {
    sprintf(str2,"%s: %s",prefix,str);
    debuglog(str2);
  }
#endif

  free(str2);
  free(str);
}

void check_if_game_is_over(struct board **board, int *started) {
  char *str, *str2;
  int status;

  str = (char *) malloc(100*sizeof(char));
  str2 = (char *) malloc(100*sizeof(char));

  if ((status = game_ended(*board))) {
    *started = 0;
    if (status == 2) {
      if ((*board)->color_at_move == WHITE)
	sprintf(str,"0-1 {Black mates}");
      else
	sprintf(str,"1-0 {White mates}");
      printf("%s\n",str);
      if (mode == XBOARD_MODE) {
	sprintf(str2,"output sent = %s",str);
	infolog(str2);
      }
    } else if (status == 1) {
      sprintf(str,"1/2-1/2 {Stalemate}");
      printf("%s\n",str);
      if (mode == XBOARD_MODE) {
	sprintf(str2,"output sent = %s",str);
	infolog(str2);
	infolog("Stalemate");
      }
    } else if (status == 3) {
      sprintf(str,"1/2-1/2 {50 move rule}");
      printf("%s\n",str);
      infolog("Draw according to the 50 move rule claimed");
    } else if (status == 5) {
      //TODO: Make sure that draw by repetition isn't claimed here
      //unless draw_score() >= eval(board). Note: remember also to
      //_not_ set started = 0 if the draw isn't claimed.
      sprintf(str,"1/2-1/2 {Draw by repetition}");
      printf("%s\n",str);
      infolog("Draw according to the draw by repetition rule claimed");
    } else {
      sprintf(str,"1/2-1/2 {Insufficient material}");
      printf("%s\n",str);
      infolog("Draw because of insufficient material.");
    }
#ifdef AM_DEBUG
    if (gamelog) {
      sprintf(str2,"Game result: %s",str);
      debuglog(str);
    }
#endif
    resultlog(status,(*board)->color_at_move,engine_color);
  }
  free(str2);
  free(str);
}

/* Returns the sequence of played moves so far. */
char *get_gamestring(int move_number, char *opening_str) {
  char *dragstr;
  int i = 0, pos = 0;
  
  if (!move_number) {
    sprintf(opening_str,"null");
  } else {
    dragstr = (char *) malloc(20*sizeof(char));
    while (i < move_number) {
      move2str(&((*historik)[i++].move),dragstr);
      strcpy(&(opening_str[pos]),dragstr);
      pos += strlen(dragstr);
    }
    free(dragstr);
  }

  return opening_str;
}

void computer_make_move(struct board **board, int *started, int max_depth,
			int *book, int pondering, int *wait_for_oppmove) {
  struct move move;
  struct move new_move;
  char *str;
  char *str2;
  int status;
  struct move_list_entry *movelist;
  int mcount;
  int to_search_depth;
  int last_nodes, pre_pre_last_nodes;
  int book_spr;
  int val;

  if (pondering) {
    if (!(*book)) {
      /* First find move to ponder. */
      //stop_opp_clock();
      //start_own_clock();
      //printf("board before calling find move to ponder:\n");
      //showboard(*board);
      stored_board = **board;
      mcount = 0;
      preanalysis(*board,&movelist,&mcount,1);
      status = iterative_deepening_aspiration_search
	(*board,movelist,mcount,max_depth,MOVETYPE_FIND_MOVE_TO_PONDER,
	 &to_search_depth,&last_nodes,&pre_pre_last_nodes,0);
      postanalysis(*board,&movelist,mcount,&move,to_search_depth,1,
		   last_nodes,pre_pre_last_nodes);
      //stop_own_clock();
      //start_opp_clock();

      /* If a best move was found, then play it and then start pondering. */
      if ((status & PENDING_INPUT) || pending_input) {
	//do nothing
      } else if (status & BEST_MOVE_FOUND) {
	move.old_castling_status[WHITE] = (*board)->castling_status[WHITE];
	move.old_castling_status[BLACK] = (*board)->castling_status[BLACK];
	move.old_passant = (*board)->passant;
	move.old_moves_left_to_draw = (*board)->moves_left_to_draw;
	guessed_opp_move = move;
	play_move(board,&move,"predicted opponent move",started);
	//stored_board = **board;
	
	if ((status = game_ended(*board))) {
	  //don't ponder if the opponent's predicted move ends the game
	  unmakemove(*board,&move,0);
	  *wait_for_oppmove = 1; //No pondering until next opp move done
	} else {
	  /* Start pondering. */
	  stop_opp_clock();
	  start_own_clock();
	  mcount = 0;
	  preanalysis(*board,&movelist,&mcount,1);
	  status = iterative_deepening_aspiration_search
	    (*board,movelist,mcount,max_depth,MOVETYPE_PONDER,
	     &to_search_depth,&last_nodes,&pre_pre_last_nodes,0);
	  postanalysis(*board,&movelist,mcount,&new_move,to_search_depth,
		       (status & NORMAL_SEARCH ? 0 : 1),
		       last_nodes,pre_pre_last_nodes);
	  if (status & OPPONENT_PLAYED_UNPREDICTED_MOVE) {
	    unmakemove(*board,&move,0);
	    stop_own_clock();
	    start_opp_clock();
	    restore_time();
	    failed_predictions++;
	  } else if (status & PONDERING) {
	    //printf("Search was aborted before the opponent made a move (pending_input = %d).\n",pending_input);
	    printf("Search was aborted before the opponent made a move.\n");
	    unmakemove(*board,&move,0);
	    stop_own_clock();
	    start_opp_clock();
	    restore_time();
	    *wait_for_oppmove = 1; //No pondering until next opp move done
	  } else if (status & BEST_MOVE_FOUND) {
	    play_move(board,&new_move,"engine's move (successfully pondered)",started);
	    check_if_game_is_over(board,started);
	    stop_own_clock();
	    start_opp_clock();
	    successful_predictions++;
	  } else if (status & NO_SEARCH) {
	    unmakemove(*board,&move,0);
	    stop_own_clock();
	    start_opp_clock();
	    restore_time();
	    *wait_for_oppmove = 1; //No pondering until next opp move done
	  } else {
	    printf("Unknown status returned from iterative deepening (%d)\n",status);
	    infolog("Unknown status returned from iterative deepening");
	    exit(1);
	  }
	}
      } else {
	printf("Unknown status returned from iterative deepening (%d)\n",status);
	infolog("Unknown status returned from iterative deepening");
	exit(1);
      }
    } else {
      //printf("Not pondering since still in book.\n");
    }
  } else {
    book_spr = *book;
    if (!(*book && bookmove(book,*board,&move))) {
      mcount = 0;
      preanalysis(*board,&movelist,&mcount,0);
      iterative_deepening_aspiration_search
	(*board,movelist,mcount,max_depth,MOVETYPE_NORMAL,&to_search_depth,
	 &last_nodes,&pre_pre_last_nodes,
	 (book_spr!=*book)&&((*board)->move_number > 5));
      val = postanalysis(*board,&movelist,mcount,&move,to_search_depth,0,
			 last_nodes,pre_pre_last_nodes);
      if (book_spr) {
	/* If we just got out of the book, then let's print a message
	   to the log describing the evaluation value.
	   The char* management has been moved here, so in the case
	   of an unusually deep book, sprintf doesn't buffer overflow. */
	str2 = (char *) malloc((*board)->move_number*5*sizeof(char));
	str = (char *) malloc(60+(*board)->move_number*5*sizeof(char));
	sprintf(str,"Evaluation when getting out of book = %d (opening = %s)",
		val,get_gamestring((*board)->move_number,str2));
	//printf("%s\n",str);
	infolog(str);

	free(str);
	free(str2);
      }
    }
    play_move(board,&move,"engine's move",started);
    check_if_game_is_over(board,started);
  }
  zero_success_failure_count();

  return;
}

/* Initializes the board to a position described by a FEN string. */
void set_fen_board(struct board *lboard, const char *fen) {
  char **addr;
  char *holder;
  char charact;
  int sq = 0;
  int val, piece, fenpos = 0, i, j;
  size_t len;
  const char *sep = " ";
  char *position, *color, *castling, *enpassant, *fifty_moves, *move_nbr;
  struct board spr_board;

  position = (char *) malloc(100*sizeof(char));
  color = (char *) malloc(100*sizeof(char));
  castling = (char *) malloc(100*sizeof(char));
  enpassant = (char *) malloc(100*sizeof(char));
  fifty_moves = (char *) malloc(100*sizeof(char));
  move_nbr = (char *) malloc(100*sizeof(char));

  spr_board = *lboard;   //save old pos so it can be restored in case of error

  if (strlen(fen) > 95)
    goto error;

  /* Clear the board. */
  lboard->all_pieces[BLACK] = 0;
  lboard->all_pieces[WHITE] = 0;
  lboard->rot90_pieces[BLACK] = 0;
  lboard->rot90_pieces[WHITE] = 0;
  lboard->rotNE_pieces[BLACK] = 0;
  lboard->rotNE_pieces[WHITE] = 0;
  lboard->rotNW_pieces[BLACK] = 0;
  lboard->rotNW_pieces[WHITE] = 0;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 6; j++)
      lboard->piece[i][j] = 0;


  fenpos = len = strcspn(fen,sep);
  if (fenpos + 1 >= strlen(fen) || len >= 95) goto error;
  strncpy(position,fen,len); position[len] = '\0';
  fenpos += len = strcspn(&(fen[fenpos+1]),sep); fenpos++;
  if (fenpos + 1 >= strlen(fen) || len >= 95) goto error;
  strncpy(color,&(fen[fenpos-len]),len); color[len] = '\0';
  fenpos += len = strcspn(&(fen[fenpos+1]),sep); fenpos++;
  if (fenpos + 1 >= strlen(fen) || len >= 95) goto error;
  strncpy(castling,&(fen[fenpos-len]),len); castling[len] = '\0';
  fenpos += len = strcspn(&(fen[fenpos+1]),sep); fenpos++;
  if (fenpos + 1 >= strlen(fen) || len >= 95) goto error;
  strncpy(enpassant,&(fen[fenpos-len]),len); enpassant[len] = '\0';
  fenpos += len = strcspn(&(fen[fenpos+1]),sep); fenpos++;
  if (fenpos + 1 >= strlen(fen) || len >= 95) goto error;
  strncpy(fifty_moves,&(fen[fenpos-len]),len); fifty_moves[len] = '\0';
  fenpos += len = strcspn(&(fen[fenpos+1]),sep); fenpos++;
  if (fenpos - len >= strlen(fen) || len >= 95) goto error;
  strncpy(move_nbr,&(fen[fenpos-len]),len); move_nbr[len] = '\0';

  fenpos = 0;
  while (sq < 64) {
    if (fenpos >= strlen(position)) {
      goto error;
    }
    charact = position[fenpos];
    if (charact == 'p' || charact == 'n' || charact == 'b' || charact == 'r'
	|| charact == 'q' || charact == 'k') {
      if (charact == 'p')
	piece = PAWN;
      else if (charact == 'n')
	piece = KNIGHT;
      else if (charact == 'b')
	piece = BISHOP;
      else if (charact == 'r')
	piece = ROOK;
      else if (charact == 'q')
	piece = QUEEN;
      else
	piece = KING;
      lboard->piece[BLACK][piece] |= square[sq++];
    } else if (charact == 'P' || charact == 'N' || charact == 'B'
	       || charact == 'R' || charact == 'Q'  || charact == 'K') {
      if (charact == 'P')
	piece = PAWN;
      else if (charact == 'N')
	piece = KNIGHT;
      else if (charact == 'B')
	piece = BISHOP;
      else if (charact == 'R')
	piece = ROOK;
      else if (charact == 'Q')
	piece = QUEEN;
      else
	piece = KING;
      lboard->piece[WHITE][piece] |= square[sq++];
    } else if (charact == '/') {
      //do nothing
    } else {
      addr = &holder;
      val = strtol(&(position[fenpos]),addr,10);
      if (&(position[fenpos]) == (*addr) || val < 1 || val > 8) {
	goto error;
      } else
	sq += val;
    }
    fenpos++;
  }

  if (bitcount(lboard->piece[WHITE][KING]) != 1
      || bitcount(lboard->piece[BLACK][KING]) != 1
      || bitcount(lboard->piece[WHITE][QUEEN]) > 10
      || bitcount(lboard->piece[BLACK][QUEEN]) > 10
      || bitcount(lboard->piece[WHITE][ROOK]) > 10
      || bitcount(lboard->piece[BLACK][ROOK]) > 10
      || bitcount(lboard->piece[WHITE][BISHOP]) > 10
      || bitcount(lboard->piece[BLACK][BISHOP]) > 10
      || bitcount(lboard->piece[WHITE][KNIGHT]) > 10
      || bitcount(lboard->piece[BLACK][KNIGHT]) > 10
      || bitcount(lboard->piece[WHITE][PAWN]) > 8
      || bitcount(lboard->piece[BLACK][PAWN]) > 8)
    goto error;

  for (i = 0; i < 6; i++)
    lboard->all_pieces[BLACK] = lboard->all_pieces[BLACK] | lboard->piece[BLACK][i];
  for (i = 0; i < 6; i++)
    lboard->all_pieces[WHITE] = lboard->all_pieces[WHITE] | lboard->piece[WHITE][i];
  for (i = 0; i < 64; i++)
    lboard->rot90_pieces[WHITE] = lboard->rot90_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotate90to0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rot90_pieces[BLACK] = lboard->rot90_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotate90to0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNE_pieces[WHITE] = lboard->rotNE_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotateNEto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNE_pieces[BLACK] = lboard->rotNE_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotateNEto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNW_pieces[WHITE] = lboard->rotNW_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotateNWto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNW_pieces[BLACK] = lboard->rotNW_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotateNWto0[i]]) != 0)
	 * square[i]);

  lboard->castling_status[WHITE] = 0;
  lboard->castling_status[BLACK] = 0;
  if (strchr(castling,'K') != NULL)
    lboard->castling_status[WHITE] |= SHORT_CASTLING_OK;
  if (strchr(castling,'Q') != NULL)
    lboard->castling_status[WHITE] |= LONG_CASTLING_OK;
  if (strchr(castling,'k') != NULL)
    lboard->castling_status[BLACK] |= SHORT_CASTLING_OK;
  if (strchr(castling,'q') != NULL)
    lboard->castling_status[BLACK] |= LONG_CASTLING_OK;
  if (enpassant[0] == '-')
    lboard->passant = 0;
  else {
    lboard->passant = ('8' - enpassant[1])*8 + (enpassant[0] - 'a');
    if (lboard->passant < 0 || lboard->passant > 63) {
      goto error;
    }
  }
  lboard->captures[WHITE] = 0;
  lboard->captures[BLACK] = 0;

  addr = &holder;
  val = strtol(fifty_moves,addr,10);
  if (fifty_moves == (*addr) || val < 0 || val > 99) {
    goto error;
  } else
    lboard->moves_left_to_draw = 100 - val;
  addr = &holder;
  val = strtol(move_nbr,addr,10);
  if (move_nbr == (*addr) || val < 1) {
    goto error;
  } else {
    /* The movenumber of the fen string describes the fullmove number,
       while the board->move_number describes the half move, so we have
       to adjust for that. */
    if (strcmp(color,"w") == 0)
      lboard->move_number = (val-1)*2;
    else
      lboard->move_number = (val-1)*2+1;
  }
  if (strcmp(color,"w") == 0)
    lboard->color_at_move = WHITE;
  else
    lboard->color_at_move = BLACK;
  lboard->zobrist_key = get_zobrist_key_for_board(lboard);
  lboard->zobrist_pawn_key = get_zobrist_pawn_key_for_board(lboard);
  lboard->nbr_pieces[WHITE] =
    bitcount(lboard->piece[WHITE][KNIGHT])
    + bitcount(lboard->piece[WHITE][BISHOP])
    + bitcount(lboard->piece[WHITE][ROOK])
    + bitcount(lboard->piece[WHITE][QUEEN]);
  lboard->material_pieces[WHITE] =
    bitcount(lboard->piece[WHITE][KNIGHT])*VAL_KNIGHT
    + bitcount(lboard->piece[WHITE][BISHOP])*VAL_BISHOP
    + bitcount(lboard->piece[WHITE][ROOK])*VAL_ROOK
    + bitcount(lboard->piece[WHITE][QUEEN])*VAL_QUEEN
    + bitcount(lboard->piece[WHITE][KING])*VAL_KING;
  lboard->material_pawns[WHITE] = bitcount(lboard->piece[WHITE][PAWN])*VAL_PAWN;
  lboard->nbr_pieces[BLACK] =
    bitcount(lboard->piece[BLACK][KNIGHT])
    + bitcount(lboard->piece[BLACK][BISHOP])
    + bitcount(lboard->piece[BLACK][ROOK])
    + bitcount(lboard->piece[BLACK][QUEEN]);
  lboard->material_pieces[BLACK] =
    bitcount(lboard->piece[BLACK][KNIGHT])*VAL_KNIGHT
    + bitcount(lboard->piece[BLACK][BISHOP])*VAL_BISHOP
    + bitcount(lboard->piece[BLACK][ROOK])*VAL_ROOK
    + bitcount(lboard->piece[BLACK][QUEEN])*VAL_QUEEN
    + bitcount(lboard->piece[BLACK][KING])*VAL_KING;
  lboard->material_pawns[BLACK] = bitcount(lboard->piece[BLACK][PAWN])*VAL_PAWN;
  for (i = 0; i < 64; i++)
    lboard->boardarr[i] = EMPTY;
  for (i = 0; i < 64; i++)
    for (j = 0; j <= KING; j++)
      if ((lboard->piece[BLACK][j] & square[i])
	  || (lboard->piece[WHITE][j] & square[i]))
	lboard->boardarr[i] = j;

  /* Check if the castling rights are alright. */
  if (lboard->castling_status[WHITE] & LONG_CASTLING_OK) {
    if (!(lboard->piece[WHITE][ROOK] & square[56])
	|| !(lboard->piece[WHITE][KING] & square[60]))
      goto error;
  }
  if (lboard->castling_status[WHITE] & SHORT_CASTLING_OK) {
    if (!(lboard->piece[WHITE][ROOK] & square[63])
	|| !(lboard->piece[WHITE][KING] & square[60]))
      goto error;
  }
  if (lboard->castling_status[BLACK] & LONG_CASTLING_OK) {
    if (!(lboard->piece[BLACK][ROOK] & square[0])
	|| !(lboard->piece[BLACK][KING] & square[4]))
      goto error;
  }
  if (lboard->castling_status[BLACK] & SHORT_CASTLING_OK) {
    if (!(lboard->piece[BLACK][ROOK] & square[7])
	|| !(lboard->piece[BLACK][KING] & square[4]))
      goto error;
  }

  /* Put the starting board in the repetition list. */
  is_draw_by_repetition(lboard);

  goto rel_mem;
  error: {
    if (mode == XBOARD_MODE) 
      printf("tellusererror Illegal position\n");
    else
      printf("Illegal position.\n");
    /* Restore to the original board. */
    *lboard = spr_board;
  }
  rel_mem: {
    free(position);
    free(color);
    free(castling);
    free(enpassant);
    free(fifty_moves);
    free(move_nbr);
  }
}

/* Initializes the board to a position described by a EPD string.
   EPD strings have the first four fields in common with a
   FEN string. On error NULL is returned. */
struct epd_operations *set_epd_board(struct board *lboard, char *epd) {
  char **addr;
  char *holder;
  char charact;
  int sq = 0;
  int val, piece, epdpos = 0, i, j, value = -1;
  size_t len;
  const char *sep = " ";
  char *position, *color, *castling, *enpassant;
  struct board spr_board;

  position = (char *) malloc(100*sizeof(char));
  color = (char *) malloc(100*sizeof(char));
  castling = (char *) malloc(100*sizeof(char));
  enpassant = (char *) malloc(100*sizeof(char));

  spr_board = *lboard;   //save old pos so it can be restored in case of error

  /* Clear the board. */
  lboard->all_pieces[BLACK] = 0;
  lboard->all_pieces[WHITE] = 0;
  lboard->rot90_pieces[BLACK] = 0;
  lboard->rot90_pieces[WHITE] = 0;
  lboard->rotNE_pieces[BLACK] = 0;
  lboard->rotNE_pieces[WHITE] = 0;
  lboard->rotNW_pieces[BLACK] = 0;
  lboard->rotNW_pieces[WHITE] = 0;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 6; j++)
      lboard->piece[i][j] = 0;

  /* Read all the fields of the epd string. */
  epdpos = len = strcspn(epd,sep);
  if (epdpos + 1 >= strlen(epd) || len >= 95) goto error;
  strncpy(position,epd,len); position[len] = '\0';
  epdpos += len = strcspn(&(epd[epdpos+1]),sep); epdpos++;
  if (epdpos + 1 >= strlen(epd) || len >= 95) goto error;
  strncpy(color,&(epd[epdpos-len]),len); color[len] = '\0';
  epdpos += len = strcspn(&(epd[epdpos+1]),sep); epdpos++;
  if (epdpos + 1 >= strlen(epd) || len >= 95) goto error;
  strncpy(castling,&(epd[epdpos-len]),len); castling[len] = '\0';
  epdpos += len = strcspn(&(epd[epdpos+1]),sep); epdpos++;
  if (epdpos - len >= strlen(epd) || len >= 95) goto error;
  strncpy(enpassant,&(epd[epdpos-len]),len); enpassant[len] = '\0';
  value = epdpos;

  epdpos = 0;
  while (sq < 64) {
    if (epdpos >= strlen(position)) {
      goto error;
    }
    charact = position[epdpos];
    if (charact == 'p' || charact == 'n' || charact == 'b' || charact == 'r'
	|| charact == 'q' || charact == 'k') {
      if (charact == 'p')
	piece = PAWN;
      else if (charact == 'n')
	piece = KNIGHT;
      else if (charact == 'b')
	piece = BISHOP;
      else if (charact == 'r')
	piece = ROOK;
      else if (charact == 'q')
	piece = QUEEN;
      else
	piece = KING;
      lboard->piece[BLACK][piece] |= square[sq++];
    } else if (charact == 'P' || charact == 'N' || charact == 'B'
	       || charact == 'R' || charact == 'Q'  || charact == 'K') {
      if (charact == 'P')
	piece = PAWN;
      else if (charact == 'N')
	piece = KNIGHT;
      else if (charact == 'B')
	piece = BISHOP;
      else if (charact == 'R')
	piece = ROOK;
      else if (charact == 'Q')
	piece = QUEEN;
      else
	piece = KING;
      lboard->piece[WHITE][piece] |= square[sq++];
    } else if (charact == '/') {
      //do nothing
    } else {
      addr = &holder;
      val = strtol(&(position[epdpos]),addr,10);
      if (&(position[epdpos]) == (*addr) || val < 1 || val > 8) {
	goto error;
      } else
	sq += val;
    }
    epdpos++;
  }

  if (bitcount(lboard->piece[WHITE][KING]) != 1
      || bitcount(lboard->piece[BLACK][KING]) != 1
      || bitcount(lboard->piece[WHITE][QUEEN]) > 10
      || bitcount(lboard->piece[BLACK][QUEEN]) > 10
      || bitcount(lboard->piece[WHITE][ROOK]) > 10
      || bitcount(lboard->piece[BLACK][ROOK]) > 10
      || bitcount(lboard->piece[WHITE][BISHOP]) > 10
      || bitcount(lboard->piece[BLACK][BISHOP]) > 10
      || bitcount(lboard->piece[WHITE][KNIGHT]) > 10
      || bitcount(lboard->piece[BLACK][KNIGHT]) > 10
      || bitcount(lboard->piece[WHITE][PAWN]) > 8
      || bitcount(lboard->piece[BLACK][PAWN]) > 8)
    goto error;
	       
  for (i = 0; i < 6; i++)
    lboard->all_pieces[BLACK] = lboard->all_pieces[BLACK] | lboard->piece[BLACK][i];
  for (i = 0; i < 6; i++)
    lboard->all_pieces[WHITE] = lboard->all_pieces[WHITE] | lboard->piece[WHITE][i];
  for (i = 0; i < 64; i++)
    lboard->rot90_pieces[WHITE] = lboard->rot90_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotate90to0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rot90_pieces[BLACK] = lboard->rot90_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotate90to0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNE_pieces[WHITE] = lboard->rotNE_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotateNEto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNE_pieces[BLACK] = lboard->rotNE_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotateNEto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNW_pieces[WHITE] = lboard->rotNW_pieces[WHITE]
      | (((lboard->all_pieces[WHITE] & square[rotateNWto0[i]]) != 0)
	 * square[i]);
  for (i = 0; i < 64; i++)
    lboard->rotNW_pieces[BLACK] = lboard->rotNW_pieces[BLACK]
      | (((lboard->all_pieces[BLACK] & square[rotateNWto0[i]]) != 0)
	 * square[i]);

  lboard->castling_status[WHITE] = 0;
  lboard->castling_status[BLACK] = 0;
  if (strchr(castling,'K') != NULL)
    lboard->castling_status[WHITE] |= SHORT_CASTLING_OK;
  if (strchr(castling,'Q') != NULL)
    lboard->castling_status[WHITE] |= LONG_CASTLING_OK;
  if (strchr(castling,'k') != NULL)
    lboard->castling_status[BLACK] |= SHORT_CASTLING_OK;
  if (strchr(castling,'q') != NULL)
    lboard->castling_status[BLACK] |= LONG_CASTLING_OK;
  if (enpassant[0] == '-')
    lboard->passant = 0;
  else {
    lboard->passant = ('8' - enpassant[1])*8 + (enpassant[0] - 'a');
    if (lboard->passant < 0 || lboard->passant > 63) {
      goto error;
    }
  }
  lboard->captures[WHITE] = 0;
  lboard->captures[BLACK] = 0;

  /* EPD files don't contain any information about moves left to
     draw or the move number, so just set it to default. */
  lboard->moves_left_to_draw = 100;
  lboard->move_number = 0;

  if (strcmp(color,"w") == 0)
    lboard->color_at_move = WHITE;
  else
    lboard->color_at_move = BLACK;
  lboard->zobrist_key = get_zobrist_key_for_board(lboard);
  lboard->zobrist_pawn_key = get_zobrist_pawn_key_for_board(lboard);
  lboard->nbr_pieces[WHITE] =
    bitcount(lboard->piece[WHITE][KNIGHT])
    + bitcount(lboard->piece[WHITE][BISHOP])
    + bitcount(lboard->piece[WHITE][ROOK])
    + bitcount(lboard->piece[WHITE][QUEEN]);
  lboard->material_pieces[WHITE] =
    bitcount(lboard->piece[WHITE][KNIGHT])*VAL_KNIGHT
    + bitcount(lboard->piece[WHITE][BISHOP])*VAL_BISHOP
    + bitcount(lboard->piece[WHITE][ROOK])*VAL_ROOK
    + bitcount(lboard->piece[WHITE][QUEEN])*VAL_QUEEN
    + bitcount(lboard->piece[WHITE][KING])*VAL_KING;
  lboard->material_pawns[WHITE] = bitcount(lboard->piece[WHITE][PAWN])*VAL_PAWN;
  lboard->nbr_pieces[BLACK] =
    bitcount(lboard->piece[BLACK][KNIGHT])
    + bitcount(lboard->piece[BLACK][BISHOP])
    + bitcount(lboard->piece[BLACK][ROOK])
    + bitcount(lboard->piece[BLACK][QUEEN]);
  lboard->material_pieces[BLACK] =
    bitcount(lboard->piece[BLACK][KNIGHT])*VAL_KNIGHT
    + bitcount(lboard->piece[BLACK][BISHOP])*VAL_BISHOP
    + bitcount(lboard->piece[BLACK][ROOK])*VAL_ROOK
    + bitcount(lboard->piece[BLACK][QUEEN])*VAL_QUEEN
    + bitcount(lboard->piece[BLACK][KING])*VAL_KING;
  lboard->material_pawns[BLACK] = bitcount(lboard->piece[BLACK][PAWN])*VAL_PAWN;
  for (i = 0; i < 64; i++)
    lboard->boardarr[i] = EMPTY;
  for (i = 0; i < 64; i++)
    for (j = 0; j <= KING; j++)
      if ((lboard->piece[BLACK][j] & square[i])
	  || (lboard->piece[WHITE][j] & square[i]))
	lboard->boardarr[i] = j;

  /* Check if the castling rights are alright. */
  if (lboard->castling_status[WHITE] & LONG_CASTLING_OK) {
    if (!(lboard->piece[WHITE][ROOK] & square[56])
	|| !(lboard->piece[WHITE][KING] & square[60]))
      goto error;
  }
  if (lboard->castling_status[WHITE] & SHORT_CASTLING_OK) {
    if (!(lboard->piece[WHITE][ROOK] & square[63])
	|| !(lboard->piece[WHITE][KING] & square[60]))
      goto error;
  }
  if (lboard->castling_status[BLACK] & LONG_CASTLING_OK) {
    if (!(lboard->piece[BLACK][ROOK] & square[0])
	|| !(lboard->piece[BLACK][KING] & square[4]))
      goto error;
  }
  if (lboard->castling_status[BLACK] & SHORT_CASTLING_OK) {
    if (!(lboard->piece[BLACK][ROOK] & square[7])
	|| !(lboard->piece[BLACK][KING] & square[4]))
      goto error;
  }

  /* Put the starting board in the repetition list. */
  is_draw_by_repetition(lboard);

  goto rel_mem;
  error: {
    /* Restore to the original board. */
    *lboard = spr_board;
    value = -1;
  }
  rel_mem: {
    free(position);
    free(color);
    free(castling);
    free(enpassant);
  }

  /* Okay, done with parsing the data fields, so now parse the
     operation fields. */
  if (value == -1)
    return NULL;
  else
    return get_epd_operations(lboard,&(epd[value]));
}

/* This function parses the operations part of an EPD entry. The
   parser can parse also entries that don't totally follow the
   EPD format specification. On error NULL is returned. */
struct epd_operations *get_epd_operations(struct board *board, char *row) {
  int op_pos, i, temp_pos, temp;
  int epd_counter, nbr_epd_ops, pos_spr;
  size_t len;
  const char *blank_semi_sep = " ;", *semi_sep = ";", *blank_sep = " ";
  const char *hyph_sep = "\"";
  struct epd_operation *epd_op;
  struct epd_operations *ret_epd_ops;

  ret_epd_ops = (struct epd_operations *) malloc(1*sizeof(struct epd_operations));
  ret_epd_ops->operation = NULL; //to keep track of what memory to free
  /* Count the number of operations. */
  len = 0;
  /* Find the start of the first operation. According to the epd
     standard there should always be one blank character separating
     the end of the data field section, but not all epd files follow
     this, so we need to search for the first character of the first
     operation. Also some epd files have a semi colon after the data
     fields, and others do not. */
  temp_pos = op_pos = strspn(row,blank_sep);
  if (row[temp_pos] == ';')
    temp_pos = op_pos += 1 + strspn(&(row[op_pos+1]),blank_sep);
  
  nbr_epd_ops = 0;
  while (temp_pos < strlen(row) - 1 && (len = strcspn(&(row[temp_pos]),semi_sep)) != 0) {
    if (row[temp_pos + len] != ';') {
      /* We can get here if the last operation on the
	 row doesn't have a semicolon at the end. */
      printf("EPD operation not ended with semicolon.\n");
      goto return_null;
    }
    nbr_epd_ops++;
    temp_pos += len + 1;
    temp_pos += strspn(&(row[temp_pos]),blank_sep);
    if (temp_pos >= strlen(row) || row[temp_pos] == '\n' || row[temp_pos] == '\r')
      break;
  }
  if (nbr_epd_ops == 0) {
    printf("No operations for EPD entry.\n");
    goto return_null;
  }
  /* Also count the number of semicolons, as an extra check. */
  temp = 0;
  for (i = op_pos; i < strlen(&(row[op_pos])); i++)
    if (row[i] == ';')
      temp++;
  if (temp != nbr_epd_ops) {
    /* We can get here if there is a string like "... ;;;;;" */
    printf("Malformed EPD entry.\n");
    goto return_null;
  }
  
  epd_op = (struct epd_operation *)
    malloc(nbr_epd_ops*sizeof(struct epd_operation));
  ret_epd_ops->operation = epd_op;
  ret_epd_ops->nbr_operations = nbr_epd_ops;
  /* Set the pointers to NULL so we can keep track of what
     memory to free in case of parse failure. */
  for (i = 0; i < nbr_epd_ops; i++) {
    epd_op[i].op_code = NULL;
    epd_op[i].operand = NULL;
  }
  
  /* Go through all operations. */
  epd_counter = 0;
  while (epd_counter < nbr_epd_ops) {
    /* Get the op_code. */
    op_pos += strspn(&(row[op_pos]),blank_sep); //find first non blank char
    len = strcspn(&(row[op_pos]),blank_semi_sep);
    if (len > 15) {  //according to spec, max length of op_code = 15
      printf("EPD op_code to large.\n");
      goto return_null;
    }
    if (len < 1) {
      printf("Malformed EPD entry.\n");
      goto return_null;
    }
    /*if (row[op_pos] < 'a' || row[op_pos] > 'z') {
      /* Operation must begin with a letter. /
      printf("Malformed EPD entry (op_pos = %d, row = %s).\n",op_pos,&(row[op_pos]));
      goto return_null;
    }*/
    if (!((row[op_pos] >= 'a' && row[op_pos] <= 'z')
	 || (row[op_pos] >= 'A' && row[op_pos] <= 'Z'))) {
      /* Operation must begin with a letter. According to the standard
	 all official opcodes should start with lower case letters,
	 and only unofficial opcodes (own additions) should use upper
	 case. Since the perft testsuite has unofficial opcodes
	 (starting with D) we'll allow also upper case letters here. */
      printf("Malformed EPD entry (op_pos = %d, row = %s).\n",op_pos,&(row[op_pos]));
      goto return_null;
    }
    epd_op[epd_counter].op_code = (char *) malloc(16*sizeof(char));
    strncpy(epd_op[epd_counter].op_code,&(row[op_pos]),len);
    epd_op[epd_counter].op_code[len] = '\0';

    /* Count the number of operands. */
    pos_spr = temp_pos = op_pos + len + 1;
    epd_op[epd_counter].nbr_operands = 0;
    while (row[temp_pos - 1] != ';'
	   && ((row[temp_pos] != '"' &&
		(len = strcspn(&(row[temp_pos]),blank_semi_sep)) != 0)
	       || (row[temp_pos] == '"' &&
		   (len = strcspn(&(row[++temp_pos]),hyph_sep)) != 0))) {
      epd_op[epd_counter].nbr_operands++;
      temp_pos += len + 1;
      if (temp_pos >= strlen(row) || row[temp_pos-1] == ';')
	break;
    }
    if (!(row[temp_pos - 1] == ';'
	  || (row[temp_pos - 1] == '"' && row[temp_pos] == ';'))) {
      /* Is it possible to ever get here?!? */
      printf("Malformed EPD entry.\n");
      goto return_null;
    }

    if (epd_op[epd_counter].nbr_operands > 0) {
      /* Reserve memory for the operands. According to the
	 specification the operand strings can be up to 256
	 characters long. */
      epd_op[epd_counter].operand = (char **)
	malloc(epd_op[epd_counter].nbr_operands*sizeof(char *));
      for (i = 0; i < epd_op[epd_counter].nbr_operands; i++)
	epd_op[epd_counter].operand[i] =
	  (char *) malloc((256)*sizeof(char));
      
      /* Get the operand values. */
      temp_pos = pos_spr;
      i = 0;
      while ((row[temp_pos] != '"'
	      && (len = strcspn(&(row[temp_pos]),blank_semi_sep)) != 0)
	     || (row[temp_pos] == '"'
		 && (len = strcspn(&(row[++temp_pos]),
				   hyph_sep)) != 0)) {
	if (len > 255) {
	  printf("EPD format error: Operand too long.\n");
	  goto return_null;
	}
	strncpy(epd_op[epd_counter].operand[i],&(row[temp_pos]),len);
	epd_op[epd_counter].operand[i][len] = '\0';
	i++;
	temp_pos += len + 1;
	if (row[temp_pos - 1] == '"')
	  temp_pos++;
	if (temp_pos >= strlen(row) || row[temp_pos-1] == ';')
	  break;
      }
    }
    op_pos = temp_pos;
    epd_counter++;
  }
  return ret_epd_ops;
  return_null: {
    /* Free all the allocated memory on error. */
    free_epd_operations(ret_epd_ops);
    return NULL;
  }
}

/* This function frees the memory of a "struct epd_operations" object. */
void free_epd_operations(struct epd_operations *epd_ops) {
  int i, j;

  if (epd_ops->operation != NULL) {
    for (i = 0; i < epd_ops->nbr_operations; i++) {
      /* If op_code == NULL, then the operands won't have been initialized. */
      if ((epd_ops->operation)[i].op_code != NULL) {
	free((epd_ops->operation)[i].op_code);
	if ((epd_ops->operation)[i].nbr_operands > 0
	    && (epd_ops->operation)[i].operand != NULL) {
	  for (j = 0; j < (epd_ops->operation)[i].nbr_operands; j++) {
	    free((epd_ops->operation)[i].operand[j]);
	  }
	  free((epd_ops->operation)[i].operand);
	}
      }
    }
    free(epd_ops->operation);
  }
  free(epd_ops);
}

/* Returns 1 if the move1 and move2 are identical, otherwise 0.
   In this case identical means the same piece going from the
   same square to the same destination square. Only the fields
   move.from_square, move.to_square and move.piece are compared. */
int moves_equal(struct move *move1, struct move *move2) {
  if (move1->from_square != move2->from_square
      || move1->to_square != move2->to_square
      || move1->piece != move2->piece)
    return 0;
  return 1;
}

/* This function runs perft (counts the number of visited nodes
   when searching to a specified depth). */
int run_perft(struct board **board, int depth) {
  struct board newpos;
  int alpha = -INFTY, beta = INFTY;
  struct move_list_entry *movelist;
  int mcount = 0;
  int *searched_list;
  int i, j, k;
  struct move the_move;

  preanalysis(*board,&movelist,&mcount,0);

  /* Zero the history moves table. */
  for (i = 0; i < 2; i++)
    for (j = 0; j < 64; j++)
      for (k = 0; k < 64; k++)
	historymovestable[i][j][k] = 0;
  
  searched_list = (int *) malloc(mcount*sizeof(int));
  for (i = 0; i < mcount; i++) {
    searched_list[i] = 0;
  }

  visited_nodes = 0;
  the_move.old_castling_status[WHITE] = (*board)->castling_status[WHITE];
  the_move.old_castling_status[BLACK] = (*board)->castling_status[BLACK];
  the_move.old_passant = (*board)->passant;
  the_move.old_moves_left_to_draw = (*board)->moves_left_to_draw;
  for (i = 0; i < mcount; i++) {
    j = get_next_root_move(*board,movelist,mcount,searched_list);
    if (j == -1) {
      infolog("BEST == -1");
      printf("BEST == -1\n");
      exit(1);
    }
    
    the_move.from_square = (char) get_first_bitpos(movelist[j].fsquare);
    the_move.to_square = (char) get_first_bitpos(movelist[j].tsquare);
    the_move.piece = (char) movelist[j].piece;
    the_move.type = (char) movelist[j].type;
    makemove(*board,&the_move,depth);
    perft(*board,-beta,-alpha,depth-1,1);
    unmakemove(*board,&the_move,depth);
    searched_list[j] = 1;

    /* Since only queen promotions are generated we'll need to insert the
       under promotions here to make sure that the perft value is correct. */
    if (movelist[j].type & QUEEN_PROMOTION_MOVE) {
      for (k = QUEEN_PROMOTION_MOVE; k < KNIGHT_PROMOTION_MOVE; k = k*2) {
	movelist[j].type &= ~k;
	movelist[j].type |= (k*2);
	the_move.type = (char) movelist[j].type;
	makemove(*board,&the_move,depth);
	perft(*board,-beta,-alpha,depth-1,1);
	unmakemove(*board,&the_move,depth);
      }
    }
  }
  
  free(searched_list);
  free(movelist);

  (*board)->captures[WHITE] = 0;
  (*board)->captures[BLACK] = 0;
  
  printf("Perft to depth %d: %d nodes.\n",depth,visited_nodes);
  return visited_nodes;
}

/* This function runs testsuites where the positions are
   read from an epd-file. */
void testsuite(struct board *board, char *input) {
  FILE *fp;
  char *row, *str, *str2;
  int rows = 0, unparsable = 0, solved = 0, unsolved = 0, i, j;
  struct epd_operations *epd_ops;
  struct move epd_move;
  struct move tree_move;
  struct move_list_entry *movelist;
  int mcount, depth, correct_nbr_nodes, visited_nodes;
  int to_search_depth;
  int saved_pondering_status;
  int last_nodes, pre_pre_last_nodes;

  saved_pondering_status = pondering_in_use;
  pondering_in_use = 0;
  row = (char *) malloc(1050*sizeof(char));
  str = (char *) malloc(200*sizeof(char));
  str2 = (char *) malloc(200*sizeof(char));
  set_clock_mode(FIXED_CLOCK_MODE);
  set_fixed_thinking_time(10*1000); //10 seconds search time
  printf("Opening file %s\n",&(input[10]));
  sprintf(str,"Testsuite: %s (%s)\n",&(input[10]),PACKAGE_VERSION);
  testsuitelog(str);
  if ((fp = fopen(&(input[10]),"r")) != NULL) {
    while (fgets(row,1024,fp) != NULL) {
      if (strlen(row) > 5) {
	epd_ops = set_epd_board(board,row);
	zero_success_failure_count();
	if (epd_ops == NULL) {
	  printf("EPD entry not parsable (%s).\n",row);
	  testsuitelog("EPD entry not parsable.\n");
	  infolog(row);
	  unparsable++;
	} else if (sscanf((epd_ops->operation)[0].op_code,"D%d",&depth) == 1) {
	  /* If the first opcode starts with a capital D and then a number,
	     then assume that all opcodes are mean for running the perft
	     testsuite. */
	  showboard(board);

	  /* Print the id of the position, if it exists. */
	  for (i = 0; i < epd_ops->nbr_operations; i++)
	    if (strcmp((epd_ops->operation)[i].op_code,"id") == 0) {
	      if ((epd_ops->operation)[i].nbr_operands > 0) {
		sprintf(str2,"%s: ",(epd_ops->operation)[i].operand[0]);
		testsuitelog(str2);
	      }
	    }

	  if (epd_ops->nbr_operations <= 0) {
	    testsuitelog("No operations.\n");
	    unparsable++;
	    goto done_parse_ops;
	  }
	  for (i = 0; i < epd_ops->nbr_operations; i++) {
	    if (sscanf((epd_ops->operation)[i].op_code,"D%d",&depth) == 1) {
	      if ((epd_ops->operation)[i].nbr_operands <= 0) {
		testsuitelog("No operands.\n");
		unparsable++;
		goto done_parse_ops;
	      }
	      if (sscanf((epd_ops->operation)[i].operand[0],"%d",&correct_nbr_nodes) == 1) {
		printf("Perft to depth %d ...",depth);
		if (depth <= 3) {
		  visited_nodes = run_perft(&board,depth);
		  printf("done.\n");
		  if (visited_nodes != correct_nbr_nodes) {
		    testsuitelog("Unsolved.\n");
		    unsolved++;
		    goto done_parse_ops;
		  }
		} else {
		  printf("skipped (limited depth).\n");
		}
	      } else {
		testsuitelog("Unparsable operand.\n");
		unparsable++;
		goto done_parse_ops;
	      }
	    } else {
	      testsuitelog("Unparsable operation.\n");
	      unparsable++;
	      goto done_parse_ops;
	    }
	  }
	  testsuitelog("Solved.\n");
	  solved++;
	  goto done_parse_ops;
	} else {
	  showboard(board);
	  printf("Thinking...\n");
	  mcount = 0;
	  preanalysis(board,&movelist,&mcount,0);
	  start_own_clock();
	  iterative_deepening_aspiration_search
	    (board,movelist,mcount,max_depth,MOVETYPE_NORMAL,
	     &to_search_depth,&last_nodes,&pre_pre_last_nodes,0);
	  stop_own_clock();
	  postanalysis(board,&movelist,mcount,&tree_move,to_search_depth,0,
		       last_nodes,pre_pre_last_nodes);
	  move2str(&tree_move,str);
	  printf("Best move according to search: %s\n",str);
	  /* First of all, print the id of the position, if it exists. */
	  for (i = 0; i < epd_ops->nbr_operations; i++)
	    if (strcmp((epd_ops->operation)[i].op_code,"id") == 0) {
	      if ((epd_ops->operation)[i].nbr_operands > 0) {
		sprintf(str2,"%s: ",(epd_ops->operation)[i].operand[0]);
		testsuitelog(str2);
	      }
	    }
	  /* Then check if the position was solved or not. Since some
	     testsuites have the moves written in "e2e4" format,
	     and not in SAN format, we try to parse the moves in "e2e4"
	     format if the SAN parsing fails. */
	  for (i = 0; i < epd_ops->nbr_operations; i++) {
	    if (strcmp((epd_ops->operation)[i].op_code,"bm") == 0) {
	      for (j = 0; j < (epd_ops->operation)[i].nbr_operands; j++) {
		if (san2move((epd_ops->operation)[i].operand[j],board,&epd_move)
		    || str2move((epd_ops->operation)[i].operand[j],board,&epd_move)) {
		  move2str(&epd_move,str);
		  printf("Best move according to epd: %s\n",str);
		  if (moves_equal(&tree_move,&epd_move)) {
		    testsuitelog("Solved.\n");
		    solved++;
		    goto done_parse_ops;
		  }
		} else {
		  testsuitelog("Unparsable move.\n");
		  unparsable++;
		  goto done_parse_ops;
		}
	      }
	      testsuitelog("Unsolved.\n");
	      unsolved++;
	      goto done_parse_ops;
	    } else if (strcmp((epd_ops->operation)[i].op_code,"am") == 0) {
	      for (j = 0; j < (epd_ops->operation)[i].nbr_operands; j++) {
		if (san2move((epd_ops->operation)[i].operand[j],board,&epd_move)
		    || str2move((epd_ops->operation)[i].operand[j],board,&epd_move)) {
		  move2str(&epd_move,str);
		  printf("Avoid move according to epd: %s\n",str);
		  if (moves_equal(&tree_move,&epd_move)) {
		    testsuitelog("Unsolved.\n");
		    unsolved++;
		    goto done_parse_ops;
		  }
		} else {
		  testsuitelog("Unparsable move.\n");
		  unparsable++;
		  goto done_parse_ops;
		}
	      }
	      testsuitelog("Solved.\n");
	      solved++;
	      goto done_parse_ops;
	    }
	  }
	  testsuitelog("No move operations found.\n");
	  done_parse_ops : {
	  }
	  free_epd_operations(epd_ops);
	}
	rows++;
      }
    }
    fclose(fp);
  } else {
    printf("File could not be opened.\n");
    testsuitelog("File could not be opened.\n");
  }
  printf("positions = %d\n",rows);
  testsuitelog("\nSummary:\n--------\n");
  sprintf(str,"Total positions: %d\n",rows);
  testsuitelog(str);
  sprintf(str,"Solved positions: %d\n",solved);
  testsuitelog(str);
  sprintf(str,"Unsolved positions: %d\n",unsolved);
  testsuitelog(str);
  if (unparsable > 0) {
    sprintf(str,"Parse errors: %d\n",unparsable);
    testsuitelog(str);
  }
  testsuitelog("---------------------------------------------\n\n\n");
  free(str2);
  free(str);
  free(row);
  pondering_in_use = saved_pondering_status;
}

/* This function converts a SAN move to a struct move-object. If the string
   is unparsable or illegal, 0 will be returned, otherwise 1. */
int san2move(char *which_move, struct board *board, struct move *move) {
  char *str;
  struct moves moves[16];
  int movables, piece_nbr;
  const char *suffixes = "+!?#";
  int san_piece, san_dest_file = -1, san_dest_rank = -1;
  int san_from_file = -1, san_from_rank = -1;
  int castling_move = 0, capture_move = 0, promotion_type = 0;
  bitboard target;

  str = (char *) malloc(20*sizeof(char));
  if (strlen(which_move) < 2 || strlen(which_move) > 7) {
    goto error;
  }
  /* Remove all possible suffixes. */
  which_move[strcspn(which_move,suffixes)] = '\0';

  /* Check if it's a move by a heavy piece. */
  if (which_move[0] >= 'A' && which_move[0] <= 'Z') {
    switch (which_move[0]) {
    case 'K' : san_piece = KING; break;
    case 'Q' : san_piece = QUEEN; break;
    case 'R' : san_piece = ROOK; break;
    case 'B' : san_piece = BISHOP; break;
    case 'N' : san_piece = KNIGHT; break;
    case 'O' : if (strcmp(which_move,"O-O") == 0) {
                   castling_move = 1;
		   san_piece = KING;
		   san_from_file = 4;
		   san_dest_file = 6;
		   if (board->color_at_move == WHITE) {
		     san_from_rank = 7;
		     san_dest_rank = 7;
		   } else {
		     san_from_rank = 0;
		     san_dest_rank = 0;
		   }
		   break;
               } else if (strcmp(which_move,"O-O-O") == 0) {
                   castling_move = 1;
		   san_piece = KING;
		   san_from_file = 4;
		   san_dest_file = 2;
		   if (board->color_at_move == WHITE) {
		     san_from_rank = 7;
		     san_dest_rank = 7;
		   } else {
		     san_from_rank = 0;
		     san_dest_rank = 0;
		   }
		   break;
               }
    default : goto error;
    }
    if (castling_move) {
      //do nothing
    } else if (strchr(which_move,'x') != NULL) {
      /* Capture move. */
      capture_move = 1;
      if (which_move[1] == 'x') {
	/* On the form "Qxb7". */
	san_dest_file = which_move[2] - 'a';
	san_dest_rank = '8' - which_move[3];
      } else if (which_move[2] == 'x') {
	if (which_move[1] >= 'a' && which_move[1] <= 'h') {
	  /* On the form "Qaxb7". */
	  san_from_file = which_move[1] - 'a';
	} else {
	  /* On the form "Q7xb7". */
	  san_from_rank = '8' - which_move[1];
	}
	san_dest_file = which_move[3] - 'a';
	san_dest_rank = '8' - which_move[4];
      } else if (which_move[3] == 'x') {
	/* On the form "Qa7xb7". */
	san_from_file = which_move[1] - 'a';
	san_from_rank = '8' - which_move[2];
	san_dest_file = which_move[4] - 'a';
	san_dest_rank = '8' - which_move[5];
      } else {
	goto error;
      }
    } else {
      /* Non-capture move. */
      if (strlen(which_move) == 3) {
	/* On the form "Ne2". */
	san_dest_file = which_move[1] - 'a';
	san_dest_rank = '8' - which_move[2];
      } else if (strlen(which_move) == 4) {
	if (which_move[1] >= 'a' && which_move[1] <= 'h') {
	  /* On the form "Nce2". */
	  san_from_file = which_move[1] - 'a';
	} else {
	  /* On the form "N3e2". */
	  san_from_rank = '8' - which_move[1];
	}
	san_dest_file = which_move[2] - 'a';
	san_dest_rank = '8' - which_move[3];
      } else if (strlen(which_move) == 5) {
	/* On the form "Nc3e2". */
	san_from_file = which_move[1] - 'a';
	san_from_rank = '8' - which_move[2];
	san_dest_file = which_move[3] - 'a';
	san_dest_rank = '8' - which_move[4];
      } else {
	goto error;
      }
    }
  } else {
    /* Pawn move. */
    san_piece = PAWN;
    if (strchr(which_move,'x') != NULL) {
      /* Capture move. */
      capture_move = 1;
      if (which_move[0] == 'x') {
	san_dest_file = which_move[1] - 'a';
	san_dest_rank = '8' - which_move[2];
      } else {
	san_from_file = which_move[0] - 'a';
	san_dest_file = which_move[2] - 'a';
	san_dest_rank = '8' - which_move[3];
      }
    } else {
      /* Non-capture move. */
      san_dest_file = which_move[0] - 'a';
      san_dest_rank = '8' - which_move[1];
    }
    /* Check if promotion. */
    if (strchr(which_move,'=') != NULL) {
      switch (*(strchr(which_move,'=') + 1)) {
      case 'Q' : promotion_type = QUEEN_PROMOTION_MOVE; break;
      case 'R' : promotion_type = ROOK_PROMOTION_MOVE; break;
      case 'B' : promotion_type = BISHOP_PROMOTION_MOVE; break;
      case 'N' : promotion_type = KNIGHT_PROMOTION_MOVE; break;
      default : goto error;
      }
    }
  }

  if (san_dest_file < 0 || san_dest_file > 7
      || san_dest_rank < 0 || san_dest_rank > 7
      || san_from_file < -1 || san_from_file > 7    //can also be -1
      || san_from_rank < -1 || san_from_rank > 7)   //can also be -1
    goto error;

  /* Okay, if we get here, the san string was parsed successfully.
     Now we have to see if this move can be generated from the current
     board position. */
  generate_moves(board,moves,&movables);
  piece_nbr = 0;
  while (piece_nbr < movables) {
    while (moves[piece_nbr].targets != 0) {
      target = getlsb(moves[piece_nbr].targets);
      move->from_square = (char) get_first_bitpos(moves[piece_nbr].source);
      move->to_square = (char) get_first_bitpos(target);
      move->piece = moves[piece_nbr].piece;
      move->type = get_movetype(board,move);
      /* Check the piece type. */
      if (san_piece == move->piece) {
	/* Check if the destination square is okay. */
	if (move->to_square % 8 == san_dest_file
	    && move->to_square / 8 == san_dest_rank) {
	  /* Check the from square. */
	  if ((san_from_file == -1
	       || move->from_square % 8 == san_from_file)
	      &&
	      (san_from_rank == -1
	       || move->from_square / 8 == san_from_rank)) {
	    /* The engine only generates queen promotion moves, so
	       we have to make sure it still recognizes lower promotions. */
	    if (promotion_type != 0 && (move->type & QUEEN_PROMOTION_MOVE)) {
	      move->type &= ~QUEEN_PROMOTION_MOVE;
	      move->type |= promotion_type;
	    }

	    /* Finally make sure the castling flags agree, and that
	       the move is legal. */
	    if (capture_move == ((move->type & CAPTURE_MOVE) != 0)
		&& legal_move(*move,board))
	      goto alright;
	  }
	}
      }
      moves[piece_nbr].targets &= ~target;
    }
    piece_nbr++;
  }
  error: { free(str); return 0; }
  alright: { free(str); return 1; }
}

/* This function converts a string to a struct move-object. If the string
   is parsable 1 is returned, otherwise 0. */
int str2move(char *which_move, struct board *board, struct move *move) {
  int frow, fcol, trow, tcol;
  char *str;
  int i, movetype;

  if (strlen(which_move) != 4 && strlen(which_move) != 5) {
    return 0;
  }

  frow = '8' - which_move[1];
  fcol = which_move[0] - 'a';
  trow = '8' - which_move[3];
  tcol = which_move[2] - 'a';
  if ((frow > 7) || (frow < 0) ||
      (fcol > 7) || (fcol < 0) ||
      (trow > 7) || (trow < 0) ||
      (tcol > 7) || (tcol < 0)) {
    return 0;
  }
  (*move).from_square = frow*8 + fcol;
  (*move).to_square = trow*8 + tcol;


  /* Find out what kind of piece made this move. */

  move->piece = board->boardarr[move->from_square];
  if (move->piece < PAWN || move->piece > KING) {
    str = (char *) malloc(100*sizeof(char));
    sprintf(str,"Error (invalid move format): %s",which_move);
    printf("%s\n",str);
    infolog(str);
    free(str);
    showboard(board);
    return 0;
  }

  /* Find out if this move was a capture or a normal move. */
  movetype = NORMAL_MOVE;
  if (board->all_pieces[Oppcolor] & square[(*move).to_square])
    movetype = CAPTURE_MOVE;

  /* Find out if the move is a passant move. First check if a pawn
     makes the move. */
  if ((*move).piece == PAWN) {
    /* Then check if the pawn is making an attack, and if the attack goes
       towards a square that is not occupied by an enemy piece. */
    if ((attack.pawn[(int)Color][frow*8+fcol] & square[(*move).to_square])
	&& !(board->all_pieces[Oppcolor] & square[(*move).to_square]))
      movetype = CAPTURE_MOVE | PASSANT_MOVE;
  }

  /* Find out if the move is a castling move. */
  if ((*move).piece == KING) {
    if (Color == WHITE) {
      if (((*move).from_square ==60) &&
	  (((*move).to_square == 58) || ((*move).to_square == 62)))
	movetype = CASTLING_MOVE;
    } else {     //color == BLACK
      if (((*move).from_square == 4) &&
	  (((*move).to_square == 2) || ((*move).to_square == 6)))
	movetype = CASTLING_MOVE;
    }
  }
  (*move).type = movetype;

  /* If the move is a pawn that advances to the last row, then find out
     what kind of promotion it is. */
  if (((*move).piece == PAWN) && (square[(*move).to_square] & pawn_lastrow[(int)Color])) {
    if (strlen(which_move) == 5) {
      switch(which_move[4]) {
      case 'r' : (*move).type = (*move).type | ROOK_PROMOTION_MOVE;     break;
      case 'q' : (*move).type = (*move).type | QUEEN_PROMOTION_MOVE;     break;
      case 'n' : (*move).type = (*move).type | KNIGHT_PROMOTION_MOVE;     break;
      case 'b' : (*move).type = (*move).type | BISHOP_PROMOTION_MOVE;     break;
      default : str = (char *) malloc(100*sizeof(char));
		sprintf(str,"Error (invalid piece): %s",which_move);
		printf("%s\n",str);
		infolog(str);
		free(str);
		return 0;
      }
    } else {
      str = (char *) malloc(100*sizeof(char));
      sprintf(str,"Error (invalid move format): %s",which_move);
      printf("%s\n",str);
      infolog(str);
      free(str);
      return 0;
    }
  }

  return 1;
}

void move2str(struct move *move, char *str) {
  int l_square, row, col;

  l_square = (int) move->from_square;
  row = l_square / 8;
  col = l_square % 8;
  str[1] = '8' - row;
  str[0] = col + 'a';
  l_square = (int) move->to_square;
  row = l_square / 8;
  col = l_square % 8;
  str[3] = '8' - row;
  str[2] = col + 'a';
  str[4] = '\0';
  if (move->piece == PAWN) {
    if (move->type & QUEEN_PROMOTION_MOVE) {
      str[4] = 'q'; str[5] = '\0';
    } else if (move->type & ROOK_PROMOTION_MOVE) {
      str[4] = 'r'; str[5] = '\0';
    } else if (move->type & KNIGHT_PROMOTION_MOVE) {
      str[4] = 'n'; str[5] = '\0';
    } else if (move->type & BISHOP_PROMOTION_MOVE) {
      str[4] = 'b'; str[5] = '\0';
    }
  }
}

/* Returns false(=0) on error, and true(=1) if no error. */
int parsemove(char *input, struct board **board, int *started,
	      int *wait_for_oppmove) {
  struct move move;
  char *str;
  char *str2;
  int returnval = 0;
  int status;

  str2 = (char *) malloc(100*sizeof(char));
  str = (char *) malloc(100*sizeof(char));
  if (str2move(input,*board,&move) || san2move(input,*board,&move)) {
    if (!(*started) && mode != XBOARD_MODE)
      printf("Illegal move (No game started): %s\n",input);
    else {
      /* If we are in xboard mode, we check if the move is legal,
         otherwise we accept illegal moves. */
      if (mode == XBOARD_MODE && !legal_move(move,*board)) {
	sprintf(str,"Illegal move: %s",input);
	printf("%s\n",str);
	infolog(str);
      } else {
	*wait_for_oppmove = 0;
	play_move(board,&move,"Opponent's move",started);

	/* Check if game is over. If draw by repetition is reached, then a
	   draw will not be claimed unless draw_score() >= eval(board)
	   TODO: use the function check_if_game_is_over() here in order to
	   avoid duplication of code. */
	if ((status = game_ended(*board))) {
	  if (status == 5) {
	    if (draw_score() >= eval(*board,-INFTY,INFTY)) {
	      sprintf(str,"1/2-1/2 {Draw by repetition}");
	      printf("%s\n",str);
	      infolog("Draw according to the draw by repetition rule claimed");
	      *started = 0;
	      returnval = 0;
#ifdef AM_DEBUG
	      if (gamelog) {
		sprintf(str2,"Game result: %s",str);
		debuglog(str);
	      }
#endif
	      resultlog(status,(*board)->color_at_move,engine_color);
	    }
	  } else {
	    *started = 0;
	    returnval = 0;
	    if (status == 2) {
	      if (1 - (*board)->color_at_move == BLACK)
		sprintf(str,"0-1 {Black mates}");
	      else
		sprintf(str,"1-0 {White mates}");
	      printf("%s\n",str);
	      if (mode == XBOARD_MODE) {
		sprintf(str2,"output sent = %s",str);
		infolog(str2);
	      }
	    } else if (status == 1) {
	      sprintf(str,"1/2-1/2 {Stalemate}");
	      printf("%s\n",str);
	      if (mode == XBOARD_MODE) {
		sprintf(str2,"output sent = %s",str);
		infolog(str2);
		infolog("Stalemate");
	      }
	    } else if (status == 3) {  //draw by the 50 move rule
	      sprintf(str,"1/2-1/2 {50 move rule}");
	      printf("%s\n",str);
	      infolog("Draw according to the 50 move rule claimed");
	    } else if (status == 5) {
	    } else {
	      sprintf(str,"1/2-1/2 {Insufficient material}");
	      printf("%s\n",str);
	      infolog("Draw because of insufficient material.");
	    }
#ifdef AM_DEBUG
	    if (gamelog) {
	      sprintf(str2,"Game result: %s",str);
	      debuglog(str);
	    }
#endif
	    resultlog(status,(*board)->color_at_move,engine_color);
	  }
	}

	/* If xboard mode is on and *started = 0, then we will make the
	   move, but not start thinking until xboard sends a go. This
	   is done by returning a 0.*/
	if (*started)
	  returnval = 1;
      }
    }
  } else {
    if (mode != XBOARD_MODE)
      printf("Command could not be interpreted. Press '?' for help.\n");
    else {
      printf("Error (unknown command): %s\n",input);
    }
  }
  free(str);
  free(str2);
  return returnval;
}

void parse() {
  int started = 0;       //is zero before a game is started
  char *input;
  int run = 1, i;
  char *str;
  char *str2;
  char *str3;
  int temp;
  struct board *board;
  int book = 1;
  int wait_for_oppmove = 0;

  historik = (struct history_entry **) malloc(sizeof(struct history_entry *));
  *historik = (struct history_entry *)
    malloc(hlistsize*sizeof(struct history_entry));
  repetition_list = (struct repetition_entry **)
    malloc(sizeof(struct repetition_entry *));
  *repetition_list = (struct repetition_entry *)
    malloc(rlistsize*sizeof(struct repetition_entry));
  input_from_thread = (char *) malloc(100*sizeof(char));
  pending_input = 0;
  start_input_thread();
  input = (char *) malloc(100*sizeof(char));
  str = (char *) malloc(100*sizeof(char));
  str2 = (char *) malloc(100*sizeof(char));
  str3 = (char *) malloc(100*sizeof(char));
  board = (struct board *) malloc(1*sizeof(struct board));
  killers = (struct killers **) calloc((unsigned) (max_depth*2+1),
				       sizeof(struct killers *));
  for (i = 0; i <= max_depth*2; i++)
    killers[i] = (struct killers *) calloc((unsigned) NBR_KILLERS,
					   sizeof(struct killers));

  /* Initialize the board to the starting position. */
  set_fen_board(board,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  zero_success_failure_count();

  /*//set_fen_board(board,"8/8/P7/1K6/8/P4p2/8/k7 w - - 0 1");
  //set_fen_board(board,"8/8/8/6P1/8/p3K2k/8/8 w - - 0 1"); //denna blir fel, borde bli ungefär noll, men blir -612 => nu korrekt
  //set_fen_board(board,"8/8/8/6P1/8/p3K1k1/8/8 w - - 0 1"); //denna blir fel, borde bli ungefär noll, men blir 538 => nu korrekt
  //set_fen_board(board,"8/K7/8/6P1/8/p5k1/8/8 w - - 0 1"); //denna blir fel, borde bli mycket starkt negativ (ca -650), men blir -102 => nu korrekt
  //set_fen_board(board,"8/K7/6P1/8/8/p5k1/8/8 b - - 0 1");
  //set_fen_board(board,"8/8/7P/3KP3/8/p7/8/1k7 b - - 0 1"); //=> funkar
  set_fen_board(board,"N7/8/8/1K6/4k3/Q7/8/8 w - - 0 1");
  showboard(board);
  printf("Color = %d\n",Color);
  printf("eval = %d\n",eval(board,-INFTY,INFTY));
  exit(1);*/

  /*showboard(board);
  printf("a = %d\n",board->boardarr[7]);
  printf("b = %d\n",board->boardarr[8]);
  printf("c = %d\n",board->boardarr[16]);
  printf("d = %d\n",board->boardarr[40]);
  printf("e = %d\n",board->boardarr[55]);
  printf("f = %d\n",board->boardarr[63]);*/

#ifdef AM_DEBUG
  if (mode == DEBUG_MODE)
    printf("debug");
#endif
    
  while (run) {
    if (started && pondering_in_use && !wait_for_oppmove)
      if (Color != engine_color)
	computer_make_move(&board,&started,max_depth,
			   &book,1,&wait_for_oppmove);

    if (!pending_input) {
      usleep(50);
    } else {
      strcpy(input,input_from_thread);
      if (strcmp(input,"quit") == 0) {
	started = 0;
	run = 0;
	stop_input_thread();
      }
      pending_input = 0;

      /*mode = XBOARD_MODE;
	started = 1;
	if (parsemove(input,&board,&started)) {
	//if (san2move(input,*board,&move)) {
	showboard(board);
	} else {
	printf("Errormove!\n");
	showboard(board);
	exit(1);
	}
	continue;*/

      if (mode == XBOARD_MODE) {
	infolog(input);
	if (strcmp(input,"new") == 0) {
	  /* Initialize the board to the starting position. */
	  set_fen_board(board,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	  zero_success_failure_count();
	  elapsed_time_in_curr_game = 0;
	  supposed_elapsed_time_in_curr_game = 0;
	  engine_color = BLACK;
	  started = 1;
	  book = 1;
	  stop_own_clock();
	  stop_opp_clock();
	  own_time = 300000;    //5 minutes;
	  opp_time = 300000;    //5 minutes;
	  max_depth = MAX_ITERATIONS;
	  wait_for_oppmove = 1; //No pondering until next opp move done
	  //non_reversible_move();   //zero the repetition counters
	} else if (strcmp(input,"random") == 0) {
	  //do nothing, we always use random evaluation
	} else if (strcmp(input,"force") == 0) {
	  started = 0;
	  stop_own_clock();
	  stop_opp_clock();
	  wait_for_oppmove = 1; //No pondering until next opp move done
	} else if (strncmp(input,"level",5) == 0) {
	  int scanned, minutes, seconds;
	  scanned = sscanf(input,"level %d %d %d",
			   &moves_per_timecontrol,&base,&increment);
	  if (scanned == 3)
	    base = base*60*1000; //since xboard gives the time in minutes
	  else {
	    scanned = sscanf(input,"level %d %d:%d %d",&moves_per_timecontrol,
			     &minutes,&seconds,&increment);
	    //printf("scanned = %d, minutes = %d, seconds = %d\n",scanned,minutes,seconds);
	    base = minutes*60*1000+seconds*1000;
	  }
	  //printf("base ========== %d\n",base);
	  increment = increment*1000;  //since xboard gives the time in seconds
	  set_clock_mode(NORMAL_CLOCK_MODE);
	  if (moves_per_timecontrol > 0)
	    timectrl = TIMECTRL_NEWTIME;
	  else if (increment > 0)
	    timectrl = TIMECTRL_INC;
	  else
	    timectrl = TIMECTRL_NOINC;
	  sprintf(str,"timectrl = %d",timectrl);
	  infolog(str);
	} else if (strncmp(input,"protover",8) == 0) {
	  printf("feature ping=1 setboard=1 variants=\"normal\" analyze=0 myname=\"%s\" colors=1 reuse=1 time=1 draw=1 sigint=0 sigterm=1 done=1\n",PACKAGE_STRING);
	} else if (strncmp(input,"accepted",8) == 0) {
	  //do nothing
	} else if (strncmp(input,"rejected",8) == 0) {
	  //do nothing
	} else if (strncmp(input,"ping",4) == 0) {
	  sscanf(input,"ping %d",&temp);
	  printf("pong %d\n",temp);
	} else if (strcmp(input,"computer") == 0) {
	  infolog("Opponent is a computer.");
	} else if (strncmp(input,"name",4) == 0) {
	  sscanf(input,"name %s",str);
	  sprintf(str2,"Opponent's name: %s",str);
	  infolog(str2);
	} else if (strncmp(input,"rating",6) == 0) {
	  sscanf(input,"rating %s %s",str,str2);
	  sprintf(str3,"Own rating: %s",str);
	  infolog(str3);
	  sprintf(str3,"Opponent rating: %s",str2);
	  infolog(str3);
	} else if (strncmp(input,"time",4) == 0) {
	  sscanf(input,"time %d",&own_time);
	  /* Internally the engine works with milliseconds, and xboard gives
	     us the time in centiseconds, so we have to multiply by 10 here.*/
	  own_time *= 10;
	} else if (strncmp(input,"otim",4) == 0) {
	  sscanf(input,"otim %d",&opp_time);
	  /* Internally the engine works with milliseconds, and xboard gives
	     us the time in centiseconds, so we have to multiply by 10 here.*/
	  opp_time *= 10;
	} else if (strncmp(input,"result",6) == 0) {
	  started = 0;
	} else if (strcmp(input,"post") == 0) {
	  post = 1;
	} else if (strcmp(input,"nopost") == 0) {
	  post = 0;
	} else if (strcmp(input,"hard") == 0) {
	  pondering_in_use = 1;
	} else if (strcmp(input,"easy") == 0) {
	  pondering_in_use = 0;
	} else if (strcmp(input,"draw") == 0) {
	  temp = Color;
	  board->color_at_move = engine_color;
	  if (eval(board,-INFTY,INFTY) > draw_score()) {
	    //decline the offer when you have a better position
	    infolog("draw declined");
	  } else {
	    /* If the positions are equal or you are worse off than
	       your opponent, then accept a draw. However, we shouldn't
	       quit playing until xboard tells us that the game is over.
	       That's because when playing on ICS, it's possible that the
	       draw offer has been withdrawn by the time we accept it. */
	    printf("offer draw\n");
	    infolog("draw accepted");
	  }
	  board->color_at_move = temp;
	} else if (strcmp(input,"dumpa") == 0) {
	  //Used for debugging purposes
	  showboard(board);
	} else if (strcmp(input,"quit") == 0) {
	  started = 0;
	  run = 0;
	  //stop_input_thread();
	} else if (strcmp(input,"white") == 0) {
	  board->color_at_move = WHITE;
	  engine_color = BLACK;
	  stop_own_clock();
	  stop_opp_clock();
	} else if (strcmp(input,"black") == 0) {
	  board->color_at_move = BLACK;
	  engine_color = WHITE;
	  stop_own_clock();
	  stop_opp_clock();
	} else if (strncmp(input,"setboard",8) == 0) {
	  set_fen_board(board,&(input[9]));
	  zero_success_failure_count();
	  showboard(board);
	} else if (strncmp(input,"st",2) == 0) {
	  sscanf(input,"st %d",&temp);
	  set_clock_mode(FIXED_CLOCK_MODE);
	  /* Remove 0.5 seconds to make sure the time limit isn't exceeded
	     since also some post search stuff needs to be taken care of. */
	  set_fixed_thinking_time(temp*1000 - 500);
	} else if (strncmp(input,"sd",2) == 0) {
	  sscanf(input,"sd %d",&temp);
	  max_depth = temp;
	} else if (strcmp(input,"go") == 0) {
	  started = 1;
	  engine_color = Color;
	  if (board->color_at_move == BLACK && board->move_number == 0) {
	    board->color_at_move = WHITE;
	    start_opp_clock();
	  } else {
	    start_own_clock();
	    computer_make_move(&board,&started,max_depth,
			       &book,0,&wait_for_oppmove);
	    stop_own_clock();
	    start_opp_clock();
	  }
	} else {
	  if (parsemove(input,&board,&started,&wait_for_oppmove)) {
	    stop_opp_clock();
	    start_own_clock();
	    computer_make_move(&board,&started,max_depth,
			       &book,0,&wait_for_oppmove);
	    stop_own_clock();
	    start_opp_clock();
	  }
	}
      }



#ifdef AM_DEBUG
      else if (mode == DEBUG_MODE) {
	if (strcmp(input,"debexit") == 0) {
	  mode = NORMAL_MODE;
	} else if ((strcmp(input,"help") == 0) || (strcmp(input,"?") == 0))
	  showhelp(mode);
	else if (strcmp(input,"show") == 0) {
	  showboard(board);
	} else if (strcmp(input,"set") == 0)
	  showsettings(board,white,black,started,gamelog);
	else if (strcmp(input,"gamelog") == 0)
	  gamelog = 1;
	else if (strcmp(input,"nogamelog") == 0)
	  gamelog = 0;
	else
	  printf("Error (unknown command): %s - Press '?' for help.\n",input);
      }
#endif    //AM_DEBUG




      else {   //NORMAL_MODE
	if (strcmp(input,"xboard") == 0) {
	  mode = XBOARD_MODE;
	  printf("\n");   //xboard wants a newline here
	  signal(SIGINT,SIG_IGN);  //ignore SIGINT to work okay with xboard
	  infolog("xboard");
	}
#ifdef AM_DEBUG
	else if (strcmp(input,"debug") == 0) {
	  mode = DEBUG_MODE;
	}
#endif    //AM_DEBUG
	else if (strcmp(input,"hist") == 0) {
	  //showhistory();
	} else if (strcmp(input,"quit") == 0) {
	  run = 0;
	  //stop_input_thread();
	} else if ((strcmp(input,"help") == 0) || (strcmp(input,"?") == 0))
	  showhelp(mode);
	else if (strcmp(input,"show") == 0) {
	  showboard(board);
	} else if (strcmp(input,"enginewhite") == 0) {
	  engine_color = WHITE;
	} else if (strncmp(input,"testsuite",9) == 0) {
	  testsuite(board,input);
	} else if (strncmp(input,"perft",5) == 0) {
	  if (sscanf(&(input[6]),"%d",&temp) == 0 || temp <= 0) {
	    printf("Faulty depth specified.\n");
	  } else if (temp > max_depth) {
	    printf("Cannot run perft to deeper than max_depth.\n");
	  } else {
	    printf("Running perft to depth %d.\n",temp);
	    i = run_perft(&board,temp);
	    printf("Perft to depth %d: %d nodes.\n",temp,i);
	  }
	} else if (strncmp(input,"setboard",8) == 0) {
	  set_fen_board(board,&(input[9]));
	  zero_success_failure_count();
	  showboard(board);
	} else if (strncmp(input,"play",4) == 0) {
	  int debug_book = 0;
	  int debug_max_depth = max_depth;
	  int parsable = 0;
	  int old_clock_mode = get_clock_mode();
	  if (strlen(input) > 4) {
	    if (strstr(input,"-d") != NULL) {          //depth specified
	      sscanf(strstr(input,"-d"),"-d %d",&debug_max_depth);
	      parsable = 1;
	      own_time = INFTY;
	      opp_time = INFTY;
	      base = INFTY;
	      increment = 0;
	      moves_per_timecontrol = 0;
	      timectrl = TIMECTRL_NOINC;
	    } else if (strstr(input,"-t") != NULL) {   //time specified
	      int debug_seconds;
	      sscanf(strstr(input,"-t"),"-t %d",&debug_seconds);
	      set_clock_mode(FIXED_CLOCK_MODE);
	      set_fixed_thinking_time(debug_seconds*1000);
	      parsable = 1;
	    }
	  }
	  if (parsable) {
	    int old_post = post;
	    post = 1;
	    start_own_clock();
	    computer_make_move(&board,&started,debug_max_depth,
			       &debug_book,0,&wait_for_oppmove);
	    stop_own_clock();
	    set_clock_mode(old_clock_mode);
	    post = old_post;
	  } else
	    printf("Command takes the form: play [-d depth] [-t time]\n");
	} else if (strcmp(input,"humanwhite") == 0) {
	  engine_color = BLACK;
	} else if (strcmp(input,"dumpa") == 0) {
	  //Used for debuging purposes
	  for (i = 0; i < 64; i++) {
	    if ((board->rotNW_pieces[WHITE] | board->rotNW_pieces[BLACK]) & square[i])
	      printf("1");
	    else
	      printf("0");
	    if (i == 55 || i == 47 || i == 39 || i == 31
		|| i == 23 || i == 15 || i == 7)
	      printf("\n");
	  }
	  printf("\n\n");
	} else if (strcmp(input,"engineblack") == 0) {
	  engine_color = BLACK;
	} else if (strcmp(input,"humanblack") == 0) {
	  engine_color = WHITE;
	  board->color_at_move = BLACK;
	} else if (strcmp(input,"start") == 0) {
	  if (started != 1) {
	    //non_reversible_move();   //zero the repetition counters
	    elapsed_time_in_curr_game = 0;
	    supposed_elapsed_time_in_curr_game = 0;
	    stop_own_clock();
	    stop_opp_clock();
	    own_time = 300000;    //5 minutes
	    opp_time = 300000;    //5 minutes
	    base = 300000;        //5 minutes
	    increment = 0;
	    moves_per_timecontrol = 0;
	    timectrl = TIMECTRL_NOINC;
	    started = 1;
	    book = 1;
	    if (Color == engine_color) {
	      start_own_clock();
	      computer_make_move(&board,&started,max_depth,
				 &book,0,&wait_for_oppmove);
	      stop_own_clock();
	    }
	    start_opp_clock();
	  }
	} else if (strcmp(input,"set") == 0)
#ifndef AM_DEBUG
	  showsettings(board,engine_color,started);
#else
	showsettings(board,engine_color,started,gamelog);
#endif
	else if (input[0] != '\0') {
	  if (parsemove(input,&board,&started,&wait_for_oppmove)) {
	    stop_opp_clock();
	    start_own_clock();
	    computer_make_move(&board,&started,max_depth,
			       &book,0,&wait_for_oppmove);
	    stop_own_clock();
	    start_opp_clock();
	  }
	}
      }
#ifdef AM_DEBUG
      if (mode == DEBUG_MODE)
	printf("debug");
#endif
    }
  }
  free(pawn_table);
  free(ref_table);
  free(transp_table);
  free(input_from_thread);
  free(input);
  free(str3);
  free(str2);
  free(str);
  free(*repetition_list);
  free(repetition_list);
  free(*historik);
  free(historik);
  free(board);
  for (i = 0; i <= max_depth*2; i++) {
    free(killers[i]);
  }
  free(killers);
}
