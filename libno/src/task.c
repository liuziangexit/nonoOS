#include <task.h>
#include <syscall.h>

pid_t get_pid() { return syscall(SYSCALL_GETPID, 0); }
