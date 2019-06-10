#ifndef _BITBOARDS_
#define _BITBOARDS_

typedef long long int int64;
typedef int64 bitboard;

#define LONG_CASTLING_OK 1
#define SHORT_CASTLING_OK 2
#define CASTLED 4

struct board {
  bitboard piece[2][6];       //2 colors, black or white, and then 6
                              //types of pieces of each color
  bitboard all_pieces[2];
  bitboard rot90_pieces[2];   //pieces that are rotated 90 degrees
  bitboard rotNE_pieces[2];   //board that is rotated to northeast (a1-h8 diag)
  bitboard rotNW_pieces[2];   //board that is rotated to northwest (h1-a8 diag)

  char castling_status[2];    //describes castling status
                              //(see #define-clause above)
  char passant;               //indicates if passant move is possible
  signed char captures[2];    //used for determining at what depth a
                              //piece is captured
  char moves_left_to_draw;    //used to implement the 50-move-rule
  int move_number;            //keeps track of the number of half moves
                              //played so far
  char color_at_move;         //keeps track of whose turn it is
  int64 zobrist_key;          //used for the hashtable probes
  int64 zobrist_pawn_key;     //used for the pawn hashtable

  /*--------------------------------------------------------------------
   | Store the material value of the pieces and pawns, so that doesn't |
   | need to be calculated at each leaf node.                          |
   --------------------------------------------------------------------*/
  int material_pieces[2];
  int material_pawns[2];

  /*--------------------------------------------------------------------
   | Store the number of the pieces and pawns => quicker than counting |
   | the bits whenever one needs to find out the number.               |
   --------------------------------------------------------------------*/
  int nbr_pieces[2]; //don't count the king here
  //int nbr_pawns[2];

  /*----------------------------------------------------------------
   | Board array that can be used to quickly answer questions like |
   | "What piece is located at square A4?"                         |
   ----------------------------------------------------------------*/
  char boardarr[64];

  /*---------------------------------------------------------------
   | Tells if the different sides are dangerous for the opponent. |
   ---------------------------------------------------------------*/
  char dangerous[2];

  /*-------------------------
   | Measures king tropism. |
   -------------------------*/
  int tropism[2];

  int safety[2];

};

/* The attackstruct is a database of bitboards representing the
   squares attacked by a certain piece on a certain square. */
struct attack {
  /* Piece color is irrelevant except for pawns which move in
     opposite directions. */
  bitboard pawn[2][64];
  bitboard hslider[64][256];
  bitboard vslider[64][256];

  /* Attack bitboards for diagonals in the a1-h8 direction. */
  bitboard sliderNE[64][256];

  /* Attack bitboards for diagonals in the a8-h1 direction. */
  bitboard sliderNW[64][256];

  bitboard knight[64];
  bitboard king[64];
};

/* The defense-struct is a database of bitboards representing the
   squares that defend a certain square. */
struct defense {
  bitboard pawn[2][64];
};

#define NORMAL_MOVE 1
#define CAPTURE_MOVE 2
#define CASTLING_MOVE 4
#define PASSANT_MOVE 8
#define QUEEN_PROMOTION_MOVE 16
#define ROOK_PROMOTION_MOVE 32
#define BISHOP_PROMOTION_MOVE 64
#define KNIGHT_PROMOTION_MOVE 128
//#define PROMOTION_MOVE (QUEEN_PROMOTION_MOVE | ROOK_PROMOTION_MOVE | BISHOP_PROMOTION_MOVE | KNIGHT_PROMOTION_MOVE)
#define PROMOTION_MOVE 240

struct move_list_entry {
  bitboard fsquare;   //from square
  bitboard tsquare;   //to square
  int piece;          //which kind of piece made the move?
  int type;           //type of move (see #define-clause above)
  int value;          //how good is the move?
};

struct move {
  /* Move specific stuff. */
  char from_square;
  char to_square;
  char piece;                  //which kind of piece made the move?
  char type;                   //type of move (see #define-clause above)
  char old_to_square;

