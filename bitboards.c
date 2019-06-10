/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "bitboards.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "parse.h"
#include "alfabeta.h"

bitboard square[64];
bitboard lower[64];
bitboard higher[64];
struct attack attack;
struct defense defense;
bitboard pawn_start[2];
bitboard pawn_2nd_row[2];
bitboard pawn_lastrow[2];
bitboard pawn_passantrow[2];
bitboard possible_pawn_attack[2][64];
bitboard passed_pawn[2][64];
int pawn_forward[2][64];
int bits_in_16bits[65536];
int first_bit_in_16bits[65536];
int last_bit_in_16bits[65536];
bitboard col_bitboard[8];
bitboard right_col[8];
bitboard left_col[8];
bitboard adj_cols[8];  //bitboard of adjacent columns including the row itself
bitboard adj_cols_not_self[8];  //like adj_cols but not including the row itself
bitboard abcd_col;
bitboard efgh_col;
bitboard abc_col;
bitboard fgh_col;
bitboard d3d4d5d6;
bitboard e3e4e5e6;
bitboard ab_col;
bitboard gh_col;
int pieceval[6];
int square2col[64];
int square2row[64];
int row_nbr[64];
int dist_to_side[64];
int dist_to_corn[64];
int dist_to_square[64][64];
int direction[64][64];
bitboard ray[64][19];
bitboard ray90[64][19];
bitboard rayNE[64][19];
bitboard rayNW[64][19];
bitboard horiz_vert_cross[64];
bitboard nw_ne_cross[64];
bitboard white_squares;
bitboard black_squares;
bitboard rook_starting_square[2];
bitboard pawn_duo[64];
int king_safety[16][16];

/* Stuff for handling the sliders. */
int rotate90to0[64];
int rotate0to90[64];
int rotateNEto0[64];
int rotate0toNE[64];
int rotateNWto0[64];
int rotate0toNW[64];
int diagNE_length[64];
int diagNE_start[64];
int diagNW_length[64];
int diagNW_start[64];
int ones[9];
int connected_bits[256];

char promotion_piece[129];

void set_connected_bits() {
  int prev;
  int i, j;

  for (i = 0; i < 256; i++) {
    connected_bits[i] = 0;
    prev = (i & 1) ? 1 : 0;
    for (j = 1; j < 8; j++)
      if (i & (1<<j)) {
	if (prev)
	  connected_bits[i]+=1;
	else
	  prev = 1;
      } else
	prev = 0;
  }
}

void set_square(bitboard *lsquare) {
  int i;

  lsquare[0] = 1;
  for (i = 1; i < 64; i++)
    lsquare[i] = lsquare[i-1] << 1;
}

void set_lower(bitboard *llower) {
  int i;

  llower[0] = 0;
  llower[1] = 1;
  for (i = 2; i < 64; i++)
    llower[i] = (llower[i-1] << 1) | 1;
}

void set_higher(bitboard *lhigher) {
  int i;

  lhigher[0] = ~1;
  for (i = 1; i < 64; i++)
    lhigher[i] = lhigher[i-1] & (~((int64) pow((double)2,(double)i)));
}

void set_rotate90to0(int *rot) {
  int i;

  for (i = 0; i < 64; i++)
    rot[(i%8)*8 + 7 - i/8] = i;
}

void set_rotate0to90(int *rot) {
  int i;

  for (i = 0; i < 64; i++)
    rot[i] = (i%8)*8 + 7 - i/8;
}

void set_rotate0toNE(int *rot) {
  int lsquare = 0;
  int i, j;

  /* Do the upper left hand side of the board. */
  for (i = 0; i < 8; i++)
    for (j = 0; j < (i+1); j++)
      rot[(i-j)*8 + j] = lsquare++;

  /* Do the lower right hand side of the board. */
  for (i = 1; i < 8; i++)
    for (j = 0; j < (8-i); j++)
      rot[(7-j)*8 + j + i] = lsquare++;
}

void set_rotateNEto0(int *rot) {
  int i;

  for (i = 0; i < 64; i++)
    rot[rotate0toNE[i]] = i;
}

void set_rotate0toNW(int *rot) {
  int lsquare = 0;
  int i, j;

  /* Do the upper right hand side of the board. */
  for (i = 0; i < 8; i++)
    for (j = 0; j < (i+1); j++)
      rot[(i-j)*8 + (7-j)] = lsquare++;

  /* Do the lower left hand side of the board. */
  for (i = 1; i < 8; i++)
    for (j = 0; j < (8-i); j++)
      rot[(7-j)*8 + (7-j) - i] = lsquare++;
}

void set_rotateNWto0(int *rot) {
  int i;

  for (i = 0; i < 64; i++)
    rot[rotate0toNW[i]] = i;
}

void set_square2col(int *arr) {
  int i;
  for (i = 0; i < 64; i++)
    arr[i] = i % 8;
}

/* Note that this is counted from top to bottom, so square2row[3] = 0, etc. */
void set_square2row(int *arr) {
  int i;
  for (i = 0; i < 64; i++)
    arr[i] = i / 8;
}

void set_row_nbr(int *lrow_nbr) {
  int i;
  for (i = 0; i < 64; i++)
    lrow_nbr[i] = (63 - i) / 8;
}

void set_dist_to_side(int *ldist_to_side) {
  int i;
  /* Distance to side is minimum of rank,7-rank,file,7-file. */
  for (i = 0; i < 64; i++)
    ldist_to_side[i] = minval(minval(i/8,7-i/8),minval(i%8,7-i%8));
}

