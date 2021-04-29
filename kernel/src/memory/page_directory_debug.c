#include <mmu.h>
#include <stdint.h>
#include <stdio.h>

static void print_flags(uint32_t pte) {
  assert(pte & PTE_P);
  if (pte & PTE_W) {
    printf("W1");
  } else {
    printf("W0");
  }
  if (pte & PTE_U) {
    printf("U1");
  } else {
    printf("U0");
  }
  if (pte & PTE_PWT) {
    printf("PWT1");
  } else {
    printf("PWT0");
  }
  if (pte & PTE_PCD) {
    printf("PCD1");
  } else {
    printf("PCD0");
  }
}

void page_directory_debug(const uint32_t *pd) {
  assert(((uintptr_t)pd) % _4K == 0);
  printf("page_directory_debug\n******************************\n");
  for (uint32_t pd_idx = 0; pd_idx < 1024; pd_idx++) {
    if ((pd[pd_idx] & PTE_P)) {
      uintptr_t ps_page_frame = (uintptr_t)(pd[pd_idx] & 0xFFC00000);
      if (pd[pd_idx] & PTE_PS) {
        // 4M页
        printf("vaddr: 0x%09llx -> paddr: 0x%09llx | ", (int64_t)(pd_idx * _4M),
               (int64_t)(ps_page_frame));
        print_flags(pd[pd_idx]);
        printf("PS1\n");
      } else {
        // 4K页
        uint32_t *pt = (uint32_t *)(pd[pd_idx] & ~0xFFF);
        for (uint32_t pt_idx = 0; pt_idx < 1024; pt_idx++) {
          if ((pt[pt_idx] & PTE_P) == 0) {
            uintptr_t page_frame = (uintptr_t)(pt[pt_idx] & ~0xFFF);
            printf("vaddr: 0x%09llx -> paddr: 0x%09llx | ",
                   (int64_t)(pt_idx * _4K + ps_page_frame),
                   (int64_t)(page_frame));
            print_flags(pt[pt_idx]);
            printf("PS0\n");
          }
        }
      }
    }
  }
  printf("******************************\n");
}