  /* Board specific stuff. */
  char old_castling_status[2];
  char old_passant;
  char old_moves_left_to_draw;
};

void set_bitboards(void);

/* Initializes the board to the starting position. */
void set_board(struct board *board);

/* This function returns the number of bits that are set in an int64. */
int bitcount(int64 n);

/* This function returns the lowest set one bit of a number. */
#define getlsb(w)    ((w) & (~(w) + 1))
//#define getlsb_test(w)        ((w) & -(w))  //this one seems to be slower
//#define gettest(w)       ((w) & (w - 1))

/* This function returns the number of the first set bit in an int64.
   The search is done from LSB to MSB. */
int get_first_bitpos(int64 n);

/* This function returns the number of the first set bit in an int64.
   The search is done from MSB to LSB. */
int get_last_bitpos(int64 n);

/* This function returns the position of the first bit that is set.
   We search from LSB to MSB. -1 is returned if no bit is set. */
int fbit(int64 bitpattern);

/* This function returns the position of the first bit that is set.
   We search from MSB to LSB. -1 is returned if no bit is set. */
int lbit(int64 bitpattern);



void set_connected_bits(void);
void set_square(bitboard *lsquare);
void set_lower(bitboard *llower);
void set_higher(bitboard *lhigher);
void set_rotate90to0(int *rot);
void set_rotate0to90(int *rot);
void set_rotate0toNE(int *rot);
void set_rotateNEto0(int *rot);
void set_rotate0toNW(int *rot);
void set_rotateNWto0(int *rot);
void set_row_nbr(int *lrow_nbr);
void set_dist_to_side(int *ldist_to_side);
void set_dist_to_corn(int *ldist_to_corn);
void set_dist_to_square(int arr[64][64]);
void set_direction(int arr[64][64]);
bitboard get_ray(int target, int direction);
bitboard get_ray90(int target, int direction);
bitboard get_rayNE(int target, int direction);
bitboard get_rayNW(int target, int direction);
void set_ray(bitboard arr[64][19]);
void set_ray90(bitboard arr[64][19]);
void set_rayNE(bitboard arr[64][19]);
void set_rayNW(bitboard arr[64][19]);
void set_horiz_vert_cross(bitboard arr[64]);
void set_nw_ne_cross(bitboard arr[64]);
void set_col_bitboard(bitboard *lcol_bitboard);
void set_adjacent_columns(bitboard *ladj_cols);
void set_right_col(bitboard *lright_col);
void set_left_col(bitboard *lleft_col);
void set_ones(int *arr);
void set_pieceval(int *lpieceval);
void set_pawn_start(bitboard pawnstart[2]);
void set_pawn_2nd_row(bitboard pawn2ndrow[2]);
void set_pawn_lastrow(bitboard pawnlastrow[2]);
void set_pawn_passantrow(bitboard passantrow[2]);
void set_black_and_white_squares(void);
void set_diagNE_length(int diagNElen[64]);
void set_diagNE_start(int diagNEstart[64]);
void set_diagNW_length(int diagNWlen[64]);
void set_diagNW_start(int diagNWstart[64]);
void set_attack_knight(bitboard *knight);
void set_attack_king(bitboard *king);
void set_attack_hslider(bitboard hslider[64][256]);
void set_attack_vslider(bitboard vslider[64][256]);
void set_attack_sliderNE(bitboard sliderNE[64][256]);
void set_attack_sliderNW(bitboard sliderNW[64][256]);
void set_attack_pawn(bitboard pawn[2][64]);
void set_defense_pawn(bitboard pawn[2][64]);
void set_possible_pawn_attack(bitboard arr[2][64]);
void set_testboard(struct board *lboard);
void set_testboard2(struct board *lboard);
void set_testboard3(struct board *lboard);
void set_testboard4(struct board *lboard);
void set_testboard5(struct board *lboard);
int iterated_bitcount(int64 n);
void set_pawn_forward(int arr[2][64]);
void set_last_bit_in_16bits(int *arr);
void set_first_bit_in_16bits(int *arr);
void set_bits_in_16bits(int *arr);



#endif       //_BITBOARDS_

