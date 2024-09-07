#include "shell.h"
#include "sync.h"
#include "task.h"
#include <compiler_helper.h>
#include <kernel_object.h>
#include <memory_manager.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector.h>
#include <virtual_memory.h>

#define INITIAL_GETS_BUFFER_LEN 256

uint32_t task_count();

static bool __sr = false;
static pid_t fg, pid;

struct program_info {
  char *name;
  char *program;
  uint32_t size;
};
typedef struct program_info program_info_t;

// 这只是我们在没有实现文件系统之前的一个权宜之计
// 因此，我们用简单的线性表去实现
vector_t program_list;

static void new_program(char *name, char *program, uint32_t size) {
  program_info_t p;
  p.name = name;
  p.program = program;
  p.size = size;
  vector_add(&program_list, &p);
}

static program_info_t *find_program(const char *name) {
  for (uint32_t i = 0; i < vector_count(&program_list); i++) {
    program_info_t *p = (program_info_t *)vector_get(&program_list, i);
    if (strcmp(p->name, name) == 0) {
      return p;
    }
  }
  return NULL;
}

static pid_t run_user_program(const char *name, int argc, char **argv) {
  program_info_t *prog = find_program(name);
  if (!prog) {
    return 0;
  }

  struct task_args args;
  task_args_init(&args);
  for (int i = 0; i < argc; i++) {
    task_args_add(&args, argv[i], 0, false, 0);
  }

  uint32_t save;
  enter_critical_region(&save);

  pid_t pid = task_create_user(prog->program, (uintptr_t)prog->size, name, 0,
                               DEFAULT_ENTRY, false, &args);
  task_args_destroy(&args, true);
  shell_set_fg(pid);

  leave_critical_region(&save);

  task_join(pid, NULL);
  shell_set_fg(task_current()->id);

  return pid;
}

static void shell_init() {
  vector_init(&program_list, sizeof(program_info_t), NULL);

  extern char _binary____program_count_down_main_exe_start[],
      _binary____program_count_down_main_exe_size[];
  new_program("count_down", _binary____program_count_down_main_exe_start,
              (uint32_t)_binary____program_count_down_main_exe_size);

  extern char _binary____program_user_read_input_main_exe_start[],
      _binary____program_user_read_input_main_exe_size[];
  new_program("user_read_input",
              _binary____program_user_read_input_main_exe_start,
              (uint32_t)_binary____program_user_read_input_main_exe_size);

  extern char _binary____program_signal_test_main_exe_start[],
      _binary____program_signal_test_main_exe_size[];
  new_program("signal_test", _binary____program_signal_test_main_exe_start,
              (uint32_t)_binary____program_signal_test_main_exe_size);

  extern char _binary____program_abort_main_exe_start[],
      _binary____program_abort_main_exe_size[];
  new_program("abort", _binary____program_abort_main_exe_start,
              (uint32_t)_binary____program_abort_main_exe_size);

  extern char _binary____program_bad_access_main_exe_start[],
      _binary____program_bad_access_main_exe_size[];
  new_program("bad_access", _binary____program_bad_access_main_exe_start,
              (uint32_t)_binary____program_bad_access_main_exe_size);

  extern char _binary____program_hello_world_hello_exe_start[],
      _binary____program_hello_world_hello_exe_size[];
  new_program("hello", _binary____program_hello_world_hello_exe_start,
              (uint32_t)_binary____program_hello_world_hello_exe_size);
}

int shell_main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);

  // waiting all other task to quit
  {
    disable_interrupt();
    while (task_count() != 2) {
      enable_interrupt();
      task_sleep(50);
      disable_interrupt();
    }
    enable_interrupt();
  }

  shell_set_fg(task_current()->id);
  pid = task_current()->id;
  __sr = true;

  shell_init();

  task_display();

  // https://en.wikipedia.org/wiki/Code_page_437
  printf("System was compiled at " __DATE__ ", " __TIME__ ".\n");
  printf("nonoOS has been loaded. ");
  putchar(1);
  putchar(1);
  putchar(1);
  printf("\n\n");

  char *str = malloc(INITIAL_GETS_BUFFER_LEN);
  assert(str);
  size_t str_len = INITIAL_GETS_BUFFER_LEN;

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

    // printf("you have entered: %s\n\n", str, (int)r);

    if (strlen(str) > 2 && str[0] == '.' && str[1] == '/') {
      const char *program_name_end = strstr(str + 2, " ");
      if (!program_name_end)
        program_name_end = str + strlen(str);
      char program_name[128] = {0};
      memcpy(program_name, str + 2, program_name_end - str - 2);

      // chatgpt good job
      uint32_t argc = 0;
      char *argv[128]; // Fixed-size array to hold arguments

      // Parse the arguments manually
      const char *args_start = program_name_end;
      while (args_start < str + str_len && *args_start == ' ') {
        args_start++; // Skip leading spaces
      }

      while (args_start < str + str_len && *args_start) {
        char temp_arg[128] = {0}; // Temporary buffer for the argument
        char *arg_start = temp_arg;

        // Read characters until a space or end of string is found
        while (args_start < str + str_len && *args_start &&
               *args_start != ' ') {
          *arg_start++ = *args_start++;
        }

        // Allocate memory for the argument and copy it to argv
        argv[argc] = (char *)malloc(strlen(temp_arg) + 1);
        if (argv[argc] == NULL) {
          // Handle memory allocation failure
          panic("Memory allocation failed.\n");
        }
        strcpy(argv[argc], temp_arg);
        argc++;

        // Skip any spaces between arguments
        while (args_start < str + str_len && *args_start == ' ') {
          args_start++;
        }
      }

      if (!run_user_program(program_name, argc, argv)) {
        if (strcmp(program_name, "virtual_memory_print") == 0) {
          virtual_memory_print(virtual_memory_current());
        } else {
          printf("program %s not found\n", str + 2);
        }
      }
    } else if (strcmp(str, "ps") == 0) {
      task_display();
    }
  }
  __unreachable
}

bool shell_ready() { return __sr; }

void shell_set_fg(pid_t pid) { atomic_store(&fg, pid); }

pid_t shell_fg() { return atomic_load(&fg); }

pid_t shell_pid() { return pid; }
