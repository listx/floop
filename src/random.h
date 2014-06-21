#ifndef RANDOM_H
#define RANDOM_H

#include "types.h"

struct xs_prng {
	u64 s[16];
	int p;
};

struct rng_stream {
	struct xs_prng rng;
	u64 *buf;
};

extern void xs1024_seed(struct xs_prng *rng);
extern void xs1024_seed_manual(struct xs_prng *rng, u64 seed);
extern u64 xs1024_next(struct xs_prng *rng);
extern void xs1024_warmup(struct xs_prng *rng);
extern void reverse(int *arr, size_t arr_size);
extern void shuffle(struct xs_prng *rng, int *arr, size_t arr_size);
extern u64 get_rand_n(struct xs_prng *rng, u64 n);
extern void gen_stream(struct rng_stream *rstream, size_t count);

#endif
