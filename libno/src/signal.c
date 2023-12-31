#include <signal.h>

#ifndef LIBNO_USER

// 注册信号处理回调
void signal(int sig, void (*handler)(int)) {}

// 向某个线程发送信号
int kill(pid_t, int) { return 0; }

// 向某个进程(task_group中的所有task)发送信号
int killpg(pid_t, int) { return 0; }

#else

// 注册信号处理回调
void signal(int sig, void (*handler)(int)) {}

// 向某个线程发送信号
int kill(pid_t, int) { return 0; }

// 向某个进程(task_group中的所有task)发送信号
int killpg(pid_t, int) { return 0; }

#endif

// This function initializes the signal set set to exclude all of the defined
// signals. It always returns 0.
int sigemptyset(sigset_t *set) { return 0; }

// This function initializes the signal set set to include all of the defined
// signals. Again, the return value is 0.
int sigfillset(sigset_t *set) { return 0; }

// This function adds the signal signum to the signal set set.
// The return value is 0 on success and -1 on failure.
int sigaddset(sigset_t *set, int signum) { return 0; }

// This function removes the signal signum from the signal set set.
// The return value and error conditions are the same as for sigaddset.
int sigdelset(sigset_t *set, int signum) { return 0; }

// The sigismember function tests whether the signal signum is a member of the
// signal set set. It returns 1 if the signal is in the set, 0 if not, and -1 if
// there is an error.
int sigismember(const sigset_t *set, int signum) { return 0; }