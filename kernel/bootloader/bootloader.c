#include "defs.h"
#include "elf.h"
#include "x86.h"

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE 512
#define ELFHDR ((struct elfhdr *)0x10000) // scratch space

/* waitdisk - wait for disk ready */
static void waitdisk(void) {
  while ((inb(0x1F7) & 0xC0) != 0x40)
    /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
static void readsect(void *dst, uint32_t secno) {
  // wait for disk to be ready
  waitdisk();

  outb(0x1F2, 1); // count = 1
  outb(0x1F3, secno & 0xFF);
  outb(0x1F4, (secno >> 8) & 0xFF);
  outb(0x1F5, (secno >> 16) & 0xFF);
  outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
  outb(0x1F7, 0x20); // cmd 0x20 - read sectors

  // wait for disk to be ready
  waitdisk();

  // read a sector
  insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void readseg(uintptr_t va, uint32_t count, uint32_t offset) {
  uintptr_t end_va = va + count;

  // round down to sector boundary
  va -= offset % SECTSIZE;

  // translate from bytes to sectors; kernel starts at sector 1
  uint32_t secno = (offset / SECTSIZE) + 1;

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for (; va < end_va; va += SECTSIZE, secno++) {
    readsect((void *)va, secno);
  }
}

/* bootmain - the entry of bootloader */
void bootmain(void) {
  // read the 1st page off disk
  readseg((uintptr_t)ELFHDR, 512 * 8, 0);

  // is this a valid ELF?
  if (ELFHDR->e_magic != ELF_MAGIC) {
    goto bad;
  }

  struct proghdr *ph, *ph_end;

  // load each program segment (ignores ph flags)
  ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
  ph_end = ph + ELFHDR->e_phnum;
  for (; ph < ph_end; ph++) {
    readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
  }

  // search entry_flag
  uintptr_t entry = 0xffffffff;
  for (uint32_t *p = (uint32_t *)ELFHDR->e_entry;
       p < (uint32_t *)(ELFHDR->e_entry + 512); p++) {
    if (*(uint32_t *)((uintptr_t)p & 0xFFFFFF) == 8899174) {
      uintptr_t diff = *(uint16_t *)((uintptr_t)(p + 1) & 0xFFFFFF);
      entry = ((uintptr_t)p - diff) & 0xFFFFFF;
      break;
    }
  }

  if (entry != 0xffffffff) {
    ((void (*)(void))entry)();
  }

bad:
  outw(0x8A00, 0x8A00);
  outw(0x8A00, 0x8E00);

  /* do nothing */
  while (1)
    hlt();
}