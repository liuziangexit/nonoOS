#include <kernel_object.h>
#include <mutex.h>
#include <syscall.h>

uint32_t mtx_create() {
  return syscall(SYSCALL_MTX, 1, USER_MTX_ACTION_CREATE);
}

void mtx_lock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_LOCK, id);
}

void mtx_timedlock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_TIMEDLOCK, id);
}

void mtx_trylock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_TRYLOCK, id);
}

void mtx_unlock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_UNLOCK, id);
}