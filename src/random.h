#ifndef RANDOM_H
#define RANDOM_H

#include "types.h"

/*
 * The core PRNG state data structure. We keep track of 16 u64's for a total of
 1024 bits of state. The period of this generator is an enormous 2^1024.
 */
struct xs_prng {
	u64 s[16];
	int p;
};

/*
 * This is just a wrapper around the PRNG. The only additional member is the
 * buffer pointer, which gives a place for the PRNG to write data.
 */
struct rng_stream {
	struct xs_prng rng;
	u64 *buf;
};

extern void xs1024_seed(struct xs_prng *rng);
extern void xs1024_seed_manual(struct xs_prng *rng, u64 seed);
extern u64 xs1024_next(struct xs_prng *rng);
extern void xs1024_warmup(struct xs_prng *rng);
extern void gen_stream(struct rng_stream *rstream, size_t count);
extern void reverse(int *arr, size_t arr_size);
extern void shuffle(struct xs_prng *rng, int *arr, size_t arr_size);
extern u64 get_rand_n(struct xs_prng *rng, u64 n);

#endif
