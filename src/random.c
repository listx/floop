#include <assert.h>
#include <stdio.h>

#include "random.h"

/*
 * xorshift1024* generator, from the paper ``An experimental exploration of
 * Marsaglia's xorshift generators, scrambled'' (2014) by Sebastiano Vigna.
 *
 * The state array s[16] must be initialized from non-zero seeds. It has a
 * period of (2^1024 - 1).
 */

inline u64 xs1024_next(struct xs_prng *rng)
{
	u64 s0 = rng->s[rng->p];
	u64 s1 = rng->s[rng->p = (rng->p + 1) & 15];
	s1 ^= s1 << 31;
	s1 ^= s1 >> 11;
	s0 ^= s0 >> 30;
	return (rng->s[rng->p] = s0 ^ s1) * 1181783497276652981LL;
}

/*
 * According to Vigna, xs1024 needs to generate at least "a few hundreds"[1] (sic)
 * iterations to start behaving correctly. Thus, we use a simple warmup function
 * to get the generator ready.
 *
 * [1]: Sebastiano Vigna, "An experimental exploration of Marsaglia's xorshift
 * generators, scrambled" (2014), p. 24.
 */
void xs1024_warmup(struct xs_prng *rng)
{
	int i;
	for (i = 0; i < 1000; i++) {
		xs1024_next(rng);
	}
}

/*
 * Use /dev/urandom as the seed.
 */
void xs1024_seed(struct xs_prng *rng)
{
	int elements;
	FILE *urandom;

	elements = 16;
	urandom = fopen("/dev/urandom", "r");
	fread(&rng->s, sizeof (*(rng->s)), elements, urandom);
	fclose(urandom);

	rng->p = 0;
}

/*
 * Manually seed the generator with a given number; useful for debugging.
 */
void xs1024_seed_manual(struct xs_prng *rng, u64 seed)
{
	int i, elements;

	elements = 16;
	for (i = 0; i < elements; i++) {
		rng->s[i] = seed + 1;
	}

	rng->p = 0;
}


/*
 * Write random bytes into a buffer.
 */
void gen_stream(struct rng_stream *rstream, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++) {
		rstream->buf[i] = xs1024_next(&rstream->rng);
	}
}

/*
 * Reverse an array in-place; this is only used for debugging purposes.
 */
void reverse(int *arr, size_t arr_size)
{
	size_t i;
	int j, tmp;
	for (i = 0, j = (arr_size - 1); i < (arr_size >> 1); i++, j--) {
		tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
	}
}

/*
 * Randomly shuffle an array in-place. This uses the Fischer-Yates shuffling
 * algorithm.
 */
void shuffle(struct xs_prng *rng, int *arr, size_t arr_size)
{
	int tmp;
	size_t i, j;

	for (i = 0; i < (arr_size - 1); i++) {
		j = i + get_rand_n(rng, arr_size - i);
		tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
	}
}

/*
 * Get a random number between 0 and (n - 1).
 */
u64 get_rand_n(struct xs_prng *rng, u64 n)
{
	assert(n > 1);
	u64 x, rand_limit;
	rand_limit = 0xffffffffffffffffULL - n;
	while ((x = xs1024_next(rng)) > rand_limit) {};
	return x % n;
}
