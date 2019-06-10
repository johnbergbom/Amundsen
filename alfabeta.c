/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "alfabeta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <sys/resource.h>
#endif
#include "makemove.h"
#include "genmoves.h"
#include "eval.h"
#include "hash.h"
#include "slump.h"
#include "parse.h"
#include "display.h"
#include "timectrl.h"
#include "swap.h"
//#include "egtbprobe.h" //nalimov

extern int pieceval[6];
extern bitboard square[64];
extern int nbr_men_bases_found;
extern bitboard pawn_start[2];
extern bitboard white_squares;
extern bitboard black_squares;
extern int max_depth;
extern int pending_input;
extern char *input_from_thread;
extern int own_time;
extern int opp_time;
extern int post;
extern int engine_color;
extern struct move guessed_opp_move;
extern struct board stored_board;
extern int pondering_in_use;

/*------------------------------------------
 | Variables used for statistics purposes. |
 ------------------------------------------*/
int visited_nodes;               //total number (quiescence + normal search)
int quiescence_nodes;
int cutoffs = 0;                 //counts the number of cutoffs
int cutoffs_first = 0;           //cutoffs at the first move tried
int nullmoves_tried = 0;
int nullmove_cutoffs = 0;
int lmr_reductions = 0;
int lmr_reductions_tried = 0;
int fpruned_moves = 0;

/* The following variables are used for hash statistics. */
int probes = 0;
int sc_hits = 0;                 //a score was returned
int m_hits = 0;                  //a move was returned
int b_hits = 0;                  //alpha or beta bounds were changed
int an_hits = 0;                 //avoid nullmove
int misses = 0;

/*-------------------------------------------------------------------
 | We don't do the time checking too often, so we define a constant |
 | telling how many nodes should be searched before we check if     |
 | there is time left.                                              |
 -------------------------------------------------------------------*/
int abort_search;
int NODES_TO_SEARCH_BETWEEN_TIME_CHECKS = 1000;
int node_count_at_last_check = 0;
int search_status;

/* Killers keep track of the moves that cause a cutoff
   at each level of the search. */
struct killers **killers;

/* Declare the history moves table + success/failure count for non-captures
   and non-promotions. */
int historymovestable[2][64][64];
int failed_cutoff_count[2][16][64];
int success_cutoff_count[2][16][64];

int maxval(int a, int b) {
  if (a > b)
    return a;
  else
    return b;
}

int minval(int a, int b) {
  if (a > b)
    return b;
  else
    return a;
}

int compare_boards(struct board *board1, struct board *board2) {
  int differ = 0, i;

  for (i = 0; i < 6; i++)
    if (board1->piece[WHITE][i] != board2->piece[WHITE][i]) {
      differ = 1;
      printf("a\n");
    }
  for (i = 0; i < 6; i++)
    if (board1->piece[BLACK][i] != board2->piece[BLACK][i]) {
      differ = 1;
      printf("b\n");
    }
  if (board1->all_pieces[WHITE] != board2->all_pieces[WHITE]) {
    differ = 1;
    printf("c\n");
  }
  if (board1->all_pieces[BLACK] != board2->all_pieces[BLACK]) {
    differ = 1;
    printf("d\n");
  }
  if (board1->rot90_pieces[WHITE] != board2->rot90_pieces[WHITE]) {
    differ = 1;
    printf("e\n");
  }
  if (board1->rot90_pieces[BLACK] != board2->rot90_pieces[BLACK]) {
    differ = 1;
    printf("f\n");
  }
  if (board1->rotNE_pieces[WHITE] != board2->rotNE_pieces[WHITE]) {
    differ = 1;
    printf("g\n");
  }
  if (board1->rotNE_pieces[BLACK] != board2->rotNE_pieces[BLACK]) {
    differ = 1;
    printf("h\n");
  }
  if (board1->rotNW_pieces[WHITE] != board2->rotNW_pieces[WHITE]) {
    differ = 1;
    printf("i\n");
  }
  if (board1->rotNW_pieces[BLACK] != board2->rotNW_pieces[BLACK]) {
    differ = 1;
    printf("j\n");
  }
  if (board1->castling_status[WHITE] != board2->castling_status[WHITE]) {
    differ = 1;
    printf("k (%d, %d)\n",board1->castling_status[WHITE],board2->castling_status[WHITE]);
  }
  if (board1->castling_status[BLACK] != board2->castling_status[BLACK]) {
    differ = 1;
    printf("l\n");
  }
  if (board1->passant != board2->passant) {
    differ = 1;
    printf("m\n");
  }
  if (board1->captures[WHITE] != board2->captures[WHITE]) {
    differ = 1;
    printf("n\n");
  }
  if (board1->captures[BLACK] != board2->captures[BLACK]) {
    differ = 1;
    printf("o\n");
  }
  if (board1->moves_left_to_draw != board2->moves_left_to_draw) {
    differ = 1;
    printf("p\n");
  }
  if (board1->move_number != board2->move_number) {
    differ = 1;
    printf("q\n");
  }
  if (board1->color_at_move != board2->color_at_move) {
    differ = 1;
    printf("r\n");
  }
  if (board1->zobrist_key != board2->zobrist_key) {
    differ = 1;
    printf("s\n");
  }
  if (board1->zobrist_pawn_key != board2->zobrist_pawn_key) {
    differ = 1;
    printf("t\n");
  }
  if (board1->material_pieces[WHITE] != board2->material_pieces[WHITE]) {
    differ = 1;
    printf("u\n");
  }
  if (board1->material_pieces[BLACK] != board2->material_pieces[BLACK]) {
    differ = 1;
    printf("v\n");
  }
  if (board1->material_pawns[WHITE] != board2->material_pawns[WHITE]) {
    differ = 1;
    printf("w\n");
  }
  if (board1->material_pawns[BLACK] != board2->material_pawns[BLACK]) {
    differ = 1;
    printf("x\n");
  }
  if (board1->nbr_pieces[WHITE] != board2->nbr_pieces[WHITE]) {
    differ = 1;
    printf("y\n");
  }
  if (board1->nbr_pieces[BLACK] != board2->nbr_pieces[BLACK]) {
    differ = 1;
    printf("z\n");
  }
  for (i = 0; i < 64; i++)
    if (board1->boardarr[i] != board2->boardarr[i]) {
      differ = 1;
      printf("aa\n");
    }
  return differ;
}

