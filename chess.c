/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include <stdio.h>
#include "misc.h"
#include "slump.h"
#include "bitboards.h"
#include "parse.h"
#include "hash.h"
//#include "egtbprobe.h" //nalimov
#ifndef _CONFIG_
#define _CONFIG_
#include "config.h"
#endif

int main(int argc, char **argv) {
  /*int last_iter_nbr_nodes = 100, second_last_iter_nbr_nodes = 55;
  double b_factor;

  printf("root = %4.4f\n",nroot(4,(700000)));
  b_factor = nroot(4,(700000));
  b_factor = nroot(3,14.706125);
  //b_factor = last_iter_nbr_nodes / second_last_iter_nbr_nodes;
  printf("b_factor = %4.4f\n",(double)b_factor);
  exit(1);*/

  setbuf(stdin,NULL);      //set unbuffered input
  setbuf(stdout,NULL);     //set unbuffered output
  infolog("------------------------------");
  init_random_seed();
  set_bitboards();
  init_hashtables();
  printf("\nThis is %s v. %s\n",PACKAGE_NAME,PACKAGE_VERSION);
  printf("Copyright (C)2002-2009 John Bergbom\n");
  //init_nalimov_probing();
  printf("\nType \'help\' for a list of commands.\n");
  parse();
  //end_nalimov_probing();
  return 0;
}
