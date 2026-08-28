// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  struct run *tail;   // last page on the freelist; lets us steal a whole
                      // list in O(1) without walking it while holding the lock
} kmem[NCPU];

char* kmem_names[NCPU] = {
  "kmem_0",
  "kmem_1",
  "kmem_2",
  "kmem_3",
  "kmem_4",
  "kmem_5",
  "kmem_6",
  "kmem_7",
};

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    initlock(&kmem[i].lock, kmem_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  // Distribute free pages round-robin across the per-CPU freelists so that
  // no single CPU owns all of memory at boot.  If every page starts on one
  // freelist (kfree() always frees to the boot CPU), the first allocations in
  // test1 all hit that one freelist and contend on its lock.
  int id = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    memset(p, 1, PGSIZE);
    struct run *r = (struct run*)p;
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    if(r->next == 0)
      kmem[id].tail = r;
    id = (id + 1) % NCPU;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int id = cpuid();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  if(r->next == 0)
    kmem[id].tail = r;
  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r){
    kmem[id].freelist = r->next;
    if(kmem[id].freelist == 0)
      kmem[id].tail = 0;
  }
  release(&kmem[id].lock);

  if(r == 0){
    // Our own freelist is empty: steal another CPU's entire list in one
    // O(1) step (no walking the list while holding its lock), keep one page,
    // and splice the rest onto our own list.  We hold both locks at once, in
    // index order to avoid deadlock, so the stolen pages are never briefly
    // invisible; otherwise a concurrent kalloc could see every list empty and
    // report a spurious out-of-memory failure.
    for(int i = 1; i < NCPU; i++){
      int other = (id + i) % NCPU;
      int lo = id < other ? id : other;
      int hi = id < other ? other : id;
      acquire(&kmem[lo].lock);
      acquire(&kmem[hi].lock);

      r = kmem[other].freelist;
      if(r){
        struct run *tail = kmem[other].tail;
        struct run *rest = r->next;
        kmem[other].freelist = 0;
        kmem[other].tail = 0;
        r->next = 0;
        // Our own list is empty here (only this CPU ever pushes to it, and
        // interrupts are off), so the stolen remainder becomes our whole list.
        kmem[id].freelist = rest;
        kmem[id].tail = rest ? tail : 0;
      }

      release(&kmem[hi].lock);
      release(&kmem[lo].lock);

      if(r)
        break;
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}
