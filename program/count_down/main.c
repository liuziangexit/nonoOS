#include <stdio.h>
#include <unistd.h>

#define SECONDS 5

int main(int argc, char **argv) {
  if (argc != 1) {
    return -1;
  }
  printf("parameter from kernel: %s", argv[0]);
  for (int i = 0; i < SECONDS; i++) {
    printf("count down %d\n", SECONDS - i);
    sleep(1000);
  }
  printf("count down 0\nbye bye 6!\n");
  return SECONDS;
}