void set_dist_to_corn(int *ldist_to_corn) {
  int i;
  /* Distance to corn is minval(rank,7-rank) + minval(file,7-file). */
  for (i = 0; i < 64; i++)
    ldist_to_corn[i] = minval(i/8,7-i/8) + minval(i%8,7-i%8);
}

void set_dist_to_square(int arr[64][64]) {
  int i, j;

  /* The distance from one square to another is calculated as
     maxval(row difference, column difference). */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 64; j++)
      arr[i][j] = maxval(maxval(i/8-j/8,j/8-i/8),maxval(i%8-j%8,j%8-i%8));
}

void set_direction(int arr[64][64]) {
  int i, j;

  /* Start by zeroing all squares. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 64; j++)
      arr[i][j] = 0;

  for (i = 0; i < 64; i++)
    for (j = 0; j < 64; j++) {
      if (i != j) {
	if (i%8 == j%8) //same column
	  arr[i][j] = i < j ? 8 : -8;
	else if (i/8 == j/8) //same row
	  arr[i][j] = i < j ? 1 : -1;
	else if ((maxval(i,j) - minval(i,j)) % 7 == 0 && i%8 < j%8 && i/8 > j/8)
	  arr[i][j] = -7; //same NE-diagonal, direction upward
	else if ((maxval(i,j) - minval(i,j)) % 7 == 0 && i%8 > j%8 && i/8 < j/8)
	  arr[i][j] = 7; //same NE-diagonal, direction downward
	else if ((maxval(i,j) - minval(i,j)) % 9 == 0 && i%8 < j%8 && i/8 < j/8)
	  arr[i][j] = 9; //same NW-diagonal, direction downward
	else if ((maxval(i,j) - minval(i,j)) % 9 == 0 && i%8 > j%8 && i/8 > j/8)
	  arr[i][j] = -9; //same NW-diagonal, direction upward
      }
    }
}

void set_king_safety(int arr[16][16]) {
  int safety, tropism, edge;
  int safety_vector[16] = {
    0,  8,  20,  32,  40,  60, 80, 96,
    108, 114, 120, 144, 168, 180, 190, 200};
  int tropism_vector[16] = {
    0,  0,  0,  7,  13,  20, 32, 48,
    67, 80, 100, 120, 140, 160, 180, 200};

  for (edge = 0; edge < 32; edge++) {
    safety = edge;
    tropism = 0;
    while (tropism <= edge) {
      if (tropism < 16 && safety < 16)
        king_safety[safety][tropism] =
            (safety * safety_vector[minval(edge, 15)] +
            tropism * tropism_vector[minval(edge, 15)]) / maxval(safety + tropism, 1);
      safety--;
      tropism++;
    }
  }
}

/* About the ray handling: Since an index can't have a negative value,
   but a direction can, we always access the rays through a function call,
   that will automatically add nine to the supplied direction. */
bitboard get_ray(int target, int dir) {
  return ray[target][dir+9];
}

bitboard get_ray90(int target, int dir) {
  return ray90[target][dir+9];
}

bitboard get_rayNE(int target, int dir) {
  return rayNE[target][dir+9];
}

bitboard get_rayNW(int target, int dir) {
  return rayNW[target][dir+9];
}

void set_ray(bitboard arr[64][19]) {
  int i, j, x, y;

  /* Start by zeroing all rays. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++)
      arr[i][j] = 0;

  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++) {
      x = i % 8;
      y = i / 8;
      if (j-9 == -9) //NW
	while (x-- > 0 && y-- > 0)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == -8) //N
	while (y-- > 0)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == -7) //NE
	while (x++ < 7 && y-- > 0)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == -1) //W
	while (x-- > 0)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == 1) //E
	while (x++ < 7)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == 7) //SW
	while (x-- > 0 && y++ < 7)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == 8) //S
	while (y++ < 7)
	  arr[i][j] |= square[8*y+x];
      else if (j-9 == 9) //SE
	while (x++ < 7 && y++ < 7)
	  arr[i][j] |= square[8*y+x];
    }
}

void set_ray90(bitboard arr[64][19]) {
  int i, j, x, y;

  /* Start by zeroing all rays. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++)
      arr[i][j] = 0;

  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++) {
      x = i % 8;
      y = i / 8;
      if (j-9 == -9) //NW
	while (x-- > 0 && y-- > 0)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == -8) //N
	while (y-- > 0)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == -7) //NE
	while (x++ < 7 && y-- > 0)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == -1) //W
	while (x-- > 0)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == 1) //E
	while (x++ < 7)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == 7) //SW
	while (x-- > 0 && y++ < 7)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == 8) //S
	while (y++ < 7)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
      else if (j-9 == 9) //SE
	while (x++ < 7 && y++ < 7)
	  arr[i][j] |= square[rotate0to90[8*y+x]];
    }
}

void set_rayNE(bitboard arr[64][19]) {
  int i, j, x, y;

  /* Start by zeroing all rays. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++)
      arr[i][j] = 0;

  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++) {
      x = i % 8;
      y = i / 8;
      if (j-9 == -9) //NW
	while (x-- > 0 && y-- > 0)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == -8) //N
	while (y-- > 0)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == -7) //NE
	while (x++ < 7 && y-- > 0)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == -1) //W
	while (x-- > 0)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == 1) //E
	while (x++ < 7)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == 7) //SW
	while (x-- > 0 && y++ < 7)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == 8) //S
	while (y++ < 7)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
      else if (j-9 == 9) //SE
	while (x++ < 7 && y++ < 7)
	  arr[i][j] |= square[rotate0toNE[8*y+x]];
    }
}

void set_rayNW(bitboard arr[64][19]) {
  int i, j, x, y;

  /* Start by zeroing all rays. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++)
      arr[i][j] = 0;

  for (i = 0; i < 64; i++)
    for (j = 0; j < 19; j++) {
      x = i % 8;
      y = i / 8;
      if (j-9 == -9) //NW
	while (x-- > 0 && y-- > 0)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == -8) //N
	while (y-- > 0)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == -7) //NE
	while (x++ < 7 && y-- > 0)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == -1) //W
	while (x-- > 0)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == 1) //E
	while (x++ < 7)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == 7) //SW
	while (x-- > 0 && y++ < 7)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == 8) //S
	while (y++ < 7)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
      else if (j-9 == 9) //SE
	while (x++ < 7 && y++ < 7)
	  arr[i][j] |= square[rotate0toNW[8*y+x]];
    }
}

void set_horiz_vert_cross(bitboard arr[64]) {
  int i;

  for (i = 0; i < 64; i++) {
    arr[i] = get_ray(i,8) | get_ray(i,-1) | get_ray(i,-8) | get_ray(i,1)
      | square[i];
  }
}

void set_nw_ne_cross(bitboard arr[64]) {
  int i;

  for (i = 0; i < 64; i++) {
    arr[i] = get_ray(i,7) | get_ray(i,-9) | get_ray(i,-7) | get_ray(i,9)
      | square[i];
  }
}

void set_rook_starting_square(bitboard arr[2]) {
  arr[WHITE] = square[56] | square[63];
  arr[BLACK] = square[0] | square[7];
}

void set_col_bitboard(bitboard *lcol_bitboard) {
  int row, col;

  /* Initialize to zero. */
  for (col = 0; col < 8; col++)
    lcol_bitboard[col] = 0;

  for (col = 0; col < 8; col++)
    for (row = 0; row < 8; row++)
      lcol_bitboard[col] = lcol_bitboard[col] | square[row*8 + col];
}

void set_cols() {
  abcd_col = col_bitboard[0] | col_bitboard[1]
    | col_bitboard[2] | col_bitboard[3];
  efgh_col = col_bitboard[4] | col_bitboard[5]
    | col_bitboard[6] | col_bitboard[7];
  abc_col = col_bitboard[0] | col_bitboard[1] | col_bitboard[2];
  fgh_col = col_bitboard[5] | col_bitboard[6] | col_bitboard[7];
  ab_col = col_bitboard[0] | col_bitboard[1];
  gh_col = col_bitboard[6] | col_bitboard[7];
}

void set_adjacent_columns(bitboard *ladj_cols) {
  int i;

  ladj_cols[0] = col_bitboard[0] | col_bitboard[1];
  for (i = 1; i < 7; i++)
    ladj_cols[i] = col_bitboard[i-1] | col_bitboard[i] | col_bitboard[i+1];
  ladj_cols[7] = col_bitboard[6] | col_bitboard[7];
}

void set_adjacent_columns_not_self(bitboard *ladj_cols_not_self) {
  int i;

  for (i = 0; i < 8; i++)
    ladj_cols_not_self[i] = adj_cols[i] & ~(col_bitboard[i]);
}

void set_right_col(bitboard *lright_col) {
  int i;
  for (i = 0; i < 7; i++)
    lright_col[i] = col_bitboard[i+1];
  lright_col[7] = 0;
}

void set_left_col(bitboard *lleft_col) {
  int i;
  lleft_col[0] = 0;
  for (i = 1; i < 8; i++)
    lleft_col[i] = col_bitboard[i-1];
}

void set_ones(int *arr) {
  arr[0] = 0;
  arr[1] = 1;
  arr[2] = 1 | 2;
  arr[3] = 1 | 2 | 4;
  arr[4] = 1 | 2 | 4 | 8;
  arr[5] = 1 | 2 | 4 | 8 | 16;
  arr[6] = 1 | 2 | 4 | 8 | 16 | 32;
  arr[7] = 1 | 2 | 4 | 8 | 16 | 32 | 64;
  arr[8] = 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128;
}

void set_pieceval(int *lpieceval) {
  lpieceval[PAWN] = VAL_PAWN;
  lpieceval[KNIGHT] = VAL_KNIGHT;
  lpieceval[BISHOP] = VAL_BISHOP;
  lpieceval[ROOK] = VAL_ROOK;
  lpieceval[QUEEN] = VAL_QUEEN;
  lpieceval[KING] = VAL_KING;
}

void set_pawn_start(bitboard pawnstart[2]) {
  int i;

  pawnstart[WHITE] = 0;
  pawnstart[BLACK] = 0;
  for (i = 48; i < 56; i++)
    pawnstart[WHITE] = pawnstart[WHITE] | square[i];
  for (i = 8; i < 16; i++)
    pawnstart[BLACK] = pawnstart[BLACK] | square[i];
}

void set_pawn_2nd_row(bitboard pawn2ndrow[2]) {
  int i;

  pawn2ndrow[WHITE] = 0;
  pawn2ndrow[BLACK] = 0;
  for (i = 40; i < 48; i++)
    pawn2ndrow[WHITE] = pawn2ndrow[WHITE] | square[i];
  for (i = 16; i < 24; i++)
    pawn2ndrow[BLACK] = pawn2ndrow[BLACK] | square[i];
}

void set_pawn_lastrow(bitboard pawnlastrow[2]) {
  int i;

  pawnlastrow[WHITE] = 0;
  pawnlastrow[BLACK] = 0;
  for (i = 0; i < 8; i++)
    pawnlastrow[WHITE] = pawnlastrow[WHITE] | square[i];
  for (i = 56; i < 64; i++)
    pawnlastrow[BLACK] = pawnlastrow[BLACK] | square[i];
}

void set_pawn_passantrow(bitboard passantrow[2]) {
  int i;

  passantrow[WHITE] = 0;
  passantrow[BLACK] = 0;
  for (i = 32; i < 40; i++)
    passantrow[WHITE] = passantrow[WHITE] | square[i];
  for (i = 24; i < 32; i++)
    passantrow[BLACK] = passantrow[BLACK] | square[i];
}

void set_black_and_white_squares() {
  int i;

  white_squares = 0;
  black_squares = 0;

  for (i = 0; i < 64; i++) {
    if ((i/8 + i%8) % 2 == 0)   //check if row + col is even or odd
      white_squares |= square[i];
    else
      black_squares |= square[i];
  }
}

/* Calculates the length of each diagonal. Eg. diagNE_length[b4] == 6 */
void set_diagNE_length(int diagNElen[64]) {
  int row, col, i;

  for (row = 0; row < 8; row++) {
    col = 0;
    for (i = row+1; i < 9; i++)
      diagNElen[row*8 + col++] = i;
    i = 7;
    while (col < 8)
      diagNElen[row*8 + col++] = i--;
  }
}

/* Calculates where a certain diagonal starts. Eg. diagNE_start[b4] == a3 */
void set_diagNE_start(int diagNEstart[64]) {
  int row, col;

  /* Initialize to zero. */
  for (col = 0; col < 64; col++)
    diagNEstart[col] = 0;

  /* Calculate the upper row. */
  for (col = 0; col < 8; col++)
    diagNEstart[col] = col * 8;

  /* Calculate the upper left half of the board by copying the earlier
     calculated upper diagonal value. Eg. diagNEstart[b7] == diag45start[c8] */
  for (row = 1; row < 8; row++)
    for (col = 0; col + row < 8; col++)
      diagNEstart[row*8 + col] = diagNEstart[(row-1)*8 + col + 1];

  /* Calculate the bottom row. */
  for (col = 1; col < 8; col++)
    diagNEstart[7*8 + col] = 7*8 + col;

  /* Calculate the bottom right half of the board by copying the earlier
     calculated value that's diagonally below.
     Eg. diagNEstart[e2] == diagNEstart[d1] */
  for (row = 6; row > 0; row--)
    for (col = 8 - row; col < 8; col++)
      diagNEstart[row*8 + col] = diagNEstart[(row+1)*8 + col - 1];
}

/* Calculates the length of each diagonal. Eg. diagNW_length[f7] == 4 */
void set_diagNW_length(int diagNWlen[64]) {
  int row, col, i;

  for (row = 0; row < 8; row++) {
    col = 7;
    for (i = row+1; i < 9; i++)
      diagNWlen[row*8 + col--] = i;
    i = 7;
    while (col >= 0)
      diagNWlen[row*8 + col--] = i--;
  }
}

/* Calculates where a certain diagonal starts. Eg. diagNW_start[f6] == h4 */
void set_diagNW_start(int diagNWstart[64]) {
  int row, col;

  /* Initialize to zero. */
  for (col = 0; col < 64; col++)
    diagNWstart[col] = 0;

  /* Calculate the upper row. */
  for (col = 7; col >= 0; col--)
    diagNWstart[col] = (8-col) * 8 - 1;

  /* Calculate the upper right half of the board by copying the earlier
     calculated upper diagonal value. Eg. diagNEstart[c7] == diag45start[b8] */
  for (row = 1; row < 8; row++)
    for (col = 7; (7-col) + row < 8; col--)
      diagNWstart[row*8 + col] = diagNWstart[(row-1)*8 + col - 1];

  /* Calculate the bottom row. */
  for (col = 6; col >= 0; col--)
    diagNWstart[7*8 + col] = 7*8 + col;

  /* Calculate the bottom left half of the board by copying the earlier
     calculated value that's diagonally below.
     Eg. diagNWstart[d2] == diagNWstart[e1] */
  for (row = 6; row > 0; row--)
    for (col = row - 1; col >= 0; col--)
      diagNWstart[row*8 + col] = diagNWstart[(row+1)*8 + col + 1];
}

void set_attack_knight(bitboard *knight) {
  int rad, kol, i, j;
  int squares[12][12], temp[8][8];

  /* Empty squares and knight. */
  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      knight[(rad-2)*8 + (kol-2)] = 0;
      squares[rad][kol] = 0;
    }

  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      squares[rad-2][kol+1] = 1;
      squares[rad-1][kol+2] = 1;
      squares[rad+1][kol+2] = 1;
      squares[rad+2][kol+1] = 1;
      squares[rad+2][kol-1] = 1;
      squares[rad+1][kol-2] = 1;
      squares[rad-1][kol-2] = 1;
      squares[rad-2][kol-1] = 1;
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++)
	  temp[i-2][j-2] = squares[i][j];
      for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++)
	  if (temp[i][j])
	    knight[(rad-2)*8 + (kol-2)] = knight[(rad-2)*8+(kol-2)]
	      | square[i*8+j];
      /* Empty squares. */
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++)
	  squares[i][j] = 0;
    }
}

