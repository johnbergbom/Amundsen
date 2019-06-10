/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "eval.h"
#include "parse.h"
#include "bitboards.h"
#include "hash.h"
#include "alfabeta.h"
#include "swap.h"
#include <stdlib.h>

/* If endgame == 0 there is no special endgame. Otherwise the number tells us
   what kind of endgame we are dealing with. See eval.h for endgame types. */
int endgame = 0;

/* Before the search the material values of the pieces and pawns are
   calculated. That way it's possible to see if any pieces or pawns have
   been traded. This is used to encourage piece exchanges when ahead, and
   pawn exchanges when behind. */
int presearch_pieceval[2];
int presearch_pawnval[2];

struct pawn_entry pawn_struct;

extern bitboard col_bitboard[8];
extern bitboard adj_cols_not_self[8];
extern bitboard adj_cols[8];
extern int row_nbr[64];
extern int connected_bits[256];
extern struct defense defense;
extern bitboard possible_pawn_attack[2][64];
extern bitboard right_col[8];
extern bitboard left_col[8];
extern bitboard square[64];
extern int dist_to_corn[64];
extern bitboard pawn_start[2];
extern bitboard pawn_lastrow[2];
extern int dist_to_side[64];
extern bitboard white_squares;
extern bitboard black_squares;
extern bitboard abcd_col;
extern bitboard efgh_col;
extern bitboard abc_col;
extern bitboard fgh_col;
extern bitboard ab_col;
extern bitboard gh_col;
extern int dist_to_square[64][64];
extern struct attack attack;
extern int square2col[64];
extern int square2row[64];
extern bitboard d3d4d5d6;
extern bitboard e3e4e5e6;
extern bitboard pawn_duo[64];
extern bitboard passed_pawn[2][64];
extern int king_safety[16][16];
extern int ones[9];
extern int rotate0toNE[64];
extern int diagNE_start[64];
extern int diagNE_length[64];
extern int direction[64][64];
extern int rotate0toNW[64];
extern int diagNW_start[64];
extern int diagNW_length[64];
extern int rotate0to90[64];
extern bitboard horiz_vert_cross[64];
extern bitboard nw_ne_cross[64];

static const int king_reach_early[64] = {
    5,   6,  -2,  -6,  -6,  -2,   6,   5,
   -8, -10, -14, -16, -16, -14, -10,  -8,
  -10, -15, -18, -20, -20, -18, -15, -10,
  -14, -20, -25, -30, -30, -25, -20, -14,
  -14, -20, -25, -30, -30, -25, -20, -14,
  -10, -15, -18, -20, -20, -18, -15, -10,
   -8, -10, -14, -16, -16, -14, -10,  -8,
    5,   6,  -2,  -6,  -6,  -2,   6,   5
};

static const unsigned king_reach_late[64] = {
  3, 5, 5, 5, 5, 5, 5, 3,
  5, 8, 8, 8, 8, 8, 8, 5,
  5, 8, 8, 8, 8, 8, 8, 5,
  5, 8, 8, 8, 8, 8, 8, 5,
  5, 8, 8, 8, 8, 8, 8, 5,
  5, 8, 8, 8, 8, 8, 8, 5,
  5, 8, 8, 8, 8, 8, 8, 5,
  3, 5, 5, 5, 5, 5, 5, 3
};

static const unsigned w_king_reach_endgame_normal[64] = {
  -40, -20, -20, -20, -20, -20, -20, -40,
  -20,  30,  40,  40,  40,  40,  30, -20,
  -20,  30,  60,  60,  60,  60,  30, -20,
  -20,  30,  60,  60,  60,  60,  30, -20,
  -20,  20,  40,  40,  40,  40,  20, -20,
  -20,  20,  20,  20,  20,  20,  20, -20,
  -20,   0,   0,   0,   0,   0,   0, -20,
  -40, -20, -20, -20, -20, -20, -20, -40
};

static const unsigned b_king_reach_endgame_normal[64] = {
  -40, -20, -20, -20, -20, -20, -20, -40,
  -20,   0,   0,   0,   0,   0,   0, -20,
  -20,  20,  20,  20,  20,  20,  20, -20,
  -20,  20,  40,  40,  40,  40,  20, -20,
  -20,  30,  60,  60,  60,  60,  30, -20,
  -20,  30,  60,  60,  60,  60,  30, -20,
  -20,  30,  40,  40,  40,  40,  30, -20,
  -40, -20, -20, -20, -20, -20, -20, -40
};


static const unsigned w_king_reach_endgame_qside[64] = {
  -20, -20, -20, -20, -20, -20, -40, -60,
   40,  40,  40,  40,  20, -20, -40, -60,
   40,  60,  60,  60,  20, -20, -40, -60,
   40,  60,  60,  60,  20, -20, -40, -60,
   40,  40,  40,  40,  20, -20, -40, -60,
   20,  20,  20,  20,  20, -20, -40, -60,
    0,   0,   0,   0,   0, -20, -40, -60,
  -20, -20, -20, -20, -20, -20, -40, -60
};

static const unsigned b_king_reach_endgame_qside[64] = {
  -20, -20, -20, -20, -20, -20, -40, -60,
    0,   0,   0,   0,   0, -20, -40, -60,
   20,  20,  20,  20,  20, -20, -40, -60,
   40,  40,  40,  40,  20, -20, -40, -60,
   40,  60,  60,  60,  20, -20, -40, -60,
   40,  60,  60,  60,  20, -20, -40, -60,
   40,  40,  40,  40,  20, -20, -40, -60,
  -20, -20, -20, -20, -20, -20, -40, -60
};

static const unsigned w_king_reach_endgame_kside[64] = {
  -60, -40, -20, -20, -20, -20, -20, -20,
  -60, -40, -20,  20,  40,  40,  40,  40,
  -60, -40, -20,  20,  60,  60,  60,  40,
  -60, -40, -20,  20,  60,  60,  60,  40,
  -60, -40, -20,  20,  40,  40,  40,  40,
  -60, -40, -20,  20,  20,  20,  20,  20,
  -60, -40, -20,   0,   0,   0,   0,   0,
  -60, -40, -20, -20, -20, -20, -20, -20
};

static const unsigned b_king_reach_endgame_kside[64] = {
  -60, -40, -20, -20, -20, -20, -20, -20,
  -60, -40, -20,   0,   0,   0,   0,   0,
  -60, -40, -20,  20,  20,  20,  20,  20,
  -60, -40, -20,  20,  40,  40,  40,  40,
  -60, -40, -20,  20,  60,  60,  60,  40,
  -60, -40, -20,  20,  60,  60,  60,  40,
  -60, -40, -20,  20,  40,  40,  40,  40,
  -60, -40, -20, -20, -20, -20, -20, -20
};

static const unsigned queen_posval[64] = {
  -20, -20,   0,   0,   0,   0, -20, -20,
  -20,   0,   8,   8,   8,   8,   0, -20,
    0,   8,   8,  12,  12,   8,   8,   0,
    0,   8,  12,  16,  16,  12,   8,   0,
    0,   8,  12,  16,  16,  12,   8,   0,
    0,   8,   8,  12,  12,   8,   8,   0,
  -20,   0,   8,   8,   8,   8,   0, -20,
  -20, -20,   0,   0,   0,   0, -20, -20
};

static const unsigned rook_posval[64] = {
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10,
  -10, -6, -2, 2, 2, -2, -6, -10
};

static const unsigned bishop_posval[64] = {
  -20, -20, -20, -20, -20, -20, -20, -20,
  -20,   6,   6,   3,   3,   6,   6, -20,
  -20,   6,   8,   6,   6,   8,   6, -20,
  -20,   3,   6,  10,  10,   6,   3, -20,
  -20,   3,   6,  10,  10,   6,   3, -20,
  -20,   6,   8,   6,   6,   8,   6, -20,
  -20,   6,   6,   3,   3,   6,   6, -20,
  -20, -20, -20, -20, -20, -20, -20, -20
};

static const unsigned knight_posval[64] = {
  -60, -30, -30, -30, -30, -30, -30, -60,
  -30, -24, -10, -10, -10, -10, -24, -30,
  -30,  -6,  -6,  -6,  -6,  -6,  -6, -30,
  -30,  -6,  -2,   0,   0,  -2,  -6, -30,
  -30,  -6,  -2,   0,   0,  -2,  -6, -30,
  -30,  -6,  -6,  -6,  -6,  -6,  -6, -30,
  -30, -24, -10, -10, -10, -10, -24, -30,
  -60, -30, -30, -30, -30, -30, -30, -60
};

static const unsigned white_outpost[64] = {
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0, 10, 24, 24, 10, 0, 0,
  0, 5, 10, 24, 24, 10, 5, 0,
  0, 5, 10, 20, 20, 10, 5, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0
};

static const unsigned black_outpost[64] = {
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 5, 10, 20, 20, 10, 5, 0,
  0, 5, 10, 24, 24, 10, 5, 0,
  0, 0, 10, 24, 24, 10, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0,
  0, 0,  0,  0,  0,  0, 0, 0
};

static const unsigned pawn_posval[2][64] = {
  {  0,  0,   0,   0,   0,  0,  0,  0,
    40, 40,  40,  40,  40, 40, 40, 40,
    10, 10,  10,  30,  30, 10, 10, 10,
     6,  6,   6,  16,  16,  6,  6,  6,
     3,  3,   3,  13,  13,  3,  3,  3,
     1,  1,   1,  10,  10,  1,  1,  1,
     0,  0,   0, -10, -10,  0,  0,  0,
     0,  0,   0,   0,   0,  0,  0,  0 },
  {  0,  0,   0,   0,   0,  0,  0,  0,
     0,  0,   0, -10, -10,  0,  0,  0,
     1,  1,   1,  10,  10,  1,  1,  1,
     3,  3,   3,  13,  13,  3,  3,  3,
     6,  6,   6,  16,  16,  6,  6,  6,
    10, 10,  10,  30,  30, 10, 10, 10,
    40, 40,  40,  40,  40, 40, 40, 40,
     0,  0,   0,   0,   0,  0,  0,  0 }
};

int isolated_pawn_val[8] = { 8, 20, 40, 60, 70, 80, 80, 80 };
int isolated_pawn_val2[8] = { 4, 10, 16, 24, 24, 24, 24, 24 };
int passed_pawn_val[2][8] = {
  { 0, 150, 120, 72, 48, 20, 12, 0 },
  { 0, 12, 20, 48, 72, 120, 150, 0 }
};
int connected_passed_pawn_val[2][8] = {
  { 0, 200, 100, 40, 20, 10, 0, 0 },
  { 0, 0, 10, 20, 40, 100, 200, 0 }
};
int hidden_passed_pawn_val[2][8] = {
  { 0, 0, 40, 20, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 20, 40, 0, 0 }
};
int blocking_passed_pawn_val[2][8] = {
  { 0, 75, 60, 36, 24, 10, 6, 0 },
  { 0, 6, 10, 24, 36, 60, 75, 0 }
};

/* The first dimension tells the number of open files,
   and the second dimension tells the file in question.
   The value tells the reward. Eg. rook_open_file[2][3] = 26
   tells that if there are 2 open files and the rook is on
   the D file, then the reward is 26. Note that the more
   central the file is the higher the reward, and the fewer
   open files there are the more important it is to control
   one (=higher reward). */
static const unsigned rook_open_file[9][8] = {
  {  0,  0,  0,  0,  0,  0,  0,  0 },
  { 20, 27, 35, 40, 40, 35, 27, 20 },
  { 13, 18, 23, 26, 26, 23, 18, 13 },
  { 10, 13, 17, 20, 20, 17, 13, 10 },
  { 10, 13, 17, 20, 20, 17, 13, 10 },
  { 10, 13, 17, 20, 20, 17, 13, 10 },
  { 10, 13, 17, 20, 20, 17, 13, 10 },
  { 10, 13, 17, 20, 20, 17, 13, 10 },
  { 10, 13, 17, 20, 20, 17, 13, 10 }
  };
/*static const unsigned rook_open_file[9][8] = {
  {  0,  0,  0,  0,  0,  0,  0,  0 },
  { 16, 21, 28, 32, 32, 28, 21, 16 },
  { 10, 14, 18, 20, 20, 18, 14, 10 },
  {  8, 10, 13, 16, 16, 13, 10,  8 },
  {  8, 10, 13, 16, 16, 13, 10,  8 },
  {  8, 10, 13, 16, 16, 13, 10,  8 },
  {  8, 10, 13, 16, 16, 13, 10,  8 },
  {  8, 10, 13, 16, 16, 13, 10,  8 },
  {  8, 10, 13, 16, 16, 13, 10,  8 }
  };*/

int tropism_knight[8] = { 0, 3, 3, 2, 1, 0, 0, 0 };
int tropism_bishop[8] = { 0, 2, 2, 1, 0, 0, 0, 0 };
int tropism_rook[8] = { 0, 3, 2, 1, 0, 0, 0, 0 };
int tropism_at_rook[8] = { 3, 2, 1, 0, 0, 0, 0, 0 };
int tropism_queen[8] = { 0, 4, 3, 2, 1, 0, 0, 0 };
int tropism_at_queen[8] = { 3, 2, 1, 0, 0, 0, 0, 0 };
int queen_scale_tropism[16] = { 0, 0, 1, 2, 3, 3, 4, 4,
				5, 6, 6, 7, 8, 9, 10, 10};
int queen_scale_safety[16] = { 0, 0, 1, 2, 3, 3, 4, 4, 5,
			       6, 6, 7, 8, 9, 10, 10};
int open_file_defect[8] = {6, 5, 4, 4, 4, 4, 5, 6};
int half_open_file_defect[8] = {4, 4, 3, 3, 3, 3, 4, 4};
int pawn_ending_adjustment[9] = {0, 0, 100, 200, 300, 400, 500, 600, 700};

/* Mobility stuff. */
int white_mobility;
int black_mobility;

/* Original according to Schröder. */
/*int mobility_table[170]= {   0,  2,  4,  6,  8, 10, 12, 14, 16,
			    18, 20, 22, 26, 30, 34, 38, 44, 50,
			    56, 62, 70, 78, 86, 94,102,110,118,
			   126,136,144,152,160,168,176,184,192,
			   200,208,216,224,232,240,248,256,264,
			   272,280,288,296,304,312,320,328,336,
			   344,352,360,368,376,384,392,400,408,
			   416,424,432,440,448,456,464,472,480,
			   488,496,504,512,520,528,536,544,552,
			   560,568,576,584,592,600,608,616,624,
			   632,640,648,656,664,672,680,688,696,
			   704,712,720,728,736,744,752,760,768,
			   776,784,792,800,808,816,824,832,840,
			   848,856,864,872,880,888,896,904,912,
			   920,928,936,944,952,960,968,976,984,
			   985,986,987,988,989,990,991,992,993,
			   994,995,996,997,998,999,999,999,999,
			   999,999,999,999,999,999,999,999,999,
			   999,999,999,999,999,999,999,999 };*/

/* Scaling can be done with the following command:
   for var in `cat m|sed 's/\t//g'|sed 's/ //g'| sed 's/,/ /g'`; do let var=($var*100)/256; echo $var; done|    tr '\n' ' '

*/

/* Scaled with /2.56. */
/*int mobility_table[170]= { 0,0,1,2,3,3,4,5,6,
			   7,7,8,10,11,13,14,17,19,
			   21,24,27,30,33,36,39,42,46,
			   49,53,56,59,62,65,68,71,75,
			   78,81,84,87,90,93,96,100,103,
			   106,109,112,115,118,121,125,128,131,
			   134,137,140,143,146,150,153,156,159,
			   162,165,168,171,175,178,181,184,187,
			   190,193,196,200,203,206,209,212,215,
			   218,221,225,228,231,234,237,240,243,
			   246,250,253,256,259,262,265,268,271,
			   275,278,281,284,287,290,293,296,300,
			   303,306,309,312,315,318,321,325,328,
			   331,334,337,340,343,346,350,353,356,
			   359,362,365,368,371,375,378,381,384,
			   384,385,385,385,386,386,387,387,387,
			   388,388,389,389,389,390,390,390,390,
			   390,390,390,390,390,390,390,390,390,
			   390,390,390,390,390,390,390,390 };*/

