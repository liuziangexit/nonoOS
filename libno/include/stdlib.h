#ifndef __LIBNO_STDLIB_H__
#define __LIBNO_STDLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

void abort(void);
void exit(int);

//将数字转为其字符串表示
int itoa(int num, unsigned char *str, int len, int base);

#ifdef __cplusplus
}
#endif

#endif
