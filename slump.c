/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "slump.h"
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/time.h>
#else
#include <time.h>
#endif
#include "misc.h"

/* This function initiates a seed for random number generation. The
   seed is taken from the system time. */
void init_random_seed() {
  time_t *t;
  char *str;

  t = (time_t *) malloc(1*sizeof(time_t));
  if (time(t) == -1)
    perror("Could not get system time.");
  else {
    str = (char *) malloc(100*sizeof(char));
    sprintf(str,"Random seed: %d\n",(unsigned int)*t);
    infolog(str);
    free(str);
    srand((unsigned int)*t);
  }
  free(t);
}

/*--------------------------------------------------------
 | This function returns a number between zero and high. |
 --------------------------------------------------------*/
int get_random_number(int high) {
  return (int) ((high+1)*(rand()/(RAND_MAX+1.0)));
}

/*------------------------------------------------
 | This function returns a 64 bit random number. |
 ------------------------------------------------*/
int64 rand64() {
  return rand() ^ ((int64) rand() << 15) ^ ((int64) rand() << 30)
    ^ ((int64) rand() << 45) ^ ((int64) rand() << 60);
}

