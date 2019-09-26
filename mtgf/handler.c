#include "gfserver-student.h"
#include "gfserver.h"
#include "content.h"

#include "workload.h"

static void Pthread_mutex_lock(pthread_mutex_t *mutex) {
  int rc = pthread_mutex_lock(mutex);
  assert(rc == 0);
}

static void Pthread_mutex_unlock(pthread_mutex_t *mutex) {
  int rc = pthread_mutex_unlock(mutex);
  assert(rc == 0);
}

static void Pthread_cond_signal(pthread_cond_t *cond){
  int rc = pthread_cond_signal(cond);
  assert(rc == 0);
}

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//
gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){

	printf("gfs_handler called with path %s\n", path);

	work_arg *warg = (work_arg *) arg;

	queue_item *item = (queue_item*) malloc(sizeof(queue_item));
	/* Set item path and context */
	memset(item->path, 0, 500);
	strcpy(item->path, path);
	item->ctx = *ctx;
	
	/* Aquire queue lock and enqueue work item */
	Pthread_mutex_lock(warg->queue_lock);
	steque_enqueue(warg->work_queue, (steque_item) item);
	Pthread_cond_signal(warg->cons_cond);
	/* Signal to handler threads */
	Pthread_mutex_unlock(warg->queue_lock);

	*ctx = NULL;

	return gfh_success;
}