void set_attack_king(bitboard *king) {
  int rad, kol, i, j;
  int squares[12][12], temp[8][8];

  /* Empty squares and king. */
  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      king[(rad-2)*8 + (kol-2)] = 0;
      squares[rad][kol] = 0;
    }

  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      squares[rad-1][kol+1] = 1;
      squares[rad][kol+1] = 1;
      squares[rad+1][kol+1] = 1;
      squares[rad+1][kol] = 1;
      squares[rad+1][kol-1] = 1;
      squares[rad][kol-1] = 1;
      squares[rad-1][kol-1] = 1;
      squares[rad-1][kol] = 1;
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++)
	  temp[i-2][j-2] = squares[i][j];
      for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++)
	  if (temp[i][j])
	    king[(rad-2)*8 + (kol-2)] = king[(rad-2)*8+(kol-2)]
	      | square[i*8+j];
      /* Empty squares. */
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++)
	  squares[i][j] = 0;
    }
}

void set_attack_hslider(bitboard hslider[64][256]) {
  int kol, occ, i, file;

  /* Initialize to zero. */
  for (i = 0; i < 64; i++)
    for (kol = 0; kol < 256; kol++)
      hslider[i][kol] = 0;

  for (kol = 0; kol < 64; kol++) {
    file = kol % 8;
    for (occ = 0; occ < 256; occ++) {
      if (file > 0) {
	for (i = file - 1; i >= 0; i--) {  //go to the left from current file
	  /* In this attack bitboard, the first encountered
	     piece should be capturable. */
	  hslider[kol][occ] = hslider[kol][occ] | square[kol + i - file];
	  if ((1 << i) & occ)     //if we found a piece, then stop
	    break;
	}
      }
      if (file < 7) {
	for (i = file + 1; i < 8; i++) {  //go to the right from current file
	  hslider[kol][occ] = hslider[kol][occ] | square[kol + i - file];
	  if ((1 << i) & occ)     //if we found a piece, then stop
	    break;
	}
      }
    }
  }
}

