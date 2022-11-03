#include <assert.h>
#include <compiler_helper.h>
#include <ring_buffer.h>

void ring_buffer_init(struct ring_buffer *rbuf, void *buf, uint32_t cap) {
  rbuf->buf = buf;
  rbuf->cap = cap;
  rbuf->wpos = 0;
  rbuf->rpos = 0;
}

uint32_t ring_buffer_readable(const struct ring_buffer *buf) {
  if (buf->wpos == buf->rpos) {
    return 0;
  } else if (buf->wpos > buf->rpos) {
    return buf->wpos - buf->rpos;
  } else if (buf->wpos < buf->rpos) {
    return buf->wpos + (buf->cap - buf->rpos);
  }
  __unreachable;
}

uint32_t ring_buffer_read(struct ring_buffer *buf, void *dst, uint32_t len) {
  uint32_t readable = ring_buffer_readable(buf);
  if (!readable) {
    return 0;
  }
  uint32_t toread = readable < len ? readable : len;
  unsigned char *bdst = (unsigned char *)dst;
  unsigned char *bbuf = (unsigned char *)buf->buf;
  for (uint32_t i = 0; i < toread; i++) {
    bdst[i] = bbuf[(buf->rpos + i) % buf->cap];
  }
  buf->rpos = (buf->rpos + toread) % buf->cap;
  return toread;
}

bool ring_buffer_write(struct ring_buffer *buf, bool force, const void *src,
                       uint32_t len) {
  if (force) {
    if (len > buf->cap - 1) {
      return false;
    }
  } else {
    if (len + ring_buffer_readable(buf) >= buf->cap) {
      return false;
    }
  }

  //写进buffer
  unsigned char *bsrc = (unsigned char *)src;
  unsigned char *bbuf = (unsigned char *)buf->buf;
  for (uint32_t i = 0; i < len; i++) {
    bbuf[(buf->wpos + i) % buf->cap] = bsrc[i];
  }

  //修改读写位置
  if (force && (len + ring_buffer_readable(buf)) >= buf->cap) {
    //如果写的时候覆盖了未读内容
    buf->wpos = (buf->wpos + len) % buf->cap;
    buf->rpos = (buf->wpos + 1) % buf->cap;
  } else {
    buf->wpos = (buf->wpos + len) % buf->cap;
  }
  return true;
}

//把缓冲区里readable的数据拷出来，就像缓冲区是线性的一样
//这不会影响内部的读写位置
void ring_buffer_copyout(struct ring_buffer *buf, uint32_t begin, uint32_t end,
                         void *dst) {
  unsigned char *bdst = (unsigned char *)dst;
  unsigned char *bsrc = (unsigned char *)buf->buf;
  for (uint32_t i = 0; i <= end - begin; i++) {
    bdst[i] = bsrc[(buf->rpos + (begin + i)) % buf->cap];
  }
}

//遍历readable数据，就像缓冲区是线性的一样
//如果遍历完了，返回null
void *ring_buffer_foreach(struct ring_buffer *buf, uint32_t *iterator,
                          uint32_t end) {
  if (*iterator != end) {
    void *ret = buf->buf + ((buf->rpos + *iterator) % buf->cap);
    (*iterator)++;
    return ret;
  }
  return 0;
}
