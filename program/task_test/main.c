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

// 扮演一个FIFO的Q
vector_t v;

int producer() {
  printf("producer: BEGIN! pid is %lld\n", (int64_t)get_pid());

  // for loop：每秒往vector末尾里塞一个数字，5、4、3、2、1、0
  for (int32_t i = 5; i >= 0; i--) {
    printf("producer: adding %d to vector\n", i);

    // 上锁
    mtx_lock(cv_mut);

    // 塞数字
    // 等价于std::vector::push_back
    vector_add(&v, &i);

    // 发通知
    cv_notify_one(cv, cv_mut);

    // 解锁
    mtx_unlock(cv_mut);

    // 等一秒
    sleep(1000);
  }

  printf("producer: QUIT\n");
  return 0;
}

int consumer() {
  // sleep(3000);
  printf("consumer: BEGIN! pid is %lld\n", (int64_t)get_pid());

  // while
  // loop：不断从vector中拿数字。如果vector是空的则等待通知。如果发现拿到0则退出。
  while (true) {
    // 上锁
    mtx_lock(cv_mut);

    // 如果vector是空的...
    while (vector_count(&v) == 0) {
      // ...则等通知
      printf("consumer: WAITING\n");
      cv_wait(cv, cv_mut);
    }

    // 在这里，vector必然不是空的

    // 拿vector里的首个元素并将其从vector中移除
    int value = *(int *)vector_get(&v, 0);
    vector_remove(&v, 0);

    printf("consumer: read %d from vector\n", value);

    // 解锁
    mtx_unlock(cv_mut);

    // 如果发现这次拿到的是0，就退出while loop
    if (value == 0)
      break;
  }

  printf("consumer: QUIT\n");
  return 0;
}

int main(int argc, char **argv) {
  // 主线程，启动！
  printf("main: BEGIN\n");

  // 初始化vector
  vector_init(&v, sizeof(int), malloc);
  // 初始化条件变量和互斥锁
  cv = cv_create();
  cv_mut = mtx_create();

  // 创建producer线程
  printf("main: creating producer\n");
  pid_t producer_pid =
      create_task(0, 0, "producer", false, (uintptr_t)producer, 0, 0, 0);

  // 创建consumer线程
  printf("main: creating consumer\n");
  pid_t consumer_pid =
      create_task(0, 0, "consumer", false, (uintptr_t)consumer, 0, 0, 0);

  // 等待producer退出
  printf("main: WAITING for producer to quit...\n");
  join(producer_pid);
  printf("main: producer ended!!\n");

  // 等待consumer退出
  printf("main: WAITING for consumer to quit...\n");
  join(consumer_pid);
  printf("main: consumer ended!!\n");

  // 主线程退出
  printf("main: EXIT\n");
  return 0;
}