// Copyright (c) 2014 Stanford University
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void
usage(const char* prog)
{
    printf("Time how long it takes to append into a file and fdatasync it.\n");
    printf("The results are printed in seconds total for all the writes.\n");
    printf("\n");
    printf("Usage: %s [options]\n", prog);
    printf("  --count=NUM      "
           "Number of sequential appends to measure [default 1000]\n");
    printf("  --direct=yes|no  "
           "Whether to use O_DIRECT [default no]\n");
    printf("  --file=NAME      "
           "File to create/truncate and write into [default bench.dat]\n");
    printf("  --help           "
           "Print this help message and exit\n");
    printf("  --offset=BYTES   "
           "Number of bytes to skip at start of file [default 0]\n");
    printf("  --size=BYTES     "
           "Number of bytes to append in each iteration [default 1]\n");
}

int
main(int argc, char** argv)
{
    // Set defaults.
    uint64_t count = 1000;
    bool direct = false;
    const char* filename = "bench.dat";
    off_t offset = 0;
    uint64_t size = 1;

    // Parse command-line arguments.
    while (true) {
        static struct option longOptions[] = {
           {"count",  required_argument, NULL, 'c'},
           {"direct", required_argument, NULL, 'd'},
           {"file",   required_argument, NULL, 'f'},
           {"offset", required_argument, NULL, 'o'},
           {"size",   required_argument, NULL, 's'},
           {"help",  no_argument, NULL, 'h'},
           {0, 0, 0, 0}
        };
        int c = getopt_long(argc, argv, "c:d:f:ho:s:", longOptions, NULL);

        // Detect the end of the options.
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'c':
                count = atol(optarg);
                break;
            case 'd':
                direct = (strcmp(optarg, "yes") == 0);
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
            case 'f':
                filename = optarg;
                break;
            case 'o':
                offset = atol(optarg);
                break;
            case 's':
                size = atol(optarg);
                break;
            case '?':
            default:
                // getopt_long already printed an error message.
                usage(argv[0]);
                exit(1);
        }
    }

    // Create random buffer from /dev/urandom. This will be used for writes
    // later. Use mmap rather than malloc so that it's aligned for O_DIRECT.
    char* buf = mmap(NULL, size,
                     PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS,
                     -1, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(1);
    }
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open(/dev/urandom) failed: %s\n", strerror(errno));
        exit(1);
    }
    ssize_t bytes_read = read(fd, buf, size);
    if (bytes_read != size) {
        fprintf(stderr, "read failed: %s\n", strerror(errno));
        exit(1);
    }
    int r = close(fd);
    if (r != 0) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        exit(1);
    }

    // Prepare the file for appending.
    int flags = O_WRONLY|O_CREAT|O_TRUNC;
    if (direct) {
        flags |= O_DIRECT;
    }
    fd = open(filename, flags, 0666);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", filename, strerror(errno));
        exit(1);
    }
    r = posix_fallocate(fd, 0, offset + size * count);
    if (r != 0) {
        fprintf(stderr, "fallocate failed: %s\n", strerror(r));
        exit(1);
    }
    r = fsync(fd);
    if (r != 0) {
        fprintf(stderr, "fsync failed: %s\n", strerror(errno));
        exit(1);
    }
    off_t at = lseek(fd, offset, SEEK_SET);
    if (at != offset) {
        fprintf(stderr, "lseek failed: %s\n", strerror(errno));
        exit(1);
    }

    // Time and do the appends.
    struct timespec start;
    r = clock_gettime(CLOCK_REALTIME, &start);
    if (r != 0) {
        fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
        exit(1);
    }
    uint64_t i;
    for (i = 0; i < count; ++i) {
        ssize_t written = write(fd, buf, size);
        if (written != size) {
            fprintf(stderr, "write failed: %s\n", strerror(errno));
            exit(1);
        }
        r = fdatasync(fd);
        if (r != 0) {
            fprintf(stderr, "fdatasync failed: %s\n", strerror(errno));
            exit(1);
        }
    }
    struct timespec end;
    r = clock_gettime(CLOCK_REALTIME, &end);
    if (r != 0) {
        fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
        exit(1);
    }

    // Clean up.
    r = close(fd);
    if (r != 0) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        exit(1);
    }
    r = unlink(filename);
    if (r != 0) {
        fprintf(stderr, "unlink failed: %s\n", strerror(errno));
        exit(1);
    }
    r = munmap(buf, size);
    if (r != 0) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        exit(1);
    }

    // Print results.
    struct timespec elapsed;
    if (end.tv_nsec > start.tv_nsec) {
      elapsed.tv_sec = end.tv_sec - start.tv_sec;
      elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    } else { // carry
      elapsed.tv_sec = end.tv_sec - start.tv_sec - 1;
      elapsed.tv_nsec = 1000 * 1000 * 1000 + end.tv_nsec - start.tv_nsec;
    }
    printf("Total: %lu.%09lu seconds\n", elapsed.tv_sec, elapsed.tv_nsec);

    return 0;
}