/* Scaled with /2.00. */
/*int mobility_table[170]= { 0,1,2,3,4,5,6,7,8,9,10,11,13,15,17,19,22,25,28,31,35,39,43,47,51,55,59,63,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232,236,240,244,248,252,256,260,264,268,272,276,280,284,288,292,296,300,304,308,312,316,320,324,328,332,336,340,344,348,352,356,360,364,368,372,376,380,384,388,392,396,400,404,408,412,416,420,424,428,432,436,440,444,448,452,456,460,464,468,472,476,480,484,488,492,492,493,493,494,494,495,495,496,496,497,497,498,498,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499,499 };*/

/* Scaled with /3.00. */
/*int mobility_table[170]= { 0,0,1,2,2,3,4,4,5,6,6,7,8,10,11,12,14,16,18,20,23,26,28,31,34,36,39,42,45,48,50,53,56,58,61,64,66,69,72,74,77,80,82,85,88,90,93,96,98,101,104,106,109,112,114,117,120,122,125,128,130,133,136,138,141,144,146,149,152,154,157,160,162,165,168,170,173,176,178,181,184,186,189,192,194,197,200,202,205,208,210,213,216,218,221,224,226,229,232,234,237,240,242,245,248,250,253,256,258,261,264,266,269,272,274,277,280,282,285,288,290,293,296,298,301,304,306,309,312,314,317,320,322,325,328,328,328,329,329,329,330,330,330,331,331,331,332,332,332,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333,333 };*/

static const unsigned kbnk_white_bishop[64] = {
  0, 40, 70, 100, 120, 140, 150, 150,
  40, 70, 100, 130, 210, 190, 160, 150,
  70, 100, 130, 160, 230, 210, 190, 140,
  100, 130, 170, 200, 250, 230, 210, 120,
  120, 210, 230, 250, 200, 170, 130, 100,
  140, 190, 210, 230, 160, 130, 100, 70,
  150, 160, 190, 210, 130, 100, 70, 40,
  150, 150, 140, 120, 100, 70, 40, 0
};

static const unsigned kbnk_black_bishop[64] = {
  150, 150, 140, 120, 100, 70, 40, 0,
  150, 160, 190, 210, 130, 100, 70 , 40,
  140, 190, 210, 230, 160, 130, 100, 70,
  120, 210, 230, 250, 200, 170, 130, 100,
  100, 130, 170, 200, 250, 230, 210, 120,
  70, 100, 130, 160, 230, 210, 190, 140,
  40, 70, 100, 130, 210, 190, 160, 150,
  0, 40, 70, 100, 120, 140, 150, 150
};


