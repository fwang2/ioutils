/* fair-lio: a speed/skew benchmark
 * The memory buffers are round-robined over the submitted requests so
 * that any scatter/gather issues are seen by all targets in a balanced
 * manner.
 *
 * The libaio-devel package is required to build. It's not pretty, but it
 * wasn't really made except for anything but a one-off test.
 *
 * Originally by Dave Dillow <dillowda@ornl.gov>
 * Modified by Youngjae Kim <kimy1@ornl.gov> and Sarp Oral <oralhs@ornl.gov>
 * Oak Ridge Leadership Computing Facility
 * National Center for Computational Science
 * Oak Ridge National Laboratory
 *
 *
 * Copyright (C) 2009-2011 UT-Battelle, LLC
 * This source code was developed under contract DE-AC05-00OR22725
 * and there is a non-exclusive license for use of this work by or
 * on behalf of the US Government.
 * 
 * UT-Battelle, LLC AND THE GOVERNMENT MAKE NO REPRESENTATIONS AND
 * DISCLAIM ALL WARRANTIES, BOTH EXPRESSED AND IMPLIED. THERE ARE NO
 * EXPRESS OR IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
 * PARTICULAR PURPOSE, OR THAT THE USE OF THE SOFTWARE WILL NOT
 * INFRINGE ANY PATENT, COPYRIGHT, TRADEMARK, OR OTHER PROPRIETARY
 * RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE INTENDED RESULTS
 * OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY OR DAMAGE.
 * The user assumes responsibility for all liabilities, penalties, fines,
 * claims, causes of action, and costs and expenses, caused by, resulting
 * from or arising out of, in whole or in part the use, storage or disposal
 * of the SOFTWARE.
 */
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libaio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

/* CLOCK_MONOTONIC_HR is not widely available, but we want to use it if we can.
 */
#ifndef CLOCK_MONOTONIC_HR
#define CLOCK_MONOTONIC_HR CLOCK_MONOTONIC
#endif

struct dev_info {
	struct iocb *io_base;
	off_t offset;
	off_t size;
	int replace;
	int fd;
	unsigned long random_range;
	unsigned int count, writes;
	unsigned short off_rand_state[3];
	unsigned short rw_rand_state[3];
	char *name;
	char status;
};

struct buf_list {
	struct buf_list *next;
};

static struct dev_info *devs;
static struct buf_list *buf_ready, *buf_p, *buf_next;
static int buf_dev, ndev, random_offsets, loop_at_end;
static unsigned long req_size;
static double write_pct;

int prepare_request(struct dev_info *dev, struct iocb *iocb)
{
	double r;

	/* If we're replacing buffers on this device, and this buffer is next
	 * one to be replaced, then put the current buffer on the next
	 * replacement list, and pop off the current replacement into
	 * this slot.
	 */
	if (dev->replace >= 0 && iocb == &dev->io_base[dev->replace]) {
		buf_p = (struct buf_list *) iocb->u.c.buf;
		buf_p->next = buf_next;
		buf_next = buf_p;
		iocb->u.c.buf = buf_ready;
		buf_ready = buf_ready->next;
		dev->replace++;

		/* We've run out of replacement buffers, so we're done with
		 * this device. Reset the list for the next device.
		 */
		if (!buf_ready) {
			dev->replace = -1;
			buf_dev++;
			buf_dev %= ndev;
			devs[buf_dev].replace = 0;
			buf_ready = buf_next;
			buf_next = NULL;
		}
	}

	if (random_offsets) {
		r = erand48(dev->off_rand_state);
		dev->offset = dev->random_range * r;
		dev->offset *= req_size;
		iocb->u.c.offset = dev->offset;
	} else {
		iocb->u.c.offset = dev->offset;
		dev->offset += req_size;

		if (dev->size <= (dev->offset + req_size)) {
			if (loop_at_end)
				dev->offset = 0;
			else
				return 1;
		}
	}

	r = erand48(dev->rw_rand_state);
	if (r < write_pct) {
		iocb->aio_lio_opcode = IO_CMD_PWRITE;
		dev->writes++;
	} else
		iocb->aio_lio_opcode = IO_CMD_PREAD;

	dev->count++;
	return 0;
}

