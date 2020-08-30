#ifndef __LIBNO_RING_BUFFER_H__
#define __LIBNO_RING_BUFFER_H__
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
// force参数指示当空闲空间已被耗尽时，是否继续写入，这将导致一些未读的数据被抹掉
bool ring_buffer_write(struct ring_buffer *buf, bool force, const void *src,
                       uint32_t len);
//把缓冲区里的数据拷出来，就像缓冲区是线性的一样
//这不会影响内部的读写位置
void *ring_buffer_copyout(struct ring_buffer *buf, uint32_t begin, uint32_t end,
                          void *dst);

#endif