int perft(struct board *board, int alpha, int beta, int depth, int ply) {
  int movables                = 0;
  int org_alpha               = alpha;
  int org_beta                = beta;
  int best                    = 0;
  int order                   = 0;
  int cutcount                = 0;
  int swap_count              = 0;
  int killers_processed       = 0;
  int hashf                   = ALPHA;
  int transp_table_closed     = 0;
  int ref_table_closed        = 0;
  int do_null                 = 1;
  struct move move;       //doesn't need initialization
  struct move best_move;  //doesn't need initialization
  struct move refmove;    //doesn't need initialization
  struct move transpmove; //doesn't need initialization
  int own_king_check;         //doesn't need initialization
  int legal_moves;            //doesn't need initialization
  int hash_flag;              //doesn't need initialization
  //struct move best_move_below;            //doesn't need initialization
  struct board saved_board;
  int retval;
  struct moves moves[16];
  struct swap_entry swap_list[16];
  struct hist_entry hist_list[NBR_HISTMOVES];
  bitboard spec_moves;
  int value, i;
  int type;
  int cutoff_val;
  int best_value;

  best_value = retval = -INFTY;

  if (in_check(board)) {
    own_king_check = 1;
  } else {
    own_king_check = 0;
  }

  /*--------------------------------------------------------------
   | Check for draws due to repetition of 3 identical positions. |
   --------------------------------------------------------------*/
  if (is_draw_by_repetition(board)) {
    return draw_score();
  }

  /*------------------------------------------------------------
   | Probe the hash tables (note that when running perft, then |
   | the purpose with probing the hashtable isn't to cut off   |
   | moves, but rather to make sure that the move generation   |
   | works identically to how it works in a regular search).   |
   ------------------------------------------------------------*/
  probes++;
  
  hash_flag = probe_hash((char)depth,board,&transpmove,&refmove,
			 &alpha,&beta,&retval,ply,&cutoff_val,&do_null,0);
  if (hash_flag & CUTOFF_VALUE) {
    sc_hits++;
    transp_table_closed = 1;
  } else if (hash_flag & TRANSP_MOVE || hash_flag & REF_MOVE) {
    m_hits++;
  } else if (org_alpha != alpha || org_beta != beta) {
    b_hits++;
  } else if (hash_flag == UNKNOWN_NEW)
    misses++;

  /*----------------------------
   | Detect illegal positions. |
   ----------------------------*/
  if (opp_in_check(board)) {
    if (!transp_table_closed) {
      record_transposition((char)depth,board,&move,0,EXACT,ply,KINGTAKEN);
      transp_table_closed = 1;
    }
    return KINGTAKEN;
  }

  /*-------------------------------------
   | Check if draw by the 50 move rule. |
   -------------------------------------*/
  if (board->moves_left_to_draw <= 0) {
    best_value = draw_score();
    if (!transp_table_closed) {
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      transp_table_closed = 1;
    }
    return best_value;
  }

  /*--------------------------------------------------------------
   | Very simple internal node recognizer that recognizes draws  |
   | due to insufficient material. => NOTE: If this is turned on |
   | then the engine doesn't pass the perftsuite.epd test. The   |
   | testsuite is at fault, and of course we have this check in  |
   | the real search.                                            |
   -------------------------------------------------------------*/
  /*if (board->nbr_pieces[WHITE] <= 1 && board->nbr_pieces[BLACK] <= 1
      && board->material_pawns[WHITE] == 0
      && board->material_pawns[BLACK] == 0) {
    if (board->piece[WHITE][BISHOP] && board->piece[BLACK][BISHOP]) {
      /* KBKB is a draw if the bishops are on the same color. /
      if (((board->piece[WHITE][BISHOP] & white_squares)
	   && (board->piece[BLACK][BISHOP] & white_squares))
	  || ((board->piece[WHITE][BISHOP] & black_squares)
	      && (board->piece[BLACK][BISHOP] & black_squares)))
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBKB, bishops on same colored squares
    } else if (board->nbr_pieces[WHITE] == 0
	       && board->material_pieces[BLACK] < (VAL_KING + VAL_ROOK)) {
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBK, KNK or KK
    } else if (board->nbr_pieces[BLACK] == 0
	       && board->material_pieces[WHITE] < (VAL_KING + VAL_ROOK)) {
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBK, KNK or KK
    }
  }*/

  if (depth == 0) {
    visited_nodes++;
    value = 0;
    if (!transp_table_closed) {
      if (value == KINGTAKEN)
	record_transposition((char)depth,board,&move,0,EXACT,ply,value);
      else if (value >= beta)
	record_transposition((char)depth,board,&move,0,BETA,ply,value);
      else if (value <= alpha)
	record_transposition((char)depth,board,&move,0,ALPHA,ply,value);
      else
	record_transposition((char)depth,board,&move,0,EXACT,ply,value);
      transp_table_closed = 1;
    }
    return 0;
  } else {
    legal_moves = 0;
    move.old_castling_status[WHITE] = board->castling_status[WHITE];
    move.old_castling_status[BLACK] = board->castling_status[BLACK];
    move.old_passant = board->passant;
    move.old_moves_left_to_draw = board->moves_left_to_draw;
    //best_move_below.from_square = best_move_below.to_square = 0;
    while (get_next_move(board,moves,&movables,&hash_flag,&refmove,
			 &transpmove,killers[ply],
			 &move,&order,swap_list,
			 &swap_count,&killers_processed,
			 hist_list,own_king_check,&spec_moves,depth)) {
      saved_board = *board;
      cutcount++;
      makemove(board,&move,depth);
      value = -perft(board,-beta,-alpha,depth-1,ply+1);
      unmakemove(board,&move,depth);
      if (compare_boards(&saved_board,board)) {
	printf("Makemove/unmakemove error!\n");
	printf("board1:\n");
	showboard(&saved_board);
	printf("board2:\n");
	showboard(board);
	printmove(&move);
	exit(1);
      }
      if (value == -KINGTAKEN)
	value = KINGTAKEN;

      if (value != KINGTAKEN) {
	legal_moves++;
	if (value > alpha)
	  hashf = EXACT;
	if (value > best_value) {
	  best_move = move;
	  best_value = value;
	}
	retval = maxval(retval,value);
	alpha = maxval(alpha,retval);
	if (retval >= beta && !ref_table_closed) {
	  /* We got a cutoff here, so we'll add this move to the history
	     moves table. */
	  historymovestable[(int)Color][move.from_square]
	    [move.to_square] += (1 << depth);
	  cutoffs++;
	  if (cutcount == 1)
	    cutoffs_first++;
	  /*--------------------------------------------------------------
	   | Add this move as a killer move, but make sure that we don't |
	   | pollute the killer slots with good captures or promotions.  |
	   --------------------------------------------------------------*/
	  //if (order > 6) {
	    /*for (i = 1; i < NBR_KILLERS; i++)
	      killers[depth-own_king_check][i] =
	      killers[depth-own_king_check][i-1];*/
	    killers[ply][best].fsquare =
	      square[move.from_square];
	    killers[ply][best].tsquare =
	      square[move.to_square];
	    killers[ply][best].value = retval;
	    //}
	  record_refutation((char)depth,board,&move,BETA,ply,retval);
	  ref_table_closed = 1;
	  if (!transp_table_closed) {
	    record_transposition((char)depth,board,&move,1,BETA,ply,retval);
	    transp_table_closed = 1;
	  }
	}
      }

      /* Since the move generation doesn't generate under-promotions we
	 need a hack here to pass the perftsuite.epd test... */
      if (move.type & QUEEN_PROMOTION_MOVE) {
	type = move.type;
	for (i = QUEEN_PROMOTION_MOVE; i < KNIGHT_PROMOTION_MOVE; i = i*2) {
	  type &= ~i;
	  type |= (i*2);
	  move.type = type;
	  cutcount++;
	  makemove(board,&move,depth);
	  value = -perft(board,-beta,-alpha,depth-1,ply+1);
	  unmakemove(board,&move,depth);
	  if (compare_boards(&saved_board,board)) {
	    printf("Makemove/unmakemove error!\n");
	    printf("board1:\n");
	    showboard(&saved_board);
	    printf("board2:\n");
	    showboard(board);
	    printmove(&move);
	    exit(1);
	  }
	  if (value == -KINGTAKEN)
	    value = KINGTAKEN;
	  
	  if (value != KINGTAKEN) {
	    legal_moves++;
	    if (value > alpha)
	      hashf = EXACT;
	    if (value > best_value) {
	      best_move = move;
	      best_value = value;
	    }
	    retval = maxval(retval,value);
	    alpha = maxval(alpha,retval);
	    if (retval >= beta && !ref_table_closed) {
	      /* We got a cutoff here, so we'll add this move to the history
		 moves table. */
	      historymovestable[(int)Color][move.from_square]
		[move.to_square] += (1 << depth);
	      cutoffs++;
	      if (cutcount == 1)
		cutoffs_first++;
	      /*--------------------------------------------------------------
	       | Add this move as a killer move, but make sure that we don't |
	       | pollute the killer slots with good captures or promotions.  |
	       --------------------------------------------------------------*/
	      //if (order > 6) {
		/*for (i = 1; i < NBR_KILLERS; i++)
		  killers[depth-own_king_check][i] =
		  killers[depth-own_king_check][i-1];*/
		killers[ply][best].fsquare =
		  square[move.from_square];
		killers[ply][best].tsquare =
		  square[move.to_square];
		killers[ply][best].value = retval;
		//}
	      record_refutation((char)depth,board,&move,BETA,ply,retval);
	      ref_table_closed = 1;
	      if (!transp_table_closed) {
		record_transposition((char)depth,board,&move,1,BETA,ply,retval);
		transp_table_closed = 1;
	      }
	    }
	  }
	}
      }
    }

    /*---------------------------------------------------------------------
     | If we get here, and retval == -INFTY, it means all possible moves  |
     | from this position leads to a check position. If we are already in |
     | check, then it's mate. If we're not in check, then it's stalemate. |
     ---------------------------------------------------------------------*/
    if (legal_moves == 0) {
      if (own_king_check) {
	best_value = -INFTY + ply; //quick mates are better
      } else
	best_value = draw_score(); //stalemate
      if (!transp_table_closed) {
	record_transposition((char)depth,board,&best_move,0,EXACT,ply,best_value);
	transp_table_closed = 1;
      }
    } else {
      if (best_value != -INFTY) {
	best_value = retval;
	if (!transp_table_closed) {
	  record_transposition((char)depth,board,&best_move,1,hashf,ply,best_value);
	  transp_table_closed = 1;
	}
      } else {
	best_value = retval;
	if (!transp_table_closed) {
	  record_transposition((char)depth,board,&best_move,0,hashf,ply,best_value);
	  transp_table_closed = 1;
	}
      }
    }
    return best_value;
  }
}

