#ifndef __LIBNO_VECTOR_H__
#define __LIBNO_VECTOR_H__
#include <stdint.h>

struct vector {
  uint32_t obj_size;
  void *mem;
  uint32_t count;
  uint32_t capacity;
};
typedef struct vector vector_t;

void vector_init(vector_t *vec, uint32_t obj_size);
void vector_destroy(vector_t *vec);
uint32_t vector_count(vector_t *vec);

uint32_t vector_add(vector_t *vec, void *obj);
void vector_remove(vector_t *vec, uint32_t index);
void *vector_get(vector_t *vec, uint32_t index);

void vector_reserve(vector_t *vec, uint32_t new_capacity);
void vector_shrink(vector_t *vec, uint32_t new_capacity);
// void vector_resize(vector_t *vec, uint32_t new_size);
// void vector_begin(vector_t *vec);
// void vector_end(vector_t *vec);

#endif