void eval_pawns(struct board *board, int *wval, int *bval) {
  int i;
  unsigned char connected[2] = { 0, 0 };
  bitboard pawn_col, pieces, temp_bitboard;
  int counter;
  bitboard advanced_white_d_pawns, advanced_white_e_pawns;
  bitboard advanced_black_d_pawns, advanced_black_e_pawns;
  int sq, temp;
  int isolated_pawns_white, isolated_pawns_black;
  int isolated_pawns_white2, isolated_pawns_black2;
  int attackers, defenders, backing_attackers, backing_defenders, weakness;

 if (probe_pawn_hash(board,&pawn_struct) == UNKNOWN) {
    pawn_struct.white_value = pawn_struct.black_value = 0;
    pawn_struct.passed_pawn[0] = pawn_struct.passed_pawn[1]
      = pawn_struct.weak_pawn[0] = pawn_struct.weak_pawn[1]
      = pawn_struct.pawn_hole[0] = pawn_struct.pawn_hole[1]
      = pawn_struct.half_open_file[0] = pawn_struct.half_open_file[1]
      = pawn_struct.double_isolated_pawn[WHITE]
      = pawn_struct.double_isolated_pawn[BLACK]
      = pawn_struct.open_file = pawn_struct.center
      = isolated_pawns_white = isolated_pawns_black
      = isolated_pawns_white2 = isolated_pawns_black2 = 0;
    
    
    /*---------------------------------------------------------
     | Determine how blocked the center is.                   |
     | 0.) Blocked                                            |
     | 1.) Half-open: One of the front pawns on the D or E    |
     | file has no opposing pawn in front of itself, or the   |
     | front pawns on the D and E files are on the same rank, |
     | or one player has no advanced pawns.                   |
     | 2.) Open: at most one pawn on the D/E files.           |
     ---------------------------------------------------------*/
    if (bitcount((col_bitboard[3] | col_bitboard[4]) &
		 (board->piece[WHITE][PAWN] | board->piece[BLACK][PAWN])) <= 1)
      pawn_struct.center = 2;
    else {
      advanced_white_d_pawns = board->piece[WHITE][PAWN] & d3d4d5d6;
      advanced_white_e_pawns = board->piece[WHITE][PAWN] & e3e4e5e6;
      advanced_black_d_pawns = board->piece[BLACK][PAWN] & d3d4d5d6;
      advanced_black_e_pawns = board->piece[BLACK][PAWN] & e3e4e5e6;
      if (!(((d3d4d5d6 & board->piece[WHITE][PAWN]) >> 8)
	    & board->piece[BLACK][PAWN]) ||
	  !(((e3e4e5e6 & board->piece[WHITE][PAWN]) >> 8)
	    & board->piece[BLACK][PAWN]) ||
	  (advanced_white_d_pawns == 0 && advanced_white_e_pawns == 0) ||
	  (advanced_black_d_pawns == 0 && advanced_black_e_pawns == 0) ||
	  (advanced_white_d_pawns != 0 && advanced_white_e_pawns != 0
	   && square2row[get_first_bitpos(advanced_white_d_pawns)]
	   == square2row[get_first_bitpos(advanced_white_e_pawns)]) ||
	  (advanced_black_d_pawns != 0 && advanced_black_e_pawns != 0
	   && square2row[get_first_bitpos(advanced_black_d_pawns)]
	   == square2row[get_first_bitpos(advanced_black_e_pawns)]))
	pawn_struct.center = 1;
    }

    /*--------------------------------------------------------------------
     | Check for unmoved center pawns that are blocked by an enemy pawn. |
     --------------------------------------------------------------------*/
    if (((board->piece[WHITE][PAWN] & pawn_start[WHITE]) >> 8)
	& board->piece[BLACK][PAWN] & (col_bitboard[3] | col_bitboard[4]))
      pawn_struct.white_value -= 12;
    if (((board->piece[BLACK][PAWN] & pawn_start[BLACK]) << 8)
	& board->piece[WHITE][PAWN] & (col_bitboard[3] | col_bitboard[4]))
      pawn_struct.black_value -= 12;


    /*-----------------------------------------
     | Go through the white pawns one by one. |
     -----------------------------------------*/
    pieces = board->piece[WHITE][PAWN];
    while (pieces != 0) {
      sq = get_first_bitpos(pieces);

      /* Check pawn's placement. */
      pawn_struct.white_value += pawn_posval[WHITE][sq];

      /*-------------------------------------------------------------------
       | Is pawn isolated? If isolated, then don't do weak pawn analysis. |
       -------------------------------------------------------------------*/
      if (!(adj_cols_not_self[square2col[sq]] & board->piece[WHITE][PAWN])) {
	isolated_pawns_white++;
	/* Doubled isolated pawn? */
	if ((temp = bitcount(col_bitboard[square2col[sq]]
			     & board->piece[WHITE][PAWN])) > 1) {
	  pawn_struct.double_isolated_pawn[WHITE] =
	    col_bitboard[square2col[sq]] & board->piece[WHITE][PAWN];
	  if (temp == 2)
	    pawn_struct.white_value -= -10;
	  else
	    pawn_struct.white_value -= -15;
	}
	/* No opponent pawn right in front? */
	if (!((square[sq] >> 8) & board->piece[BLACK][PAWN]))
	  isolated_pawns_white2++;
      } else {
	/*------------------------------------------------------------------
	 | Do weak pawn analysis. If the pawn has an enemy pawn in front   |
	 | of it, or if it has more pawn defenders than pawn attackers,    |
	 | then it's considered strong. If it can get more pawn defenders  |
	 | than pawn attackers either by advancing itself or by advancing  |
	 | some other pawn, then it's considered strong. If the best thing |
	 | that can be achieved is an equal amount of pawn defenders and   |
	 | pawn attackers, then the pawn is possibly weak. In all other    |
	 | cases the pawn is considered weak.                              |
	 ------------------------------------------------------------------*/
	do {
	  weakness = STRONG_PAWN;
	  /* If an opposite colored pawn is in front of the pawn, then
	     the pawn is not weak. */
	  if (get_ray(sq,-8) & board->piece[BLACK][PAWN])
	    break;
	  defenders = bitcount(attack.pawn[BLACK][sq]
			       & board->piece[WHITE][PAWN]);
	  attackers = bitcount(attack.pawn[WHITE][sq]
			       & board->piece[BLACK][PAWN]);
	  if (defenders > attackers)
	    break;
	  else if (defenders == 0 && attackers == 0) {
	    weakness = LITTLE_WEAK_PAWN;
	    break;
	  } else if (defenders == attackers)
	    weakness = POSSIBLY_WEAK_PAWN;
	  else {
	    weakness = WEAK_PAWN;
	    //break;
	  }
	  /* If the pawn can advance forward and the pawn is defended
	     by more pawns than the number of pawns attacking (at the
	     new square), then the pawn is strong. */
	  backing_defenders = bitcount(attack.pawn[BLACK][sq-8]
				       & board->piece[WHITE][PAWN]);
	  backing_attackers = bitcount(attack.pawn[WHITE][sq-8]
				       & board->piece[BLACK][PAWN]);
	  if (backing_defenders >= backing_attackers) {
	    if (weakness == WEAK_PAWN) {
	      weakness = LITTLE_WEAK_PAWN; //weak but can get strong
	      break;
	    } else if (weakness == POSSIBLY_WEAK_PAWN) {
	      weakness = STRONG_PAWN;
	      break;
	    }
	  }
	  if (square2row[sq] == 6
	      && !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		   & square[sq-16])) {
	    backing_defenders = bitcount(attack.pawn[BLACK][sq-16]
					 & board->piece[WHITE][PAWN]);
	    backing_attackers = bitcount(attack.pawn[WHITE][sq-16]
					 & board->piece[BLACK][PAWN]);
	    if (backing_defenders >= backing_attackers) {
	      if (weakness == WEAK_PAWN) {
		weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		break;
	      } else if (weakness == POSSIBLY_WEAK_PAWN) {
		weakness = STRONG_PAWN;
		break;
	      }
	    }
	  }

	  /* If another pawn can be advanced in order to defend the
	     pawn, then it's a strong pawn. */
	  if (square2col[sq] > 0) {
	    temp_bitboard = col_bitboard[square2col[sq]-1] & board->piece[WHITE][PAWN];
	    if ((square2row[sq] <= 4)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & square[sq+7])
		&& (temp_bitboard & square[sq+15])) {
	      backing_defenders = bitcount(attack.pawn[BLACK][sq+7]
					   & board->piece[WHITE][PAWN]);
	      backing_attackers = bitcount(attack.pawn[WHITE][sq+7]
					   & board->piece[BLACK][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	    if ((square2row[sq] == 3)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & (square[sq+7] | square[sq+15]))
		&& (temp_bitboard & square[sq+23])) {
	      backing_defenders = bitcount(attack.pawn[BLACK][sq+7]
					   & board->piece[WHITE][PAWN]);
	      backing_attackers = bitcount(attack.pawn[WHITE][sq+7]
					   & board->piece[BLACK][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	  }
	  if (square2col[sq] < 7) {
	    temp_bitboard = col_bitboard[square2col[sq]+1] & board->piece[WHITE][PAWN];
	    if ((square2row[sq] <= 4)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & square[sq+9])
		&& (temp_bitboard & square[sq+17])) {
	      backing_defenders = bitcount(attack.pawn[BLACK][sq+9]
					   & board->piece[WHITE][PAWN]);
	      backing_attackers = bitcount(attack.pawn[WHITE][sq+9]
					   & board->piece[BLACK][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	    if ((square2row[sq] == 3)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & (square[sq+9] | square[sq+17]))
		&& (temp_bitboard & square[sq+25])) {
	      backing_defenders = bitcount(attack.pawn[BLACK][sq+9]
					   & board->piece[WHITE][PAWN]);
	      backing_attackers = bitcount(attack.pawn[WHITE][sq+9]
					   & board->piece[BLACK][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	  }
	} while (0);

	/*-------------------------------------------------------------------
	 | Assign a penalty for weak pawns. Assign a greater penalty if the |
	 |  pawn is on a half open file.                                    |
	 -------------------------------------------------------------------*/
	if (weakness != STRONG_PAWN) {
	  if (weakness == LITTLE_WEAK_PAWN || weakness == POSSIBLY_WEAK_PAWN) {
	      if (!(col_bitboard[square2col[sq]] & board->piece[BLACK][PAWN]))
		pawn_struct.white_value -= 6;
	      else
		pawn_struct.white_value -= 3;
	  } else { //WEAK_PAWN
	    if (!(col_bitboard[square2col[sq]] & board->piece[BLACK][PAWN]))
	      pawn_struct.white_value -= 20;
	    else
	      pawn_struct.white_value -= 12;
	    pawn_struct.weak_pawn[WHITE] |= col_bitboard[square2col[sq]];
	    pawn_struct.pawn_hole[WHITE] |= square[sq-8];
	  }
	}

	/* Check for doubled pawns. */
	temp = bitcount(col_bitboard[square2col[sq]]
			& board->piece[WHITE][PAWN]);
	if (temp > 1) {
	  if (temp == 2)
	    pawn_struct.white_value -= 4;
	  else if (temp == 3)
	    pawn_struct.white_value -= 7;
	  else
	    pawn_struct.white_value -= 10;
	}

	/* Check for two pawns side by side. */
	if (pawn_duo[sq] & board->piece[WHITE][PAWN])
	  pawn_struct.white_value += 2;

      }

      /*-----------------------------------------------------------
       | A passed pawn is given an added constant value + a bonus |
       | related to the number of rows it has advanced.           |
       -----------------------------------------------------------*/
      if (!(passed_pawn[WHITE][sq] & board->piece[BLACK][PAWN])) {
	if (get_last_bitpos(col_bitboard[square2col[sq]]
			    & board->piece[WHITE][PAWN]) == sq)
	  pawn_struct.white_value += passed_pawn_val[WHITE][square2row[sq]];
	pawn_struct.passed_pawn[WHITE] |= col_bitboard[square2col[sq]];
	connected[WHITE] |= (1<<(square2col[sq]));

	/* A passed pawn that's blocked by an enemy piece is less valuable. */
	if ((board->all_pieces[BLACK] & ~(board->piece[BLACK][PAWN]))
	    & square[sq-8]) {
	  pawn_struct.white_value -=
	    blocking_passed_pawn_val[WHITE][square2row[sq]];
	}
      } else {
	/*-----------------------------------------------------------------
	 | Ok, so now we know that the pawn isn't a passed pawn, so check |
	 | if it's a "hidden" passed pawn (a pawn that can easily turn    |
	 | in to a passed pawn).                                          |
	 -----------------------------------------------------------------*/
	if (!(passed_pawn[WHITE][sq-8] & board->piece[BLACK][PAWN])
	    && (square2row[sq] < 3)
	    && (square[sq-8] & board->piece[BLACK][PAWN])
	    && (((square2col[sq] < 7)
		 && (square[sq+7] & board->piece[WHITE][PAWN])
		 && !(get_ray(sq+7,-8) & board->piece[BLACK][PAWN])
		 && (square2col[sq] == 6
		     || !(get_ray(sq+6,-8) & board->piece[BLACK][PAWN])))
		|| ((square2col[sq] > 0)
		    && (square[sq+9] & board->piece[WHITE][PAWN])
		    && !(get_ray(sq+9,-8) & board->piece[BLACK][PAWN])
		    && (square2col[sq] == 1
			|| !(get_ray(sq+10,-8) & board->piece[BLACK][PAWN]))))) {
	  pawn_struct.white_value += hidden_passed_pawn_val[WHITE][square2row[sq]];
	}
      }

      pieces &= ~(square[sq]);
    }

    /*-----------------------------------------
     | Go through the black pawns one by one. |
     -----------------------------------------*/
    pieces = board->piece[BLACK][PAWN];
    while (pieces != 0) {
      sq = get_first_bitpos(pieces);

      /* Check pawn's placement. */
      pawn_struct.black_value += pawn_posval[BLACK][sq];

      /*-------------------------------------------------------------------
       | Is pawn isolated? If isolated, then don't do weak pawn analysis. |
       -------------------------------------------------------------------*/
      if (!(adj_cols_not_self[square2col[sq]] & board->piece[BLACK][PAWN])) {
	isolated_pawns_black++;
	/* Doubled isolated pawn? */
	if ((temp = bitcount(col_bitboard[square2col[sq]]
			     & board->piece[BLACK][PAWN])) > 1) {
	  pawn_struct.double_isolated_pawn[BLACK] =
	    col_bitboard[square2col[sq]] & board->piece[BLACK][PAWN];
	  if (temp == 2)
	    pawn_struct.black_value -= -10;
	  else
	    pawn_struct.black_value -= -15;
	}
	/* No opponent pawn right in front? TODO: Crafty har här snarare koll
	   av ifall det finns någon motståndarbonde framför på samma kolumn
	   (dvs. get_ray() snarare än square[]. */
	if (!((square[sq] >> 8) & board->piece[WHITE][PAWN]))
	  isolated_pawns_black2++;
      } else {
	/*------------------------------------------------------------------
	 | Do weak pawn analysis. If the pawn has an enemy pawn in front   |
	 | of it, or if it has more pawn defenders than pawn attackers,    |
	 | then it's considered strong. If it can get more pawn defenders  |
	 | than pawn attackers either by advancing itself or by advancing  |
	 | some other pawn, then it's considered strong. If the best thing |
	 | that can be achieved is an equal amount of pawn defenders and   |
	 | pawn attackers, then the pawn is possibly weak. In all other    |
	 | cases the pawn is considered weak.                              |
	 ------------------------------------------------------------------*/
	do {
	  weakness = STRONG_PAWN;
	  /* If an opposite colored pawn is in front of the pawn, then
	     the pawn is not weak. */
	  if (get_ray(sq,8) & board->piece[WHITE][PAWN])
	    break;
	  defenders = bitcount(attack.pawn[WHITE][sq]
			       & board->piece[BLACK][PAWN]);
	  attackers = bitcount(attack.pawn[BLACK][sq]
			       & board->piece[WHITE][PAWN]);
	  if (defenders > attackers)
	    break;
	  else if (defenders == 0 && attackers == 0) {
	    weakness = LITTLE_WEAK_PAWN;
	    break;
	  } else if (defenders == attackers)
	    weakness = POSSIBLY_WEAK_PAWN;
	  else {
	    weakness = WEAK_PAWN;
	    //break;
	  }
	  /* If the pawn can advance forward and the pawn is defended
	     by more pawns than the number of pawns attacking (at the
	     new square), then the pawn is strong. */
	  backing_defenders = bitcount(attack.pawn[WHITE][sq+8]
				       & board->piece[BLACK][PAWN]);
	  backing_attackers = bitcount(attack.pawn[BLACK][sq+8]
				       & board->piece[WHITE][PAWN]);
	  if (backing_defenders >= backing_attackers) {
	    if (weakness == WEAK_PAWN) {
	      weakness = LITTLE_WEAK_PAWN; //weak but can get strong
	      break;
	    } else if (weakness == POSSIBLY_WEAK_PAWN) {
	      weakness = STRONG_PAWN;
	      break;
	    }
	  }
	  if (square2row[sq] == 1
	      && !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		   & square[sq+16])) {
	    backing_defenders = bitcount(attack.pawn[WHITE][sq+16]
					 & board->piece[BLACK][PAWN]);
	    backing_attackers = bitcount(attack.pawn[BLACK][sq+16]
					 & board->piece[WHITE][PAWN]);
	    if (backing_defenders >= backing_attackers) {
	      if (weakness == WEAK_PAWN) {
		weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		break;
	      } else if (weakness == POSSIBLY_WEAK_PAWN) {
		weakness = STRONG_PAWN;
		break;
	      }
	    }
	  }

	  /* If another pawn can be advanced in order to defend the
	     pawn, then it's a strong pawn. */
	  if (square2col[sq] > 0) {
	    temp_bitboard = col_bitboard[square2col[sq]-1] & board->piece[BLACK][PAWN];
	    if ((square2row[sq] >= 3)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & square[sq-9])
		&& (temp_bitboard & square[sq-17])) {
	      backing_defenders = bitcount(attack.pawn[WHITE][sq-9]
					   & board->piece[BLACK][PAWN]);
	      backing_attackers = bitcount(attack.pawn[BLACK][sq-9]
					   & board->piece[WHITE][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	    if ((square2row[sq] == 4)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & (square[sq-9] | square[sq-17]))
		&& (temp_bitboard & square[sq-25])) {
	      backing_defenders = bitcount(attack.pawn[WHITE][sq-9]
					   & board->piece[BLACK][PAWN]);
	      backing_attackers = bitcount(attack.pawn[BLACK][sq-9]
					   & board->piece[WHITE][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	  }
	  if (square2col[sq] < 7) {
	    temp_bitboard = col_bitboard[square2col[sq]+1] & board->piece[BLACK][PAWN];
	    if ((square2row[sq] >= 3)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & square[sq-7])
		&& (temp_bitboard & square[sq-15])) {
	      backing_defenders = bitcount(attack.pawn[WHITE][sq-7]
					   & board->piece[BLACK][PAWN]);
	      backing_attackers = bitcount(attack.pawn[BLACK][sq-7]
					   & board->piece[WHITE][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	    if ((square2row[sq] == 4)
		&& !((board->all_pieces[WHITE] | board->all_pieces[BLACK])
		     & (square[sq-7] | square[sq-15]))
		&& (temp_bitboard & square[sq-23])) {
	      backing_defenders = bitcount(attack.pawn[WHITE][sq-7]
					   & board->piece[BLACK][PAWN]);
	      backing_attackers = bitcount(attack.pawn[BLACK][sq-7]
					   & board->piece[WHITE][PAWN]);
	      if (backing_defenders >= backing_attackers) {
		if (backing_defenders + defenders + 1 >
		    backing_attackers + attackers) {
		  if (weakness == WEAK_PAWN) {
		    weakness = LITTLE_WEAK_PAWN; //weak but can get strong
		    break;
		  } else if (weakness == POSSIBLY_WEAK_PAWN) {
		    weakness = STRONG_PAWN;
		    break;
		  }
		}
	      }
	    }
	  }
	} while (0);

	/*-------------------------------------------------------------------
	 | Assign a penalty for weak pawns. Assign a greater penalty if the |
	 |  pawn is on a half open file.                                    |
	 -------------------------------------------------------------------*/
	if (weakness != STRONG_PAWN) {
	  if (weakness == LITTLE_WEAK_PAWN || weakness == POSSIBLY_WEAK_PAWN) {
	      if (!(col_bitboard[square2col[sq]] & board->piece[WHITE][PAWN]))
		pawn_struct.black_value -= 6;
	      else
		pawn_struct.black_value -= 3;
	  } else { //WEAK_PAWN
	    if (!(col_bitboard[square2col[sq]] & board->piece[WHITE][PAWN]))
	      pawn_struct.black_value -= 20;
	    else
	      pawn_struct.black_value -= 12;
	    pawn_struct.weak_pawn[BLACK] |= col_bitboard[square2col[sq]];
	    pawn_struct.pawn_hole[BLACK] |= square[sq+8];
	  }
	}

	/* Check for doubled pawns. */
	temp = bitcount(col_bitboard[square2col[sq]]
			& board->piece[WHITE][PAWN]);
	if (temp > 1) {
	  if (temp == 2)
	    pawn_struct.white_value -= 4;
	  else if (temp == 3)
	    pawn_struct.white_value -= 7;
	  else
	    pawn_struct.white_value -= 10;
	}

	/* Check for two pawns side by side. */
	if (pawn_duo[sq] & board->piece[WHITE][PAWN])
	  pawn_struct.white_value += 2;

      }

      /*-----------------------------------------------------------
       | A passed pawn is given an added constant value + a bonus |
       | related to the number of rows it has advanced.           |
       -----------------------------------------------------------*/
      if (!(passed_pawn[BLACK][sq] & board->piece[WHITE][PAWN])) {
	if (get_first_bitpos(col_bitboard[square2col[sq]]
			     & board->piece[BLACK][PAWN]) == sq)
	  pawn_struct.black_value += passed_pawn_val[BLACK][square2row[sq]];
	pawn_struct.passed_pawn[BLACK] |= col_bitboard[square2col[sq]];
	connected[BLACK] |= (1<<(square2col[sq]));

	/* A passed pawn that's blocked by an enemy piece is less valuable. */
	if ((board->all_pieces[WHITE] & ~(board->piece[WHITE][PAWN]))
	    & square[sq+8]) {
	  pawn_struct.black_value -=
	    blocking_passed_pawn_val[BLACK][square2row[sq]];
	}
      } else {
	/*-----------------------------------------------------------------
	 | Ok, so now we know that the pawn isn't a passed pawn, so check |
	 | if it's a "hidden" passed pawn (a pawn that can easily turn    |
	 | in to a passed pawn).                                          |
	 -----------------------------------------------------------------*/
	if (!(passed_pawn[BLACK][sq+8] & board->piece[WHITE][PAWN])
	    && (square2row[sq] > 4)
	    && (square[sq+8] & board->piece[WHITE][PAWN])
	    && (((square2col[sq] < 7)
		 && (square[sq-9] & board->piece[BLACK][PAWN])
		 && !(get_ray(sq-9,8) & board->piece[WHITE][PAWN])
		 && (square2col[sq] == 6
		     || !(get_ray(sq-10,8) & board->piece[WHITE][PAWN])))
		|| ((square2col[sq] > 0)
		    && (square[sq-7] & board->piece[BLACK][PAWN])
		    && !(get_ray(sq-7,8) & board->piece[WHITE][PAWN])
		    && (square2col[sq] == 1
			|| !(get_ray(sq-6,8) & board->piece[WHITE][PAWN]))))) {
	  pawn_struct.black_value += hidden_passed_pawn_val[BLACK][square2row[sq]];
	}
      }

      pieces &= ~(square[sq]);
    }

    /* Now give a non-linear penalty for isolated pawns. */
    if (isolated_pawns_white > 0) {
      pawn_struct.white_value -= isolated_pawn_val[isolated_pawns_white];
      if (isolated_pawns_white2 > 0) {
	pawn_struct.white_value -= isolated_pawn_val2[isolated_pawns_white2];
      }
    }
    if (isolated_pawns_black > 0) {
      pawn_struct.black_value -= isolated_pawn_val[isolated_pawns_black];
      if (isolated_pawns_black2 > 0) {
	pawn_struct.black_value -= isolated_pawn_val2[isolated_pawns_black2];
      }
    }

    /*---------------------------------------------------------
     | Check for defects in the pawn chains. This information |
     | will later be used when calculating king safety.       |
     ---------------------------------------------------------*/
    pawn_struct.defects_k[WHITE] = eval_king_file(board,6,WHITE);
    pawn_struct.defects_k[BLACK] = eval_king_file(board,6,BLACK);
    pawn_struct.defects_q[WHITE] = eval_king_file(board,1,WHITE);
    pawn_struct.defects_q[BLACK] = eval_king_file(board,1,BLACK);
    pawn_struct.defects_d[WHITE] = eval_king_file(board,3,WHITE);
    pawn_struct.defects_d[BLACK] = eval_king_file(board,3,BLACK);
    pawn_struct.defects_e[WHITE] = eval_king_file(board,4,WHITE);
    pawn_struct.defects_e[BLACK] = eval_king_file(board,4,BLACK);

    /*----------------------------------------------
     | Check openings in the pawn chain for white. |
     ----------------------------------------------*/
    counter = 0;
    for (i = 0; i < 8; i++) {
      pawn_col = board->piece[WHITE][PAWN] & col_bitboard[i];
      if (pawn_col == 0) {
	counter++;
	if (board->piece[BLACK][PAWN] & col_bitboard[i]) {
	  pawn_struct.half_open_file[WHITE] |= col_bitboard[i];
	} else
	  pawn_struct.open_file |= col_bitboard[i];
      }
    }
    /*-----------------------------------------
     | It's bad to have no open files at all. |
     -----------------------------------------*/
    if (counter == 0)
      pawn_struct.white_value -= 20;

    /*----------------------------------------------
     | Check openings in the pawn chain for black. |
     ----------------------------------------------*/
    counter = 0;
    for (i = 0; i < 8; i++) {
      pawn_col = board->piece[BLACK][PAWN] & col_bitboard[i];
      if (pawn_col == 0) {
	counter++;
	if (board->piece[WHITE][PAWN] & col_bitboard[i]) {
	  pawn_struct.half_open_file[BLACK] |= col_bitboard[i];
	}
      }
    }
    /*-----------------------------------------
     | It's bad to have no open files at all. |
     -----------------------------------------*/
    if (counter == 0)
      pawn_struct.black_value -= 20;


    /*--------------------------------------------------
     | Give a higher value for connected passed pawns. |
     --------------------------------------------------*/
    //TODO: The following if clause can possibly be replaced with:
    //if (connected_bits[connected[WHITE]]) {
    if (bitcount(connected[WHITE]) > 1) {
      if (bitcount(connected[WHITE] & (128+64)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[7] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[6] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (64+32)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[6] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[5] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (32+16)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[5] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[4] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (16+8)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[4] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[3] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (8+4)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[3] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[2] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (4+2)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[2] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[1] & board->piece[WHITE][PAWN])])];
      }
      if (bitcount(connected[WHITE] & (2+1)) == 2) {
	pawn_struct.white_value += connected_passed_pawn_val[WHITE]
	  [maxval(square2row[get_last_bitpos(col_bitboard[1] & board->piece[WHITE][PAWN])],
		  square2row[get_last_bitpos(col_bitboard[0] & board->piece[WHITE][PAWN])])];
      }
    }

    //TODO: The following if clause can possibly be replaced with:
    //if (connected_bits[connected[BLACK]]) {
    if (bitcount(connected[BLACK]) > 1) {
      if (bitcount(connected[BLACK] & (128+64)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[7] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[6] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (64+32)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[6] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[5] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (32+16)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[5] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[4] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (16+8)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[4] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[3] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (8+4)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[3] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[2] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (4+2)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[2] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[1] & board->piece[BLACK][PAWN])])];
      }
      if (bitcount(connected[BLACK] & (2+1)) == 2) {
	pawn_struct.black_value += connected_passed_pawn_val[BLACK]
	  [minval(square2row[get_first_bitpos(col_bitboard[1] & board->piece[BLACK][PAWN])],
		  square2row[get_first_bitpos(col_bitboard[0] & board->piece[BLACK][PAWN])])];
      }
    }


    /*---------------------------------------------------
     | Record the pawn structure in the pawn hashtable. |
     ---------------------------------------------------*/
    record_pawn_hash(board,&pawn_struct);

  }

  *wval += pawn_struct.white_value;
  *bval += pawn_struct.black_value;
}

int eval_king_file(struct board *board, int file, int color) {
  int defects = 0, f;
  //bitboard file_mask;
  
  if (color == WHITE) {
    for (f = file-1; f <= file+1; f++) {
      if (!(col_bitboard[f] & (board->piece[WHITE][PAWN]
			       | board->piece[BLACK][PAWN])))
	defects += open_file_defect[f];
      else {
	if (!(col_bitboard[f] & board->piece[BLACK][PAWN]))
	  defects += half_open_file_defect[f] / 2;
	else {
	  //More defects the closer the enemy pawn is
	  defects +=
	    maxval(0,(square2row[get_first_bitpos
				 (col_bitboard[f]
				  & board->piece[BLACK][PAWN])] - 2)) & 3;
	}
	if (!(col_bitboard[f] & board->piece[WHITE][PAWN]))
	  defects += half_open_file_defect[f];
	else {
	  //if (!((col_bitboard[f] & pawn_start[WHITE])
	  //    & board->piece[WHITE][PAWN])) {
	  if (!(square[48+f] & board->piece[WHITE][PAWN])) {
	    defects++;
	    if (!(square[40+f] & board->piece[WHITE][PAWN]))
	      defects++;
	  }
	}
      }
    }
  } else{
    for (f = file-1; f <= file+1; f++) {
      if (!(col_bitboard[f] & (board->piece[WHITE][PAWN]
			       | board->piece[BLACK][PAWN])))
	defects += open_file_defect[f];
      else {
	if (!(col_bitboard[f] & board->piece[WHITE][PAWN]))
	  defects += half_open_file_defect[f] / 2;
	else {
	  //More defects the closer the enemy pawn is
	  defects +=
	    maxval(0,(5-square2row[get_first_bitpos
				   (col_bitboard[f]
				    & board->piece[WHITE][PAWN])])) & 3;
	}
	if (!(col_bitboard[f] & board->piece[BLACK][PAWN]))
	  defects += half_open_file_defect[f];
	else {
	  if (!(square[8+f] & board->piece[BLACK][PAWN])) {
	    defects++;
	    if (!(square[16+f] & board->piece[BLACK][PAWN]))
	      defects++;
	  }
	}
      }
    }
  }
  
  return minval(defects,15);
}

/*-----------------------
 | Evaluate the rooks. |
 -----------------------*/
void eval_rooks(struct board *board, int *wval, int *bval) {
  bitboard pieces;
  char saved_color;
  int nbr_open_files, sq, trop, opp_king_sq, dist;

  nbr_open_files = bitcount(pawn_struct.open_file & pawn_start[BLACK]);
  saved_color = Color;

  /*---------------------------------
   | Iterate through white's rooks. |
   ---------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[BLACK][KING]);
  board->color_at_move = WHITE; //needed because generate_horizontal_moves used
  pieces = board->piece[WHITE][ROOK];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Check rook's placement. */
    *wval += rook_posval[sq];

    /*-----------------------------------------------------------------
     | Check if the rooks are behind a passed pawn and the pawn is    |
     | the only pawn on the file. Give a greater reward if it's a     |
     | passed pawn of own color, but also a little reward if the rook |
     | stands behind a passed pawn of enemy color.                    |
     -----------------------------------------------------------------*/
    if (square[sq] & pawn_struct.passed_pawn[WHITE]
	&& bitcount(board->piece[WHITE][PAWN]
		    & col_bitboard[square2col[sq]]
		    & pawn_struct.passed_pawn[WHITE]) == 1) {
      if (square2row[sq] >
	  square2row[get_first_bitpos(board->piece[WHITE][PAWN]
				      & col_bitboard[square2col[sq]]
				      & pawn_struct.passed_pawn[WHITE])]) {
	*wval += 24;
      }
    }
    if (square[sq] & pawn_struct.passed_pawn[BLACK]
	&& bitcount(board->piece[BLACK][PAWN]
		    & col_bitboard[square2col[sq]]
		    & pawn_struct.passed_pawn[BLACK]) == 1) {
      if (square2row[sq] <
	  square2row[get_last_bitpos(board->piece[BLACK][PAWN]
				     & col_bitboard[square2col[sq]]
				     & pawn_struct.passed_pawn[BLACK])])
	*wval += 5;
    }

    /*-------------------------------------------------------------------
     | If a rook is on the 7th rank and the opponent either has pawns   |
     | there, or else has the king trapped on the 8th rank, then that's |
     | rewarded, and if in addition to that there is more than one      |
     | rook on the 7th rank, then that's also rewarded.                 |
     -------------------------------------------------------------------*/
    if (square[sq] & pawn_start[BLACK]
	&& ((board->piece[BLACK][KING] & pawn_lastrow[WHITE])
	    || (board->piece[BLACK][PAWN] & pawn_start[BLACK]))) {
      *wval += 24;
      if (bitcount(board->piece[WHITE][ROOK] & pawn_start[BLACK]) > 1)
	*wval += 10;
    }

    /*------------------------------------------------------------------
     | If a weak pawn is on a half open file, then it's good to attack |
     | the weak pawn with a rook.                                      |
     ------------------------------------------------------------------*/
    if ((square[sq] & pawn_struct.weak_pawn[BLACK])
	&& (square[sq] & pawn_struct.half_open_file[WHITE])) {
      if (square2row[sq] >
	  square2row[get_last_bitpos(board->piece[BLACK][PAWN]
				     & col_bitboard[square2col[sq]])]) {
	*wval += 15;
      }
    }

    /*-------------------------------------------------------------
     | It's good to place rooks in open files. If the rook is not |
     | on an open file, but there are open files on the board,    |
     | then penalize the rook if it cannot reach any open file in |
     | one move. Give some points if the rook is on a half open   |
     | file (a file where there are no pawns of one's own color). |
     | The importance of an open rook file depends on the number  |
     | of pawns on the board - the more pawns the higher the      |
     | bonus.                                                     |
     -------------------------------------------------------------*/
    trop = 7;
    if (square[sq] & pawn_struct.open_file) {
      *wval += rook_open_file[nbr_open_files][square2col[sq]];
      trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
    } else {
      if (pawn_struct.open_file) {
	//TODO: Would it be faster to use get_ray() than
	//to use generate_horizontal_moves?
	if ((generate_horizontal_moves(board,sq)
	     & pawn_struct.open_file) == 0) {
	  *wval -= 16; //10;
	}
      }
      if (square[sq] & pawn_struct.half_open_file[WHITE]) {
	*wval += 10; //7;
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      } else if (!(get_ray(sq,-8) & board->piece[WHITE][PAWN])) {
	//no own pawns in front of the rook
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      }
    }

    /*----------------------------------
     | Check the tropism for the rook. |
     ----------------------------------*/
    if (board->dangerous[WHITE]) {
      dist = dist_to_square[sq][opp_king_sq];
      board->tropism[WHITE] += maxval(tropism_at_rook[trop],tropism_rook[dist]);
    }
  }

  /*---------------------------------
   | Iterate through black's rooks. |
   ---------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[WHITE][KING]);
  board->color_at_move = BLACK; //needed because generate_horizontal_moves used
  pieces = board->piece[BLACK][ROOK];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Check rook's placement. */
    *bval += rook_posval[sq];

    /*-----------------------------------------------------------------
     | Check if the rooks are behind a passed pawn and the pawn is    |
     | the only pawn on the file. Give a greater reward if it's a     |
     | passed pawn of own color, but also a little reward if the rook |
     | stands behind a passed pawn of enemy color.                    |
     -----------------------------------------------------------------*/
    if (square[sq] & pawn_struct.passed_pawn[BLACK]
	&& bitcount(board->piece[BLACK][PAWN]
		    & col_bitboard[square2col[sq]]
		    & pawn_struct.passed_pawn[BLACK]) == 1) {
      if (square2row[sq] <
	  square2row[get_last_bitpos(board->piece[BLACK][PAWN]
				     & col_bitboard[square2col[sq]]
				     & pawn_struct.passed_pawn[BLACK])])
	*bval += 24;
    }
    if (square[sq] & pawn_struct.passed_pawn[WHITE]
	&& bitcount(board->piece[WHITE][PAWN]
		    & col_bitboard[square2col[sq]]
		    & pawn_struct.passed_pawn[WHITE]) == 1) {
      if (square2row[sq] >
	  square2row[get_first_bitpos(board->piece[WHITE][PAWN]
				      & col_bitboard[square2col[sq]]
				      & pawn_struct.passed_pawn[WHITE])])
	*bval += 5;
    }
    
    /*-------------------------------------------------------------------
     | If a rook is on the 7th rank and the opponent either has pawns   |
     | there, or else has the king trapped on the 8th rank, then that's |
     | rewarded, and if in addition to that there is more than one      |
     | rook on the 7th rank, then that's also rewarded.                 |
     -------------------------------------------------------------------*/
    if (square[sq] & pawn_start[WHITE]
	&& ((board->piece[WHITE][KING] & pawn_lastrow[BLACK])
	    || (board->piece[WHITE][PAWN] & pawn_start[WHITE]))) {
      *bval += 24;
      if (bitcount(board->piece[BLACK][ROOK] & pawn_start[WHITE]) > 1)
	*bval += 10;
    }
    
    /*------------------------------------------------------------------
     | If a weak pawn is on a half open file, then it's good to attack |
     | the weak pawn with a rook.                                      |
     ------------------------------------------------------------------*/
    if ((square[sq] & pawn_struct.weak_pawn[WHITE])
	&& (square[sq] & pawn_struct.half_open_file[BLACK])) {
      if (square2row[sq] <
	  square2row[get_first_bitpos(board->piece[WHITE][PAWN]
				      & col_bitboard[square2col[sq]])]) {
	*bval += 15;
      }
    }
    
    /*-------------------------------------------------------------
     | It's good to place rooks in open files. If the rook is not |
     | on an open file, but there are open files on the board,    |
     | then penalize the rook if it cannot reach any open file in |
     | one move. Give some points if the rook is on a half open   |
     | file (a file where there are no pawns of one's own color). |
     | The importance of an open rook file depends on the number  |
     | of pawns on the board - the more pawns the higher the      |
     | bonus.                                                     |
     -------------------------------------------------------------*/
    trop = 7;
    if (square[sq] & pawn_struct.open_file) {
      *bval += rook_open_file[nbr_open_files][square2col[sq]];
      trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
    } else {
      if (pawn_struct.open_file) {
	if ((generate_horizontal_moves(board,sq)
	     & pawn_struct.open_file) == 0) {
	  *bval -= 16; //10;
	}
      }
      if (square[sq] & pawn_struct.half_open_file[BLACK]) {
	*bval += 10; //7;
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      } else if (!(get_ray(sq,8) & board->piece[BLACK][PAWN])) {
	//no own pawns in front of the rook
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      }
    }

    /*----------------------------------
     | Check the tropism for the rook. |
     ----------------------------------*/
    if (board->dangerous[BLACK]) {
      dist = dist_to_square[sq][opp_king_sq];
      board->tropism[BLACK] += maxval(tropism_at_rook[trop],tropism_rook[dist]);
    }
  }

  board->color_at_move = saved_color;

}

/*--------------------------------------
 | Evaluate the mobility of the rooks. |
 --------------------------------------*/
void eval_rooks_mobility(struct board *board) {
  bitboard pieces, blockers;
  char saved_color;
  int sq, s;
  int wmob = 0, bmob = 0;

  /*---------------------------------
   | Iterate through white's rooks. |
   ---------------------------------*/
  blockers = board->all_pieces[BLACK] | board->piece[WHITE][PAWN];
  pieces = board->piece[WHITE][ROOK];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*------------------------------------------------------------------
     | Check mobility of the rook by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish      |
     | mobility if one's own pieces are in the way of the rook, only   |
     | own pawns and enemy pieces are punished. Give more reward for   |
     | vertical mobility than for horizontal mobility, since in most   |
     | cases the strength of the rook depends on the influence it      |
     | exercises vertically.                                           |
     ------------------------------------------------------------------*/
    //North
    s = sq-8;
    while (s >= 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s -= 8;
    }
    //South
    s = sq+8;
    while (s <= 63 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 8;
    }
    //West
    if (sq%8 > 0) {
      s = sq-1;
      while (s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s -= 1;
      }
    }
    //East
    s = sq+1;
    while (s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 1;
    }
  }

  /*---------------------------------
   | Iterate through black's rooks. |
   ---------------------------------*/
  blockers = board->all_pieces[WHITE] | board->piece[BLACK][PAWN];
  pieces = board->piece[BLACK][ROOK];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*------------------------------------------------------------------
     | Check mobility of the rook by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish      |
     | mobility if one's own pieces are in the way of the rook, only   |
     | own pawns and enemy pieces are punished. Give more reward for   |
     | vertical mobility than for horizontal mobility, since in most   |
     | cases the strength of the rook depends on the influence it      |
     | exercises vertically.                                           |
     ------------------------------------------------------------------*/
    //North
    s = sq-8;
    while (s >= 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s -= 8;
    }
    //South
    s = sq+8;
    while (s <= 63 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 8;
    }
    //West
    if (sq%8 > 0) {
      s = sq-1;
      while (s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s -= 1;
      }
    }
    //East
    s = sq+1;
    while (s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 1;
    }
  }

  /*------------------------------------------------------------------------
   | Make sure that the mobility score is fair. It's no problem if white   |
   | and black has the same amount of rooks, but for example white has RR  |
   | and black has Q, then white will get a much better mobility score     |
   | although black's lone queen is almost as strong. Therefore adjust     |
   | the mobility score if white and black has different number of rooks.  |
   | For example if white has two rooks and black has one, then divide     |
   | white's mobility value by two. If white has three rooks and black     |
   | two, then give white's mobility score = (wmob*2)/3                    |
   |                                                                       |
   | Skip mobility checking altogether if one side doesn't have any rooks. |
   ------------------------------------------------------------------------*/
  /*if (wmob && bmob) {
    int wnum, bnum;
    wnum = bitcount(board->piece[WHITE][ROOK]);
    bnum = bitcount(board->piece[BLACK][ROOK]);
    if (wnum == bnum) {
      white_mobility += wmob;
      black_mobility += bmob;
    } else if (wnum > bnum) {
      //white_mobility = 0;
      white_mobility += (wmob*bnum)/wnum;
      black_mobility += bmob;
      /*printf("white_mobility = %d, wmob = %d, bmob = %d, wnum = %d, bnum = %d\n",white_mobility,wmob,bmob,wnum,bnum);
      showboard(board);
      wnum = 3;
      bnum = 1;
      white_mobility = 0;
      white_mobility += (wmob*bnum)/wnum;
      printf("white_mobility2 = %d, wmob = %d, bmob = %d, wnum = %d, bnum = %d\n",white_mobility,wmob,bmob,wnum,bnum);
      exit(1);/
    } else { //bnum > wnum
      //black_mobility = 0;
      white_mobility += wmob;
      black_mobility += (bmob*wnum)/bnum;
      /*printf("black_mobility = %d, wmob = %d, bmob = %d, wnum = %d, bnum = %d\n",white_mobility,wmob,bmob,wnum,bnum);
      showboard(board);
      exit(1);/
    }
  }*/
  white_mobility += wmob;
  black_mobility += bmob;

}

/*-----------------------
 | Evaluate the queens. |
 -----------------------*/
void eval_queens(struct board *board, int *wval, int *bval) {
  bitboard pieces, piece, blockers;
  int sq, opp_king_sq, trop, dist;

  /*----------------------------------
   | Iterate through white's queens. |
   ----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[BLACK][KING]);
  pieces = board->piece[WHITE][QUEEN];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);
    *wval += queen_posval[sq]; //check placement

    /*-----------------------------------
     | Check the tropism for the queen. |
     -----------------------------------*/
    if (board->dangerous[WHITE]) {
      trop = 7;
      if (!(get_ray(sq,-8) & board->piece[WHITE][PAWN])) {
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      }
      dist = dist_to_square[sq][opp_king_sq];
      board->tropism[WHITE] += maxval(tropism_queen[dist],
				      tropism_at_queen[trop]);
    }
  }


  /*----------------------------------
   | Iterate through black's queens. |
   ----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[WHITE][KING]);
  pieces = board->piece[BLACK][QUEEN];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);
    *bval += queen_posval[sq]; //check placement

    /*-----------------------------------
     | Check the tropism for the queen. |
     -----------------------------------*/
    if (board->dangerous[BLACK]) {
      trop = 7;
      if (!(get_ray(sq,8) & board->piece[BLACK][PAWN])) {
	trop = maxval(sq%8-opp_king_sq%8,opp_king_sq%8-sq%8); //file distance
      }
      dist = dist_to_square[sq][opp_king_sq];
      board->tropism[BLACK] += maxval(tropism_queen[dist],
				      tropism_at_queen[trop]);
    }
  }

  /*-----------------------------------------------------
   | 7th rank attacks by rook and queen is very strong. |
   -----------------------------------------------------*/
  if (board->piece[WHITE][QUEEN] & pawn_start[BLACK]
      && board->piece[WHITE][ROOK] & pawn_start[BLACK])
    *wval += 50;
  if (board->piece[BLACK][QUEEN] & pawn_start[WHITE]
      && board->piece[BLACK][ROOK] & pawn_start[WHITE])
    *bval += 50;

  /*----------------------------------------------
   | Give penalty for when the queen is offside. |
   ----------------------------------------------*/
  if (bitcount(board->piece[WHITE][PAWN]) > 4) {
    pieces = board->piece[WHITE][QUEEN];
    while (pieces != 0) {
      piece = getlsb(pieces);
      if ((piece & ab_col && board->piece[BLACK][KING] & gh_col)
	  || (piece & gh_col && board->piece[BLACK][KING] & ab_col))
	*wval -= 30;
      pieces &= ~piece;
    }
  }
  if (bitcount(board->piece[BLACK][PAWN]) > 4) {
    pieces = board->piece[BLACK][QUEEN];
    while (pieces != 0) {
      piece = getlsb(pieces);
      if ((piece & ab_col && board->piece[WHITE][KING] & gh_col)
	  || (piece & gh_col && board->piece[WHITE][KING] & ab_col))
	*bval -= 30;
      pieces &= ~piece;
    }
  }
}

