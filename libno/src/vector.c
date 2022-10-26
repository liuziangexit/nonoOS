#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector.h>

static void check_mem(void *mem) {
  if (!mem) {
    printf("vector !vec->mem\n");
    abort();
  }
}

static void check_oor(vector_t *vec, uint32_t idx) {
  if (idx >= vec->count) {
    printf("vector out of range\n");
    abort();
  }
}

void vector_init(vector_t *vec, uint32_t obj_size, void *(*allocator)(size_t)) {
  assert(vec);
  assert(obj_size != 0);
  vec->obj_size = obj_size;
  vec->capacity = obj_size * 15;
  vec->count = 0;
  if (vec->allocator) {
    vec->allocator = allocator;
  } else {
    vec->allocator = malloc;
  }
  vec->mem = vec->allocator(vec->capacity);
  check_mem(vec->mem);
}

void vector_destroy(vector_t *vec) {
  if (vec->mem)
    free(vec->mem);
}

uint32_t vector_count(vector_t *vec) { return vec->count; }

uint32_t vector_add(vector_t *vec, void *obj) {
  if (vec->count == vec->capacity) {
    vector_reserve(vec, vec->capacity * 2);
  }
  memcpy(vec->mem + vec->obj_size * vec->count, obj, vec->obj_size);
  vec->count++;
  return vec->count - 1;
}

void vector_remove(vector_t *vec, uint32_t index) {
  check_oor(vec, index);
  memcpy(vec->mem + vec->obj_size * index,
         vec->mem + vec->obj_size * (index + 1), vec->obj_size - index + 1);
  vec->count--;
}

void *vector_get(vector_t *vec, uint32_t index) {
  check_oor(vec, index);
  return vec->mem + vec->obj_size * index;
}

void vector_reserve(vector_t *vec, uint32_t new_capacity) {
  if (vec->capacity < new_capacity) {
    void *new_mem = vec->allocator(new_capacity * vec->obj_size);
    check_mem(new_mem);
    memcpy(new_mem, vec->mem, vec->obj_size * vec->count);
    free(vec->mem);
    vec->mem = new_mem;
    vec->capacity = new_capacity;
  }
}

void vector_shrink(vector_t *vec, uint32_t new_capacity) {
  if (new_capacity < vec->capacity) {
    void *new_mem = vec->allocator(new_capacity * vec->obj_size);
    check_mem(new_mem);
    memcpy(new_mem, vec->mem, vec->obj_size * new_capacity);
    free(vec->mem);
    vec->mem = new_mem;
    vec->capacity = new_capacity;
    vec->count = new_capacity;
  }
}

void vector_clear(vector_t *vec) { vec->count = 0; }
