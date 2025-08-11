Kernel Integration Guide — reader_writer system call

Prerequisites

- Linux kernel source tree (e.g., 5.x or 6.x).
- Basic kernel build environment set up for your distro/target.

Files Provided

- `kernel/rw_syscall.c`: Implementation of the syscall using `SYSCALL_DEFINE1(reader_writer, ...)`.

Steps (x86_64)

1) Copy the syscall implementation into the kernel source tree, e.g.:
   - Place `rw_syscall.c` into `kernel/` (or another suitable directory) and add it to the kernel `Makefile` in that directory.

   In `kernel/Makefile`, add a line near similar object files:

   `obj-y += rw_syscall.o`

2) Declare syscall number in the syscall table.

   Edit `arch/x86/entry/syscalls/syscall_64.tbl` and add a new line with an unused number (example uses 449; choose a free one in your tree):

   `449  common  reader_writer   sys_reader_writer`

   Notes:
   - On modern kernels, `SYSCALL_DEFINE1(reader_writer, ...)` generates the symbol `__x64_sys_reader_writer`, which is correctly mapped from `sys_reader_writer` in the table.
   - Ensure the number 449 is not already in use. Adjust as needed.

3) Add a prototype (older kernels only).

   Some older trees require an explicit prototype in `include/linux/syscalls.h`:

   `asmlinkage long sys_reader_writer(struct rw_args __user *user_args);`

   For contemporary kernels using `SYSCALL_DEFINE*` macros, this is typically not required.

4) Build and install the kernel.

   Typical sequence:
   - `make olddefconfig`
   - `make -j$(nproc)`
   - `make modules_install`
   - `make install`
   - Update bootloader if necessary and reboot into the new kernel.

5) User-space definition of the syscall number.

   Ensure `user/reader_writer_api.h` uses the same syscall number (e.g., 449). If your distro exposes it via headers after installation, you may omit the manual define.

Validation

- After booting into the new kernel, run the tester from `user/`.
- Use strace to confirm the new syscall is invoked: `strace -e trace=reader_writer ./rw_test ...` (if strace is updated with the name); otherwise filter by number.

Troubleshooting

- Build errors in `syscall_64.tbl`: likely a duplicate or invalid number; pick another.
- Link errors about missing symbol: ensure `obj-y += rw_syscall.o` is present and the file is in the right directory.
- Copy faults (`-EFAULT`): verify user buffers are valid and sizes are correct.

