#include "thread.h"

/* 0x20000 * 8 bytes per u64 type = 1MiB per thread */
size_t FLOOP_BUFSIZE_PER_THREAD = 0x20000;
size_t FLOOP_THREADS = 1;
/* Generate bytes forever. */
size_t FLOOP_ITERATIONS = 0;

/* Initialize mutexes and condition variables */
void init_thread_vars(void)
{
	int ok;

	ok = pthread_mutex_init(&lock, NULL);
	if (ok != 0)
		fatal(1, "pthread_mutex_init() failed");

	ok = pthread_cond_init(&wake, NULL);
	if (ok != 0)
		fatal(1, "pthread_cond_init() failed");
}

/* Wake up worker threads and put them in an idle loop; we use a condition
 * variable to wake them up and put them to work.
 */
void init_threads(int threads, struct worker *wunit)
{
	int i;

	for (i = 0; i < threads; i++) {
		wunit[i].thread_id = i;
		pthread_create(&wunit[i].pthread_id, NULL, start_routine, (void*)&wunit[i]);
		debug("thread %d created\n", wunit[i].thread_id);
	}
}

void *start_routine(void *wunit)
{
	idle_work_loop((struct worker*)wunit);
	return NULL;
}

/*
 * Idle as long as there is no work; if there is work, then do it and resume the
 * idle loop.
 */
void idle_work_loop(struct worker *wunit)
{
	int bucket;
	u64 *buf_master;
	buf_master = wunit->rstream.buf;
	int i;
	u64 bit;

#ifdef NDEBUG
	/* seed and warmup the thread's rng */
	xs1024_seed(&wunit->rstream.rng);
	xs1024_warmup(&wunit->rstream.rng);
#endif

	pthread_mutex_lock(&lock);
	debug("holder of lock: thread %d\n", wunit->thread_id);
	for (i = 0, bit = 1;;i++) {
		/* sleep until woken; check this thread's flag */
		while (!(thread_workers & (bit << wunit->thread_id))) {
			debug("SLEEPING thread %d; iteration %d...\n", wunit->thread_id, i);
			pthread_cond_wait(&wake, &lock);
		}

		pthread_mutex_unlock(&lock);

		/* determine which bucket we are pouring our bits into */
		bucket = bucket_list[wunit->thread_id];

		/* Before spitting out bytes, figure out WHERE to spit it out to. */
		wunit->rstream.buf = buf_master + (bucket * FLOOP_BUFSIZE_PER_THREAD);

		/* Generate the bytes. */
		debug("thread %d: generating bytes at location %p\n", wunit->thread_id, wunit->rstream.buf);
#ifndef NDEBUG
		xs1024_seed_manual(&wunit->rstream.rng, wunit->thread_id);
		xs1024_warmup(&wunit->rstream.rng);
#endif
		gen_stream(&wunit->rstream, FLOOP_BUFSIZE_PER_THREAD);
		debug("thread %d: location %p has value %"PRIu64"\n", wunit->thread_id, wunit->rstream.buf, wunit->rstream.buf[0]);

		pthread_mutex_lock(&lock);
		/* turn off this thread's flag */
		thread_workers &= ~(bit << wunit->thread_id);
	}

}

void manager_thread(void)
{
	int i, j;
	struct rng_stream rstream;
	u64 *buf_master;
	struct worker *wunit;

	rstream.buf = (u64 *)malloc(sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD);
	if (rstream.buf == NULL) {
		fprintf(stderr, "malloc failure\n");
	}

	/* Initialize the manager thread's rng */
	xs1024_seed(&rstream.rng);
	xs1024_warmup(&rstream.rng);

	/* Allocate master buffer */
	buf_master = malloc(sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);
	memset((void *)buf_master, 0xf0, sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);

	/* Initialize mutex locks and condition variables. */
	init_thread_vars();

	/* Set up worker threads. Each thread should have its own RNG,
	which is independently seeded (this is done later when the
	threads are spawned, because it is an an embarrasingly parallel
	task.) */
	wunit = malloc(sizeof(struct worker) * FLOOP_THREADS);
	for (i = 0; (unsigned int)i < FLOOP_THREADS; i++) {
		wunit[i].working = false;
		wunit[i].rstream.buf = buf_master;
	}

	/* Spawn worker threads. */
	thread_workers = 0;
	init_threads(FLOOP_THREADS, wunit);

	/*
	 * The manager now enters a loop; here, the manager randomizes
	 * the notes before each measure.
	 */
	for (i = 0;;i++) {
		pthread_mutex_lock(&lock);
#ifdef NDEBUG
		/* Randomly shuffle the number of threads. */
		shuffle(&rstream.rng, bucket_list, FLOOP_THREADS);
#else
		/* Only reverse the list; we can visually check to see
		 * if the bucket list is being reversed by using small
		 * numbers like 3 threads and buffer of 2. */
		reverse(bucket_list, FLOOP_THREADS);
#endif
		/* Since all threads are ready, (and the work
		 * arrangement has been set), let's wake them up (and
		 * have them start work IMMEDIATELY)! */
		debug("manager: waking up threads...(buf_master at %p)\n", buf_master);
		for (j = 0; (unsigned int)j < FLOOP_THREADS; j++) {
			thread_workers |= 1 << j;
			pthread_cond_signal(&wake);
		}
		pthread_mutex_unlock(&lock);

		/* the threads are now waking up and doing work! */

		/* Upon receiving a phone call "I'm done for this iteration!"
		 * from a worker, check if this is the last worker */
		for (;;) {
			pthread_mutex_lock(&lock);
			if (!thread_workers) {
				thread_workers = 0;
				break;
			}
			pthread_mutex_unlock(&lock);
		}
		/*
		 * When all threads are done writing to the master
		 * buffer (no stepping on each other's toes because each
		 * thread has its own unique position in the buffer), do
		 * a *single* fwrite() call of this master buffer to
		 * STDOUT.
		 */
		debug("manager: FWRITE %zd bytes to stdout on iteration %d\n"
		      , sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS
		      , i);
		fwrite(buf_master, sizeof(*buf_master), FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS, stdout);
		fflush(stdout);
#ifndef NDEBUG
		/* write all 1s to the master buffer, for debugging purposes */
		memset((void *)buf_master, 0xff, sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);
#endif
		pthread_mutex_unlock(&lock);
	}

}
