#include <stdio.h>

int main(int argc, char **args) {
  if (argc != 2) {
    printf("Usage: idtgen filename\n");
    return 0;
  }
  FILE *fp = fopen(args[1], "w+");
  if (fp == NULL) {
    printf("could not write to %s\n", args[1]);
    return 0;
  }

  fprintf(fp, "# handler\n");
  fprintf(fp, ".text\n");
  fprintf(fp, ".globl __interrupt_entry\n");

  int i;
  for (i = 0; i < 256; i++) {
    fprintf(fp, ".globl vector%d\n", i);
    fprintf(fp, "vector%d:\n", i);
    if ((i < 8 || i > 14) && i != 17) {
      //对于没有errcode的，手动push一个0
      fprintf(fp, "  pushl $0\n");
    }
    //push异常编号
    fprintf(fp, "  pushl $%d\n", i);
    fprintf(fp, "  jmp __interrupt_entry\n");
  }
  fprintf(fp, "\n");
  fprintf(fp, "# vector table\n");
  fprintf(fp, ".data\n");
  fprintf(fp, ".globl __idt_vectors\n");
  fprintf(fp, "__idt_vectors:\n");
  for (i = 0; i < 256; i++) {
    fprintf(fp, "  .long vector%d\n", i);
  }

  fclose(fp);
  return 0;
}