void set_attack_vslider(bitboard vslider[64][256]) {
  int kol, occ, i, rank;

  /* Initialize to zero. */
  for (i = 0; i < 64; i++)
    for (kol = 0; kol < 256; kol++)
      vslider[i][kol] = 0;

  for (kol = 0; kol < 64; kol++) {
    rank = kol / 8;
    for (occ = 0; occ < 256; occ++) {
      if (rank < 7) {
	for (i = rank + 1; i < 8; i++) {  //go south from the current rank
	  /* In this attack bitboard, the first encountered
	     piece should be capturable. */
	  vslider[kol][occ] = vslider[kol][occ] | square[kol + (i-rank)*8];
	  if ((1 << (7-i)) & occ)     //if we found a piece, then stop
	    break;
	}
      }
      if (rank > 0) {
	for (i = rank - 1; i >= 0; i--) {  //go north from the current piece
	  vslider[kol][occ] = vslider[kol][occ] | square[kol + (i-rank)*8];
	  if ((1 << (7-i)) & occ)
	    break;
	}
      }
    }
  }
}

void set_attack_sliderNE(bitboard sliderNE[64][256]) {
  int kol, occ, file, i, j, rank;

  /* Initialize to zero. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 256; j++)
      sliderNE[i][j] = 0;

  for (kol = 0; kol < 64; kol++) {
    file = kol % 8;
    rank = kol / 8;
    for (occ = 0; occ < (1 << diagNE_length[kol]); occ++) {
      if (file + rank < 8) {  //if we are at the upper left half of the board
	if (file > 0) {
	  j = 1;
	  /* Go southwest from the current square until you hit the side. */
	  for (i = file - 1; i >= 0; i--) {
	    /* In this attack bitboard, the first encountered
	       piece should be capturable. */
	    sliderNE[kol][occ] = sliderNE[kol][occ] | square[kol + j*7];
	    j++;
	    if ((1 << i) & occ)  //if we found a piece, then stop
	      break;
	  }
	}
	if (file < diagNE_length[kol]) {
	  j = 1;
	  /* Go northeast from the current square until you hit the side. */
	  for (i = file + 1; i < diagNE_length[kol]; i++) {
	    sliderNE[kol][occ] = sliderNE[kol][occ] | square[kol - j*7];
	    j++;
	    if ((1 << i) & occ)  //if we found a piece, then stop
	      break;
	  }
	}
      } else {     //we are at the lower right half of the board
	if (rank < 7) {
	  /* Go southwest from the current square until you hit the side. */
	  j = rank + 1;   //j indicates which row we're at
	  for (i = file - 1; i >= (diagNE_start[kol] % 8); i--) {
	    sliderNE[kol][occ] = sliderNE[kol][occ] | square[j*8 + i];
	    if ((1 << (7-j)) & occ)   //if we found a piece, then stop
	      break;
	    j++;
	  }
	}
	if (file < 7) {
	  /* Go northeast from the current square until you hit the side. */
	  j = rank - 1;   //j indicates which row we're at
	  for (i = file + 1; i < 8; i++) {
	    sliderNE[kol][occ] = sliderNE[kol][occ] | square[j*8 + i];
	    if ((1 << (7 - j)) & occ)
	      break;
	    j--;
	  }
	}
      }
    }
  }
}

