#include "../test/bare_hashmap.h"
#include "../test/kmem_cache.h"
#include "../test/kmem_page.h"
#include "../test/ring_buffer.h"
#include "../test/task.h"
#include <../test/atomic_test.h>
#include <cga.h>
#include <clock.h>
#include <debug.h>
#include <defs.h>
#include <gdt.h>
#include <interrupt.h>
#include <kbd.h>
#include <kernel_object.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <picirq.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <task.h>
#include <tty.h>
#include <virtual_memory.h>
#include <x86.h>

// 用来测试程序映像很大的时候会怎么样
// static char fuck[1024 * 1024 * 32];

void kmain();
void ktask0();

// 此函数用的是bootloader栈
void kentry() {
  extern uint32_t kernel_pd[];
  extern uint32_t program_begin[], program_end[];
  _Alignas(_4K) uint32_t temp_pd[1024];

  // 切换到临时页表，因为temp_pd是在bootloader栈上，这部分是V=P，所以不需要V2P
  memcpy(temp_pd, kernel_pd, 4096);
  lcr3((uintptr_t)temp_pd);

  // 现在只map了内核代码的前4M，我们需要map完整的内核映像
  // 假如内核映像没有用完最后一个4M页怎么办？这已经考虑过了，不会有影响的
  for (uintptr_t p = ((uintptr_t)program_begin) + _4M;
       p < (uintptr_t)program_end; p += _4M) {
    kernel_pd[p >> PDXSHIFT] = V2P(p) | PTE_P | PTE_PS | PTE_W;
  }
  // 虚拟地址0到16M -> 物理地址0到16M
  // 虚拟地址3G到3G+16M -> 物理地址0到16M
  for (uintptr_t p = 0; p < _4M * 4; p += _4M) {
    kernel_pd[p >> PDXSHIFT] = p | PTE_P | PTE_PS | PTE_W;
    kernel_pd[P2V(p) >> PDXSHIFT] = p | PTE_P | PTE_PS | PTE_W;
  }
  // 现在用的还是bootloader栈。要换成boot栈，它是内核映像结束之后第一个对齐的物理4M页
  // 把它map到虚拟内存最后一个4M页上
  const uintptr_t physical_boot_stack =
      V2P(ROUNDUP((uint32_t)program_end, _4M));
  kernel_pd[1023] = (physical_boot_stack & ~0x3FFFFF) | PTE_P | PTE_PS | PTE_W;

  // kernel_pd改好了，从temp_pd切回kernel_pd
  lcr3(V2P((uintptr_t)kernel_pd));

  // 确定kernel_pd是在bss外面的
  extern char bss_begin[], bss_end[];
  assert((uintptr_t)V2P(kernel_pd) >= (uintptr_t)bss_end ||
         (uintptr_t)V2P(kernel_pd) < (uintptr_t)bss_begin);
  // 清bss https://en.wikipedia.org/wiki/.bss#BSS_in_C
  extern char bss_begin[], bss_end[];
  memset(bss_begin, 0, bss_end - bss_begin);

  boot_stack_paddr = physical_boot_stack;

  // 在新栈上调kmain
  asm volatile("movl 0, %%ebp;movl $0xFFFFFFFF, %%esp;call kmain;" ::
                   : "memory");
  __builtin_unreachable();
}

// 此函数用的是boot栈
void kmain() {
  gdt_init();
  idt_init();
  pic_init();
  kbd_init();
  terminal_init();
  print_e820();
  printf("\n");
  kmem_init(e820map);
  kmem_page_init();
  kmem_page_debug();
  kmem_alloc_init();
  kmem_cache_init();
  free_region_init(e820map);
  virtual_memory_check();
  kernel_object_init();

  task_init();
  uint32_t esp, ebp, new_esp, new_ebp;
  const uint32_t stack_top = KERNEL_VIRTUAL_BOOT_STACK + KERNEL_BOOT_STACK_SIZE;
  uint32_t used_stack, current_stack_frame_size;
  extern uint32_t kernel_pd[];
  _Alignas(4096) uint32_t temp_pd[1024];
  // 将现在用的栈切换成这个内核任务的栈
  resp(&esp);
  rebp(&ebp);
  used_stack = stack_top - esp;
  current_stack_frame_size = ebp - esp;
  struct ktask *init = task_find(1);
  new_esp = init->kstack + (TASK_STACK_SIZE * 4096 - used_stack);
  new_ebp = new_esp + current_stack_frame_size;

  memcpy((void *)new_esp, (void *)esp, used_stack);
  // 然后unmap原来的内核栈，这样如果还有代码访问那个地方，就会被暴露出来
  // 首先还需要将kernel_pd的内容拷到temp_pd，然后临时使用temp_pd
  memcpy(temp_pd, kernel_pd, _4K);
  lcr3(linear2physical(kernel_pd, (uintptr_t)temp_pd));
  // 改kernel_pd
  kernel_pd[1023] = 0;
  // 取消0-4M的映射，要访问0-4M，请访问3G - 3G+4M
  kernel_pd[0] = 0;
  // 所以cga需要去高地址访问他的buffer了
  cga_enable_indirect_mem();
  // 在任务栈上调ktask0
  asm volatile("movl %0, %%ebp;movl %1, %%esp;call ktask0;"
               :
               : "r"(new_ebp), "r"(new_esp)
               : "memory");
  __builtin_unreachable();
}

