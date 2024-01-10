#ifndef __KERNEL_SHELL_H__
#define __KERNEL_SHELL_H__
#include "task.h"
#include <stdbool.h>

int shell_main(int argc, char **argv);

bool shell_ready();
void shell_set_fg(pid_t);
pid_t shell_fg();
pid_t shell_pid();

// 执行程序
int shell_execute(const char *name, int argc, char **argv);

#endif