int quiescence(struct board *board, int alpha, int beta, int ply, int depth) {
  int order             = 0;
  int swap_list_count   = 0;
  int own_king_check    = 0;
  int retval;           //doesn't need initialization
  struct move move;     //doesn't need initialization
  struct swap_entry swap_list[16];
  int q_val;
  bitboard spec_moves;
  int stat_val;

  if (opp_in_check(board))
    return KINGTAKEN;

  visited_nodes++;
  quiescence_nodes++;

  /*-------------------------------------------------------------------
   | Init retval to the static score of the position. Then the engine |
   | will take whatever is best of continuing the capture line, or to |
   | accept the static score of the position. Note that "stand pat"   |
   | cutoffs are NOT allowed in case of check.                        |
   | TODO: Move the stat_val = eval(...) to after the in_check(...)   |
   | call. The static value isn't used if the king is in check, so we |
   | don't want to do any eval in vain.                               |
   -------------------------------------------------------------------*/
  stat_val = eval(board,alpha,beta);
  own_king_check = in_check(board);
  if (!own_king_check) {
    //stat_val = eval(board,alpha,beta);
    retval = stat_val;
    if (retval >= beta)
      return retval;
    else
      alpha = maxval(alpha,retval);
  } else {
    retval = -INFTY;
  }

  move.old_castling_status[WHITE] = board->castling_status[WHITE];
  move.old_castling_status[BLACK] = board->castling_status[BLACK];
  move.old_passant = board->passant;
  move.old_moves_left_to_draw = board->moves_left_to_draw;
  while (get_next_quiet_move(board,&move,swap_list,&order,
			     &swap_list_count,own_king_check,
			     &spec_moves,stat_val,alpha)) {
    makemove(board,&move,depth);
    q_val = -quiescence(board,-beta,-alpha,ply+1,depth-1);
    unmakemove(board,&move,depth);
    if (q_val != -KINGTAKEN) {
      if (own_king_check) {
	return maxval(retval,q_val); //quit if check and one legal move is found
      }
      retval = maxval(retval,q_val);
      if (retval >= beta)
	return retval;
      else
	alpha = maxval(alpha,retval);
    }
  }
  /*----------------------------------------------------------------------
   | We get here when we have gone through all the capture moves, and    |
   | when none of them were good enough to merit a cutoff. If the king   |
   | is in check then search aborts after the first legal move is found, |
   | so if we get here and the king is in check, then a check mate       |
   | score is returned. This doesn't however catch stalemates.           |
   ----------------------------------------------------------------------*/
  if (own_king_check) {
    return -INFTY + ply;
  } else
    return retval;

}

