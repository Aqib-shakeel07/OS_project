#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "reader_writer_api.h"

typedef struct {
  int thread_index;
  int iterations;
  const char *message_prefix;
  size_t message_prefix_len;
  volatile uint64_t *writes_done;
  volatile uint64_t *reads_done;
  volatile bool *stop_flag;
} worker_args_t;

static void *writer_worker(void *arg) {
  worker_args_t *cfg = (worker_args_t *)arg;
  char buffer[256];

  for (int i = 0; i < cfg->iterations && !*cfg->stop_flag; i++) {
    int len = snprintf(buffer, sizeof(buffer), "%s[w%02d] iter=%d time=%ld", cfg->message_prefix,
                       cfg->thread_index, i, (long)time(NULL));
    if (len < 0) len = 0;
    if (len > (int)sizeof(buffer)) len = (int)sizeof(buffer);

    long rc = rw_write(buffer, (size_t)len);
    if (rc < 0) {
      fprintf(stderr, "writer %d: rw_write error: %ld (errno=%d)\n", cfg->thread_index, rc, errno);
      break;
    }
    __sync_fetch_and_add((unsigned long long *)cfg->writes_done, 1ULL);
  }
  return NULL;
}

static void *reader_worker(void *arg) {
  worker_args_t *cfg = (worker_args_t *)arg;
  char buffer[512];

  for (int i = 0; i < cfg->iterations && !*cfg->stop_flag; i++) {
    long rc = rw_read(buffer, sizeof(buffer) - 1);
    if (rc < 0) {
      fprintf(stderr, "reader %d: rw_read error: %ld (errno=%d)\n", cfg->thread_index, rc, errno);
      break;
    }
    buffer[rc >= 0 ? rc : 0] = '\0';

    if (cfg->message_prefix_len > 0 && rc > 0) {
      if (strncmp(buffer, cfg->message_prefix, cfg->message_prefix_len) != 0) {
        fprintf(stderr, "reader %d: WARNING unexpected prefix: '%.*s'\n", cfg->thread_index,
                (int)cfg->message_prefix_len, buffer);
      }
    }
    __sync_fetch_and_add((unsigned long long *)cfg->reads_done, 1ULL);
  }
  return NULL;
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [-R readers] [-W writers] [-I iterations] [-s prefix] [-S seconds]\n"
          "  -R: number of reader threads (default 4)\n"
          "  -W: number of writer threads (default 2)\n"
          "  -I: iterations per thread (default 10000)\n"
          "  -s: message prefix used by writers (default 'msg')\n"
          "  -S: run for N seconds instead of fixed iterations (overrides -I)\n",
          prog);
}

int main(int argc, char **argv) {
  int num_readers = 4;
  int num_writers = 2;
  int iterations = 10000;
  const char *prefix = "msg";
  int duration_seconds = 0; 

  int opt;
  while ((opt = getopt(argc, argv, "R:W:I:s:S:h")) != -1) {
    switch (opt) {
      case 'R': num_readers = atoi(optarg); break;
      case 'W': num_writers = atoi(optarg); break;
      case 'I': iterations = atoi(optarg); break;
      case 's': prefix = optarg; break;
      case 'S': duration_seconds = atoi(optarg); break;
      case 'h': default: usage(argv[0]); return 2;
    }
  }

  if (num_readers < 0) num_readers = 0;
  if (num_writers < 0) num_writers = 0;
  if (iterations < 0) iterations = 0;

  if (rw_reset() != 0) {
    fprintf(stderr, "Failed to reset kernel RW buffer. Are you running the right kernel?\n");
  }

  volatile uint64_t reads_done = 0, writes_done = 0;
  volatile bool stop_flag = false;

  pthread_t *threads = calloc((size_t)(num_readers + num_writers), sizeof(pthread_t));
  worker_args_t *args = calloc((size_t)(num_readers + num_writers), sizeof(worker_args_t));
  if (!threads || !args) {
    fprintf(stderr, "Allocation failure\n");
    return 1;
  }

  const size_t prefix_len = strlen(prefix);

  int idx = 0;
  for (int i = 0; i < num_writers; i++, idx++) {
    args[idx] = (worker_args_t){ .thread_index = i, .iterations = iterations, .message_prefix = prefix,
                                 .message_prefix_len = prefix_len, .writes_done = &writes_done,
                                 .reads_done = &reads_done, .stop_flag = &stop_flag };
    if (pthread_create(&threads[idx], NULL, writer_worker, &args[idx]) != 0) {
      fprintf(stderr, "Failed to create writer thread %d\n", i);
    }
  }
  for (int i = 0; i < num_readers; i++, idx++) {
    args[idx] = (worker_args_t){ .thread_index = i, .iterations = iterations, .message_prefix = prefix,
                                 .message_prefix_len = prefix_len, .writes_done = &writes_done,
                                 .reads_done = &reads_done, .stop_flag = &stop_flag };
    if (pthread_create(&threads[idx], NULL, reader_worker, &args[idx]) != 0) {
      fprintf(stderr, "Failed to create reader thread %d\n", i);
    }
  }

  time_t start = time(NULL);
  if (duration_seconds > 0) {
    sleep((unsigned int)duration_seconds);
    stop_flag = true;
  }

  for (int i = 0; i < num_readers + num_writers; i++) {
    pthread_join(threads[i], NULL);
  }
  time_t end = time(NULL);

  struct rw_stats stats = {0};
  rw_get_stats(&stats);

  double elapsed = difftime(end, start);
  if (elapsed <= 0) elapsed = 1.0;

  printf("Readers: %d, Writers: %d\n", num_readers, num_writers);
  printf("User-space counted  reads=%llu writes=%llu\n",
         (unsigned long long)reads_done, (unsigned long long)writes_done);
  printf("Kernel-space reported reads=%llu writes=%llu length=%zu\n",
         (unsigned long long)stats.reads, (unsigned long long)stats.writes, stats.length);
  printf("Elapsed: %.2fs | Reads/s: %.0f | Writes/s: %.0f\n", elapsed,
         reads_done / elapsed, writes_done / elapsed);

  free((void *)threads);
  free((void *)args);
  return 0;
}

