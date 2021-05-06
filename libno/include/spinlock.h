#ifndef __LIBNO_SPINLOCK_H__
#define __LIBNO_SPINLOCK_H__
#include <stdbool.h>
#include <stdint.h>
//#include <task.h>

struct spinlock {
  uint32_t val;
  // pid_t owner;
};
typedef struct spinlock spinlock_t;

void spin_init(spinlock_t *l);
void spin_lock(spinlock_t *l);
bool spin_trylock(spinlock_t *l);
void spin_unlock(spinlock_t *l);
// void spin_owns();

#endif
