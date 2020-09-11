#include "bare_hashmap.h"
#include <assert.h>
#include <defs.h>
#include <memory_manager.h>
#include <stdbool.h>
#include <string.h>

void bare_init(void *page, uint32_t pgcnt) {
  assert((uintptr_t)page % 4096 == 0);
  memset(page, 0, pgcnt * 4096);
}

static uint32_t __bare_hash(uint32_t pgcnt, uint32_t key) {
  return key % (pgcnt * 512);
}

static void __bare_grow(void *opage, uint32_t opgcnt, void *npage,
                        uint32_t npgcnt);

//寻找一个key的位置
static bool __bare_find(void *page, uint32_t pgcnt, uint32_t key,
                        uint32_t *result) {
  assert((uintptr_t)page % 4096 == 0);
  const uint32_t h = __bare_hash(pgcnt, key);
  uint32_t idx = h;
  while (*(uint32_t *)((page + idx * 8)) != key) {
    idx = __bare_hash(pgcnt, (idx + 1));
    //绕了一圈了都没找到空位，说明确实没有
    if (idx == h) {
      return false;
    }
  }
  //找到了
  *result = idx;
  return true;
}

//可能引起grow，如果发生了，旧的page会被free，新的page位置和大小会通过npage和npgcnt返回
//如果没有发生grow，则npage和npgcnt会等于原来的值
//如果grow失败而因此put也失败了，npage会是0，此时npgcnt的值未定义
uint32_t bare_put(void *page, uint32_t pgcnt, uint32_t key, uint32_t value,
                  void **npage, uint32_t *npgcnt) {
  assert((uintptr_t)page % 4096 == 0);
  assert(value != 0);
  const uint32_t h = __bare_hash(pgcnt, key);
  uint32_t idx = h;
  while (*(uint32_t *)((page + idx * 8)) != 0 &&
         *(uint32_t *)((page + idx * 8)) != key) {
    idx = __bare_hash(pgcnt, (idx + 1));
    //绕了一圈了都没找到空位，说明要grow
    if (idx == h) {
      //分配原来两倍的内存
      *npage = kmem_page_alloc(pgcnt * 2);
      if (!*npage)
        return 0; //本次put需要grow，但没有足够内存了
      *npgcnt = pgcnt * 2;
      __bare_grow(page, pgcnt, *npage, *npgcnt);
      //回收老的内存
      kmem_page_free(page, pgcnt);
      //到此grow就做完了，现在再put
      void *check_pg;
      uint32_t check_pgcnt;
      uint32_t prev =
          bare_put(*npage, *npgcnt, key, value, &check_pg, &check_pgcnt);
      //这个递归调用肯定是成功并且不需要grow的，证实这一点
      assert(check_pg == *npage && check_pgcnt == *npgcnt);
      return prev;
    }
  }
  //找到了位置，开始put啰
  uint32_t prev = 0;
  if (*(uint32_t *)((page + idx * 8)) == key)
    prev = *((uint32_t *)(page + idx * 8) + 1);
  *(uint32_t *)(page + idx * 8) = key;
  *((uint32_t *)(page + idx * 8) + 1) = value;

  *npage = page;
  *npgcnt = pgcnt;

  return prev;
}

uint32_t bare_get(void *page, uint32_t pgcnt, uint32_t key) {
  assert((uintptr_t)page % 4096 == 0);
  uint32_t idx;
  if (!__bare_find(page, pgcnt, key, &idx))
    return 0;
  return *(uint32_t *)((page + idx * 8) + 1);
}

uint32_t bare_del(void *page, uint32_t pgcnt, uint32_t key) {
  assert((uintptr_t)page % 4096 == 0);
  uint32_t idx;
  if (!__bare_find(page, pgcnt, key, &idx))
    return 0;
  uint32_t prev = *(uint32_t *)((page + idx * 8) + 1);
  *(uint32_t *)((page + idx * 8)) = 0;
  *(uint32_t *)((page + idx * 8) + 1) = 0;
  return prev;
}

static void __bare_grow(void *opage, uint32_t opgcnt, void *npage,
                        uint32_t npgcnt) {
  assert((uintptr_t)opage % 4096 == 0);
  assert((uintptr_t)npage % 4096 == 0);
  assert(npgcnt > opgcnt);

  // remapping
  for (uint32_t i = 0; i < opgcnt * 1024; i++) {
    uint32_t *key = (uint32_t *)(opage + i * 8);
    uint32_t *val = (uint32_t *)(opage + i * 8 + 4);
    if (val != 0) {
      void *check_pg;
      uint32_t check_pgcnt;
      bare_put(npage, npgcnt, *key, *val, &check_pg, &check_pgcnt);
      assert(check_pg == npage && check_pgcnt == npgcnt);
    }
  }
}