/*-----------------------
 | Evaluate the queens. |
 -----------------------*/
void eval_queens_mobility(struct board *board) {
  bitboard pieces, piece, blockers;
  int sq, s;
  int wmob = 0, bmob = 0;

  /*----------------------------------
   | Iterate through white's queens. |
   ----------------------------------*/
  blockers = board->all_pieces[BLACK] | board->piece[WHITE][PAWN];
  pieces = board->piece[WHITE][QUEEN];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*-------------------------------------------------------------------
     | Check mobility of the queen by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish       |
     | mobility if one's own pieces are in the way of the queen, only   |
     | own pawns and enemy pieces are punished. Give more reward for    |
     | vertical mobility than for horizontal mobility, since in most    |
     | cases the strength of the rook depends on the influence it       |
     | exercises vertically.                                            |
     -------------------------------------------------------------------*/
    //North
    s = sq-8;
    while (s >= 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s -= 8;
    }
    //South
    s = sq+8;
    while (s <= 63 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 8;
    }
    //West
    if (sq%8 > 0) {
      s = sq-1;
      while (s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s -= 1;
      }
    }
    //East
    s = sq+1;
    while (s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 1;
    }
    //North east
    s = sq-7;
    while (s >= 0 && s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s -= 7;
    }
    //North west
    if (sq%8 > 0) {
      s = sq-9;
      while (s >= 0 && s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s -= 9;
      }
    }
    //South east
    s = sq+9;
    while (s <= 63 && s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 9;
    }
    //South west
    if (sq%8 > 0) {
      s = sq+7;
      while (s <= 63 && s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s += 7;
      }
    }
  }

  /*----------------------------------
   | Iterate through black's queens. |
   ----------------------------------*/
  blockers = board->all_pieces[WHITE] | board->piece[BLACK][PAWN];
  pieces = board->piece[BLACK][QUEEN];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*-------------------------------------------------------------------
     | Check mobility of the queen by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish       |
     | mobility if one's own pieces are in the way of the queen, only   |
     | own pawns and enemy pieces are punished. Give more reward for    |
     | vertical mobility than for horizontal mobility, since in most    |
     | cases the strength of the rook depends on the influence it       |
     | exercises vertically.                                            |
     -------------------------------------------------------------------*/
    //North
    s = sq-8;
    while (s >= 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s -= 8;
    }
    //South
    s = sq+8;
    while (s <= 63 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 8;
    }
    //West
    if (sq%8 > 0) {
      s = sq-1;
      while (s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s -= 1;
      }
    }
    //East
    s = sq+1;
    while (s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 1;
    }
    //North east
    s = sq-7;
    while (s >= 0 && s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s -= 7;
    }
    //North west
    if (sq%8 > 0) {
      s = sq-9;
      while (s >= 0 && s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s -= 9;
      }
    }
    //South east
    s = sq+9;
    while (s <= 63 && s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 9;
    }
    //South west
    if (sq%8 > 0) {
      s = sq+7;
      while (s <= 63 && s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s += 7;
      }
    }
  }

  /*---------------------------------------------------------
   | Make sure that the mobility score is fair. For a more  |
   | thorough explanation, see the corresponding section in |
   | the eval_rooks() function.                             |
   ---------------------------------------------------------*/
  /*if (wmob && bmob) {
    int wnum, bnum;
    wnum = bitcount(board->piece[WHITE][QUEEN]);
    bnum = bitcount(board->piece[BLACK][QUEEN]);
    if (wnum == bnum) {
      white_mobility += wmob;
      black_mobility += bmob;
    } else if (wnum > bnum) {
      white_mobility += (wmob*bnum)/wnum;
      black_mobility += bmob;
    } else { //bnum > wnum
      white_mobility += wmob;
      black_mobility += (bmob*wnum)/bnum;
    }
    }*/
  //if (do_mobility) {
    white_mobility += wmob;
    black_mobility += bmob;
    //}
}

