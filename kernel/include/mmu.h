// This file contains definitions for the
// x86 memory management unit (MMU).
#ifndef __KERNEL_MMU_H__
#define __KERNEL_MMU_H__

// Eflags register
#define FL_CF 0x00000001        // Carry Flag
#define FL_PF 0x00000004        // Parity Flag
#define FL_AF 0x00000010        // Auxiliary carry Flag
#define FL_ZF 0x00000040        // Zero Flag
#define FL_SF 0x00000080        // Sign Flag
#define FL_TF 0x00000100        // Trap Flag
#define FL_IF 0x00000200        // Interrupt Flag
#define FL_DF 0x00000400        // Direction Flag
#define FL_OF 0x00000800        // Overflow Flag
#define FL_IOPL_MASK 0x00003000 // I/O Privilege Level bitmask
#define FL_IOPL_0 0x00000000    //   IOPL == 0
#define FL_IOPL_1 0x00001000    //   IOPL == 1
#define FL_IOPL_2 0x00002000    //   IOPL == 2
#define FL_IOPL_3 0x00003000    //   IOPL == 3
#define FL_NT 0x00004000        // Nested Task
#define FL_RF 0x00010000        // Resume Flag
#define FL_VM 0x00020000        // Virtual 8086 mode
#define FL_AC 0x00040000        // Alignment Check
#define FL_VIF 0x00080000       // Virtual Interrupt Flag
#define FL_VIP 0x00100000       // Virtual Interrupt Pending
#define FL_ID 0x00200000        // ID flag

// Application segment type bits
#define STA_X 0x8 // Executable segment
#define STA_E 0x4 // Expand down (non-executable segments)
#define STA_C 0x4 // Conforming code segment (executable only)
#define STA_W 0x2 // Writeable (non-executable segments)
#define STA_R 0x2 // Readable (executable segments)
#define STA_A 0x1 // AccessedF

// System segment type bits
#define STS_T32A 0x9 // Available 32-bit TSS
#define STS_IG32 0xE // 32-bit Interrupt Gate
#define STS_TG32 0xF // 32-bit Trap Gate

// Control Register flags
#define CR0_PE 0x00000001  // Protection Enable
#define CR0_WP 0x00010000  // Write Protect
#define CR0_PG 0x80000000  // Paging
#define CR4_PSE 0x00000010 // Page size extension

// various segment selectors.
#define SEG_IDX_KCODE 1
#define SEG_IDX_KDATA 2
#define SEG_IDX_UCODE 3
#define SEG_IDX_UDATA 4
#define SEG_IDX_TSS 5

#define SEG_KCODE (SEG_IDX_KCODE * 8) // kernel code
#define SEG_KDATA (SEG_IDX_KDATA * 8) // kernel data + stack
#define SEG_UCODE (SEG_IDX_UCODE * 8) // user code
#define SEG_UDATA (SEG_IDX_UDATA * 8) // user data + stack
#define SEG_TSS (SEG_IDX_TSS * 8)     // user data + stack

#define DPL_KERNEL (0)
#define DPL_USER (3)

#define KERNEL_CS ((SEG_KCODE) | DPL_KERNEL)
#define KERNEL_DS ((SEG_KDATA) | DPL_KERNEL)
#define USER_CS ((SEG_UCODE) | DPL_USER)
#define USER_DS ((SEG_UDATA) | DPL_USER)

// cpu->gdt[NSEGS] holds the above segments.
#define NSEGS 6

#ifndef __ASSEMBLER__
#include <defs.h>
// Segment Descriptor
struct segdesc {
  uint32_t lim_15_0 : 16;  // Low bits of segment limit
  uint32_t base_15_0 : 16; // Low bits of segment base address
  uint32_t base_23_16 : 8; // Middle bits of segment base address
  uint32_t type : 4;       // Segment type (see STS_ constants)
  uint32_t s : 1;          // 0 = system, 1 = application
  uint32_t dpl : 2;        // Descriptor Privilege Level
  uint32_t p : 1;          // Present
  uint32_t lim_19_16 : 4;  // High bits of segment limit
  uint32_t avl : 1;        // Unused (available for software use)
  uint32_t rsv1 : 1;       // Reserved
  uint32_t db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
  uint32_t g : 1;          // Granularity: limit scaled by 4K when set
  uint32_t base_31_24 : 8; // High bits of segment base address
};