void check_input(struct board *board) {
  char *input;
  struct move move;
  int tmp;

  if (pending_input && !(search_status & PENDING_INPUT)) {
    input = (char *) malloc(100*sizeof(char));
    strcpy(input,input_from_thread);
    //printf("Input received during search: %s\n",input);
    /* For certain commands the control should be transferred back
       to parse(). In those cases let's leave the input unread. */
    if (strcmp(input,"quit") == 0
	|| strcmp(input,"new") == 0
	|| strcmp(input,"force") == 0
	|| strncmp(input,"level",5) == 0
	|| strncmp(input,"result",6) == 0
	|| strcmp(input,"white") == 0
	|| strcmp(input,"black") == 0
	|| strncmp(input,"setboard",8) == 0
	|| strcmp(input,"go") == 0) {
      printf("Aborting search because of input.\n");
      search_status = (NO_SEARCH | PENDING_INPUT);
      abort_search = 1;
    } else {
      if (strncmp(input,"ping",4) == 0) {
	/* If in pondering mode, then answer the ping, otherwise keep
	   the ping command in the input queue and continue searching.
	   Then it will automatically be answered when the search is
	   done. */
	printf("ping received\n");
	if (search_status & (FINDING_MOVE_TO_PONDER | PONDERING)) {
	  sscanf(input,"ping %d",&tmp);
	  printf("pong %d\n",tmp);
	  pending_input = 0;
	} else
	  search_status |= PENDING_INPUT;
      } else if (strncmp(input,"time",4) == 0) {
	sscanf(input,"time %d",&own_time);
	/* Internally the engine works with milliseconds, and xboard gives
	   us the time in centiseconds, so we have to multiply by 10 here.*/
	own_time *= 10;
	pending_input = 0;
      } else if (strncmp(input,"otim",4) == 0) {
	sscanf(input,"otim %d",&opp_time);
	/* Internally the engine works with milliseconds, and xboard gives
	   us the time in centiseconds, so we have to multiply by 10 here.*/
	opp_time *= 10;
	pending_input = 0;
      } else if (strcmp(input,"post") == 0) {
	post = 1;
	pending_input = 0;
      } else if (strcmp(input,"nopost") == 0) {
	post = 0;
	pending_input = 0;
      } else if (strcmp(input,"draw") == 0) {
	tmp = Color;
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
	board->color_at_move = tmp;
	pending_input = 0;
      } else if (strncmp(input,"st",2) == 0) {
	sscanf(input,"st %d",&tmp);
	set_clock_mode(FIXED_CLOCK_MODE);
	/* Remove 0.5 seconds to make sure the time limit isn't exceeded
	   since also some post search stuff needs to be taken care of. */
	set_fixed_thinking_time(tmp*1000 - 500);
	pending_input = 0;
      } else if (strncmp(input,"sd",2) == 0) {
	sscanf(input,"sd %d",&tmp);
	max_depth = tmp;
	pending_input = 0;
      } else if ((search_status & (FINDING_MOVE_TO_PONDER | PONDERING))
		 && (str2move(input,&stored_board,&move)
		     || san2move(input,&stored_board,&move))) {
	//printf("Parsable move received during pondering.\n");
	if (search_status & PONDERING) {
	  /* TODO: Here we could use the moves_equal function. */
	  if (move.from_square == guessed_opp_move.from_square
	      && move.to_square == guessed_opp_move.to_square) {
	    printf("Opponent played predicted move.\n");
	    /* Ok, the opponent made the predicted move. Now continue
	       the search without the pondering flag set. */
	    search_status &= ~(FINDING_MOVE_TO_PONDER | PONDERING);
	    search_status |= NORMAL_SEARCH;
	    pending_input = 0;
	    update_gratis_time();
	  } else {
	    printf("Predicted move not made, aborting search.\n");
	    infolog("Predicted move not made, aborting search.");
	    search_status =
	      (NO_SEARCH | OPPONENT_PLAYED_UNPREDICTED_MOVE | PENDING_INPUT);
	    abort_search = 1;
	  }
	} else { //if (search_status & FINDING_MOVE_TO_PONDER) {
	  printf("Opponent moved while finding move to ponder, aborting search.");
	  search_status =
	    (NO_SEARCH | PENDING_INPUT);
	  abort_search = 1;
	} /*else {
	  printf("Error, move not expected here, ignoring.\n");
	  infolog("Error, move not expected here, ignoring.");
	  pending_input = 0;
	  }*/
      } else if (input[0] == '\0') {
	fprintf(stderr,">");
	pending_input = 0;
      } else {
	/*if (mode != XBOARD_MODE)
	  printf("Command could not be interpreted. Press '?' for help.\n");
	  else*/
	printf("Error (unknown command): %s\n",input);
	pending_input = 0;
      }
    }
    free(input);
  }
}

