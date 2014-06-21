#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include "debug.h"
#include "random.h"
#include "thread.h"

#define FLOOP_VERSION "0.1.2"

void show_help(void);
void show_ver(void);

struct option longopts[] = {
	{"help",	0,	NULL,	'h'},
	{"version",	0,	NULL,	'v'},
	{"threads",	1,	NULL,	't'},
	{"bufsize",	1,	NULL,	'b'},
	{"count",	1,	NULL,	'c'},
	{0,0,0,0}
};

const char optstring[] = "hvt:b:c:";

int main(int argc, char **argv)
{
	int o, tmp;

	if (argc == 1) {
		show_help();
		goto exit;
	}

	while ((o = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (o) {
		case 'h':
			show_help();
			goto exit;
			break;
		case 'v':
			show_ver();
			goto exit;
			break;
		case 't':
			tmp = strtol(optarg, NULL, 10);

			if (tmp < 0 || tmp > 64) {
				sentinel("threads must be between 1 and 64, inclusive");
			} else
				FLOOP_THREADS = tmp;

			break;
		case 'b':
			FLOOP_BUFSIZE_PER_THREAD = strtol(optarg, NULL, 16);
			break;
		case 'c':
			tmp = strtol(optarg, NULL, 10);

			if (tmp < 0) {
				sentinel("iterations cannot be less than 0");
			} else
				FLOOP_ITERATIONS = tmp;

			break;
		default:
			printf("unclean arguments\n");
			break;
		}
	}

	/* Exit if there were any unrecognized arguments. */
	if (optind < argc)
		sentinel("unrecognized option: `%s'", argv[optind]);

	master_thread();
exit:
	exit(EXIT_SUCCESS);
error:
	exit(EXIT_FAILURE);
}

void show_help(void)
{
	printf("Usage: floop [OPTIONS]\n");
	printf("  -h --help           Show help message\n");
	printf("  -v --version        Show version\n");
	printf("  -t --threads NUM    Number of threads to use.\n");
	printf("  -b --bufsize HEXNUM Buffer size per thread, in hex (0x...); this is the number\n");
	printf("                      of u64's generated on each iteration by a thread.\n");
	printf("  -c --count NUM      Number of iterations; use 0 to generate forever.\n");
}

void show_ver(void)
{
	printf("floop version %s\n", FLOOP_VERSION);
}
