#include <syscall.h>
#include <task.h>
#include <unistd.h>

void sleep(uint64_t ms) {
  // printf("SLEEP %lld %lld %lld\n", ms, (int64_t)(ms >> 32),
  //        (int64_t)(ms & 0xFFFFFFFF));
  syscall(SYSCALL_TASK, 3, USER_TASK_ACTION_SLEEP, (uint32_t)(ms >> 32),
          (uint32_t)(ms & 0xFFFFFFFF));
}
