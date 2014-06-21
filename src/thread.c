#include "thread.h"

/* 0x20000 * 8 bytes per u64 type = 1MiB per thread */
size_t FLOOP_BUFSIZE_PER_THREAD = 0x20000;
/*
 * Default to 1 thread. A maximum of 64 threads are supported, due to the nature
 * of the worker thread flagging system. For more information on worker flags,
 * see idel_work_loop().
 */
size_t FLOOP_THREADS = 1;
/* Generate bytes forever. */
size_t FLOOP_ITERATIONS = 0;

/*
 * Initialize mutexes and condition variables. Report any errors.
 */
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

/*
 * Wake up worker threads and put them in an idle loop; we use a condition
 * variable to wake them up and put them to work.
 */
void init_threads(int threads, struct worker *worker)
{
	int i;

	for (i = 0; i < threads; i++) {
		worker[i].thread_id = i;
		pthread_create(&worker[i].pthread_id, NULL, start_routine, (void*)&worker[i]);
		debug("thread %d created\n", worker[i].thread_id);
	}
}

/*
 * Entry point for a worker thread; it is just a wrapper around
 * idle_work_loop(). We need this wrapper because of the argument type (a
 * function that takes and returns a void pointer) required by pthread_create().
 */
void *start_routine(void *worker)
{
	idle_work_loop((struct worker*)worker);
	return NULL;
}

/*
 * Idle between work states; once we are awoken from our sleep, do the work, and
 * go back to sleep. The seeding of the PRNG state is done once at the beginning
 * before entering the loop. We do a warmup because it needs to be done to help
 * reduce the initial bias of the PRNG[1]. If debugging, then the seed is reset
 * after every loop to the same seed (the thread id), to make the output more
 * amenable to human analysis.
 *
 * [1]: Sebastiano Vigna, "An experimental exploration of Marsaglia's xorshift
 * generators, scrambled" (2014), p. 24.
 */
void idle_work_loop(struct worker *worker)
{
	int bucket;
	u64 *buf_master;
	buf_master = worker->rstream.buf;
	int i;
	u64 bit;

#ifdef NDEBUG
	xs1024_seed(&worker->rstream.rng);
	xs1024_warmup(&worker->rstream.rng);
#endif

	pthread_mutex_lock(&lock);
	debug("holder of lock: thread %d\n", worker->thread_id);
	for (i = 0, bit = 1;;i++) {
		/*
		 * Sleep until woke. We use a system of set bits (called
		 * 'flags') in a single u64 structure. The rule is that the
		 * thread only awakes and performs work if the bit is turned on
		 * to 1. Once we finish work, we turn off that bit
		 * ourselves. I.e., the master switches on the bit, and then we
		 * switch it off when we're done here.
		 */
		while (!(thread_workers & (bit << worker->thread_id))) {
			debug("SLEEPING thread %d; iteration %d...\n", worker->thread_id, i);
			pthread_cond_wait(&wake, &lock);
		}

		pthread_mutex_unlock(&lock);

		/*
		 * Determine which bucket we are pouring our bits into, and then
		 * adjust our offset to the master buffer by this bucket
		 * number. A 'bucket' here just means a unique segment of the
		 * master buffer where all threads write to. Every worker thread
		 * (before being woken up by the master thread) is assigned its
		 * own bucket.
		 */
		bucket = bucket_list[worker->thread_id];
		worker->rstream.buf = buf_master + (bucket * FLOOP_BUFSIZE_PER_THREAD);

		/*
		 * Generate the bytes. We have our seeding routine down here,
		 * and not at the start of this loop, because we want to do it
		 * when we do not own the lock (for greater parallelism). Also,
		 * we don't bother with the warmup because when we are debugging
		 * we don't care about the quality of the PRNG itself.
		 */
		debug("thread %d: generating bytes at location %p\n", worker->thread_id, worker->rstream.buf);
#ifndef NDEBUG
		xs1024_seed_manual(&worker->rstream.rng, worker->thread_id);
#endif
		gen_stream(&worker->rstream, FLOOP_BUFSIZE_PER_THREAD);
		debug("thread %d: location %p has value %"PRIu64"\n", worker->thread_id, worker->rstream.buf, worker->rstream.buf[0]);

		pthread_mutex_lock(&lock);
		/*
		 * We are done with this iteration, so turn off this thread's
		 * flag. Of course, we want to do it when we own the lock as a
		 * matter of principle (the master thread checks the flags, so
		 * we do it only when the master is not looking to avoid
		 * possible race conditions).
		 */
		thread_workers &= ~(bit << worker->thread_id);
	}

}

