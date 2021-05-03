#include <defs.h>
#include <panic.h>

void disable_interrupt();
void enable_interrupt();

enum memory_order {
  RELAXED = 0,       // 没有顺序制约
  COMPILER_ONLY = 1, // 仅编译器fence
  ACQUIRE = 2, // fence之后的RW不能重排到fence之前的R之前 + 编译器fence
  RELEASE = 4, // fence之前的RW不能重排到fence之后的W之后 + 编译器fence
  SEQ_CST = 8 // ACQUIRE + RELEASE + 全局唯一顺序(Single Total Order)
};

// x86 实现
__always_inline static inline void memory_barrier(enum memory_order order) {
  if (order == RELAXED) {
    return;
  }
  if (order == COMPILER_ONLY) {
    /*
    阻止编译器重排指令
    Intel ICC: __memory_barrier()
    MSVC: _ReadWriteBarrier()
     */
    // GCC
    asm volatile("" ::: "memory");
    return;
  }
  if (order == RELEASE) {
    /*
    RW/W
    用一个RW/RW的mfence屏障来实现
    */
    asm volatile("mfence" ::: "memory");
    return;
  }
  if (order == ACQUIRE) {
    /*
    R/RW
    用一个R/RW的lfence屏障来实现
    */
    asm volatile("lfence" ::: "memory");
    return;
  }
  if (order == SEQ_CST) {
    /*
    RW/RW
    在x86，一个mfence就足够实现全局顺序了
    分两种情况:
    1)单处理器情况，这种情况下自动就是全局唯一顺序了，因为“全局”只有这一个核心
    2)SMP情况，这种情况下，一个mfence也已经足够，因为本身x86就保证了TSO，也就是
    到处都有SFENCE，阻止“W/W”形式的重排。那么只需要在这里加"RW/RW"的屏障就够了
    */
    asm volatile("mfence" ::: "memory");
    return;
  }
  painc("unknown memory_order");
}
