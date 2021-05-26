#ifndef __LIBNO_TASK_H__
#define __LIBNO_TASK_H__
#ifdef LIBNO_USER
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t pid_t;
pid_t get_pid();

#define DEFAULT_ENTRY 0
pid_t create_task(void *program, uint32_t program_size, const char *name,
                  bool new_group, uintptr_t entry, ...);
#endif
#endif
