#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H 1
#include <stdbool.h>
#include <stdint.h>

struct ring_buffer {
  void *buf;
  uint32_t cap;
  uint32_t wpos;
  uint32_t rpos;
};

//初始化
void ring_buffer_init(struct ring_buffer *rbuf, void *buf, uint32_t cap);
//还能读多少字节
uint32_t ring_buffer_readable(const struct ring_buffer *buf);
//读
//返回实际读取的字节数
uint32_t ring_buffer_read(struct ring_buffer *buf, void *dst, uint32_t len);
//写
//返回写入是否成功
bool ring_buffer_write(struct ring_buffer *buf, const void *src, uint32_t len);

#endif
