#include <kernel_object.h>
#include <mutex.h>
#include <syscall.h>

uint32_t mtx_create() {
  return (uint32_t)syscall(SYSCALL_MTX, 1, USER_MTX_ACTION_CREATE);
}

void mtx_lock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_LOCK, id);
}

bool mtx_timedlock(uint32_t id, uint64_t ms) {
  return syscall(SYSCALL_MTX, 4, USER_MTX_ACTION_TIMEDLOCK, id,
                 (uint32_t)(ms >> 32), (uint32_t)(ms & 0xFFFFFFFF));
}

bool mtx_trylock(uint32_t id) {
  return syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_TRYLOCK, id);
}

void mtx_unlock(uint32_t id) {
  syscall(SYSCALL_MTX, 2, USER_MTX_ACTION_UNLOCK, id);
}