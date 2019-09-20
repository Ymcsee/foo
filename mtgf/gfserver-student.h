/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "gfserver.h"
#include "content.h"

typedef struct work_arg {
  steque_t *work_queue;
  pthread_mutex_t *queue_lock;
  pthread_cond_t *cons_cond;
  pthread_cond_t *main_cond;
  unsigned short port;
} work_arg ;

typedef struct queue_item {
  char path[256];
  gfcontext_t *ctx;
} queue_item;

void init_threads(size_t numthreads);
void cleanup_threads();

#endif // __GF_SERVER_STUDENT_H__
