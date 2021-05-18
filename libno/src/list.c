#include <assert.h>
#include <compiler_helper.h>
#include <list.h>

void list_sort_add(list_entry_t *listelm, list_entry_t *elm,
                   int (*compare)(const void *a, const void *b),
                   uint32_t offset) {
  assert(listelm && elm && compare);
  list_entry_t *p = list_next(listelm);
  while (p != listelm) {
    int cmp = compare((void *)elm - offset, (void *)p - offset);
    if (cmp > 0) {
      // 如果elm大于当前遍历的元素
      list_entry_t *next = list_next(p);
      if (next == listelm) {
        // 如果当前遍历的元素是链表里最后一个元素了
        list_add_after(p, elm);
        return;
      }
      p = next;
      continue;
    } else if (cmp < 0) {
      // 当前遍历的元素比elm小
      list_add_before(p, elm);
      return;
    } else {
      // 当前遍历的元素和elm一样
      list_add_after(p, elm);
      return;
    }
  }
  // 链表是空的
  list_add_after(p, elm);
}
