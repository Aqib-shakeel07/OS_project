#pragma once
#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_reader_writer
#define __NR_reader_writer 548
#endif

enum rw_operation {
  RW_OP_READ = 0,
  RW_OP_WRITE = 1,
  RW_OP_RESET = 2,
  RW_OP_STATS = 3,
};

struct rw_args {
  int operation;
  void *buffer;   
  size_t length;  
};

struct rw_stats {
  uint64_t reads;
  uint64_t writes;
  size_t length;
};

static inline long rw_syscall(struct rw_args *args) {
  return syscall(__NR_reader_writer, args);
}

static inline long rw_read(void *buffer, size_t length) {
  struct rw_args args = { .operation = RW_OP_READ, .buffer = buffer, .length = length };
  return rw_syscall(&args);
}

static inline long rw_write(const void *buffer, size_t length) {
  struct rw_args args = { .operation = RW_OP_WRITE, .buffer = (void *)buffer, .length = length };
  return rw_syscall(&args);
}

static inline int rw_reset(void) {
  struct rw_args args = { .operation = RW_OP_RESET, .buffer = NULL, .length = 0 };
  long rc = rw_syscall(&args);
  return (int)rc;
}

static inline int rw_get_stats(struct rw_stats *out_stats) {
  struct rw_args args = { .operation = RW_OP_STATS, .buffer = out_stats, .length = sizeof(*out_stats) };
  long rc = rw_syscall(&args);
  return (int)rc;
}

