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
#include <vector.h>

uint32_t cv;
uint32_t cv_mut;
vector_t v;

int producer() {
  printf("producer: BEGIN! pid is %lld\n", (int64_t)get_pid());

  for (int32_t i = 5; i >= 0; i--) {
    printf("producer: adding %d to vector\n", i);
    mtx_lock(cv_mut);
    vector_add(&v, &i);
    cv_notify_one(cv, cv_mut);
    mtx_unlock(cv_mut);
    sleep(1000);
  }

  printf("producer: QUIT\n");
  return 0;
}

int consumer() {
  //sleep(3000);
  printf("consumer: BEGIN! pid is %lld\n", (int64_t)get_pid());

  while (true) {
    mtx_lock(cv_mut);
    while (vector_count(&v) == 0) {
      printf("consumer: WAITING\n");
      cv_wait(cv, cv_mut);
    }

    int value = *(int *)vector_get(&v, 0);
    vector_remove(&v, 0);

    printf("consumer: read %d from vector\n", value);
    mtx_unlock(cv_mut);
    if (value == 0)
      break;
  }

  printf("consumer: QUIT\n");
  return 0;
}

int main(int argc, char **argv) {
  printf("main: BEGIN\n");

  vector_init(&v, sizeof(int), malloc);
  cv = cv_create();
  cv_mut = mtx_create();

  printf("main: creating producer\n");
  pid_t producer_pid =
      create_task(0, 0, "producer", false, (uintptr_t)producer, 0, 0, 0);

  printf("main: creating consumer\n");
  pid_t consumer_pid =
      create_task(0, 0, "consumer", false, (uintptr_t)consumer, 0, 0, 0);

  printf("main: WAITING for producer to quit...\n");
  join(producer_pid);
  printf("main: producer ended!!\n");

  printf("main: WAITING for consumer to quit...\n");
  join(consumer_pid);
  printf("main: consumer ended!!\n");

  printf("main: EXIT\n");
  return 0;
}