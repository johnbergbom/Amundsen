/*
 * Copyright (C) 2002-2009 John Bergbom
 */

#include "threads.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

pthread_t input_thread_id;
int input_thread_status = 0;
extern char *input_from_thread;
extern int pending_input;

/*------------------------------------------------------
 | The sole purose of this thread is to get unblocking |
 | reading of input in the parse() function. This can  |
 | be achieved in other ways as well, but having a     |
 | separate thread (probably) makes the program easier |
 | to port to different platform.                      |
 ------------------------------------------------------*/
void *input_thread() {
  while (input_thread_status) {
    if (pending_input)
      usleep(50);
    if (input_thread_status && !pending_input) {
      fgets(input_from_thread,100,stdin);   //reads a whole line
      //remove trailing newline
      input_from_thread[strlen(input_from_thread)-1] = '\0';
      pending_input = 1;
    }
  }
  pthread_exit(NULL);
}


/*-----------------------------------------
 | This function starts the input thread. |
 -----------------------------------------*/
int start_input_thread() {
  int stat;

  input_thread_status = 1;
  stat = pthread_create(&input_thread_id,NULL,input_thread,NULL);
  if (stat == 0) {
    printf("Search thread started\n");
    return 1;
  } else {
    perror("Couldn't start search thread.");
    input_thread_status = 0;
  }
  return 0;
}

/*----------------------------------------
 | This function stops the input thread. |
 ----------------------------------------*/
void stop_input_thread() {
  input_thread_status = 0;
  if (pthread_join(input_thread_id,NULL) == 0) {
    printf("Input thread exited gracefully.\n");
  } else {
    perror("Error while exiting the input thread.\n");
  }
}