int material_gain(struct board *board, struct move *move) {
  int gain = 0;
  if (move->type & PROMOTION_MOVE) {
    gain -= VAL_PAWN;
    if (move->type & QUEEN_PROMOTION_MOVE)
      gain += VAL_QUEEN;
    else if (move->type & ROOK_PROMOTION_MOVE)
      gain += VAL_ROOK;
    else if (move->type & BISHOP_PROMOTION_MOVE)
      gain += VAL_BISHOP;
    else
      gain += VAL_KNIGHT;
  }
  if (move->type & PASSANT_MOVE) {
    gain += VAL_PAWN;
  } else if (move->type & CAPTURE_MOVE) {
    gain += pieceval[board->boardarr[move->to_square]];
  }
  return gain;
}

int alphabeta(struct board *board, int alpha, int beta, int depth, int null_ok,
	      int ply, int threat, int white_checks, int black_checks, int pv,
	      struct move *best_move) {
  int movables                            = 0;
  int best                                = 0;
  int order                               = 0;
  int cutcount                            = 0;
  int org_alpha                           = alpha;
  int org_beta                            = beta;
  int orgnull_ok                          = null_ok;
  int swap_count                          = 0;
  int killers_processed                   = 0;
  int fprune                              = 0;
  int hashf                               = ALPHA;
  int null_threat                         = 0;
  int limited_razoring                    = 0;
  int check_move;                         //doesn't need initialization
  struct move move;                       //doesn't need initialization
  struct move best_move_below;            //doesn't need initialization
  int extension;                          //doesn't need initialization
  struct move refmove;                    //doesn't need initialization
  struct move transpmove;                 //doesn't need initialization
  int own_king_check;                     //doesn't need initialization
  int legal_moves, tried_legal_moves;     //doesn't need initialization
  int hash_flag;                          //doesn't need initialization
  int probe_value;                        //doesn't need initialization
  int pawn_push;                          //doesn't need initialization
  int lmr_threat;                         //doesn't need initialization
  int likely_good_move;                   //doesn't need initialization
  int possibly_good_move;                 //doesn't need initialization
  int cutoff_val;
  struct swap_entry swap_list[16];
  struct hist_entry hist_list[NBR_HISTMOVES];
  int retval;
  struct moves moves[16];
  int R_adapt;
  bitboard spec_moves;
  int material_balance;
  int fmax;
  int value, best_value;
  struct move searched_moves[256];
  int i;

  best_value = retval = -INFTY;

  visited_nodes++;

  /*----------------------------------------
   | Check if there is time left to think. |
   ----------------------------------------*/
  if (abort_search)
    return 0;
  if (visited_nodes - node_count_at_last_check
      >= NODES_TO_SEARCH_BETWEEN_TIME_CHECKS) {
    node_count_at_last_check = visited_nodes;
    if (time_is_up(1)) {
      abort_search = 1;
      return 0;
    }
    if (pondering_in_use)
      check_input(board);
  }

  /*----------------------------
   | Detect illegal positions. |
   ----------------------------*/
  if (opp_in_check(board)) {
    return KINGTAKEN;
  }

  /*------------------------------------------------------------------
   | Extension of depth has to be done before probing the hashtable. |
   ------------------------------------------------------------------*/
  if (in_check(board)) {
    own_king_check = 1;
    depth++;
    extension = 1;
    if (Color == WHITE) white_checks++; else black_checks++;
  } else if (threat) {
    own_king_check = 0;
    depth++;
    extension = 1;
  } else {
    own_king_check = 0;
    extension = 0;
  }

  /*-----------------------------------------------
   | Find out if futility pruning should be done. |
   -----------------------------------------------*/
  material_balance = board->material_pieces[Color]
    + board->material_pawns[Color]
    - board->material_pieces[Oppcolor]
    - board->material_pawns[Oppcolor];

  /* Decide about limited razoring at pre-pre-frontier nodes. */
  if (!pv && !extension && depth == 3
      && ((material_balance + RAZOR_MARGIN) <= alpha)
      && board->nbr_pieces[Oppcolor] > 3) {
    depth--;
    limited_razoring = 1;
  }

  /* Decide about extended futility pruning at pre-frontier nodes. */
  if (!pv && !extension && depth == 2
      && ((material_balance + EXT_FUTILITY_PRUNING_MARGIN) <= alpha)) {
    retval = fmax = material_balance + EXT_FUTILITY_PRUNING_MARGIN;
    fprune = 1;
  }

  /* Decide about normal futility pruning at frontier nodes. */
  if (!pv && !own_king_check && depth == 1
      && ((material_balance + FUTILITY_PRUNING_MARGIN) <= alpha)) {
    retval = fmax = material_balance + FUTILITY_PRUNING_MARGIN;
    fprune = 1;
  }

  /*------------------------------------------------------------
   | Check if draw due to repetition of 3 identical positions, |
   | or due to the 50 move rule.                               |
   ------------------------------------------------------------*/
  if (is_draw_by_repetition(board) || board->moves_left_to_draw <= 0)
    return draw_score();

  /*--------------------------------------------------------------
   | Probe the hash tables (both the transposition table and the |
   | refutation table are probed in the probe_hash()-function).  |
   --------------------------------------------------------------*/
  probes++;
  hash_flag = probe_hash((char)depth,board,&transpmove,&refmove,
			 &alpha,&beta,&retval,ply,&cutoff_val,&null_ok,pv);
  if (hash_flag & CUTOFF_VALUE) {
    sc_hits++;
    return cutoff_val;
  } else if (hash_flag & TRANSP_MOVE || hash_flag & REF_MOVE) {
    m_hits++;
  } else if (org_alpha != alpha || org_beta != beta) {
    b_hits++;
  } else if (orgnull_ok != null_ok) {
    an_hits++;
  } else if (hash_flag == UNKNOWN_NEW)
    misses++;

  /*---------------------------------------------------------------
   | Very simple internal node recognizer that recognizes draws   |
   | due to insufficient material. The draw_score isn't           |
   | path-dependent, so therefore we want to store it in the hash |
   | (by making sure that not exactly draw_score() is stored).    |
   ---------------------------------------------------------------*/
  if (board->material_pawns[WHITE] == 0 && board->material_pawns[BLACK] == 0
      && board->nbr_pieces[WHITE] <= 1 && board->nbr_pieces[BLACK] <= 1) {
    if (board->piece[WHITE][BISHOP] && board->piece[BLACK][BISHOP]) {
      /* KBKB is a draw if the bishops are on the same color. */
      if (((board->piece[WHITE][BISHOP] & white_squares)
	   && (board->piece[BLACK][BISHOP] & white_squares))
	  || ((board->piece[WHITE][BISHOP] & black_squares)
	      && (board->piece[BLACK][BISHOP] & black_squares)))
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBKB, bishops on same colored squares
    } else if (board->nbr_pieces[WHITE] == 0
	       && board->material_pieces[BLACK] < (VAL_KING + VAL_ROOK)) {
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBK, KNK or KK
    } else if (board->nbr_pieces[BLACK] == 0
	       && board->material_pieces[WHITE] < (VAL_KING + VAL_ROOK)) {
      best_value = draw_score() + (Color == WHITE ? -1 : 1);
      record_transposition((char)depth,board,&move,0,EXACT,ply,best_value);
      return best_value; //KBK, KNK or KK
    }
  }

  /*------------------------------------------------------------
   | Probe the endgame tablebases. => This code is commented   |
   | out for the time being since I wasn't able to get hold of |
   | Andrew Kadatch to ask for his permission to use his       |
   | compression code (however I did get Eugene Nalimov's      |
   | permission to use his part).                              |
   ------------------------------------------------------------*/
  //nalimov
  /*if (ply < 4 && bitcount(board->all_pieces[WHITE] | board->all_pieces[BLACK]) <= nbr_men_bases_found) {
    if (probe_endgame_table(board,&probe_value)) {
      //move.value = probe_value;
      record_transposition((char)depth,board,&move,0,EXACT,ply,probe_value);
      return probe_value;
    }
    }*/

  /*---------------------------------------------------------------
   | Do null-move pruning if the king is not in check and if both |
   | players has at least one piece left (otherwise the risk of   |
   | zugzwang is too great), and if some other conditions are     |
   | fulfilled as well.                                           |
   |                                                              |
   | TODO: Maybe try to apply some of the conditions for late     |
   | move reduction to the nullmove pruning condition, for        |
   | example !limited_razoring.                                   |
   ---------------------------------------------------------------*/
  if (!pv && !fprune && own_king_check == 0 && extension == 0) {
    if (null_ok && board->nbr_pieces[WHITE] && board->nbr_pieces[BLACK]
	&& material_balance + VAL_PAWN >= beta
	&& white_checks < 3 && black_checks < 3
	&& beta > (-INFTY + 150)
	&& ((depth <= 1 && material_balance + 20 > alpha)
	    || (depth > 1 && eval(board,alpha,beta) > alpha))) {

      /*---------------------------------------------------------------
       | Adaptive nullmove search: Use R = 3 in the upper part of the |
       | search tree except for in the late endgame, or else R = 2.   |
       ---------------------------------------------------------------*/
      if (depth <= 5
	       || (depth <= 7 && (board->nbr_pieces[WHITE] < 3
				  || board->nbr_pieces[BLACK] < 3))) {
	R_adapt = 2;
      } else {
	R_adapt = 3;
      }
      if (ply == 1)
	R_adapt--;

      move.old_passant = board->passant;
      make_nullmove(board);
      if (depth-1-R_adapt <= 0) {
	value = -quiescence(board,-beta,1-beta,ply+1,depth-1-R_adapt);
      } else {
	value = -alphabeta(board,-beta,1-beta,depth-1-R_adapt,0,ply+1,0,
			   white_checks,black_checks,0,&best_move_below);
      }
      unmake_nullmove(board,move.old_passant);
      nullmoves_tried++;

      /*------------------------------------------------------------
       | If not moving gets the side to move mated, then this is a |
       | dangerous position, so then let's extend the search. Else |
       | if the value is greater than beta then cutoff.            |
       ------------------------------------------------------------*/
      if (value <= (-INFTY + 150)) {
	null_threat = 1;
      } else if (value >= beta) {
	record_transposition((char)depth,board,&move,0,BETA,ply,value);
	nullmove_cutoffs++;
	return value;
      }
    }
  }

  /*---------------------------------------------------------
   | Internal iterative deepening: if this is a PV node and |
   | there is no best move from the hash table, then do a   |
   | shallow search to find a best move to search first.    |
   ---------------------------------------------------------*/
  if (pv && depth >= 4 && !(hash_flag & REF_MOVE)
      && !(hash_flag & TRANSP_MOVE)) {
    best_move_below.from_square = best_move_below.to_square = 0;
    value = alphabeta(board,alpha,beta,depth-2,0,ply,
		      0,white_checks,black_checks,0,
		      &best_move_below);
    if (best_move_below.from_square != 0 && best_move_below.to_square != 0) {
      hash_flag |= REF_MOVE;
      refmove.from_square = best_move_below.from_square;
      refmove.to_square = best_move_below.to_square;
    }
  }

  /*---------------------------------------------------
   | Initiate some stuff needed for undoing of moves. |
   ---------------------------------------------------*/
  legal_moves = tried_legal_moves = 0;
  move.old_castling_status[WHITE] = board->castling_status[WHITE];
  move.old_castling_status[BLACK] = board->castling_status[BLACK];
  move.old_passant = board->passant;
  move.old_moves_left_to_draw = board->moves_left_to_draw;

  while (get_next_move(board,moves,&movables,&hash_flag,&refmove,
		       &transpmove,killers[ply],&move,&order,swap_list,
		       &swap_count,&killers_processed,hist_list,
		       own_king_check,&spec_moves,depth)) {
    check_move = checking_move(board,&move);

    /*------------------------------------------------------------
     | Futility pruning. TODO: Try to use likely_good_move here. |
     ------------------------------------------------------------*/
    if (!null_threat && fprune
	&& (fmax + material_gain(board,&move)) <= alpha	&& !check_move
	/*&& !likely_good_move*/) {
      fpruned_moves++;

      /* In order to correctly assess mate positions it is necessary
	 to keep track of the number of legal moves for a certain position.
	 There are cases when all moves are futility pruned, and in that
	 case we won't know whether to return a mate score or something else.
	 Therefore, if the number of legal moves is zero, then make the move
	 and see if one is in check. If not then increase the legal moves
	 counter. */
      if (legal_moves == 0) {
	makemove(board,&move,depth);
	if (!opp_in_check(board))
	  legal_moves++;
	unmakemove(board,&move,depth);
      }
      continue;
    }

    /*---------------------------------------------------
     | Check if this move is likely to be good based on |
     | simple cutoff statistics.                        |
     ---------------------------------------------------*/
    {
      int succ, fail;
      succ = success_cutoff_count[(int)Color]
	[board->boardarr[move.from_square]][move.to_square];
      fail = failed_cutoff_count[(int)Color]
	[board->boardarr[move.from_square]][move.to_square];
      if (succ > fail) {
	likely_good_move = 1;
	possibly_good_move = 1;
      } else if (succ == fail) {
	likely_good_move = 0;
	possibly_good_move = 1;
      } else {
	likely_good_move = 0;
	possibly_good_move = 0;
      }
    }
    /*if (likely_good_move)
      infolog("likely good"); //45762
    else
      infolog("likely not good"); //143136
    if (possibly_good_move)
      infolog("possibly good"); //133980
    else
      infolog("possibly not good"); //54917
    */

    /*-----------------------------------------------------------
     | If pawn push to the 7th rank at a leaf node then mark it |
     | as a threat move.                                        |
     -----------------------------------------------------------*/
    if ((depth == 1) && (move.piece == PAWN)
	&& (square[move.to_square] & (pawn_start[WHITE] | pawn_start[BLACK])))
      pawn_push = 1;
    else
      pawn_push = 0;

    /*--------------------------------------------------
     | Decide if this move should be pruned. In effect |
     | this is a soft transition to quiescence search. |
     --------------------------------------------------*/
    /*if (!pv && depth-1+pawn_push == 1 && depth-1+null_threat == 1) {
      if (!check_move && !own_king_check && !(move.type & CAPTURE_MOVE)
	  && !pawn_push && !extension && !null_threat && !limited_razoring
	  && !(move.type & PROMOTION_MOVE) && tried_legal_moves >= 7
	  && !likely_good_move) {
	continue;
      }
      }*/

    /*--------------------------------------------------
     | Decide if quiescence can be skipped altogether. |
     --------------------------------------------------*/
    if (!pv && depth-1+pawn_push <= 0 && depth-1+null_threat <= 0) {
      if (!check_move && !own_king_check && !(move.type & CAPTURE_MOVE)
	  && !pawn_push && !extension && !null_threat && !limited_razoring
	  && !(move.type & PROMOTION_MOVE) && tried_legal_moves >= 7
	  && !likely_good_move) {
	continue;
      }
    }


    makemove(board,&move,depth);

    /*--------------------------------------------------------------
     | PV-search. Search the first move using the full window, and |
     | later moves with a minimal window, and make a re-search if  |
     | the minimal window search raises alpha.                     |
     --------------------------------------------------------------*/
    //TODO: Try this: (pv || tried_legal_moves == 0) and also just (pv)
    if (pv && tried_legal_moves == 0) {
      if (depth-1+pawn_push <= 0 && depth-1+null_threat <= 0) {
	value = -quiescence(board,-beta,-alpha,ply+1,depth-1);
      } else {
	value = -alphabeta(board,-beta,-alpha,depth-1,1,ply+1,
			   pawn_push+null_threat,white_checks,black_checks,1,
			   &best_move_below);
      }
    } else {
      /*---------------------------------------------------------------------
       | Late move reduction. If we have already searched the first few     |
       | moves and the move doesn't seem dangerous then make a shallow null |
       | move search, and if the result was below alpha then be satisfied   |
       | with that. Otherwise make a nominal depth search.                  |
       | TODO: See if the LMR condition can be further optimized using the  |
       | white_checks and black_checks values (and possible other           |
       | conditions taken from the nullmove pruning).                       |
       ---------------------------------------------------------------------*/
      lmr_threat = 0;
      if (!pv
	  && !own_king_check
	  && !extension
	  && depth >= 2
	  && ((ply > 3 && tried_legal_moves >= 2)
	      || (ply == 3 && !likely_good_move && tried_legal_moves >= 2)
	      || (ply == 2 && !possibly_good_move && tried_legal_moves >= 4)
	      || (ply == 1 && !possibly_good_move && tried_legal_moves >= 8))
	  && !(move.type & CAPTURE_MOVE)
	  && !(move.type & PROMOTION_MOVE)
	  && !pawn_push
	  && !null_threat
	  && !limited_razoring
	  && !check_move
	  /*&& !likely_good_move*/) { //don't check !likely_good_move here!!
	/* TODO: Try calling alphabeta with null_ok = 0 if
	   tried_legal_moves == 0. */
	if (depth == 2) {
	  /*----------------------------------------------------------------
	   | If we are at a pre-frontier node, and it's highly unlikely    |
	   | that this move is any good, then skip the move in question.   |
	   |                                                               |
	   | Around 45% of the pre-frontier moves will be cutoff this way, |
	   | and tests showed that of these moves only about 0.25% were    |
	   | such that they would have raised alpha if actually played.    |
	   |                                                               |
	   | In addition self play has shown that this actually makes      |
	   | Amundsen play better. So this isn't as risky as it seems. The |
	   | extra search depth gained makes it pay off.                   |
	   ----------------------------------------------------------------*/
	  if (!possibly_good_move && retval + 10 <= alpha) {
	    unmakemove(board,&move,depth);
	    continue;
	  } else {
	    value = -quiescence(board,-alpha-1,-alpha,ply+1,depth-2);
	  }
	} else {
	  value = -alphabeta(board,-alpha-1,-alpha,depth-2,1,ply+1,0,
			     white_checks,black_checks,0,&best_move_below);
	}
	lmr_reductions_tried++;
	if (value <= alpha) {
	  lmr_reductions++;
	} else
	  lmr_threat = 1;
      } else {
	value = alpha + 1; //hack to make sure full depth search is done
      }
      
      if (value > alpha) {
	if (depth-1+pawn_push <= 0 && depth-1+null_threat <= 0
	    && depth-1+lmr_threat <= 0) {
	  /* TODO: Why isn't null window used here? Something like
	     this should probably be used:
	    value = -quiescence(board,-alpha-1,-alpha,ply+1,depth-1);
	  if (value > alpha && value < beta && value != -KINGTAKEN)
	  value = -quiescence(board,-beta,-alpha,ply+1,depth-1);*/
	  value = -quiescence(board,-beta,-alpha,ply+1,depth-1);
	} else {
	  /* TODO: Try calling alphabeta with null_ok = 0 if
	     tried_legal_moves == 0. */
	  /* If lmr failed, then try to call alphabeta without
	     reduced depth but still with a null move window,
	     and if that also fails, then do another re-search
	     using full window and non-reduced depth. */
	  value = -alphabeta(board,-alpha-1,-alpha,depth-1,1,
			     ply+1,pawn_push+null_threat+lmr_threat,
			     white_checks,black_checks,0,&best_move_below);
	  if (value > alpha && value < beta && value != -KINGTAKEN
	      && depth > 2) {
	    //Reinefeld's idea: re-search only if depth > 2
	    //and tighter bound on the re-search!
	    //TODO: This wasn't actually tested so thorouhgly, and it
	    //might actually be worse than the previous code. Test this!
            //TODO: Check out if this re-search only if depth > 2
            //also requires that for depth <= 2 PVS isn't used
            //(i.e. that at depth <= 2 all moves will be searched
            //with the full window)
	    value = -alphabeta(board,-beta,-value,depth-1,1,
			       ply+1,pawn_push+null_threat+lmr_threat,
			       white_checks,black_checks,pv,&best_move_below);
	  }
	}
      }
    }

    cutcount++;
    unmakemove(board,&move,depth);
    if (value == -KINGTAKEN)
      value = KINGTAKEN;
    
    if (value != KINGTAKEN) {
      legal_moves++;
      if (tried_legal_moves < 256)
	searched_moves[tried_legal_moves] = move;
      tried_legal_moves++;
      if (value > alpha)
	hashf = EXACT;
      if (value > best_value) {
	*best_move = move;
	best_value = value;
      }
      retval = maxval(retval,value);
      alpha = maxval(alpha,retval);
      if (retval >= beta) {
	/*--------------------------------------------------
	 | We got a cutoff here, so we'll add this move to |
	 | the history moves table as well as update the   |
	 | success/failure count for non-captures and non- |
	 | promotions.                                     |
	 ----- --------------------------------------------*/
	historymovestable[(int)Color][move.from_square]
	  [move.to_square] += (1 << depth);
	for (i = tried_legal_moves - 2; i >= 0; i--) {
	  if (!(searched_moves[i].type & CAPTURE_MOVE)
	      && !(searched_moves[i].type & PROMOTION_MOVE)) {
	    failed_cutoff_count
	      [(int)Color]
	      [board->boardarr[searched_moves[i].from_square]]
	      [searched_moves[i].to_square] += 1;
	  }
	}
	success_cutoff_count
	  [(int)Color]
	  [board->boardarr[move.from_square]]
	  [move.to_square] += 1;

	cutoffs++;
	if (cutcount == 1)
	  cutoffs_first++;
	/*--------------------------------------------------------------
	 | Add this move as a killer move, but make sure that we don't |
	 | pollute the killer slots with good captures or promotions.  |
	 --------------------------------------------------------------*/
	if (order > 7) {
	  /*for (i = 1; i < NBR_KILLERS; i++)
	    killers[depth-own_king_check][i] =
	    killers[depth-own_king_check][i-1];*/
	  killers[ply][best].fsquare =
	    square[move.from_square];
	  killers[ply][best].tsquare =
	    square[move.to_square];
	  killers[ply][best].value = retval;
	}
	record_refutation((char)depth,board,&move,BETA,ply,retval);
	record_transposition((char)depth,board,&move,1,BETA,ply,retval);
	return retval;
      }
    }
  }

  /*-----------------------------------------------------------------
   | If all moves were (futility) pruned, but there are still legal |
   | moves, then we need to give a correct value to retval.         |
   -----------------------------------------------------------------*/
  if (legal_moves > 0 && tried_legal_moves == 0) {
    //TODO: See if it's better to call quiescence here.
    retval = eval(board,alpha,beta);
  }

  /*---------------------------------------------------------------------
   | If we get here, and retval == -INFTY, it means all possible moves  |
   | from this position leads to a check position. If we are already in |
   | check, then it's mate. If we're not in check, then it's stalemate. |
   ---------------------------------------------------------------------*/
  if (legal_moves == 0) {
    if (own_king_check) {
      best_value = -INFTY + ply; //quick mates are better
    } else
      best_value = draw_score(); //stalemate
    record_transposition((char)depth,board,best_move,0,EXACT,ply,best_value);
  } else {
    if (best_value != -INFTY) {
      best_value = retval;
      record_transposition((char)depth,board,best_move,1,hashf,ply,best_value);
    } else {
      best_value = retval;
      record_transposition((char)depth,board,best_move,0,hashf,ply,best_value);
    }
  }
  return best_value;
}
