#include <condition_variable.h>
#include <kernel_object.h>
#include <stdbool.h>
#include <stdint.h>
#include <syscall.h>

uint32_t cv_create() {
  return (uint32_t)syscall(SYSCALL_CV, 1, USER_CV_ACTION_CREATE);
}

void cv_wait(uint32_t cv_id, uint32_t mut_id) {
  syscall(SYSCALL_CV, 3, USER_CV_ACTION_WAIT, cv_id, mut_id);
}

bool cv_timedwait(uint32_t cv_id, uint32_t mut_id, uint64_t ms) {
  return syscall(SYSCALL_CV, 5, USER_CV_ACTION_TIMEDWAIT, cv_id, mut_id,
                 (uint32_t)(ms >> 32), (uint32_t)(ms & 0xFFFFFFFF));
}

void cv_notify_one(uint32_t cv_id, uint32_t mut_id) {
  syscall(SYSCALL_CV, 3, USER_CV_ACTION_NOTIFY_ONE, cv_id, mut_id);
}

void cv_notify_all(uint32_t cv_id, uint32_t mut_id) {
  syscall(SYSCALL_CV, 3, USER_CV_ACTION_NOTIFY_ALL, cv_id, mut_id);
}
