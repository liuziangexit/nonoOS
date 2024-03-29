#include <compiler_helper.h>
#include <condition_variable.h>
#include <mutex.h>
#include <shared_memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <unistd.h>

uint32_t mut_id;
uint32_t cv;
uint32_t cv_mut;
bool quit;

int thr_main() {
  printf("new_thread: pid is %lld\n", (int64_t)get_pid());
  printf("new_thread: mtx_trylock()\n");
  if (!mtx_trylock(mut_id)) {
    printf("task_test: mtx_trylock() failed, mtx_timedlock(1s)\n");
    if (!mtx_timedlock(mut_id, 1000)) {
      printf("new_thread: mtx_timedlock() failed, since timeout expired\n");
      printf("new_thread: mtx_lock()\n");
      mtx_lock(mut_id);
    }
  }
  printf("new_thread: locked\n");
  printf("new_thread: sleep 2s\n");
  sleep(2000);
  printf("new_thread: mtx_unlock()\n");
  mtx_unlock(mut_id);
  printf("new_thread: quit\n");
  return 888;
}

int thr_cv_main() {
  printf("new_cv_thread %lld: start\n", (int64_t)get_pid());
  printf("new_cv_thread %lld: lock %d\n", (int64_t)get_pid(), cv_mut);
  mtx_lock(cv_mut);
  while (true) {
    if (quit) {
      printf("new_cv_thread %lld: condition met\n", (int64_t)get_pid());
      mtx_unlock(cv_mut);
      break;
    } else {
      printf("new_cv_thread %lld: condition not met\n", (int64_t)get_pid());
    }
    printf("new_cv_thread %lld: wait\n", (int64_t)get_pid());
    cv_wait(cv, cv_mut);
    printf("new_cv_thread %lld: been notified\n", (int64_t)get_pid());
  }
  printf("new_cv_thread %lld: quit\n", (int64_t)get_pid());
  return 8898;
}

int main(int argc, char **argv) {
  UNUSED(argc);
  printf("task_test: begin\n");
  union {
    uint32_t integer;
    unsigned char str[4];
  } punning;
  // 测一下内核传进来的共享内存
  memcpy(&punning.str, argv[0], 4);
  printf("task_test: shared memory id is %lld\n", (int64_t)punning.integer);
  void *vaddr = shared_memory_map(punning.integer, 0);
  printf("task_test: shared memory map to 0x%08llx\n",
         (int64_t)(uintptr_t)vaddr);
  printf("task_test: shared memory content: %s\n", vaddr);
  // shared_memory_unmap(vaddr);
  memcpy(&punning.str, argv[1], 4);
  uint32_t prog_shid = punning.integer;
  memcpy(&punning.str, argv[2], 4);
  const uint32_t prog_size = punning.integer;
  printf("task_test: program size: %lld\n", (int64_t)prog_size);
  void *vaddr_prog = shared_memory_map(prog_shid, 0);
  const pid_t pid = create_task(
      vaddr_prog, prog_size, vaddr, true, DEFAULT_ENTRY, CREATE_TASK_REF, 3,
      "I AM task_test 1", "I AM task_test 2", "I AM task_test 3");
  printf("task_test: task %lld created\n", (int64_t)pid);
  const pid_t pid2 = create_task(vaddr_prog, prog_size, vaddr, true,
                                 DEFAULT_ENTRY, CREATE_TASK_REF, 0);
  printf("task_test: task %lld created\n", (int64_t)pid2);
  printf("task_test: waiting %lld to quit\n", (int64_t)pid);
  int32_t ret = join(pid);
  printf("task_test: %lld exited with code %d\n", (int64_t)pid, ret);
  sleep(1000);
  printf("task_test: waiting %lld to quit\n", (int64_t)pid2);
  int32_t ret2 = join(pid2);
  printf("task_test: %lld exited with code %d\n", (int64_t)pid2, ret2);

  // 测试线程mutex
  mut_id = mtx_create();
  pid_t new_thr =
      create_task(0, 0, "new_thread", false, (uintptr_t)thr_main, 0, 0, 0);
  // pid_t new_thr = create_task(0, 0, "new_thread", false, (uintptr_t)thr_main,
  //                             CREATE_TASK_REF, 0, 0);
  UNUSED(new_thr);
  // int32_t new_thr_ret = join(new_thr);
  // printf("task_test: %lld exited with code %d\n", (int64_t)new_thr,
  //        new_thr_ret);

  printf("task_test: mtx_trylock()\n");
  if (!mtx_trylock(mut_id)) {
    printf("task_test: mtx_trylock() failed, mtx_timedlock(1s)\n");
    if (!mtx_timedlock(mut_id, 1000)) {
      printf("task_test: mtx_timedlock() failed, since timeout expired\n");
      printf("task_test: mtx_lock()\n");
      mtx_lock(mut_id);
    }
  }
  printf("task_test: locked\n");
  // TODO reentry测试
  //  mtx_lock(mut_id);
  printf("task_test: sleep 2s\n");
  sleep(2000);
  printf("task_test: mtx_unlock()\n");
  mtx_unlock(mut_id);

  cv = cv_create();
  cv_mut = mtx_create();
  quit = false;
  create_task(0, 0, "new_cv_thread", false, (uintptr_t)thr_cv_main, 0, 0, 0);
  create_task(0, 0, "new_cv_thread", false, (uintptr_t)thr_cv_main, 0, 0, 0);

  // cv_notify_one(cv, cv_mut);
  printf("task_test: notify all while condition not set\n");
  cv_notify_all(cv, cv_mut);
  sleep(2000);
  mtx_lock(cv_mut);
  printf("task_test: set condition to true then notify all\n");
  quit = true;
  mtx_unlock(cv_mut);
  cv_notify_all(cv, cv_mut);

  printf("task_test: exit\n");
  return 0;
}