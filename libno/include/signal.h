#ifndef __LIBNO_SIGNAL_H__
#define __LIBNO_SIGNAL_H__
#include "signal_def.h"
#include <stdint.h>
typedef uint32_t pid_t;

// 这个文件里面按照POSIX标准定义了信号相关的宏和函数
// 实际的实现在kernel里的那个signal.h和signal.c里

struct sigset {
  // 1表示true，0表示false
  unsigned char s[SIGMAX];
};
typedef struct sigset sigset_t;

// Interrupt from keyboard
#define SIGINT 2

// Quit from keyboard
#define SIGQUIT 3

// Abort signal from abort(3)
#define SIGABRT 6

// Kill signal
#define SIGKILL 9

// Invalid memory reference
#define SIGSEGV 11
// Address not mapped to object.
#define SEGV_MAPERR 1
// Invalid permissions for mapped object.
#define SEGV_ACCERR 2

// Termination signal
// Unlike the SIGKILL signal, it can be caught and interpreted or ignored by the
// process.
#define SIGTERM 15

// The signal handler is set to default signal handler.
#define SIG_DFL 0
// The signal is ignored.
#define SIGEV_SIGNAL 0
// 注册信号处理回调
// 是对signal_set_handler的包装
void signal(int sig, void (*handler)(int));

// 等待本线程发生指定的信号
// 是对signal_wait的包装
//  On success, sigwait() returns 0.  On error, it returns a positive
//        error number (listed in ERRORS).
int sigwait(const sigset_t *restrict set, int *restrict sig);

// 向某个线程发送信号
// 是对signal_fire的包装
int kill(pid_t pid, int sig);

// 向某个进程(task_group中的所有task)发送信号
// 是对signal_fire的包装
int killpg(pid_t pid, int sig);

// This function initializes the signal set set to exclude all of the defined
// signals. It always returns 0.
int sigemptyset(sigset_t *set);

// This function initializes the signal set set to include all of the defined
// signals. Again, the return value is 0.
int sigfillset(sigset_t *set);

// This function adds the signal signum to the signal set set.
// The return value is 0 on success and -1 on failure.
int sigaddset(sigset_t *set, int signum);

// This function removes the signal signum from the signal set set.
// The return value and error conditions are the same as for sigaddset.
int sigdelset(sigset_t *set, int signum);

// The sigismember function tests whether the signal signum is a member of the
// signal set set. It returns 1 if the signal is in the set, 0 if not, and -1 if
// there is an error.
int sigismember(const sigset_t *set, int signum);

#endif