/*------------------------
 | Evaluate the bishops. |
 ------------------------*/
void eval_bishops(struct board *board, int *wval, int *bval) {
  bitboard pieces;
  int sq, opp_king_sq;

  /*-----------------------------------
   | Iterate through white's bishops. |
   -----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[BLACK][KING]);
  pieces = board->piece[WHITE][BISHOP];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Check bishop's placement. */
    *wval += bishop_posval[sq];

    /*-----------------------------------------------------------
     | Check for a trapped bishop at a7, b8, h7 or g8 for white |
     -----------------------------------------------------------*/
    if (sq == 8 && board->piece[BLACK][PAWN] & square[17])
      *wval -= 174;
    else if (sq == 1 && board->piece[BLACK][PAWN] & square[10])
      *wval -= 174;
    if (sq == 15 && board->piece[BLACK][PAWN] & square[22])
      *wval -= 174;
    else if (sq == 6 && board->piece[BLACK][PAWN] & square[13])
      *wval -= 174;

    /*----------------------------------------------------
     | Check for bad bishops (far back in the corner and |
     | attacking own pawns from behind).                 |
     ----------------------------------------------------*/
    if (((sq >= 40 && sq <= 42)
	 || (sq >= 48 && sq <= 50)
	 || (sq >= 56 && sq <= 58))
	&& ((board->piece[WHITE][PAWN] & square[sq-7])
	    || (board->piece[WHITE][PAWN]
		& square[sq-14])))
      *wval -= 10; //7;
    else if (((sq >= 45 && sq <= 47)
	      || (sq >= 53 && sq <= 55)
	      || (sq >= 61 && sq <= 63))
	     && ((board->piece[WHITE][PAWN] & square[sq-9])
		 || (board->piece[WHITE][PAWN]
		     & square[sq-18])))
      *wval -= 10; //7;

    /*------------------------------------
     | Check the tropism for the bishop. |
     ------------------------------------*/
    if (board->dangerous[WHITE])
      board->tropism[WHITE] += tropism_bishop[dist_to_square[sq][opp_king_sq]];
  }

  /*-----------------------------------
   | Iterate through black's bishops. |
   -----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[WHITE][KING]);
  pieces = board->piece[BLACK][BISHOP];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Check bishop's placement. */
    *bval += bishop_posval[sq];

    /*-------------------------------------------------------------
      | Check for a trapped bishop at a2, b1, h2 or g1 for black. |
      ------------------------------------------------------------*/
    if (sq == 48 && board->piece[WHITE][PAWN] & square[41])
      *bval -= 174;
    else if (sq == 57 && board->piece[WHITE][PAWN] & square[50])
      *bval -= 174;
    if (sq == 55 && board->piece[WHITE][PAWN] & square[46])
      *bval -= 174;
    else if (sq == 62 && board->piece[WHITE][PAWN] & square[53])
      *bval -= 174;

    /*----------------------------------------------------
     | Check for bad bishops (far back in the corner and |
     | attacking own pawns from behind).                 |
     ----------------------------------------------------*/
    if (((sq >= 0 && sq <= 2)
	 || (sq >= 8 && sq <= 10)
	 || (sq >= 16 && sq <= 18))
	&& ((board->piece[BLACK][PAWN] & square[sq+9])
	    || (board->piece[BLACK][PAWN]
		& square[sq+18])))
      *bval -= 10; //7;
    else if (((sq >= 5 && sq <= 7)
	      || (sq >= 13 && sq <= 15)
	      || (sq >= 21 && sq <= 23))
	     && ((board->piece[BLACK][PAWN] & square[sq+7])
		 || (board->piece[BLACK][PAWN]
		     & square[sq+14])))
      *bval -= 10; //7;

    /*------------------------------------
     | Check the tropism for the bishop. |
     ------------------------------------*/
    if (board->dangerous[BLACK])
      board->tropism[BLACK] += tropism_bishop[dist_to_square[sq][opp_king_sq]];
  }

  /*------------------------------------------------------------
   | Check if bishops are placed in "holes" in the pawn chain. |
   ------------------------------------------------------------*/
  *wval += 8*bitcount(pawn_struct.pawn_hole[BLACK]
		      & board->piece[WHITE][BISHOP]);
  *bval += 8*bitcount(pawn_struct.pawn_hole[WHITE]
		      & board->piece[BLACK][BISHOP]);

  /*--------------------------------------------
   | Add points if a bishop is blocking one of |
   | the 2 center pawns.                       |
   --------------------------------------------*/
  if ((board->piece[WHITE][BISHOP] & square[19])
      && (board->piece[BLACK][PAWN] & square[11]))
    *wval += 12;
  if ((board->piece[WHITE][BISHOP] & square[20])
      && (board->piece[BLACK][PAWN] & square[12]))
    *wval += 12;
  if ((board->piece[BLACK][BISHOP] & square[43])
      && (board->piece[WHITE][PAWN] & square[51]))
    *bval += 12;
  if ((board->piece[BLACK][BISHOP] & square[44])
      && (board->piece[WHITE][PAWN] & square[52]))
    *bval += 12;

  /*-------------------------------------------------------------
   | If we are in the endgame and there are pawns on both sides |
   | of the board, then it's good to have a bishop.             |
   -------------------------------------------------------------*/
  if (endgame) {
    if ((abc_col
	 & (board->piece[WHITE][PAWN] | board->piece[BLACK][PAWN])) &&
	(fgh_col
	 & (board->piece[WHITE][PAWN] | board->piece[BLACK][PAWN]))) {
      if (board->piece[WHITE][BISHOP]
	  && board->piece[BLACK][BISHOP] == 0)
	*wval += 36;
      else if (board->piece[BLACK][BISHOP]
	  && board->piece[WHITE][BISHOP] == 0)
	*bval += 36;
    }
  }

  /*-----------------------------------------------------------------
   | Give bonus for having two bishops if the center isn't blocked. |
   -----------------------------------------------------------------*/
  if (bitcount(board->piece[WHITE][BISHOP]) >= 2) {
    if (pawn_struct.center == 1)
      *wval += 30;
    else if (pawn_struct.center == 2)
      *wval += 60;
  }
  if (bitcount(board->piece[BLACK][BISHOP]) >= 2) {
    if (pawn_struct.center == 1)
      *bval += 30;
    else if (pawn_struct.center == 2)
      *bval += 60;
  }
}

/*------------------------
 | Evaluate the bishops. |
 ------------------------*/
void eval_bishops_mobility(struct board *board) {
  bitboard pieces, blockers;
  int sq, s;
  int wmob = 0, bmob = 0;

  /*-----------------------------------
   | Iterate through white's bishops. |
   -----------------------------------*/
  blockers = board->all_pieces[BLACK] | board->piece[WHITE][PAWN];
  pieces = board->piece[WHITE][BISHOP];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*--------------------------------------------------------------------
     | Check mobility of the bishop by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish        |
     | mobility if one's own pieces are in the way of the bishop, only   |
     | own pawns and enemy pieces are punished.                          |
     --------------------------------------------------------------------*/
    //North east
    s = sq-7;
    while (s >= 0 && s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s -= 7;
    }
    //North west
    if (sq%8 > 0) {
      s = sq-9;
      while (s >= 0 && s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s -= 9;
      }
    }
    //South east
    s = sq+9;
    while (s <= 63 && s%8 != 0 && (blockers & square[s]) == 0) {
      wmob += 1;
      s += 9;
    }
    //South west
    if (sq%8 > 0) {
      s = sq+7;
      while (s <= 63 && s%8 >= 0 && (blockers & square[s]) == 0) {
	wmob += 1;
	if (s%8 == 0)
	  break;
	s += 7;
      }
    }
  }

  /*-----------------------------------
   | Iterate through black's bishops. |
   -----------------------------------*/
  blockers = board->all_pieces[WHITE] | board->piece[BLACK][PAWN];
  pieces = board->piece[BLACK][BISHOP];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*--------------------------------------------------------------------
     | Check mobility of the bishop by walking along the xray directions |
     | until we hit an enemy piece or own pawn. I.e. don't punish        |
     | mobility if one's own pieces are in the way of the bishop, only   |
     | own pawns and enemy pieces are punished.                          |
     --------------------------------------------------------------------*/
    //North east
    s = sq-7;
    while (s >= 0 && s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s -= 7;
    }
    //North west
    if (sq%8 > 0) {
      s = sq-9;
      while (s >= 0 && s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s -= 9;
      }
    }
    //South east
    s = sq+9;
    while (s <= 63 && s%8 != 0 && (blockers & square[s]) == 0) {
      bmob += 1;
      s += 9;
    }
    //South west
    if (sq%8 > 0) {
      s = sq+7;
      while (s <= 63 && s%8 >= 0 && (blockers & square[s]) == 0) {
	bmob += 1;
	if (s%8 == 0)
	  break;
	s += 7;
      }
    }
  }

  /*---------------------------------------------------------
   | Make sure that the mobility score is fair. For a more  |
   | thorough explanation, see the corresponding section in |
   | the eval_rooks() function.                             |
   ---------------------------------------------------------*/
  /*if (wmob && bmob) {
    int wnum, bnum;
    wnum = bitcount(board->piece[WHITE][BISHOP]);
    bnum = bitcount(board->piece[BLACK][BISHOP]);
    if (wnum == bnum) {
      white_mobility += wmob;
      black_mobility += bmob;
    } else if (wnum > bnum) {
      white_mobility += (wmob*bnum)/wnum;
      black_mobility += bmob;
    } else { //bnum > wnum
      white_mobility += wmob;
      black_mobility += (bmob*wnum)/bnum;
    }
    }*/
  //if (do_mobility) {
    white_mobility += wmob;
    black_mobility += bmob;
    //}

}


/*------------------------
 | Evaluate the knights. |
 ------------------------*/
void eval_knights(struct board *board, int *wval, int *bval) {
  bitboard pieces;
  int sq, opp_king_sq;

  /*-----------------------------------
   | Iterate through white's knights. |
   -----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[BLACK][KING]);
  pieces = board->piece[WHITE][KNIGHT];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Knights should be standing close to the middle. */
    *wval += knight_posval[sq];

    /*---------------------------------------------------------------
     | Check for pawn outposts (central pawn that can't be attacked |
     | by an enemy pawn and that is supported by a friendly pawn).  |
     ---------------------------------------------------------------*/
    if (white_outpost[sq] && !(possible_pawn_attack[BLACK][sq]
			       & board->piece[BLACK][PAWN])
	&& (attack.pawn[BLACK][sq] & board->piece[WHITE][PAWN]))
      *wval += white_outpost[sq];

    /*------------------------------------
     | Check the tropism for the knight. |
     ------------------------------------*/
    /*if (board->dangerous[WHITE]) {
      dist = dist_to_square[sq][opp_king_sq];
      *wval -= (dist*2 - 7);
      board->tropism[WHITE] += tropism_knight[dist];
      }*/
    //TODO: Check out if it's more cpu cache efficient to
    //calculate the distance between two squares rather than
    //use a quite big lookup table.
    if (board->dangerous[WHITE])
      board->tropism[WHITE] += tropism_knight[dist_to_square[sq][opp_king_sq]];
  }

  /*-----------------------------------
   | Iterate through black's knights. |
   -----------------------------------*/
  opp_king_sq = get_first_bitpos(board->piece[WHITE][KING]);
  pieces = board->piece[BLACK][KNIGHT];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /* Knights should be standing close to the middle. */
    *bval += knight_posval[sq];

    /*---------------------------------------------------------------
     | Check for pawn outposts (central pawn that can't be attacked |
     | by an enemy pawn and that is supported by a friendly pawn).  |
     ---------------------------------------------------------------*/
    if (black_outpost[sq] && !(possible_pawn_attack[WHITE][sq]
			       & board->piece[WHITE][PAWN])
	&& (attack.pawn[WHITE][sq] & board->piece[BLACK][PAWN]))
      *bval += black_outpost[sq];

    /*------------------------------------
     | Check the tropism for the knight. |
     ------------------------------------*/
    /*if (board->dangerous[BLACK]) {
      dist = dist_to_square[sq][opp_king_sq];
      *wval -= (dist*2 - 7);
      board->tropism[BLACK] += tropism_knight[dist];
      }*/
    if (board->dangerous[BLACK])
      board->tropism[BLACK] += tropism_knight[dist_to_square[sq][opp_king_sq]];
  }

  /*------------------------------------------------------------
   | Check if knights are placed in "holes" in the pawn chain. |
   ------------------------------------------------------------*/
  *wval += 8*bitcount(pawn_struct.pawn_hole[BLACK]
		      & board->piece[WHITE][KNIGHT]);
  *bval += 8*bitcount(pawn_struct.pawn_hole[WHITE]
		      & board->piece[BLACK][KNIGHT]);

  /*--------------------------------------------
   | Add points if a knight is blocking one of |
   | the 2 center pawns.                       |
   --------------------------------------------*/
  if ((board->piece[WHITE][KNIGHT] & square[19])
      && (board->piece[BLACK][PAWN] & square[11]))
    *wval += 12;
  if ((board->piece[WHITE][KNIGHT] & square[20])
      && (board->piece[BLACK][PAWN] & square[12]))
    *wval += 12;
  if ((board->piece[BLACK][KNIGHT] & square[43])
      && (board->piece[WHITE][PAWN] & square[51]))
    *bval += 12;
  if ((board->piece[BLACK][KNIGHT] & square[44])
      && (board->piece[WHITE][PAWN] & square[52]))
    *bval += 12;
}