void set_attack_sliderNW(bitboard sliderNW[64][256]) {
  int kol, occ, file, i, j, rank;

  /* Initialize to zero. */
  for (i = 0; i < 64; i++)
    for (j = 0; j < 256; j++)
      sliderNW[i][j] = 0;

  for (kol = 0; kol < 64; kol++) {
    file = kol % 8;
    rank = kol / 8;
    for (occ = 0; occ < (1 << diagNW_length[kol]); occ++) {
      if ((7-file) + rank < 8) {  //if we are at the upper right half of the board
	if (file < 7) {
	  j = 1;
	  /* Go southeast from the current square until you hit the side. */
	  for (i = file + 1; i <= 7; i++) {
	    /* In this attack bitboard, the first encountered piece should
	       be capturable. */
	    sliderNW[kol][occ] = sliderNW[kol][occ] | square[kol + j*9];
	    j++;
	    if ((1 << (7-i)) & occ)  //if we found a piece, then stop
	      break;
	  }
	}
	if (rank > 0) {
	  j = 1;
	  /* Go northwest from the current square until you hit the side. */
	  for (i = file - 1; (7-i) < diagNW_length[kol]; i--) {
	    sliderNW[kol][occ] = sliderNW[kol][occ] | square[kol - j*9];
	    j++;
	    if ((1 << (7-i)) & occ)  //if we found a piece, then stop
	      break;
	  }
	}
      } else {     //we are at the lower left half of the board
	if (rank < 7) {
	  /* Go southeast from the current square until you hit the side. */
	  j = rank + 1;   //j indicates which row we're at
	  for (i = file + 1; i <= (diagNW_start[kol] % 8); i++) {
	    sliderNW[kol][occ] = sliderNW[kol][occ] | square[j*8 + i];
	    if ((1 << (7-j)) & occ)   //if we found a piece, then stop
	      break;
	    j++;
	  }
	}
	if (file > 0) {
	  /* Go northwest from the current square until you hit the side. */
	  j = rank - 1;   //j indicates which row we're at
	  for (i = file - 1; i >= 0; i--) {
	    sliderNW[kol][occ] = sliderNW[kol][occ] | square[j*8 + i];
	    if ((1 << (7 - j)) & occ)
	      break;
	    j--;
	  }
	}
      }
    }
  }
}

