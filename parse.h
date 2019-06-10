#ifndef _PARSE_
#define _PARSE_

#include "misc.h"
#include "bitboards.h"

#define HUMAN 1
#define COMPUTER 2
#define WHITE 0
#define BLACK 1
#define BOOKSIZE 3144

#define NORMAL_MODE 0
#define XBOARD_MODE 1
#ifdef AM_DEBUG
#define DEBUG_MODE 2
#endif

/*----------------------------------
 | There are three types of moves: |
 | 1.) Normal moves                |
 | 2.) Find a move to ponder       |
 | 3.) Ponder                      |
 ----------------------------------*/
#define MOVETYPE_NORMAL 1
#define MOVETYPE_FIND_MOVE_TO_PONDER 2
#define MOVETYPE_PONDER 3

#define NORMAL_SEARCH 1
#define FINDING_MOVE_TO_PONDER 2
#define PONDERING 4
#define NO_SEARCH 8
#define OPPONENT_PLAYED_UNPREDICTED_MOVE 16
#define PENDING_INPUT 32
#define BEST_MOVE_FOUND 64

struct epd_operation {
  char *op_code;
  int nbr_operands;
  char **operand;
};

struct epd_operations {
  int nbr_operations;
  struct epd_operation *operation;
};

struct history_entry {
  struct board board;
  struct move move;
  int nullmove;
};

struct repetition_entry {
  int64 zobrist_key;
  char moves_left_to_draw;
};

int change_historylist_size(struct history_entry **list, int new_size);
int change_repetitionlist_size(struct repetition_entry **list, int new_size);
void move2history(struct board *board, struct move *move, int nullmove);
int is_draw_by_repetition(struct board *board);
void preanalysis(struct board *brd, struct move_list_entry **movelist,
		 int *mcount, int pondering);
int postanalysis(struct board *board, struct move_list_entry **movelist,
		 int mcount, struct move *move, int to_search_depth,
		 int pondering, int last_iter_nbr_nodes,
		 int second_last_iter_nbr_nodes);
int bookmove(int *in_book, struct board *l_board, struct move *move);
void computer_make_move(struct board **board, int *started, int max_depth,
			int *book, int pondering, int *wait_for_oppmove);

/* Initializes the board to a position described by a FEN string. */
void set_fen_board(struct board *lboard, const char *fen);

/* Initializes the board to a position described by a EPD string.
   EPD strings have the first four fields in common with a
   FEN string. On error NULL is returned. */
struct epd_operations *set_epd_board(struct board *lboard, char *epd);

/* This function parses the operations part of an EPD entry. The
   parser can parse also entries that don't totally follow the
   EPD format specification. On error NULL is returned. */
struct epd_operations *get_epd_operations(struct board *board, char *row);

/* This function frees the memory of a "struct epd_operations" object. */
void free_epd_operations(struct epd_operations *epd_ops);

int moves_equal(struct move *move1, struct move *move2);

int run_perft(struct board **board, int depth);

void testsuite(struct board *board, char *input);

/* This function converts a SAN move to a struct move-object. If the string
   is unparsable or illegal, 0 will be returned, otherwise 1. */
int san2move(char *which_move, struct board *board, struct move *move);

/* This function converts a string to a struct move-object. If the string
   is parsable 1 is returned, otherwise 0. */
int str2move(char *which_move, struct board *board, struct move *move);

void move2str(struct move *move, char *str);

int parsemove(char *input, struct board **board, int *started,
	      int *wait_for_oppmove);

void parse(void);

#endif      //_PARSE_