/*------------------------
 | Evaluate the knights. |
 ------------------------*/
void eval_knights_mobility(struct board *board) {
  bitboard pieces, blockers;
  int sq;
  int wmob = 0, bmob = 0;

  /*-----------------------------------
   | Iterate through white's knights. |
   -----------------------------------*/
  blockers = board->all_pieces[BLACK] | board->piece[WHITE][PAWN];
  pieces = board->piece[WHITE][KNIGHT];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*----------------------------------------
     | Check the mobility score for knights. |
     ----------------------------------------*/
    wmob += bitcount(attack.knight[sq] & ~blockers);

  }

  /*-----------------------------------
   | Iterate through black's knights. |
   -----------------------------------*/
  blockers = board->all_pieces[WHITE] | board->piece[BLACK][PAWN];
  pieces = board->piece[BLACK][KNIGHT];
  while (pieces) {
    sq = get_first_bitpos(pieces);
    pieces &= ~(square[sq]);

    /*----------------------------------------
     | Check the mobility score for knights. |
     ----------------------------------------*/
    bmob += bitcount(attack.knight[sq] & ~blockers);

  }

  /*---------------------------------------------------------
   | Make sure that the mobility score is fair. For a more  |
   | thorough explanation, see the corresponding section in |
   | the eval_rooks() function.                             |
   ---------------------------------------------------------*/
  /*if (wmob && bmob) {
    int wnum, bnum;
    wnum = bitcount(board->piece[WHITE][KNIGHT]);
    bnum = bitcount(board->piece[BLACK][KNIGHT]);
    if (wnum == bnum) {
      white_mobility += wmob;
      black_mobility += bmob;
    } else if (wnum > bnum) {
      white_mobility += (wmob*bnum)/wnum;
      black_mobility += bmob;
    } else { //bnum > wnum
      white_mobility += wmob;
      black_mobility += (bmob*wnum)/bnum;
    }
    }*/
  //if (do_mobility) {
    white_mobility += wmob;
    black_mobility += bmob;
    //}
}

/*-------------------------
 | Evaluate the mobility. |
 -------------------------*/
void eval_mobility(struct board *board, int *wval, int *bval) {
  //int w = 0, b = 0;
  /*if (white_mobility > black_mobility) {
    *wval += mobility_table[minval(169,white_mobility-black_mobility)];
  } else {
    *bval += mobility_table[minval(169,black_mobility-white_mobility)];
    }*/

  white_mobility = black_mobility = 0;
  eval_rooks_mobility(board);
  eval_queens_mobility(board);
  eval_bishops_mobility(board);
  eval_knights_mobility(board);
  *wval += white_mobility*2;
  *bval += black_mobility*2;
}

/*----------------------
 | Evaluate the kings. |
 ----------------------*/
void eval_kings(struct board *board, int *wval, int *bval) {
  bitboard all_pawns;
  int w_pos, b_pos;

  w_pos = get_first_bitpos(board->piece[WHITE][KING]);
  b_pos = get_first_bitpos(board->piece[BLACK][KING]);

  /* In endgames the king should be in the center if there are pawns
     on both wings, or otherwise on the wing where the pawns are. */
  if (endgame) {
    all_pawns = board->piece[WHITE][PAWN] | board->piece[BLACK][PAWN];
    if ((abcd_col & all_pawns) && (efgh_col & all_pawns)) {
      *wval += w_king_reach_endgame_normal[w_pos];
      *bval += b_king_reach_endgame_normal[b_pos];
    } else if (abcd_col & all_pawns) {
      *wval += w_king_reach_endgame_qside[w_pos];
      *bval += b_king_reach_endgame_qside[b_pos];
    } else if (all_pawns) {
      *wval += w_king_reach_endgame_kside[w_pos];
      *bval += b_king_reach_endgame_kside[b_pos];
    }
  } else {
    if (board->material_pieces[BLACK] >= VAL_KING + VAL_QUEEN + VAL_ROOK*2)
      *wval += king_reach_early[w_pos];
    else
      *wval += king_reach_late[w_pos];
    if (board->material_pieces[WHITE] >= VAL_KING + VAL_QUEEN + VAL_ROOK*2)
      *bval += king_reach_early[b_pos];
    else
      *bval += king_reach_late[b_pos];

    //WhiteCastle verkar vara castlerättigheter: WhiteCastle(0) & 1 = SHORT_CASTLING_OK, WhiteCastle(0) & 2 = LONG_CASTLING_OK

    //board->safety[WHITE] = 0;

    /*-----------------------------------------------------------------
     | If black seems dangerous, then do king safety check for white. |
     -----------------------------------------------------------------*/
    if (board->dangerous[BLACK]) {

      /*-----------------------------------------------------------
       | Add incentive to castle + check king safety with respect |
       | to the pawn chain in front of the king.                  |
       -----------------------------------------------------------*/
      if (board->castling_status[WHITE] & CASTLED) {
	*wval += 50;
	if (w_pos%8 >= 4) {
	  if (w_pos%8 > 4)
	    board->safety[WHITE] += pawn_struct.defects_k[WHITE];
	  else
	    board->safety[WHITE] += pawn_struct.defects_e[WHITE];
	} else if (w_pos%8 <= 3) {
	  if (w_pos%8 < 3)
	    board->safety[WHITE] += pawn_struct.defects_q[WHITE];
	  else
	    board->safety[WHITE] += pawn_struct.defects_d[WHITE];
	}
      } else {
	if ((board->castling_status[WHITE] & LONG_CASTLING_OK)
	    && (board->castling_status[WHITE] & SHORT_CASTLING_OK)) {
	  *wval += 10;
	  board->safety[WHITE] = minval(minval(pawn_struct.defects_k[WHITE],
					       pawn_struct.defects_e[WHITE]),
					pawn_struct.defects_q[WHITE]);
	} else if (board->castling_status[WHITE] & SHORT_CASTLING_OK) {
	  *wval += 10;
	  board->safety[WHITE] = minval(pawn_struct.defects_k[WHITE],
					pawn_struct.defects_e[WHITE]);
	} else {
	  if (board->castling_status[WHITE] & LONG_CASTLING_OK)
	    *wval += 10;
	  else
	    *wval -= 50;
	  board->safety[WHITE] = minval(pawn_struct.defects_q[WHITE],
					pawn_struct.defects_e[WHITE]);
	}
	board->safety[WHITE] = maxval(board->safety[WHITE],3);
      }

      if (!(board->piece[BLACK][QUEEN])) {
	board->safety[WHITE] = queen_scale_safety[board->safety[WHITE]];
	board->tropism[BLACK] = queen_scale_tropism[board->tropism[BLACK]];
      }

      *wval -= king_safety[board->safety[WHITE]]
	[minval(board->tropism[BLACK],15)];
    }


    /*-----------------------------------------------------------------
     | If white seems dangerous, then do king safety check for black. |
     -----------------------------------------------------------------*/
    if (board->dangerous[WHITE]) {

      /*-----------------------------------------------------------
       | Add incentive to castle + check king safety with respect |
       | to the pawn chain in front of the king.                  |
       -----------------------------------------------------------*/
      if (board->castling_status[BLACK] & CASTLED) {
	*bval += 50;
	if (b_pos%8 >= 4) {
	  if (b_pos%8 > 4)
	    board->safety[BLACK] += pawn_struct.defects_k[BLACK];
	  else
	    board->safety[BLACK] += pawn_struct.defects_e[BLACK];
	} else if (b_pos%8 <= 3) {
	  if (b_pos%8 < 3)
	    board->safety[BLACK] += pawn_struct.defects_q[BLACK];
	  else
	    board->safety[BLACK] += pawn_struct.defects_d[BLACK];
	}
      } else {
	if ((board->castling_status[BLACK] & LONG_CASTLING_OK)
	    && (board->castling_status[BLACK] & SHORT_CASTLING_OK)) {
	  *bval += 10;
	  board->safety[BLACK] = minval(minval(pawn_struct.defects_k[BLACK],
					       pawn_struct.defects_e[BLACK]),
					pawn_struct.defects_q[BLACK]);
	} else if (board->castling_status[BLACK] & SHORT_CASTLING_OK) {
	  *bval += 10;
	  board->safety[BLACK] = minval(pawn_struct.defects_k[BLACK],
					pawn_struct.defects_e[BLACK]);
	} else {
	  if (board->castling_status[BLACK] & LONG_CASTLING_OK)
	    *bval += 10;
	  else
	    *bval -= 50;
	  board->safety[BLACK] = minval(pawn_struct.defects_q[BLACK],
					pawn_struct.defects_e[BLACK]);
	}
	board->safety[BLACK] = maxval(board->safety[BLACK],3);
      }

      if (!(board->piece[WHITE][QUEEN])) {
	board->safety[BLACK] = queen_scale_safety[board->safety[BLACK]];
	board->tropism[WHITE] = queen_scale_tropism[board->tropism[WHITE]];
      }

      *bval -= king_safety[board->safety[BLACK]]
	[minval(board->tropism[WHITE],15)];
    }




    //TODO: Fix the pawn defect stuff so that these don't need to be
    //set to zero.
    //board->safety[WHITE] = 0;
    //board->safety[BLACK] = 0;

    /*w = king_safety[board->safety[WHITE]]
      [minval(board->tropism[BLACK],15)];
    b = king_safety[board->safety[BLACK]]
      [minval(board->tropism[WHITE],15)];
    *wval -= w;
    *bval -= b;
    if (w - b > 150 || b - w > 150) {
      printf("w = %d, b = %d\n",w,b);
      showboard(board);
      //exit(1);
    }*/
    /*printf("wks = %d, bks = %d\n",king_safety[board->safety[WHITE]]
	   [minval(board->tropism[BLACK],15)], king_safety[board->safety[BLACK]]
	   [minval(board->tropism[WHITE],15)]);*/


  }


}

