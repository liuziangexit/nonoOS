#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
  struct stat st;
  if (argc != 3) {
    fprintf(stderr, "Usage: <input filename> <output filename>\n");
    return -1;
  }
  // check file size
  if (stat(argv[1], &st) != 0) {
    fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
    return -1;
  }
  printf("'%s' size: %lld bytes\n", argv[1], (long long)st.st_size);
  if (st.st_size > 510) {
    fprintf(stderr, "%lld >> 510!!\n", (long long)st.st_size);
    return -1;
  }

  // read the file
  unsigned char buf[512];
  memset(buf, 0, sizeof(buf));
  FILE *ifp = fopen(argv[1], "rb");
  if (!ifp) {
    fprintf(stderr, "something goes wrong\n");
    return -1;
  }
  int size = fread(buf, 1, st.st_size, ifp);
  if (size != st.st_size) {
    fprintf(stderr, "read '%s' error, size is %d.\n", argv[1], size);
    fclose(ifp);
    return -1;
  }
  fclose(ifp);

  // modify
  buf[510] = 0x55;
  buf[511] = 0xAA;

  // save file
  FILE *ofp = fopen(argv[2], "wb+");
  size = fwrite(buf, 1, 512, ofp);
  if (size != 512) {
    fprintf(stderr, "write '%s' error, size is %d.\n", argv[2], size);
    fclose(ofp);
    return -1;
  }
  fclose(ofp);
  printf("build 512 bytes boot sector: '%s' success!\n", argv[2]);
  return 0;
}