/* The attack bitboard for the pawns represent the squares to which a
   pawn can attack, and not the squares it can go to without a capture.
   Normally piece color is irrelevant as far as the attack bitboard
   is concerned. The exception is handing of the pawns, where they move
   in opposite directions. */
void set_attack_pawn(bitboard pawn[2][64]) {
  int rad, kol, i, j;
  int wsquares[12][12], bsquares[12][12], wtemp[8][8], btemp[8][8];

  /* Empty squares and pawn. */
  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      pawn[WHITE][(rad-2)*8 + (kol-2)] = 0;
      pawn[BLACK][(rad-2)*8 + (kol-2)] = 0;
      wsquares[rad][kol] = 0;
      bsquares[rad][kol] = 0;
    }

  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      wsquares[rad-1][kol-1] = 1;
      wsquares[rad-1][kol+1] = 1;
      bsquares[rad+1][kol-1] = 1;
      bsquares[rad+1][kol+1] = 1;
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++) {
	  wtemp[i-2][j-2] = wsquares[i][j];
	  btemp[i-2][j-2] = bsquares[i][j];
	}
      for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++) {
	  if (wtemp[i][j])
	    pawn[WHITE][(rad-2)*8 + (kol-2)] |= square[i*8+j];
	  if (btemp[i][j])
	    pawn[BLACK][(rad-2)*8 + (kol-2)] |= square[i*8+j];
	}
      /* Empty squares. */
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++) {
	  wsquares[i][j] = 0;
	  bsquares[i][j] = 0;
	}
    }
}

/* The defense-bitboards for the pawns can be used to answer questions
   such as "What pawns are defending square e5?" */
void set_defense_pawn(bitboard pawn[2][64]) {
  int rad, kol, i, j;
  int wsquares[12][12], bsquares[12][12], wtemp[8][8], btemp[8][8];

  /* Empty squares and pawn. */
  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      pawn[WHITE][(rad-2)*8 + (kol-2)] = 0;
      pawn[BLACK][(rad-2)*8 + (kol-2)] = 0;
      wsquares[rad][kol] = 0;
      bsquares[rad][kol] = 0;
    }

  for (rad = 2; rad < 10; rad++)
    for (kol = 2; kol < 10; kol++) {
      wsquares[rad+1][kol-1] = 1;
      wsquares[rad+1][kol+1] = 1;
      bsquares[rad-1][kol-1] = 1;
      bsquares[rad-1][kol+1] = 1;
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++) {
	  wtemp[i-2][j-2] = wsquares[i][j];
	  btemp[i-2][j-2] = bsquares[i][j];
	}
      for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++) {
	  if (wtemp[i][j])
	    pawn[WHITE][(rad-2)*8 + (kol-2)] |= square[i*8+j];
	  if (btemp[i][j])
	    pawn[BLACK][(rad-2)*8 + (kol-2)] |= square[i*8+j];
	}
      /* Empty squares. */
      for (i = 2; i < 10; i++)
	for (j = 2; j < 10; j++) {
	  wsquares[i][j] = 0;
	  bsquares[i][j] = 0;
	}
    }
}