// 此函数用的是task1的任务栈
void ktask0() {
  {
    /*
     有同学会说了，你到现在才从temp_pd切换回kernel_pd，而temp_pd是在kmain函数的栈上的，
     不会出事吗？这里我要说，因为kmain调用是在在boot栈上做的，而boot栈没人会去动，所以没关系的
    */
    extern uint32_t kernel_pd[];
    lcr3(V2P((uintptr_t)kernel_pd));
    // 将old ebp和ret address设为0，方便stacktrace停止
    uint32_t ebp;
    rebp(&ebp);
    *(uint32_t *)ebp = 0;
    *(uint32_t *)(ebp + 4) = 0;
    // 设置内核vm
    virtual_memory_init(&kernel_vm, kernel_pd);
    // 前3GB都是给用户程序用的
    virtual_memory_alloc(&kernel_vm, 0, (uint32_t)3 * 1024 * 1024 * 1024, 0,
                         KUSER, 0);
    // 3GB到3GB+16MB是给内核DMA用的
    virtual_memory_alloc(&kernel_vm, (uintptr_t)3 * 1024 * 1024 * 1024,
                         16 * 1024 * 1024, 0, KDMA, 0);
    // 然后是内核代码
    extern uint32_t program_begin[], program_end[];
    virtual_memory_alloc(
        &kernel_vm, KERNEL_VIRTUAL_BASE + 16 * 1024 * 1024,
        ROUNDUP((uintptr_t)program_end - (uintptr_t)program_begin, _4M), 0,
        KCODE, 0);
    // 然后是normal内存区
    virtual_memory_alloc(&kernel_vm, normal_region_vaddr, normal_region_size, 0,
                         KNORMAL, 0);
    /*
    最后是map区，就是有时候系统需要访问free
    region的东西，就把它临时map到这个map区
    看看free_region_access函数就懂了
     */
    struct virtual_memory_area *map_vma = virtual_memory_alloc(
        &kernel_vm, map_region_vaddr, map_region_size, 0, KMAP, 0);
    virtual_memory_print(&kernel_vm);
    // mapregion要用的，必须free了
    virtual_memory_free(&kernel_vm, map_vma);
  }
  task_inited = TASK_INITED_MAGIC;
  task_test();
  // https://en.wikipedia.org/wiki/Code_page_437
  putchar(1);
  putchar(1);
  putchar(1);
  printf("Welcome...\n");
  printf("\n\n");

  {
    extern uint32_t kernel_pd[];
    // page_directory_debug(kernel_pd);
  }

  // 跑测试
  ring_buffer_test();
  kmem_page_test();
  bare_hashmap_test();
  kmem_cache_test();
  atomic_test();
  printf("\n\n");
  print_kernel_size();
  printf("\n");
  printf("\n\n");
  print_cur_status();
  printf("\n\n");

  task_preemptive_set(false);
  clock_init();
  enable_interrupt();

  if (true) {
    // 创建共享内存，把countdown程序的代码拷贝进去，让task_test来启动它
    extern char _binary____program_count_down_main_exe_start[],
        _binary____program_count_down_main_exe_size[];
    const char *test_string = "count_down";
    // 创建共享内存
    uint32_t shid_str = shared_memory_create(strlen(test_string));
    uint32_t shid_prog = shared_memory_create(
        (uint32_t)_binary____program_count_down_main_exe_size);
    // 拷贝测试字符串
    struct shared_memory *sh_str = shared_memory_ctx(shid_str);
    void *access_str =
        free_region_access(sh_str->physical, sh_str->pgcnt * _4K);
    memcpy(access_str, test_string, strlen(test_string));
    // 拷贝countdown程序
    struct shared_memory *sh_prog = shared_memory_ctx(shid_prog);
    void *access_prog =
        free_region_access(sh_prog->physical, sh_prog->pgcnt * _4K);
    memcpy(access_prog, _binary____program_count_down_main_exe_start,
           (uint32_t)_binary____program_count_down_main_exe_size);
    free_region_no_access(access_str);
    free_region_no_access(access_prog);

    // 通过程序参数传进去共享内存的id
    union {
      uint32_t integer;
      unsigned char str[5];
    } punning;
    struct task_args args;
    task_args_init(&args);
    punning.str[4] = '\0';
    // 字符串共享内存的id
    punning.integer = shid_str;
    task_args_add(&args, (const char *)&punning.str, 0, false);
    // 程序共享内存的id
    punning.integer = shid_prog;
    task_args_add(&args, (const char *)&punning.str, 0, false);
    // 程序长度
    punning.integer = (uint32_t)_binary____program_count_down_main_exe_size;
    task_args_add(&args, (const char *)&punning.str, 0, false);
    extern char _binary____program_task_test_main_exe_start[],
        _binary____program_task_test_main_exe_size[];
    SMART_CRITICAL_REGION
    pid_t pid =
        task_create_user((void *)_binary____program_task_test_main_exe_start,
                         (uint32_t)_binary____program_task_test_main_exe_size,
                         "task_test", 0, DEFAULT_ENTRY, false, &args);
    kernel_object_ref_safe(pid, shid_str);
    kernel_object_ref_safe(pid, shid_prog);
    kernel_object_unref(task_current()->group, shid_str, true);
    kernel_object_unref(task_current()->group, shid_prog, true);
    task_args_destroy(&args, true);
  }

  if (true) {
    struct task_args args;
    task_args_init(&args);
    task_args_add(&args, "I AM KERNEL! (1)", 0, false);
    task_args_add(&args, "I AM KERNEL! (2)", 0, false);
    extern char _binary____program_count_down_main_exe_start[],
        _binary____program_count_down_main_exe_size[];
    task_create_user((void *)_binary____program_count_down_main_exe_start,
                     (uint32_t)_binary____program_count_down_main_exe_size,
                     "count_down_1", 0, DEFAULT_ENTRY, false, &args);
    task_create_user((void *)_binary____program_count_down_main_exe_start,
                     (uint32_t)_binary____program_count_down_main_exe_size,
                     "count_down_2", 0, DEFAULT_ENTRY, false, &args);
    task_create_user((void *)_binary____program_count_down_main_exe_start,
                     (uint32_t)_binary____program_count_down_main_exe_size,
                     "count_down_3", 0, DEFAULT_ENTRY, false, &args);
    task_args_destroy(&args, true);
  }

  if (true) {
    extern char _binary____program_schd_test_main_exe_start[],
        _binary____program_schd_test_main_exe_size[];
    struct task_args args;
    task_args_init(&args);
    task_args_add(&args, "1", 0, false);
    struct task_args args2;
    task_args_init(&args2);
    task_args_add(&args2, "0", 0, false);
    task_create_user((void *)_binary____program_schd_test_main_exe_start,
                     (uint32_t)_binary____program_schd_test_main_exe_size,
                     "schd_test_1", 0, DEFAULT_ENTRY, false, &args);
    task_create_user((void *)_binary____program_schd_test_main_exe_start,
                     (uint32_t)_binary____program_schd_test_main_exe_size,
                     "schd_test_2", 0, DEFAULT_ENTRY, false, &args2);
    task_args_destroy(&args, true);
    task_args_destroy(&args2, true);
  }

  if (true) {
    extern char _binary____program_abort_main_exe_start[],
        _binary____program_abort_main_exe_size[];
    task_create_user((void *)_binary____program_abort_main_exe_start,
                     (uint32_t)_binary____program_abort_main_exe_size, "abort",
                     0, DEFAULT_ENTRY, false, 0);
  }

  if (true) {
    extern char _binary____program_bad_access_main_exe_start[],
        _binary____program_bad_access_main_exe_size[];
    task_create_user((void *)_binary____program_bad_access_main_exe_start,
                     (uint32_t)_binary____program_bad_access_main_exe_size,
                     "bad_access", 0, DEFAULT_ENTRY, false, 0);
  }

  printf("nonoOS:$ ");
  // task_display();

  task_idle();

  panic("task_idle returned?");
  __builtin_unreachable();
}
