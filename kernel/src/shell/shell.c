#include "shell.h"
#include "sync.h"
#include "task.h"
#include <atomic.h>
#include <compiler_helper.h>
#include <kernel_object.h>
#include <memory_manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITAIL_GETS_BUFFER_LEN 256

uint32_t task_count();

static bool __sr = false;

static void run_user_input_program() {
  // 创建共享内存，把countdown程序的代码拷贝进去，让task_test来启动它
  extern char _binary____program_user_read_input_main_exe_start[],
      _binary____program_user_read_input_main_exe_size[];
  uint32_t shid_prog = shared_memory_create(
      (uint32_t)_binary____program_user_read_input_main_exe_size);
  // 拷贝countdown程序
  struct shared_memory *sh_prog = shared_memory_ctx(shid_prog);
  void *access_prog = free_region_access(
      task_current()->group->vm, task_current()->group->vm_mutex,
      sh_prog->physical, sh_prog->pgcnt * _4K);
  memcpy(access_prog, _binary____program_user_read_input_main_exe_start,
         (uint32_t)_binary____program_user_read_input_main_exe_size);
  free_region_finish_access(task_current()->group->vm,
                            task_current()->group->vm_mutex, access_prog);

  uint32_t save;
  enter_critical_region(&save);

  pid_t pid = task_create_user(
      (void *)_binary____program_user_read_input_main_exe_start,
      (uint32_t)_binary____program_user_read_input_main_exe_size,
      "user_read_input", 0, DEFAULT_ENTRY, false, NULL);
  kernel_object_ref_safe(pid, shid_prog);
  kernel_object_unref(task_current()->group, shid_prog, true);

  shell_set_fg(pid);

  leave_critical_region(&save);

  printf_color(CGA_COLOR_DARK_GREY,
               "shell is waiting for user process to exit...\n\n");

  task_join(pid, NULL);
  shell_set_fg(task_current()->id);

  printf_color(CGA_COLOR_DARK_GREY, "\n\nshell is back!\n\n");
}

int shell_main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);

  // waiting all other task to quit
  {
    disable_interrupt();
    while (task_count() != 2) {
      enable_interrupt();
      task_sleep(1050);
      disable_interrupt();
    }
    enable_interrupt();
  }

  shell_set_fg(task_current()->id);
  __sr = true;

  task_display();

  // https://en.wikipedia.org/wiki/Code_page_437
  printf("System was compiled at " __DATE__ ", " __TIME__ ".\n");
  printf("nonoOS has been loaded. ");
  putchar(1);
  putchar(1);
  putchar(1);
  printf("\n\n");

  char *str = malloc(INITAIL_GETS_BUFFER_LEN);
  assert(str);
  size_t str_len = INITAIL_GETS_BUFFER_LEN;

  while (true) {
    printf_color(CGA_COLOR_DARK_GREY, "nonoOS:$ ");

    // char look = getchar();
    // printf("\n./you have entered: %c\n\n", look);

  RETRY_KGETS:
    size_t r = kgets(str, str_len);
    if (r > str_len) {
      free(str);
      str = malloc(r);
      if (!str) {
        printf("unable to allocate %d bytes for kgets\n", (int)r);
        panic("");
      }
      str_len = r;
      goto RETRY_KGETS;
    }

    printf("you have entered: %s\n\n", str, (int)r);

    if (strcmp(str, "./user_read_input_test") == 0) {
      run_user_input_program();
    } else if (strcmp(str, "ps") == 0) {
      task_display();
    }
  }
  __unreachable
}

bool shell_ready() { return __sr; }

static pid_t fg;
void shell_set_fg(pid_t pid) { atomic_store(&fg, pid); }

pid_t shell_fg() { return atomic_load(&fg); }

int shell_execute_user(const char *name, int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  if (strcmp(name, "countdown") == 0) {
    return 0;
  }
  return 0;
}
