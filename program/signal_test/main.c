#include <compiler_helper.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <unistd.h>

void subsignal(int sig) {
  printf("signal_test subrountine: received signal %d\n", sig);
}

void subrountine() {
  printf("signal_test subrountine: alive!\n");
  signal(SIGTERM, subsignal);
  printf("signal_test subrountine: handler set!\n");
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGTERM);
  int sighappen = 0;
  int ok = sigwait(&sigset, &sighappen);
  if (ok != 0) {
    printf("signal_test subrountine: sigwait call failed\n");
    abort();
  }
  if (sighappen != SIGTERM) {
    printf("signal_test subrountine: sig is not SIGTERM\n");
    abort();
  }
  printf("signal_test subrountine: very good, quitting...\n");
  exit(0);
}

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  pid_t sub =
      create_task(0, 0, "new_thread", false, (uintptr_t)subrountine, 0, 0, 0);

  printf("signal_test main: sending SIGTERM...\n");
  kill(sub, SIGTERM);
  printf("signal_test main: waiting for subrountine to die...\n");
  join(sub);
  printf("signal_test main: finish\n");
  return 0;
}