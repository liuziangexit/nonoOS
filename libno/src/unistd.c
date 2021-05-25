#include <syscall.h>
#include <unistd.h>

void sleep(uint64_t ms) {
  syscall(SYSCALL_SLEEP, 2, (uint32_t)(ms >> 32), (uint32_t)(ms & 0xFFFFFFFF));
}