/* The possible_pawn_attack represents squares that pawns can possibly
   attack either at the current position, or by advancing. */
void set_possible_pawn_attack(bitboard arr[2][64]) {
  int i;

  for (i = 0; i < 48; i++) {
    arr[WHITE][i] =
      (higher[get_first_bitpos(defense.pawn[WHITE][i])]
       & col_bitboard[get_first_bitpos(defense.pawn[WHITE][i])%8])
      | square[get_first_bitpos(defense.pawn[WHITE][i])]
      | (higher[get_last_bitpos(defense.pawn[WHITE][i])]
	 & col_bitboard[get_last_bitpos(defense.pawn[WHITE][i])%8])
      | square[get_last_bitpos(defense.pawn[WHITE][i])];
  }
  for (i = 48; i < 64; i++)
    arr[WHITE][i] = (bitboard) 0;
  for (i = 16; i < 64; i++) {
    arr[BLACK][i] =
      (lower[get_first_bitpos(defense.pawn[BLACK][i])]
       & col_bitboard[get_first_bitpos(defense.pawn[BLACK][i])%8])
      | square[get_first_bitpos(defense.pawn[BLACK][i])]
      | (lower[get_last_bitpos(defense.pawn[BLACK][i])]
	 & col_bitboard[get_last_bitpos(defense.pawn[BLACK][i])%8])
      | square[get_last_bitpos(defense.pawn[BLACK][i])];
  }
  for (i = 0; i < 16; i++)
    arr[BLACK][i] = (bitboard) 0;
}

/* The passed_pawn array is used for detecting passed pawns. */
void set_passed_pawn(bitboard arr[2][64]) {
  int i;

  for (i = 0; i < 48; i++) {
    arr[BLACK][i] =
      (higher[get_first_bitpos(defense.pawn[WHITE][i])]
       & (col_bitboard[get_first_bitpos(defense.pawn[WHITE][i])%8]
	  | col_bitboard[square2col[i]]))
      | (higher[get_last_bitpos(defense.pawn[WHITE][i])]
	 & (col_bitboard[get_last_bitpos(defense.pawn[WHITE][i])%8]
	    | col_bitboard[square2col[i]]))
      | defense.pawn[WHITE][i]
      | square[i+8];
  }
  for (i = 48; i < 64; i++)
    arr[BLACK][i] = (bitboard) 0;
  for (i = 16; i < 64; i++) {
    arr[WHITE][i] =
      (lower[get_first_bitpos(defense.pawn[BLACK][i])]
       & (col_bitboard[get_first_bitpos(defense.pawn[BLACK][i])%8]
	  | col_bitboard[square2col[i]]))
      | (lower[get_last_bitpos(defense.pawn[BLACK][i])]
	 & (col_bitboard[get_last_bitpos(defense.pawn[BLACK][i])%8]
	    | col_bitboard[square2col[i]]))
      | defense.pawn[BLACK][i]
      | square[i-8];
  }
  for (i = 0; i < 16; i++)
    arr[WHITE][i] = (bitboard) 0;
}

int iterated_bitcount(int64 n) {
  int count = 0;

  while (n) {
    count += n & 0x1;
    n >>= 1;
  }
  return count;
}

void set_pawn_forward(int arr[2][64]) {
  int i, j;

  for (i = 0; i < 2; i++)
    for (j = 0; j < 64; j++) {
      if (i == WHITE) {
	arr[i][j] = get_first_bitpos(square[j] >> 8);
      } else {
	arr[i][j] = get_first_bitpos(square[j] << 8);
      }
    }

  for (i = 0; i < 8; i++) {
    arr[WHITE][i] = -1;
    arr[BLACK][i] = -1;
  }
  for (i = 56; i < 64; i++) {
    arr[WHITE][i] = -1;
    arr[BLACK][i] = -1;
  }
}

void set_bits_in_16bits(int *arr) {
  int i;

  for (i = 0; i < 65536; i++) {
    arr[i] = iterated_bitcount((int64)i);
  }
}

void set_first_bit_in_16bits(int *arr) {
  int i;

  for (i = 0; i < 65536; i++) {
    arr[i] = fbit((int64)i);
  }
}

void set_last_bit_in_16bits(int *arr) {
  int i;

  for (i = 0; i < 65536; i++) {
    arr[i] = lbit((int64)i);
  }
}

/* This function returns the number of bits that are set in an int64. */
int bitcount(int64 n) {
  return bits_in_16bits[n & 0xffff]
    + bits_in_16bits[(n >> 16) & 0xffff]
    + bits_in_16bits[(n >> 32) & 0xffff]
    + bits_in_16bits[(n >> 48) & 0xffff];
}

/* This function returns the number of the first set bit in an int64.
   The search is done from LSB to MSB. */
int get_first_bitpos(int64 n) {
  if (first_bit_in_16bits[n & 0xffff] >= 0)
    return first_bit_in_16bits[n & 0xffff];
  if (first_bit_in_16bits[(n >> 16) & 0xffff] >= 0)
    return first_bit_in_16bits[(n >> 16) & 0xffff] + 16;
  if (first_bit_in_16bits[(n >> 32) & 0xffff] >= 0)
    return first_bit_in_16bits[(n >> 32) & 0xffff] + 32;
  if (first_bit_in_16bits[(n >> 48) & 0xffff] >= 0)
    return first_bit_in_16bits[(n >> 48) & 0xffff] + 48;

  return -1;
}

