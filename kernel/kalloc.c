// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define STEAL_PAGES 64 // most pages a cpu can steel from another

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
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
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

// void *
// kalloc(void)
// {
//   struct run *r;

//   acquire(&kmem.lock);
//   r = kmem.freelist;
//   if(r)
//     kmem.freelist = r->next;
//   release(&kmem.lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }

void *
kalloc(void)
{
  struct run *r = 0;
  push_off();
  int id = cpuid();

  acquire(&kmem[id].lock);
  if(kmem[id].freelist){
    r = kmem[id].freelist;
    if(r)
      kmem[id].freelist = r->next;
    release(&kmem[id].lock);
  }
  else{
    // Our own freelist is empty. Drop our lock before touching anyone
    // else's, so we never hold two kmem locks at once and cannot deadlock.
    release(&kmem[id].lock);

    for(int i = 0; i < NCPU; i++){
      if(i == id)
        continue;

      struct run *stolen = 0, *tail = 0;

      acquire(&kmem[i].lock);
      if(kmem[i].freelist){
        int counter = 1;
        stolen = tail = kmem[i].freelist;
        while(tail->next && counter++ < STEAL_PAGES){
          tail = tail->next;
        }
        kmem[i].freelist = tail->next;
        tail->next = 0;
      }
      release(&kmem[i].lock);

      if(stolen){
        acquire(&kmem[id].lock);
        tail->next = kmem[id].freelist; // splice, don't overwrite
        kmem[id].freelist = stolen;
        r = kmem[id].freelist;
        kmem[id].freelist = r->next;
        release(&kmem[id].lock);
        break;
      }
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  return (void*)r;
}