int master_thread(void)
{
	int i, j;
	struct rng_stream rstream;
	u64 *buf_master;
	struct worker *worker;

	/*
	 * Set up master thread's state; the two components are its buffer (the
	 * master buffer, and its segments that will become the "buckets" for
	 * each thread to write into), and its own PRNG (to determine how the
	 * buckets are randomized after every iteration).
	 */
	rstream.buf = (u64 *)malloc(sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD);
	check_mem(rstream.buf);
	buf_master = malloc(sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);
	check_mem(buf_master);
	xs1024_seed(&rstream.rng);
	xs1024_warmup(&rstream.rng);

	/*
	 * Set up worker thread data. Each thread should have its own PRNG,
	 * which is independently seeded (this is done later when the threads
	 * are spawned, because it is an an embarrasingly parallel task.)
	 */
	worker = malloc(sizeof(struct worker) * FLOOP_THREADS);
	check_mem(worker);
	for (i = 0; (unsigned int)i < FLOOP_THREADS; i++) {
		worker[i].working = false;
		worker[i].rstream.buf = buf_master;
	}

	/* Set up worker threads' buckets. */
	bucket_list = malloc(sizeof(int) * FLOOP_THREADS);
	check_mem(bucket_list);
	for (i = 0; (unsigned int)i < FLOOP_THREADS; i++) {
		bucket_list[i] = i;
	}

	/* Spawn worker threads. */
	init_thread_vars();
	thread_workers = 0;
	init_threads(FLOOP_THREADS, worker);

#ifndef NDEBUG
	/*
	 * If we're debugging, zero out the master buffer to make it easier to see where the bytes are written.
	 */
	memset((void *)buf_master, 0xf0, sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);
#endif

	for (i = 0;;i++) {
		pthread_mutex_lock(&lock);
#ifdef NDEBUG
		/*
		 * Randomize the bucket order; because the threads always pick
		 * up the buckets in the same order, this has the effect of
		 * assigning each thread a random bucket on every iteration.
		 */
		shuffle(&rstream.rng, bucket_list, FLOOP_THREADS);
#else
		/*
		 * Because we're debugging, only reverse the list; we can
		 * visually check to see if the bucket list is being reversed
		 * by using small numbers like 3 threads and buffer of 2.
		 */
		reverse(bucket_list, FLOOP_THREADS);
#endif
		debug("master: waking up threads...(buf_master at %p)\n", buf_master);
		for (j = 0; (unsigned int)j < FLOOP_THREADS; j++) {
			thread_workers |= 1 << j;
			pthread_cond_signal(&wake);
		}
		pthread_mutex_unlock(&lock);

		/*
		 * The lock has been released, and only now do the threads begin
		 * their work. We now poll the worker flags if all flags have
		 * been unset; when all workers have finished their iteration,
		 * we break out of this loop and go ahead and write the master
		 * buffer to STDOUT. We don't have to grab the lock when writing
		 * the buffer because all threads go to sleep automatically
		 * after finishing their work.
		 */
		for (;;) {
			pthread_mutex_lock(&lock);
			if (!thread_workers) {
				thread_workers = 0;
				break;
			}
			pthread_mutex_unlock(&lock);
		}
		debug("master: FWRITE %zd bytes to stdout on iteration %d\n"
		      , sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS
		      , i);
		fwrite(buf_master, sizeof(*buf_master), FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS, stdout);
		fflush(stdout);
#ifndef NDEBUG
		/* If debugging, write all 0's to the master buffer again. */
		memset((void *)buf_master, 0, sizeof(u64) * FLOOP_BUFSIZE_PER_THREAD * FLOOP_THREADS);
#endif
		pthread_mutex_unlock(&lock);
	}

error:
	exit(EXIT_FAILURE);

}
