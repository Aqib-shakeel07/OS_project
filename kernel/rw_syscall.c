// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/rwsem.h>
#include <linux/atomic.h>
#include <linux/string.h>

#define RW_SHARED_BUFFER_SIZE 4096

enum rw_operation {
    RW_OP_READ = 0,
    RW_OP_WRITE = 1,
    RW_OP_RESET = 2,
    RW_OP_STATS = 3,
};

struct rw_args {
    int operation;
    void __user *buffer; 
};

static DECLARE_RWSEM(rw_buffer_sem);
static char shared_buffer[RW_SHARED_BUFFER_SIZE];
static size_t shared_length;
static atomic64_t total_reads = ATOMIC64_INIT(0);
static atomic64_t total_writes = ATOMIC64_INIT(0);

struct rw_stats {
    u64 reads;
    u64 writes;
    size_t length;
};

SYSCALL_DEFINE1(reader_writer, struct rw_args __user *, user_args)
{
    struct rw_args args;

    if (!user_args)
        return -EINVAL;

    if (copy_from_user(&args, user_args, sizeof(args)))
        return -EFAULT;

    switch (args.operation) {
    case RW_OP_READ: {
        ssize_t to_copy;
        if (args.length == 0)
            return 0;

        down_read(&rw_buffer_sem);
        to_copy = min_t(size_t, shared_length, args.length);
        if (to_copy > 0 && args.buffer) {
            if (copy_to_user(args.buffer, shared_buffer, to_copy)) {
                up_read(&rw_buffer_sem);
                return -EFAULT;
            }
        }
        atomic64_inc(&total_reads);
        up_read(&rw_buffer_sem);
        return to_copy;
    }
    case RW_OP_WRITE: {
        ssize_t to_copy;
        if (!args.buffer || args.length == 0)
            return -EINVAL;

        to_copy = (args.length > RW_SHARED_BUFFER_SIZE)
                      ? RW_SHARED_BUFFER_SIZE
                      : (ssize_t)args.length;

        down_write(&rw_buffer_sem);
        if (copy_from_user(shared_buffer, args.buffer, to_copy)) {
            up_write(&rw_buffer_sem);
            return -EFAULT;
        }
        shared_length = to_copy;
        atomic64_inc(&total_writes);
        up_write(&rw_buffer_sem);
        return to_copy;
    }
    case RW_OP_RESET:
        down_write(&rw_buffer_sem);
        memset(shared_buffer, 0, RW_SHARED_BUFFER_SIZE);
        shared_length = 0;
        atomic64_set(&total_reads, 0);
        atomic64_set(&total_writes, 0);
        up_write(&rw_buffer_sem);
        return 0;
    case RW_OP_STATS: {
        struct rw_stats kstats;
        if (!args.buffer || args.length < sizeof(kstats))
            return -EINVAL;
        kstats.reads = (u64)atomic64_read(&total_reads);
        kstats.writes = (u64)atomic64_read(&total_writes);
        kstats.length = READ_ONCE(shared_length);
        if (copy_to_user(args.buffer, &kstats, sizeof(kstats)))
            return -EFAULT;
        return 0;
    }
    default:
        return -EINVAL;
    }
}

