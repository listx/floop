#ifndef THREAD_H
#define THREAD_H

#include <unistd.h>	/* sysconf() */
#include <pthread.h>

#include "debug.h"
#include "random.h"
#include "types.h"

pthread_mutex_t lock;
pthread_cond_t wake;
u64 thread_workers;
int threads;
int *bucket_list;

extern size_t FLOOP_BUFSIZE_PER_THREAD;
extern size_t FLOOP_THREADS;
extern size_t FLOOP_ITERATIONS;

struct worker {
	pthread_t pthread_id;
	int thread_id;
	struct rng_stream rstream;
	bool working;
};

extern void init_thread_vars(void);
extern void destroy_thread_vars(void);
extern void init_threads(int threads, struct worker *worker);
extern void *start_routine(void *worker);
extern void idle_work_loop(struct worker *worker);
extern int master_thread(void);

#endif
