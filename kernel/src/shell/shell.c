#include "shell.h"
#include "task.h"
#include <compiler_helper.h>
#include <stdio.h>

int shell_main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);

  task_display();
  printf("nonoOS:$ ");

  return 0;
  //__unreachable
}