void eval_endgame(struct board *board, int *wval, int *bval) {
  int wsquare, bsquare;

  /*------------------------------------------
   | Quit early if we aren't in the endgame. |
   ------------------------------------------*/
  if (((board->material_pieces[WHITE] + board->material_pieces[BLACK])
       <= (VAL_KING*2 + VAL_QUEEN*2))
      || (board->material_pieces[WHITE] <= VAL_KING + VAL_ROOK)
      || (board->material_pieces[BLACK] <= VAL_KING + VAL_ROOK)) {
    endgame = UNKNOWN_ENDGAME;
    wsquare = get_first_bitpos(board->piece[WHITE][KING]);
    bsquare = get_first_bitpos(board->piece[BLACK][KING]);
  } else {
    endgame = 0;
    return;
  }

  /*------------------------------------------------------
   | So we are in the endgame. See if this is an endgame |
   | that we know how to handle.                         |
   ------------------------------------------------------*/
  //TODO: Add support also for the same draw-things that
  //are in alphabeta (KBKB, KBK, KNK and KK)
  //TODO: Add the following endings: KBBK, KBPK,
  //KQKP, KQKR
  if ((board->nbr_pieces[WHITE] == 0 && board->nbr_pieces[BLACK] == 1
       && board->piece[BLACK][ROOK])
      || (board->nbr_pieces[BLACK] == 0 && board->nbr_pieces[WHITE] == 1
	  && board->piece[WHITE][ROOK]))
    endgame = KRK;
  else if ((board->nbr_pieces[WHITE] == 0 && board->nbr_pieces[BLACK] == 1
	    && board->piece[BLACK][QUEEN])
	   || (board->nbr_pieces[BLACK] == 0 && board->nbr_pieces[WHITE] == 1
	       && board->piece[WHITE][QUEEN]))
    endgame = KQK;
  else if (board->nbr_pieces[WHITE] == 0 && board->nbr_pieces[BLACK] == 0)
    endgame = PAWN_ENDGAME;
  else if ((board->nbr_pieces[WHITE] == 1 && board->piece[WHITE][ROOK]
	    && board->material_pawns[WHITE] == VAL_PAWN
	    && board->nbr_pieces[BLACK] == 1 && board->piece[BLACK][ROOK]
	    && board->material_pawns[BLACK] == 0)
	   || (board->nbr_pieces[BLACK] == 1 && board->piece[BLACK][ROOK]
	       && board->material_pawns[BLACK] == VAL_PAWN
	       && board->nbr_pieces[WHITE] == 1 && board->piece[WHITE][ROOK]
	       && board->material_pawns[WHITE] == 0))
    endgame = KRPKR;
  else if ((board->nbr_pieces[WHITE] == 2 && board->piece[WHITE][KNIGHT]
	    && board->piece[WHITE][BISHOP]
	    && board->material_pawns[WHITE] == 0
	    && board->nbr_pieces[BLACK] == 0
	    && board->material_pawns[BLACK] == 0)
	   || (board->nbr_pieces[BLACK] == 2 && board->piece[BLACK][KNIGHT]
	       && board->piece[BLACK][BISHOP]
	       && board->material_pawns[BLACK] == 0
	       && board->nbr_pieces[WHITE] == 0
	       && board->material_pawns[WHITE] == 0))
    endgame = KBNK;

  if (endgame == KRK || endgame == KQK) {
    //printf("laff1: white = %d, black = %d\n",board->nbr_pieces[WHITE],board->nbr_pieces[BLACK]);
    /*--------------------------------------------------------------
     | In KRK and KQK the color who is having the advantage should |
     | try to force the enemy king to the side, and to come close  |
     | with his king.                                              |
     --------------------------------------------------------------*/
    if (board->nbr_pieces[WHITE] == 1) {    //white is ahead
      //printf("laff2\n");
      *wval += (400-dist_to_side[bsquare]*100);
      *wval -= dist_to_square[wsquare][bsquare]*10;
      *wval += dist_to_side[wsquare]; //to make sure that white is on outside
      /**wval -= dist_to_side[bsquare]*10;
      *wval -= 2*(abs(wsquare%8-bsquare%8) + abs(wsquare/8-bsquare/8));
      *wval -= dist_to_corn[bsquare];
      *wval += dist_to_side[wsquare];*/
    } else {   //black is ahead
      //printf("laff3\n");
      *bval += (400-dist_to_side[wsquare]*100);
      *bval -= dist_to_square[bsquare][wsquare]*10;
      *bval += dist_to_side[bsquare]; //to make sure that black is on outside
      /**bval -= dist_to_side[wsquare]*10;
      *bval -= 2*(abs(wsquare%8-bsquare%8) + abs(wsquare/8-bsquare/8));
      *bval -= dist_to_corn[wsquare];
      *bval += dist_to_side[bsquare];*/
    }
  } else if (endgame == KBNK) {
    /*------------------------------------------------------------------
     | Drive the lone king into the corner of the color of the bishop. |
     ------------------------------------------------------------------*/
    if (board->nbr_pieces[WHITE] == 2) {    //white is ahead
      *wval -= dist_to_square[wsquare][bsquare];
      if (white_squares & board->piece[WHITE][BISHOP])
	*wval -= kbnk_white_bishop[bsquare];
      else
	*wval -= kbnk_black_bishop[bsquare];
    } else {   //black is ahead
      *bval -= dist_to_square[wsquare][bsquare];
      if (white_squares & board->piece[WHITE][BISHOP])
	*bval -= kbnk_white_bishop[bsquare];
      else
	*bval -= kbnk_black_bishop[bsquare];
    }
  } else if (endgame == KRPKR) {
    int pawn_pos, opp_king_pos;
    if (board->material_pawns[WHITE] == VAL_PAWN) { //white ahead
      pawn_pos = get_first_bitpos(board->piece[WHITE][PAWN]);
      opp_king_pos = get_first_bitpos(board->piece[BLACK][KING]);
      if (pawn_pos / 8 > opp_king_pos / 8) { //king in front of pawn
	int diff = pawn_pos % 8 - opp_king_pos % 8;
	if (diff == 0) { //king on same file, draw!
	  *wval = (8 - (pawn_pos / 8))*3;
	  *bval = 0;
	} else if (diff == 1 || diff == -1) { //king on adjacent file, draw!
	  *wval = (8 - (pawn_pos / 8))*6;
	  *bval = 0;
	}
      }
    } else {
      pawn_pos = get_first_bitpos(board->piece[BLACK][PAWN]);
      opp_king_pos = get_first_bitpos(board->piece[WHITE][KING]);
      if (pawn_pos / 8 < opp_king_pos / 8) { //king in front of pawn
	int diff = pawn_pos % 8 - opp_king_pos % 8;
	if (diff == 0) { //king on same file, draw!
	  *bval = (pawn_pos / 8)*3; //50;
	  *wval = 0;
	} else if (diff == 1 || diff == -1) { //king on adjacent file, draw!
	  *bval = (pawn_pos / 8)*6; //100;
	  *wval = 0;
	}
      }
    }
  } else if (endgame == PAWN_ENDGAME) {
    bitboard pawns;
    bitboard possible_white_promotions = 0, possible_black_promotions = 0;
    int pawn, dist, w_shortest_dist = 100, b_shortest_dist = 100;
    int king_dist;
    int whites_promotion_checks_blacks_king = 0;
    int blacks_promotion_checks_whites_king = 0;
    int white_ah = 0, black_ah = 0;
    int prom_sq, white_prom_sq, black_prom_sq, s;
    bitboard blockers;
    blockers = (board->all_pieces[WHITE] & ~board->piece[WHITE][KING])
      | (board->all_pieces[BLACK] & ~board->piece[BLACK][KING]);
    //blockers = board->all_pieces[WHITE] | board->all_pieces[BLACK];
    /*---------------------------------------------------
     | A key to victory in pawn endings is to have more |
     | pawns than the opponent. Make sure that isolated |
     | double pawns are counted as one pawn.            |
     ---------------------------------------------------*/
    int white_adjust = bitcount(pawn_struct.double_isolated_pawn[WHITE]) / 2;
    int black_adjust = bitcount(pawn_struct.double_isolated_pawn[BLACK]) / 2;
    *wval += pawn_ending_adjustment[bitcount(board->piece[WHITE][PAWN])
				    - white_adjust];
    *bval += pawn_ending_adjustment[bitcount(board->piece[BLACK][PAWN])
				    - black_adjust];

    /*--------------------------------------------------------------------
     | Check if this is a KPK pawn endgame. If it is then some positions |
     | can be discarded as draws. For other positions (or for KPK        |
     | positions that are unclear) use the quadrant rule instead.        |
     | TODO: I'm not totally sure that this heuristic is 100% correct.   |
     | Check this out.                                                   |
     --------------------------------------------------------------------*/
    if (bitcount(board->piece[WHITE][PAWN]) == 1
	&& !board->piece[BLACK][PAWN]) {
      int pawn_pos = get_first_bitpos(board->piece[WHITE][PAWN]);
      if (square2row[wsquare] == square2row[pawn_pos] + 1
	  && square[wsquare] & adj_cols[square2col[pawn_pos]]
	  && square2row[bsquare] == square2row[pawn_pos] - 2
	  && square[bsquare] & adj_cols[square2col[pawn_pos]]
	  && Color == WHITE && square2row[bsquare] > 0) {
	*wval = draw_score();
	*bval = 0;
	return;
      }
    } else if (bitcount(board->piece[BLACK][PAWN]) == 1
	&& !board->piece[WHITE][PAWN]) {
      int pawn_pos = get_first_bitpos(board->piece[BLACK][PAWN]);
      if (square2row[bsquare] == square2row[pawn_pos] - 1
	  && square[bsquare] & adj_cols[square2col[pawn_pos]]
	  && square2row[wsquare] == square2row[pawn_pos] + 2
	  && square[wsquare] & adj_cols[square2col[pawn_pos]]
	  && Color == BLACK && square2row[wsquare] < 7) {
	*bval = draw_score();
	*wval = 0;
	return;
      }
    }

    /*-------------------------------------------------------
     | Quadrant rule: make sure the opponent doesn't get an |
     | unstoppable passed pawn.                             |
     -------------------------------------------------------*/
    pawns = pawn_struct.passed_pawn[WHITE] & board->piece[WHITE][PAWN];
    while (pawns != 0) {
      pawn = get_first_bitpos(pawns);
      king_dist = dist_to_square[bsquare]
	[get_first_bitpos(col_bitboard[pawn%8]
			  & pawn_lastrow[WHITE])];
      prom_sq = get_first_bitpos(col_bitboard[pawn%8]
				 & pawn_lastrow[WHITE]);
      dist = dist_to_square[pawn][prom_sq]
	//white gets one move penalty if the king is
	//in front of the pawn
	+ (square2col[wsquare] == square2col[pawn]
	   && square2row[wsquare] < square2row[pawn] ? 1 : 0)
	//white gets one free move if it's on the starting square
	//since then it can go two steps forward in one move
	- (square2row[pawn] == 6 ? 1 : 0)
	
	- king_dist
	//black gets a free move if black's turn
	+ (Color == BLACK ? 1: 0);
      if (dist < 0) {
	if (dist + king_dist - (Color == BLACK ? 1: 0) < w_shortest_dist) {
	  w_shortest_dist = dist + king_dist - (Color == BLACK ? 1: 0);
	  /* Check if this promotion checks black's king. */
	  whites_promotion_checks_blacks_king = 0;
	  //South east
	  s = prom_sq+9;
	  while (s <= 63 && s%8 != 0 && (blockers & square[s]) == 0) {
	    if (s == wsquare)
	      break;
	    if (s == bsquare) {
	      whites_promotion_checks_blacks_king = 1;
	      //printf("Vit schackar svart pa %d\n",s);
	      break;
	    }
	    s += 9;
	  }
	  if (!whites_promotion_checks_blacks_king) {
	    //South west
	    if (prom_sq%8 > 0) {
	      s = prom_sq+7;
	      while (s <= 63 && s%8 >= 0 && (blockers & square[s]) == 0) {
		if (s == wsquare)
		  break;
		if (s == bsquare) {
		  whites_promotion_checks_blacks_king = 1;
		  //printf("Vit schackar svart pa %d\n",s);
		  break;
		}
		if (s%8 == 0)
		  break;
		s += 7;
	      }
	    }
	  }
	  if (!whites_promotion_checks_blacks_king) {
	    //West
	    if (prom_sq%8 > 0) {
	      s = prom_sq-1;
	      while (s%8 >= 0 && (blockers & square[s]) == 0) {
		if (s == wsquare)
		  break;
		if (s == bsquare) {
		  whites_promotion_checks_blacks_king = 1;
		  //printf("Vit schackar svart pa %d\n",s);
		  break;
		}
		if (s%8 == 0)
		  break;
		s -= 1;
	      }
	    }
	  }
	  if (!whites_promotion_checks_blacks_king) {
	    //East
	    s = prom_sq+1;
	    while (s%8 != 0 && (blockers & square[s]) == 0) {
	      if (s == wsquare)
		break;
	      if (s == bsquare) {
		whites_promotion_checks_blacks_king = 1;
		//printf("Vit schackar svart pa %d\n",s);
		break;
	      }
	      s += 1;
	    }
	  }
	  //printf("bsquare = %d\n",bsquare);
	  if (!whites_promotion_checks_blacks_king) {
	    //South
	    s = prom_sq+8;
	    //printf("latte1 = %d\n",(blockers & square[s]) == 0);
	    while (s <= 63 && ((blockers & square[s]) == 0 || s == pawn)) {
	      //printf("s = %d\n",s);
	      if (s == wsquare)
		break;
	      if (s == bsquare) {
		whites_promotion_checks_blacks_king = 1;
		//printf("Vit schackar svart pa %d\n",s);
		break;
	      }
	      s += 8;
	      //printf("latte2 = %d\n",(blockers & square[s]) == 0);
	    }
	  }

	  /* Check if the promotion square is on the A or H file. */
	  if (prom_sq == 0 || prom_sq == 7)
	    white_ah = 1;
	  else
	    white_ah = 0;
	  white_prom_sq = prom_sq;
	}
	//*bval -= 600;
	//break;
	possible_white_promotions |= square[pawn];
      }
      pawns &= ~square[pawn];
    }

    pawns = pawn_struct.passed_pawn[BLACK] & board->piece[BLACK][PAWN];
    while (pawns != 0) {
      pawn = get_first_bitpos(pawns);
      king_dist = dist_to_square[wsquare]
	[get_first_bitpos(col_bitboard[pawn%8]
			  & pawn_lastrow[BLACK])];
      prom_sq = get_first_bitpos(col_bitboard[pawn%8]
				 & pawn_lastrow[BLACK]);
      dist = dist_to_square[pawn][prom_sq]
	//black gets one move penalty if the king is
	//in front of the pawn
	+ (square2col[bsquare] == square2col[pawn]
	   && square2row[bsquare] > square2row[pawn] ? 1 : 0)
	//black gets one free move if it's on the starting square
	//since then it can go two steps forward in one move
	- (square2row[pawn] == 1 ? 1 : 0)
	- king_dist
	  //white gets a free move if white's turn
	+ (Color == WHITE ? 1: 0);
      if (dist < 0) {
	if (dist + king_dist - (Color == WHITE ? 1: 0) < b_shortest_dist) {
	  b_shortest_dist = dist + king_dist - (Color == WHITE ? 1: 0);
	  /* Check if this promotion checks white's king. */
	  blacks_promotion_checks_whites_king = 0;
	  //North east
	  s = prom_sq-7;
	  while (s >= 0 && s%8 != 0 && (blockers & square[s]) == 0) {
	    if (s == bsquare)
	      break;
	    if (s == wsquare) {
	      blacks_promotion_checks_whites_king = 1;
	      //printf("Svart schackar vit pa %d\n",s);
	      break;
	    }
	    s -= 7;
	  }
	  if (!blacks_promotion_checks_whites_king) {
	    //North west
	    if (prom_sq%8 > 0) {
	      s = prom_sq-9;
	      //printf("latte1 = %d\n",(blockers & square[s]) == 0);
	      while (s >= 0 && s%8 >= 0 && (blockers & square[s]) == 0) {
		if (s == bsquare)
		  break;
		if (s == wsquare) {
		  blacks_promotion_checks_whites_king = 1;
		  //printf("Svart schackar vit pa %d\n",s);
		  break;
		}
		if (s%8 == 0)
		  break;
		s -= 9;
	      }
	    }
	  }
	  if (!blacks_promotion_checks_whites_king) {
	    //West
	    if (prom_sq%8 > 0) {
	      s = prom_sq-1;
	      while (s%8 >= 0 && (blockers & square[s]) == 0) {
		if (s == bsquare)
		  break;
		if (s == wsquare) {
		  blacks_promotion_checks_whites_king = 1;
		  //printf("Svart schackar vit pa %d\n",s);
		  break;
		}
		if (s%8 == 0)
		  break;
		s -= 1;
	      }
	    }
	  }
	  if (!blacks_promotion_checks_whites_king) {
	    //East
	    s = prom_sq+1;
	    while (s%8 != 0 && (blockers & square[s]) == 0) {
	      if (s == bsquare)
		break;
	      if (s == wsquare) {
		blacks_promotion_checks_whites_king = 1;
		//printf("Svart schackar vit pa %d\n",s);
		break;
	      }
	      s += 1;
	    }
	  }
	  if (!blacks_promotion_checks_whites_king) {
	    //North
	    s = prom_sq-8;
	    while (s >= 0 && ((blockers & square[s]) == 0 || s == pawn)) {
	      if (s == bsquare)
		break;
	      if (s == wsquare) {
		blacks_promotion_checks_whites_king = 1;
		//printf("Svart schackar vit pa %d\n",s);
		break;
	      }
	      s -= 8;
	    }
	  }

	  /* Check if the promotion square is on the A or H file. */
	  if (prom_sq == 56 || prom_sq == 63)
	    black_ah = 1;
	  else
	    black_ah = 0;
	  black_prom_sq = prom_sq;
	}
	//*wval -= 600;
	//break;
	possible_black_promotions |= square[pawn];
      }
      pawns &= ~square[pawn];
    }

    /*-----------------------------------------------------------------
     | For unequal pawn races only reward the pawn that first queens. |
     -----------------------------------------------------------------*/
    //printf("w_shortest_dist = %d\n",w_shortest_dist);
    //printf("b_shortest_dist = %d\n",b_shortest_dist);
    if ((Color == WHITE && w_shortest_dist < b_shortest_dist)
	|| (Color == BLACK && w_shortest_dist + 1 < b_shortest_dist)) {
      //printf("lagg1\n");
      *bval -= 600;
    } else if ((Color == BLACK && b_shortest_dist < w_shortest_dist)
	|| (Color == WHITE && b_shortest_dist + 1 < w_shortest_dist)) {
      //printf("lagg2\n");
      *wval -= 600;
    } else if (w_shortest_dist < 100 && b_shortest_dist < 100) {
      /*---------------------------------------------------------------
       | For equal pawn races there are two factors to consider:      |
       | 1.) If the first pawn that promotes conrols the promotion    |
       | square of the other pawn, then only the first pawn will make |
       | it (this is only possible during A/H pawn races). Exception  |
       | is if one's own king is supporting the pawn of one's own     |
       | color OR if some piece is in the way and it's not in check.  |
       | 2.) If the first pawn that promotes checks the other king    |
       | on promotion, then only the first pawn will make it.         |
       ---------------------------------------------------------------*/
      if (white_ah && black_ah) {
	//printbitboard(get_ray(7,7));
	//exit(1);
	if ((Color == WHITE && w_shortest_dist == b_shortest_dist)
	    || (Color == BLACK && b_shortest_dist == w_shortest_dist + 1)) {
	  //printf("laff1\n");
	  if (white_prom_sq == 0 && black_prom_sq == 56 &&
	      ((bsquare == 57 || bsquare == 49)
	       || (get_ray(0,8) & (board->all_pieces[WHITE]
				   | board->all_pieces[BLACK])
		   && !whites_promotion_checks_blacks_king))) {
	    //do nothing if white_prom_sq = A8 && black_prom_sq = A1
	    //and black supports its own promotion square or if some
	    //piece is in the way and it's not check
	    //printf("laff2\n");
	  } else if (white_prom_sq == 7 && black_prom_sq == 63 &&
		     ((bsquare == 62 || bsquare == 54)
		      || (get_ray(7,8) & (board->all_pieces[WHITE]
					  | board->all_pieces[BLACK])
			  && !whites_promotion_checks_blacks_king))) {
	    //do nothing if white_prom_sq = H8 && black_prom_sq = H1
	    //and black supports its own promotion square or if some
	    //piece is in the way and it's not check
	    //printf("laff3\n");
	  } else if (white_prom_sq == 0 && black_prom_sq == 63 &&
		     (bsquare == 62
		      || (get_ray(0,9) & (board->all_pieces[WHITE]
					  | board->all_pieces[BLACK])
			  && !whites_promotion_checks_blacks_king))) {
	    //do nothing if white_prom_sq = A8 && black_prom_sq = H1
	    //and black supports its own promotion square or if some
	    //piece is in the way and it's not check
	    //printf("laff4\n");
	  } else if (white_prom_sq == 7 && black_prom_sq == 56 &&
		     (bsquare == 57
		      || (get_ray(7,7) & (board->all_pieces[WHITE]
					  | board->all_pieces[BLACK])
			  && !whites_promotion_checks_blacks_king))) {
	    //do nothing if white_prom_sq = H8 && black_prom_sq = A1
	    //and black supports its own promotion square or if some
	    //piece is in the way and it's not check
	    //printf("laff5\n");
	  } else {
	    *bval -= 600;
	    //printf("laff6\n");
	  }
	} else if ((Color == BLACK && b_shortest_dist == w_shortest_dist)
		   || (Color == WHITE
		       && w_shortest_dist == b_shortest_dist + 1)) {
	  //printf("laff2\n");
	  if (white_prom_sq == 0 && black_prom_sq == 56 &&
	      ((wsquare == 1 || wsquare == 9)
	       || (get_ray(56,-8) & (board->all_pieces[WHITE]
				     | board->all_pieces[BLACK])
		   && !blacks_promotion_checks_whites_king))) {
	    //do nothing if white_prom_sq = A8 && black_prom_sq = A1
	    //and white supports its own promotion square or if some
	    //piece is in the way and it's not check
	  } else if (white_prom_sq == 7 && black_prom_sq == 63 &&
		     ((wsquare == 6 || wsquare == 14)
		      || (get_ray(63,-8) & (board->all_pieces[WHITE]
					    | board->all_pieces[BLACK])
			  && !blacks_promotion_checks_whites_king))) {
	    //do nothing if white_prom_sq = H8 && black_prom_sq = H1
	    //and white supports its own promotion square or if some
	    //piece is in the way and it's not check
	  } else if (white_prom_sq == 0 && black_prom_sq == 63 &&
		     (wsquare == 1
		      || (get_ray(63,-9) & (board->all_pieces[WHITE]
					    | board->all_pieces[BLACK])
			  && !blacks_promotion_checks_whites_king))) {
	    //do nothing if white_prom_sq = A8 && black_prom_sq = H1
	    //and white supports its own promotion square or if some
	    //piece is in the way and it's not check
	  } else if (white_prom_sq == 7 && black_prom_sq == 56 &&
		     (wsquare == 6
		      || (get_ray(56,-7) & (board->all_pieces[WHITE]
					    | board->all_pieces[BLACK])
			  && !blacks_promotion_checks_whites_king))) {
	    //do nothing if white_prom_sq = H8 && black_prom_sq = A1
	    //and white supports its own promotion square or if some
	    //piece is in the way and it's not check
	  } else {
	    *wval -= 600;
	  }
	} else {
	  printf("Error in pawn endgame.\n");
	  infolog("Error in pawn endgame.");
	  exit(1);
	}
      } else {
	//printf("Color = %d\n",Color);
	//printf("whites_promotion_checks_blacks_king = %d\n",whites_promotion_checks_blacks_king);
	//printf("blacks_promotion_checks_whites_king = %d\n",blacks_promotion_checks_whites_king);
	/*------------------------------------------------------------------
	 | If we get here we have an equal pawn race where both promotions |
	 | are not on the A/H file. In the normal case both colors will    |
	 | then queen, so it cancels out each other. The exceptional case  |
	 | is if the _first_ color to queen also checks its opponent.      |
	 | (Make sure we don't credit the _second_ player to queen if the  |
	 | queening also checks the opponent. For example don't credit     |
	 | white if white has three steps to promotion AND black has two   |
	 | steps AND it's white's turn AND white checks black on           |
	 | promotion.                                                      |
	 ------------------------------------------------------------------*/
	if ((Color == WHITE && whites_promotion_checks_blacks_king
	     && w_shortest_dist == b_shortest_dist)
	    || (Color == BLACK && whites_promotion_checks_blacks_king
	     && w_shortest_dist + 1 == b_shortest_dist))
	  *bval -= 600;
	else if ((Color == BLACK && blacks_promotion_checks_whites_king
		 && w_shortest_dist == b_shortest_dist)
		 || (Color == WHITE && blacks_promotion_checks_whites_king
		 && w_shortest_dist == b_shortest_dist + 1))
	  *wval -= 600;
      }
    }
  } else if (endgame == UNKNOWN_ENDGAME) {
    //printf("laff1\n");
    /*------------------------------------------------------------------
     | If the king is losing, then force him to the edge of the board. |
     ------------------------------------------------------------------*/
    if (board->material_pieces[BLACK] <= VAL_KING + VAL_BISHOP
	&& board->material_pawns[BLACK] <= VAL_PAWN*2
	&& pawn_struct.passed_pawn[BLACK] == 0) {
      //printf("laff2\n");
      //printf("laff2 = %d,%d,%d,%d,%d\n",dist_to_side[27],dist_to_side[28],dist_to_side[29],dist_to_side[30],dist_to_side[31]);
      *wval += (130-dist_to_side[bsquare]*40);
      *wval -= dist_to_square[wsquare][bsquare]*5;
      *wval += dist_to_side[wsquare]; //to make sure that white is on outside
      //1136,1171
      
      /**wval -= dist_to_side[bsquare]*10;
      *wval -= 2*(abs(wsquare%8-bsquare%8) + abs(wsquare/8-bsquare/8));
      *wval -= dist_to_corn[bsquare];
      *wval += dist_to_side[wsquare];*/ //1097,1106
    }
    if (board->material_pieces[WHITE] <= VAL_KING + VAL_BISHOP
	&& board->material_pawns[WHITE] <= VAL_PAWN*2
	&& pawn_struct.passed_pawn[WHITE] == 0) {
      //printf("laff3\n");
      *bval += (130-dist_to_side[wsquare]*40);
      *bval -= dist_to_square[bsquare][wsquare]*5;
      *bval += dist_to_side[bsquare]; //to make sure that black is on outside
      
      /**bval -= dist_to_side[wsquare]*10;
      *bval -= 2*(abs(wsquare%8-bsquare%8) + abs(wsquare/8-bsquare/8));
      *bval -= dist_to_corn[wsquare];
      *bval += dist_to_side[bsquare];*/
    }

    /*--------------------------------------------------------------------
     | Give a slightly higher value if the pawns are on opposite colored |
     | squares of the opponents bishop.                                  |
     --------------------------------------------------------------------*/
    if (bitcount(board->piece[BLACK][BISHOP]) == 1) {
      if (board->piece[BLACK][BISHOP] & white_squares)
	pawn_struct.white_value += 2*bitcount(board->piece[WHITE][PAWN]
					      & black_squares);
      else
	pawn_struct.white_value += 2*bitcount(board->piece[WHITE][PAWN]
					      & white_squares);
    }
    if (bitcount(board->piece[WHITE][BISHOP]) == 1) {
      if (board->piece[WHITE][BISHOP] & white_squares)
	pawn_struct.black_value += 2*bitcount(board->piece[BLACK][PAWN]
					      & black_squares);
      else
	pawn_struct.black_value += 2*bitcount(board->piece[BLACK][PAWN]
					      & white_squares);
    }
  }
}

