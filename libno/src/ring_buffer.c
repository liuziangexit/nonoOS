#include <assert.h>
#include <ring_buffer.h>

/*
struct ring_buffer {
  void *buf;
  uint32_t cap;
  uint32_t wpos;
  uint32_t rpos;
};*/

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
  __builtin_unreachable();
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

bool ring_buffer_write(struct ring_buffer *buf, const void *src, uint32_t len) {
  if (len + ring_buffer_readable(buf) >= buf->cap) {
    return false;
  }
  unsigned char *bsrc = (unsigned char *)src;
  unsigned char *bbuf = (unsigned char *)buf->buf;
  for (uint32_t i = 0; i < len; i++) {
    bbuf[(buf->wpos + i) % buf->cap] = bsrc[i];
  }
  buf->wpos = (buf->wpos + len) % buf->cap;
  return true;
}