void usage(char *pname)
{
	fprintf(stderr, "usage: %s [OPTIONS] target(s)...\n", pname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -w,--write[=PERCENT]\tPercentage of requests "
			"that should be writes\n\t\t\t\tDefaults to 0%% "
			"(nondestructive)\n\t\t\t\t100%% writes if no "
			"percentage given with option\n\t\t\t\t"
			"values less than 100 imply random offsets\n\n");
	fprintf(stderr, "    --mixseed=SEED\t\tSeed for mixed requests\n");
	fprintf(stderr, "    --random[=SEED]\t\tRandom IO offsets "
			"(defaults to sequential IO)\n");
	fprintf(stderr, "    --range=BYTES\t\tMaximum byte offset for "
			"random IO\n");
	fprintf(stderr, "    -l,--loop\t\t\tRestart test at end-of-device\n"
			"\t\t\t\t(defaults to stopping the test)\n");
	fprintf(stderr, "    -q NUM,--qd NUM\t\tRequests in flight "
			"per target (default 4)\n");
	fprintf(stderr, "    -s BYTES,--size BYTES\tRequest size in bytes "
			"(default 1 MB)\n");
	fprintf(stderr, "    -t SECONDS,--time SECONDS\tTest runtime in "
			"seconds (default 30 seconds)\n");
	fprintf(stderr, "    -h,--help\t\t\tThis message\n");
	exit(1);
}

int time_before(const struct timespec *now, const struct timespec *ref)
{
	if (now->tv_sec < ref->tv_sec)
		return 1;

	return (now->tv_sec == ref->tv_sec && now->tv_nsec < ref->tv_nsec);
}