/*-------------------------------------------------------------------
 | This function returns a value for how good a position is. A high |
 | value for the player at move means he is well off in the game.   |
 -------------------------------------------------------------------*/
int eval(struct board *board, int alpha, int beta) {
  //extern int engine_color;
  int wvalue = 0, bvalue = 0;
  int full_eval = 1, do_mobility;
  int temp_score;

  /*------------------------------
   | Check the material balance. |
   ------------------------------*/
  wvalue += board->material_pieces[WHITE] + board->material_pawns[WHITE];
  bvalue += board->material_pieces[BLACK] + board->material_pawns[BLACK];

  /*------------------------------------------------------
   | Initialize things that have to do with king safety. |
   ------------------------------------------------------*/
  if ((board->piece[WHITE][QUEEN]
       && ((board->material_pieces[WHITE] - VAL_KING) > 1300)) ||
      ((bitcount(board->piece[WHITE][ROOK]) > 1)
       && (board->material_pieces[WHITE] - VAL_KING) > 1500))
    board->dangerous[WHITE] = 1;
  else
    board->dangerous[WHITE] = 0;
  if ((board->piece[BLACK][QUEEN]
       && ((board->material_pieces[BLACK] - VAL_KING) > 1300)) ||
      ((bitcount(board->piece[BLACK][ROOK]) > 1)
       && (board->material_pieces[BLACK] - VAL_KING) > 1500))
    board->dangerous[BLACK] = 1;
  else
    board->dangerous[BLACK] = 0;
  board->tropism[WHITE] = 0;
  board->tropism[BLACK] = 0;
  board->safety[WHITE] = 0;
  board->safety[BLACK] = 0;

  /* encourage pawn endings */
  /*if (engine_color == WHITE) {
    wvalue += (8*20 - bitcount((board->all_pieces[WHITE] & ~board->piece[WHITE][PAWN]) & ~board->piece[WHITE][KING])*20);
    //bvalue -= (8*20 - bitcount((board->all_pieces[BLACK] & ~board->piece[BLACK][PAWN]) & ~board->piece[BLACK][KING])*20);
  } else {
    //wvalue -= (8*20 - bitcount((board->all_pieces[WHITE] & ~board->piece[WHITE][PAWN]) & ~board->piece[WHITE][KING])*20);
    bvalue += (8*20 - bitcount((board->all_pieces[BLACK] & ~board->piece[BLACK][PAWN]) & ~board->piece[BLACK][KING])*20);
    }*/

  /* encourage krpkr */
  /*if (bitcount(board->piece[engine_color][ROOK]) == 1
      && bitcount(board->piece[1-engine_color][ROOK]) == 1) {
    if (engine_color == WHITE) {
      wvalue += 100;
    } else {
      bvalue += 100;
    }
  }
  if (bitcount(board->all_pieces[1-engine_color] & ~(board->piece[1-engine_color][PAWN])) > 3) {
    if (engine_color == WHITE) {
      wvalue += (8 - bitcount(board->all_pieces[1-engine_color] & ~(board->piece[1-engine_color][PAWN])))*30;
    } else {
      bvalue += (8 - bitcount(board->all_pieces[1-engine_color] & ~(board->piece[1-engine_color][PAWN])))*30;
    }
  }
  if (bitcount(board->all_pieces[engine_color] & ~(board->piece[engine_color][PAWN])) > 3) {
    if (engine_color == WHITE) {
      wvalue += (8 - bitcount(board->all_pieces[engine_color] & ~(board->piece[engine_color][PAWN])))*30;
    } else {
      bvalue += (8 - bitcount(board->all_pieces[engine_color] & ~(board->piece[engine_color][PAWN])))*30;
    }
  }
  if (bitcount(board->piece[1-engine_color][PAWN]) > 1) {
    if (engine_color == WHITE) {
      wvalue += (8 - bitcount(board->piece[1-engine_color][PAWN]))*30;
    } else {
      bvalue += (8 - bitcount(board->piece[1-engine_color][PAWN]))*30;
    }
  }
  if (bitcount(board->piece[engine_color][PAWN]) > 1) {
	if (engine_color == WHITE) {
      wvalue += (8 - bitcount(board->piece[engine_color][PAWN]))*30;
    } else {
      bvalue += (8 - bitcount(board->piece[engine_color][PAWN]))*30;
    }
  }
  if ((bitcount(board->piece[1-engine_color][PAWN]) == 1
       && bitcount(board->piece[engine_color][PAWN]) == 0)
      || (bitcount(board->piece[1-engine_color][PAWN]) == 0
	  && bitcount(board->piece[engine_color][PAWN]) == 1)) {
    if (engine_color == WHITE) {
      wvalue += 100;
    } else {
      bvalue += 100;
    }
  }
  if ((bitcount(board->all_pieces[engine_color]) == 3
       && bitcount(board->all_pieces[1-engine_color]) == 2)
      || (bitcount(board->all_pieces[engine_color]) == 2
       && bitcount(board->all_pieces[1-engine_color]) == 3)) {
    if (engine_color == WHITE) {
      wvalue += 100;
    } else {
      bvalue += 100;
    }
    }*/

  /*-------------------------------------------------------------------------
   | Add incentive to trade pieces when ahead, and trade pawns when behind. |
   -------------------------------------------------------------------------*/
  if (board->material_pieces[WHITE] > board->material_pieces[BLACK]) {
    if (board->material_pieces[BLACK] < presearch_pieceval[BLACK])
      wvalue += (presearch_pieceval[BLACK] - board->material_pieces[BLACK]) / 10;
    if (board->material_pawns[WHITE] < presearch_pawnval[WHITE])
      bvalue += 1;
  } else if (board->material_pieces[BLACK] > board->material_pieces[WHITE]) {
    if (board->material_pieces[WHITE] < presearch_pieceval[WHITE])
      bvalue += (presearch_pieceval[WHITE] - board->material_pieces[WHITE]) / 10;
    if (board->material_pawns[BLACK] < presearch_pawnval[BLACK])
      wvalue += 1;
  }

  /* Add the capture-values to make shallower captures (or promotions)
     to be worth a little bit more than deeper captures/promotions. This
     can be good if for example the engine detects that the loss of a pawn
     is inevitable to the maximum search depth. However beyond the horizon
     of the search it might turn out that it's indeed possible to save the
     pawn, and therefore we should try to keep it for as long as possible.
     Another use of this is if for example the engine has the possibility
     to promote a pawn to a queen. Then it needs some encouragement to do
     it soon, rather than waiting. */
  wvalue += board->captures[WHITE];
  bvalue += board->captures[BLACK];

  eval_pawns(board,&wvalue,&bvalue);

  eval_endgame(board,&wvalue,&bvalue);
  
  /*----------------------------------------------------------------
   | Lazy evaluation:                                              |
   | If the current evaluation is so far outside of the alpha/beta |
   | bounds that the piece evaluation cannot bring the score back  |
   | inside the bounds, then skip the piece evaluation.            |
   ----------------------------------------------------------------*/
  temp_score = (Color == WHITE ? wvalue - bvalue : bvalue - wvalue);
  if (temp_score - 350 >= beta || temp_score + 350 <= alpha)
    full_eval = 0;
  if (full_eval) {
    eval_bishops(board,&wvalue,&bvalue);
    eval_knights(board,&wvalue,&bvalue);
    eval_queens(board,&wvalue,&bvalue);
    eval_rooks(board,&wvalue,&bvalue);
    eval_kings(board,&wvalue,&bvalue);

    /*------------------------------------------------------------
     | Mobility checking is so slow that it makes amundsen play  |
     | worse if it's always done. Therefore do mobility checking |
     | only if there are lots of pieces left on the board and    |
     | it's likely that mobility can affect the bounds.          |
     ------------------------------------------------------------*/
    temp_score = (Color == WHITE ? wvalue - bvalue : bvalue - wvalue);
    if (!(temp_score - 40 >= beta
	  || temp_score + 40 <= alpha)
	&& (board->piece[WHITE][QUEEN] || board->piece[BLACK][QUEEN])
	&& (bitcount(board->piece[WHITE][PAWN])
	    + bitcount(board->piece[BLACK][PAWN]) >= 10)
	&& (board->nbr_pieces[WHITE] + board->nbr_pieces[BLACK] >= 8))
      do_mobility = 1;
    else
      do_mobility = 0;
    //TODO: Pitäisikö siirtää eval_mobility tän ehdon ulkopuolelle? Siis
    //silloin pitää samalla siirtää sitä koodia ulkopuolelle joka
    //laskee white_mobilityä ja black_mobilityä.
    if (do_mobility) {
      eval_mobility(board,&wvalue,&bvalue);
      temp_score = (Color == WHITE ? wvalue - bvalue : bvalue - wvalue);
    }
  }

  /*-------------------------------------------------
    | Drag the score gradually toward draw_score if |
    | the 50 move rule is approaching.              |
    ------------------------------------------------*/
  /*if (board->moves_left_to_draw <= 20) {
    temp_score = draw_score() + (temp_score * board->moves_left_to_draw) / 20;
    }*/

  /*---------------------------------------------------------------------
   | If the score is equal to draw_score(), then give an extra point to |
   | white. The reason for this is that draw_score's aren't stored in   |
   | the hashtable. This way we make sure that we get as many hashtable |
   | hits as possible.                                                  |
   ---------------------------------------------------------------------*/
  if (temp_score == draw_score())
    return (Color == WHITE ? temp_score+1 : temp_score-1);
  else
    return temp_score;
}