#define SEG_NULL                                                               \
  (struct segdesc) { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

#define SEG(type, base, lim, dpl)                                              \
  (struct segdesc) {                                                           \
    ((lim) >> 12) & 0xffff, (base)&0xffff, ((base) >> 16) & 0xff, type, 1,     \
        dpl, 1, (unsigned)(lim) >> 28, 0, 0, 1, 1, (unsigned)(base) >> 24      \
  }

#define SEGTSS(type, base, lim, dpl)                                           \
  (struct segdesc) {                                                           \
    (lim) & 0xffff, (base)&0xffff, ((base) >> 16) & 0xff, type, 0, dpl, 1,     \
        (unsigned)(lim) >> 16, 0, 0, 1, 0, (unsigned)(base) >> 24              \
  }
#else

#define SEG_NULLASM                                                            \
  .word 0, 0;                                                                  \
  .byte 0, 0, 0, 0

#define SEG_ASM(type, base, lim)                                               \
  .word(((lim) >> 12) & 0xffff), ((base)&0xffff);                              \
  .byte(((base) >> 16) & 0xff), (0x90 | (type)),                               \
      (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#endif

// A virtual address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(va) --/ \--- PTX(va) --/

// page directory index
#define PDX(va) (((uint32_t)(va) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(va) (((uint32_t)(va) >> PTXSHIFT) & 0x3FF)

// construct virtual address from indexes and offset
#define PGADDR(d, t, o) ((uint32_t)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
// https://www.youtube.com/watch?v=jkGZDb3100Q&t=867s
#define NPDENTRIES 1024 // # directory entries per page directory
#define NPTENTRIES 1024 // # PTEs per page table
#define PGSIZE 4096     // bytes mapped by a page

#define PTXSHIFT 12 // offset of PTX in a linear address
#define PDXSHIFT 22 // offset of PDX in a linear address

#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

// Page table/directory entry flags.
#define PTE_P 0x001   // Present
#define PTE_W 0x002   // Writeable
#define PTE_U 0x004   // User
#define PTE_PWT 0x008 // Write-Through
#define PTE_PCD 0x010 // Cache-Disable
#define PTE_A 0x020   // Accessed
#define PTE_D 0x040   // Dirty
#define PTE_PS 0x080  // Page Size

// Address in page table or page directory entry
#define PTE_ADDR(pte) ((uint32_t)(pte) & ~0xFFF)
#define PTE_FLAGS(pte) ((uint32_t)(pte)&0xFFF)

#ifndef __ASSEMBLER__
typedef uint32_t pte_t;

// Task state segment format
struct taskstate {
  uint32_t link; // Old ts selector
  uint32_t esp0; // Stack pointers and segment selectors
  uint16_t ss0;  //   after an increase in privilege level
  uint16_t padding1;
  uint32_t *esp1;
  uint16_t ss1;
  uint16_t padding2;
  uint32_t *esp2;
  uint16_t ss2;
  uint16_t padding3;
  void *cr3;     // Page directory base
  uint32_t *eip; // Saved state from last task switch
  uint32_t eflags;
  uint32_t eax; // More saved state (registers)
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t *esp;
  uint32_t *ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es; // Even more saved state (segment selectors)
  uint16_t padding4;
  uint16_t cs;
  uint16_t padding5;
  uint16_t ss;
  uint16_t padding6;
  uint16_t ds;
  uint16_t padding7;
  uint16_t fs;
  uint16_t padding8;
  uint16_t gs;
  uint16_t padding9;
  uint16_t ldt;
  uint16_t padding10;
  uint16_t t;    // Trap on task switch
  uint16_t iomb; // I/O map base address
};

// Gate descriptors for interrupts and traps
struct gatedesc {
  uint32_t off_15_0 : 16;  // low 16 bits of offset in segment
  uint32_t cs : 16;        // code segment selector
  uint32_t args : 5;       // # args, 0 for interrupt/trap gates
  uint32_t rsv1 : 3;       // reserved(should be zero I guess)
  uint32_t type : 4;       // type(STS_{IG32,TG32})
  uint32_t s : 1;          // must be 0 (system)
  uint32_t dpl : 2;        // descriptor(meaning new) privilege level
  uint32_t p : 1;          // Present
  uint32_t off_31_16 : 16; // high bits of offset in segment
};

// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
//   interrupt gate clears FL_IF, trap gate leaves FL_IF alone
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//        the privilege level required for software to invoke
//        this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, d)                                     \
  {                                                                            \
    (gate).off_15_0 = (uint32_t)(off)&0xffff;                                  \
    (gate).cs = (sel);                                                         \
    (gate).args = 0;                                                           \
    (gate).rsv1 = 0;                                                           \
    (gate).type = (istrap) ? STS_TG32 : STS_IG32;                              \
    (gate).s = 0;                                                              \
    (gate).dpl = (d);                                                          \
    (gate).p = 1;                                                              \
    (gate).off_31_16 = (uint32_t)(off) >> 16;                                  \
  }

#endif

#endif