int main(int argc, char **argv)
{
	int run_time = 30;
	int op = O_RDONLY;
	int qd = 4;
	int end_of_device = 0;
	int opt, i, j, max_req, rc, ios_ready, in_flight, count;
	unsigned long arena_size;
	unsigned long seed = time(NULL);
	unsigned long mix_seed = time(NULL);
	unsigned long random_range = 0;
	unsigned char *arena;
	struct iocb *iocbs;
	struct iocb *ioptr;
	struct iocb **iolist;
	struct iocb **iolist_ptr;
	struct io_event *events;
	struct timespec start, end, now;
	unsigned long total_data, total_writes, total_count;
	int min_data, max_data;
	io_context_t ioctx;
	double mbs, actual_time;

	struct option options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "write", optional_argument, NULL, 'w' },
		{ "loop", no_argument, NULL, 'l' },
		{ "mixseed", required_argument, NULL, 'm' },
		{ "qd", required_argument, NULL, 'q' },
		{ "random", optional_argument, NULL, 'r' },
		{ "range", required_argument, NULL, 'a' },
		{ "size", required_argument, NULL, 's' },
		{ "time", required_argument, NULL, 't' },
		{ NULL }
	};

	req_size = 1024 * 1024;
	write_pct = 0.0;

	for (;;) {
		opt = getopt_long(argc, argv, "hwlq:s:t:", options, NULL);

		if (opt == -1)
			break;

		switch (opt) {
		case 'w':
			op = O_RDWR;
			write_pct = 1.0;
			if (optarg)
				write_pct = strtod(optarg, NULL) / 100.0;
			if (write_pct < 1.0)
				random_offsets = 1;
			break;
		case 'l':
			loop_at_end = 1;
			break;
		case 'm':
			mix_seed = strtoul(optarg, NULL, 0);
			break;
		case 'q':
			qd = atoi(optarg);
			break;
		case 't':
			run_time = atoi(optarg);
			break;
		case 's':
			req_size = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			random_offsets = 1;
			if (optarg)
				seed = strtoul(optarg, NULL, 0);
			else
				seed = time(NULL);
			break;
		case 'a':
			random_range = strtoul(optarg, NULL, 0);
			break;
		case 'h':
		case '?':
		default:
			usage(argv[0]);
			break;
		}
	}

	if (run_time < 1) {
		fprintf(stderr, "%s: run time must be at least 1 second\n",
				argv[0]);
		exit(1);
	}

	if (qd < 0 || qd > 64) {
		fprintf(stderr, "%s: queue depth must be between 1 and 64\n",
				argv[0]);
		exit(1);
	}

	if (req_size < 4096 || req_size > (16 * 1024 * 1024)) {
		fprintf(stderr, "%s: request size must be between 4K and 16M\n",
				argv[0]);
		exit(1);
	}

	/* Always do a multiple of a page for direct IO.
	 */
	req_size &= ~4095;

	if (random_range) {
		random_range /= req_size;
		if (random_range < 2) {
			fprintf(stderr, "%s: random range gives no "
					"randomness\n", argv[0]);
			exit(1);
		}
	}

	if (optind == argc) {
		fprintf(stderr, "%s: need devices to benchmark\n", argv[0]);
		exit(1);
	}

	ndev = argc - optind;
	devs = malloc(ndev * sizeof(*devs));
	if (!ndev) {
		fprintf(stderr, "%s: no memory for devices\n", argv[0]);
		exit(1);
	}

	for (i = 0; i < ndev; i++) {
		devs[i].replace = -1;
		devs[i].io_base = NULL;
		devs[i].offset = 0;
		devs[i].count = 0;
		devs[i].writes = 0;
		devs[i].status = ' ';
		devs[i].name = argv[optind++];
		devs[i].fd = open(devs[i].name, op|O_DIRECT|O_SYNC);
		if (devs[i].fd < 0) {
			fprintf(stderr, "%s: opening ", argv[0]);
			perror(devs[i].name);
			exit(1);
		}
		devs[i].size = lseek(devs[i].fd, 0, SEEK_END);
		if (devs[i].size < 0) {
			fprintf(stderr, "%s: lseek ", argv[0]);
			perror(devs[i].name);
			exit(1);
		}
		if (random_offsets) {
			unsigned long t = seed + i;

			devs[i].random_range = devs[i].size / req_size;
			if (random_range && devs[i].random_range > random_range)
				devs[i].random_range = random_range;

			devs[i].off_rand_state[2] = t >> 16;
			devs[i].off_rand_state[1] = t & 0xffff;
			devs[i].off_rand_state[0] = 0x330e;

			t = mix_seed + i;
			devs[i].rw_rand_state[2] = t >> 16;
			devs[i].rw_rand_state[1] = t & 0xffff;
			devs[i].rw_rand_state[0] = 0x330e;
		}
	}

	/* The arena needs to be big enough to host the maximum number of
	 * requests possible, plus an extra set of requests to pass around
	 * the devices to fairly share any memory fragmentation seen.
	 */
	max_req = ndev * qd;
	arena_size = (max_req + qd) * req_size;
	arena = mmap(NULL, arena_size, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_POPULATE|MAP_PRIVATE, -1, 0);
	if (arena == MAP_FAILED) {
		fprintf(stderr, "%s: unable to allocate arena\n", argv[0]);
		exit(1);
	}

	memset(arena, 0, arena_size);

	iocbs = malloc(max_req * sizeof(*iocbs));
	if (!iocbs) {
		fprintf(stderr, "%s: no memory for IOCBs\n", argv[0]);
		exit(1);
	}

	iolist = malloc(max_req * sizeof(*iocbs));
	if (!iolist) {
		fprintf(stderr, "%s: no memory for IOCB list\n", argv[0]);
		exit(1);
	}

	events = malloc(max_req * sizeof(*events));
	if (!iolist) {
		fprintf(stderr, "%s: no memory for IOCB list\n", argv[0]);
		exit(1);
	}

	rc = io_queue_init(max_req, &ioctx);
	if (rc < 0) {
		fprintf(stderr, "%s: unable to initialize IO context: %s\n",
				argv[0], strerror(-rc));
		exit(1);
	}

	/* Prepopulate our IO requests
	 */
	ios_ready = 0;
	ioptr = iocbs;
	iolist_ptr = iolist;
	for (i = 0; i < qd; i++) {
		for (j = 0; j < ndev; j++) {
			if (!devs[j].io_base)
				devs[j].io_base = ioptr;

			/* Initial iocb prep, will be overwritten in
			 * prepare_request() with proper offset and operation.
			 */
			io_prep_pwrite(ioptr, devs[j].fd, arena, req_size, 0);
			ioptr->data = &devs[j];
			arena += req_size;

			if (prepare_request(&devs[j], ioptr)) {
				fprintf(stderr, "Premature end of device for "
						"%s\n", devs[j].name);
				exit(1);
			}

			*iolist_ptr++ = ioptr++;
			ios_ready++;
		}
	}

	/* Populate the replacement list
	 */
	buf_dev = 0;
	buf_next = NULL;
	buf_ready = NULL;
	for (i = 0; i < qd; i++) {
		buf_p = (struct buf_list *) arena;
		buf_p->next = buf_ready;
		buf_ready = buf_p;
		arena += req_size;
	}

	clock_gettime(CLOCK_MONOTONIC_HR, &now);
	end.tv_sec = now.tv_sec + run_time;
	end.tv_nsec = now.tv_nsec;

	devs[0].replace = 0;
	in_flight = 0;
	clock_gettime(CLOCK_MONOTONIC_HR, &start);

	/* Prime the IO pump with our prepared requests
	 */
	rc = io_submit(ioctx, ios_ready, iolist);
	if (rc < 0) {
		fprintf(stderr, "%s: io_submit() failed: %s\n",
				argv[0], strerror(-rc));
		exit(1);
	}

	in_flight += ios_ready;
	iolist_ptr = iolist;
	ios_ready = 0;

	/* Main IO loop
	 */
	while (time_before(&now, &end) && !end_of_device) {
		count = io_getevents(ioctx, 1, in_flight, events, NULL);
		if (count <= 0) {
			fprintf(stderr, "%s: io_getevents() failed: "
					"%s\n", argv[0], strerror(-rc));
			exit(1);
		}

		for (i = 0; i < count && !end_of_device; i++) {
			struct iocb *iocb = events[i].obj;
			struct dev_info *dev = iocb->data;

			if (events[i].res2) {
				fprintf(stderr, "%s: got error for %s:"
						" %lu\n", argv[0],
						dev->name, 
						events[i].res2);
				exit(1);
			}

			end_of_device = prepare_request(dev, iocb);
			if (!end_of_device) {
				*iolist_ptr++ = iocb;
				ios_ready++;
			}

			in_flight--;
		}

		/* If we're here, then odds are good we have IO to submit.
		 * Check just in case something woke us up other than an
		 * IO completion.
		 */
		if (ios_ready) {
			rc = io_submit(ioctx, ios_ready, iolist);
			if (rc < 0) {
				fprintf(stderr, "%s: io_submit() failed: %s\n",
						argv[0], strerror(-rc));
				exit(1);
			}

			in_flight += ios_ready;
			iolist_ptr = iolist;
			ios_ready = 0;
		}

		clock_gettime(CLOCK_MONOTONIC_HR, &now);
	}

	if (end_of_device) {
		actual_time = now.tv_nsec - start.tv_nsec;
		actual_time /= 10e9;
		actual_time += now.tv_sec - start.tv_sec;
	} else
		actual_time = run_time;

	/* Ok, test time is finished, drain outstanding requests
	 */
	while (in_flight) {
		count = io_getevents(ioctx, 1, in_flight, events, NULL);
		if (count <= 0) {
			fprintf(stderr, "%s: draining io_getevents() failed: "
					"%s\n", argv[0], strerror(-rc));
			exit(1);
		}

		for (i = 0; i < count; i++) {
			struct iocb *iocb = events[i].obj;
			struct dev_info *dev = iocb->data;

			if (events[i].res2) {
				fprintf(stderr, "%s: draining got error for %s:"
						" %lu\n", argv[0],
						dev->name, 
						events[i].res2);
				exit(1);
			}

			in_flight--;
		}
	}

	/* Find the targets that wrote the min and max data so we can
	 * calculate the skew.
	 */
	min_data = max_data = 0;
	total_data = total_writes = total_count= 0;
	for (i = 0; i < ndev; i++) {
		if (devs[i].count < devs[min_data].count)
			min_data = i;
		if (devs[i].count > devs[max_data].count)
			max_data = i;
		total_data += devs[i].count * req_size;
		total_writes += devs[i].writes;
		total_count += devs[i].count;
	}
	devs[min_data].status = 'm';
	devs[max_data].status = 'M';

	printf("run time: %.6f seconds%s\n", actual_time,
	       end_of_device ? " (end of device reached)" : "");
	printf("queue depth: %d\n", qd);
	printf("operation: %s ", random_offsets ? "random" : "sequential");
	if (write_pct >= 1.0)
		printf("write\n");
	else if (write_pct == 0.0)
		printf("read\n");
	else {
		printf("mixed (goal %.02f%% writes)\n", write_pct * 100);
		printf("random request seed: %lu\n", mix_seed);
	}
	if (random_offsets)
		printf("random offset seed: %lu\n", seed);
	if (random_range)
		printf("random range: %lu bytes\n", random_range * req_size);
	printf("request size: %lu bytes\n\n", req_size);

	for (i = 0; i < ndev; i++) {
		mbs = ((double) devs[i].count * req_size) / actual_time;
		mbs /= 1024 * 1024;
		printf("%c %s: %lu %.2f MB/s", devs[i].status,
			devs[i].name, devs[i].count * req_size, mbs);
		if (write_pct != 0.0 && write_pct < 1.0) {
			printf("  %02.f%% writes",
				100.0 * devs[i].writes / devs[i].count);
		}
		printf("\n");
	}

	mbs = (double) total_data / actual_time;
	mbs /= 1024 * 1024;
	printf("total: %lu %.2f MB/s", total_data, mbs);
	mbs = (double) (devs[max_data].count - devs[min_data].count);
	mbs *= req_size;
	mbs /= actual_time;
	mbs /= 1024 * 1024;
	printf(" skew %.2f MB/s", mbs);
	if (write_pct != 0.0 && write_pct < 1.0)
		printf("  %02.f%% writes", 100.0 * total_writes / total_count);
	printf("\n");

	return 0;
}
