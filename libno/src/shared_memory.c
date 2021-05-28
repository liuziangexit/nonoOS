#include <shared_memory.h>
#include <syscall.h>

uint32_t shared_memory_create(size_t size) {
  return syscall(SYSCALL_SHM, 2, USER_SHM_ACTION_CREATE, size);
}

void *shared_memory_map(uint32_t id, void *addr) {
  return (void *)syscall(SYSCALL_SHM, 3, USER_SHM_ACTION_MAP, id, addr);
}

void shared_memory_unmap(void *addr) {
  syscall(SYSCALL_SHM, 2, USER_SHM_ACTION_UNMAP, addr);
}