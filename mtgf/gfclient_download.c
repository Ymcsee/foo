#include "gfclient-student.h"

#define MAX_THREADS (32767)

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -r [num_requests]   Request download total (Default: 5)\n"           \
"  -p [server_port]    Server port (Default: 19121)\n"                         \
"  -s [server_addr]    Server address (Default: 127.0.0.1)\n"                   \
"  -t [nthreads]       Number of threads (Default 7)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"help",          no_argument,            NULL,           'h'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'r'},
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {NULL,            0,                      NULL,             0}
};

static void Usage() {
	fprintf(stderr, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

static void Pthread_mutex_lock(pthread_mutex_t *mutex) {
  int rc = pthread_mutex_lock(mutex);
  assert(rc == 0);
}

static void Pthread_mutex_unlock(pthread_mutex_t *mutex) {
  int rc = pthread_mutex_unlock(mutex);
  assert(rc == 0);
}

static void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex){
  int rc = pthread_cond_wait(cond, mutex);
  assert(rc == 0);
}

static void Pthread_cond_signal(pthread_cond_t *cond){
  int rc = pthread_cond_signal(cond);
  assert(rc == 0);
}

static void Pthread_cond_broadcast(pthread_cond_t *cond){
  int rc = pthread_cond_broadcast(cond);
  assert(rc == 0);
}
/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}

struct work_arg {
  steque_t *work_queue;
  pthread_mutex_t *queue_lock;
  pthread_mutex_t *reqs_left_lock;
  pthread_cond_t *cons_cond;
  pthread_cond_t *main_cond;
  char *server;
  unsigned short port;
  int reqs_left;
};

static void* work(void *arg) {
  work_arg *warg = (work_arg *) arg;
  char* req_path;
  gfcrequest_t *gfr;
  FILE *file;
  int returncode = 0;
  char local_path[1066];
  pthread_t tid = pthread_self();

  while (1) {

    Pthread_mutex_lock(warg->queue_lock);
    /* Spin until something to do */
    while (steque_isempty(warg->work_queue)) {
      /* Nothing left to do so return immediately */
      if (warg->reqs_left == 0){
        printf("All requests have been processed so thread %ld is exiting\n", tid);
        Pthread_mutex_unlock(warg->queue_lock);
        return 0;
      }
      Pthread_cond_wait(warg->cons_cond, warg->queue_lock);
    }
    /* Remove path work item */
    req_path = (char *) steque_pop(warg->work_queue);
    printf("Pulled path %s off of steque by thread %ld\n", req_path, tid);
    Pthread_mutex_unlock(warg->queue_lock);

    /*---- Begin provided code ----*/
    if(strlen(req_path) > 500){
      fprintf(stderr, "Request path exceeded maximum of 500 characters\n.");
      exit(EXIT_FAILURE);
    }

    localPath(req_path, local_path);

    file = openFile(local_path);

    gfr = gfc_create();
    gfc_set_server(&gfr, warg->server);
    gfc_set_path(&gfr, req_path);
    gfc_set_port(&gfr, warg->port);
    gfc_set_writefunc(&gfr, writecb);
    gfc_set_writearg(&gfr, file);

    fprintf(stdout, "Requesting %s%s\n", warg->server, req_path);

    if ( 0 > (returncode = gfc_perform(&gfr))){
      fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
      fclose(file);
      if ( 0 > unlink(local_path))
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    }
    else {
        fclose(file);
    }

    if ( gfc_get_status(&gfr) != GF_OK){
      if ( 0 > unlink(local_path))
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    }

    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr), gfc_get_filelen(&gfr));

    gfc_cleanup(&gfr);
    /*---- End provided code ----*/


    /* Decrement `warg->reqs_left` and signal */
    Pthread_mutex_lock(warg->reqs_left_lock);
    warg->reqs_left--;
    printf("Requests left to process: %d\n", warg->reqs_left);
    Pthread_cond_signal(warg->main_cond);
    Pthread_mutex_unlock(warg->reqs_left_lock);

  }

  /* Should never occur but for consistency */
  printf("Thread %ld returning\n", tid);
  return 0;
}


/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
  char *server = "localhost";
  unsigned short port = 19121;
  char *workload_path = "workload.txt";

  int i = 0;
  int option_char = 0;
  int nrequests = 5;
  int nthreads = 7;
  char *req_path = NULL;

  setbuf(stdout, NULL); // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "t:n:r:s:w:hp:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'h': // help
        Usage();
        exit(0);
        break;                      
      case 'n': // nrequests
        break;
      case 'r':
        nrequests = atoi(optarg);
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);
      case 's': // server
        server = optarg;
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
    }
  }

  if( EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }

  if (nthreads < 1) {
    nthreads = 1;
  }
  if (nthreads > MAX_THREADS) {
    nthreads = MAX_THREADS;
  }

  gfc_global_init();

  /* Init steque */
  steque_t work_queue;
  steque_init(&work_queue);

  /* Lock that protects `warg.work_queue` */
  pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
  /* Lock that protects `warg.reqs_left` */
  pthread_mutex_t reqs_left_lock = PTHREAD_MUTEX_INITIALIZER;

  /* Init a cond for the consumers `cons_cond` and a cond for the
  main thread `main_cond`
 */
  pthread_cond_t cons_cond = PTHREAD_COND_INITIALIZER;
  pthread_cond_t main_cond = PTHREAD_COND_INITIALIZER;

  /* Init global struct main and workers will read */
  work_arg warg = {
    .server = server,
    .work_queue = &work_queue,
    .queue_lock = &queue_lock,
    .cons_cond = &cons_cond,
    .main_cond = &main_cond,
    .port = port,
    .reqs_left_lock = &reqs_left_lock,
    .reqs_left = nrequests
  };

  /* add your threadpool creation here */
  pthread_t pool[nthreads];
  for (i = 0; i < nthreads; i++){
    if (pthread_create(&pool[i], NULL, work, &warg) != 0){
      fprintf(stderr, "Error creating thread # %d", i);
      exit(1);
    }
  }

  /* Build your queue of requests here */
  for(i = 0; i < nrequests; i++){
    req_path = workload_get_path();
    printf("workload_get_path returned %s and will now be enqueued\n", req_path);
    Pthread_mutex_lock(&queue_lock);
    steque_enqueue(&work_queue, (steque_item) req_path);
    Pthread_cond_signal(&cons_cond);
    Pthread_mutex_unlock(&queue_lock);
  }

  /* Wait until `warg.reqs_left` is 0 */
  Pthread_mutex_lock(warg.reqs_left_lock);
  while (warg.reqs_left > 0) {
    Pthread_cond_wait(&main_cond, warg.reqs_left_lock);
  }
  printf("Main thread: 0 requests left to process. Broadcasting signal to clean up outstanding workers\n");
  Pthread_mutex_unlock(warg.reqs_left_lock);
  /* Signal to remaining workers to return */
  Pthread_cond_broadcast(warg.cons_cond);

  /* Join threads */
  int return_code;
  for (i = 0; i < nthreads; i++) {
    pthread_join(pool[i], (void *) &return_code);
    printf("Thread # %ld joined\n", pool[i]);
  }

  /* Cleanup */
  gfc_global_cleanup(); // use for any global cleanup for AFTER your thread pool has terminated.

  return 0;
}  