/* This function returns the number of the first set bit in an int64.
   The search is done from MSB to LSB. */
int get_last_bitpos(int64 n) {
  if (last_bit_in_16bits[(n >> 48) & 0xffff] >= 0)
    return last_bit_in_16bits[(n >> 48) & 0xffff] + 48;
  if (last_bit_in_16bits[(n >> 32) & 0xffff] >= 0)
    return last_bit_in_16bits[(n >> 32) & 0xffff] + 32;
  if (last_bit_in_16bits[(n >> 16) & 0xffff] >= 0)
    return last_bit_in_16bits[(n >> 16) & 0xffff] + 16;
  if (last_bit_in_16bits[n & 0xffff] >= 0)
    return last_bit_in_16bits[n & 0xffff];

  return -1;
}

void set_pawn_duo() {
  int i;

  for (i = 0; i < 64; i++) {
    if (square2col[i] == 0)
      pawn_duo[i] = square[i+1];
    if (square2col[i] == 7)
      pawn_duo[i] = square[i-1];
    else
      pawn_duo[i] = square[i-1] | square[i+1];
  }
}

void set_bitboards() {
  set_square(square);
  set_lower(lower);
  set_higher(higher);
  set_bits_in_16bits(bits_in_16bits);
  set_first_bit_in_16bits(first_bit_in_16bits);
  set_last_bit_in_16bits(last_bit_in_16bits);
  set_col_bitboard(col_bitboard);
  set_right_col(right_col);
  set_left_col(left_col);
  set_rotate90to0(rotate90to0);
  set_rotate0to90(rotate0to90);
  set_rotate0toNE(rotate0toNE);
  set_rotateNEto0(rotateNEto0);
  set_rotate0toNW(rotate0toNW);
  set_rotateNWto0(rotateNWto0);
  set_square2col(square2col);
  set_square2row(square2row);
  set_row_nbr(row_nbr);
  set_dist_to_side(dist_to_side);
  set_dist_to_corn(dist_to_corn);
  set_dist_to_square(dist_to_square);
  set_direction(direction);
  set_ray(ray);
  set_ray90(ray90);
  set_rayNE(rayNE);
  set_rayNW(rayNW);
  set_horiz_vert_cross(horiz_vert_cross);
  set_nw_ne_cross(nw_ne_cross);
  set_rook_starting_square(rook_starting_square);
  set_adjacent_columns(adj_cols);
  set_adjacent_columns_not_self(adj_cols_not_self);
  set_ones(ones);
  set_pieceval(pieceval);
  set_pawn_start(pawn_start);
  set_pawn_2nd_row(pawn_2nd_row);
  set_pawn_lastrow(pawn_lastrow);
  set_pawn_passantrow(pawn_passantrow);
  set_king_safety(king_safety);
  set_diagNE_length(diagNE_length);
  set_diagNE_start(diagNE_start);
  set_diagNW_length(diagNW_length);
  set_diagNW_start(diagNW_start);
  set_attack_knight(attack.knight);
  set_attack_king(attack.king);
  set_attack_hslider(attack.hslider);
  set_attack_vslider(attack.vslider);
  set_attack_sliderNE(attack.sliderNE);
  set_attack_sliderNW(attack.sliderNW);
  set_attack_pawn(attack.pawn);
  set_defense_pawn(defense.pawn);
  set_possible_pawn_attack(possible_pawn_attack);
  set_passed_pawn(passed_pawn);
  set_connected_bits();
  set_black_and_white_squares();
  set_pawn_forward(pawn_forward);
  set_cols();
  set_pawn_duo();
  promotion_piece[QUEEN_PROMOTION_MOVE] = QUEEN;
  promotion_piece[ROOK_PROMOTION_MOVE] = ROOK;
  promotion_piece[BISHOP_PROMOTION_MOVE] = BISHOP;
  promotion_piece[KNIGHT_PROMOTION_MOVE] = KNIGHT;
  d3d4d5d6 = square[43] | square[35] | square[27] | square[19];
  e3e4e5e6 = square[44] | square[36] | square[28] | square[20];
}

/* This function returns the position of the first bit that is set.
   We search from LSB to MSB. -1 is returned if no bit is set. */
int fbit(int64 bitpattern) {
  int i;
  int low = 0, high = 63, middle = 31;

  /* A one can always be found in at most 6 iterations. We need one
     more than that though, when the most significant bit is set. */
  for (i = 0; i < 7; i++) {
    if (lower[middle] & bitpattern) {        //exists below middle
      high = middle - 1;
      middle = (low + high)/2;
    } else if (square[middle] & bitpattern)  //exists in the middle
      return middle;
    else {
      low = middle + 1;
      middle = (low + high)/2;
    }
  }
  return -1;   //no bit set in the bitpattern
}

/* This function returns the position of the first bit that is set.
   We search from MSB to LSB. -1 is returned if no bit is set. */
int lbit(int64 bitpattern) {
  int i;
  int low = 0, high = 63, middle = 31;

  /* A one can always be found in at most 6 iterations. We need one
     more than that though, when the most significant bit is set. */
  for (i = 0; i < 7; i++) {
    if (higher[middle] & bitpattern) {        //exists above middle
      low = middle + 1;
      middle = (low + high)/2;
    } else if (square[middle] & bitpattern)  //exists in the midde
      return middle;
    else {
      high = middle - 1;
      middle = (low + high)/2;
    }
  }
  return -1;   //no bit set in the bitpattern
}
