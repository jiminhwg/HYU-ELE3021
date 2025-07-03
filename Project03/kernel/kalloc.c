// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define MAX_PAGES ((PHYSTOP - KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  uint page_refcount[MAX_PAGES];
} ref_lock;

void
kinit()
{
  initlock(&ref_lock.lock, "ref_lock");
  initlock(&kmem.lock, "kmem");
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
  int index = pa_index(pa);
  acquire(&ref_lock.lock);
  if(ref_lock.page_refcount[index] > 1){
    ref_lock.page_refcount[index] -= 1;
    release(&ref_lock.lock);
    return;
  }
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  ref_lock.page_refcount[index] = 0;
  release(&ref_lock.lock);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    //memset((char*)r, 5, PGSIZE); // fill with junk
    int index = ((uint64)r - KERNBASE) / PGSIZE;
    acquire(&ref_lock.lock);
    ref_lock.page_refcount[index] = 1;
    release(&ref_lock.lock);
  }
  return (void*)r;
}

int pa_index(void* pa) {
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP) {
    return -1;
  }
  return ((uint64)pa - KERNBASE) / PGSIZE;
}

void add_ref(void *pa) {
  int index = pa_index(pa);
  if (index == -1) {
    return;
  }
  acquire(&ref_lock.lock);
  ref_lock.page_refcount[index]++;
  release(&ref_lock.lock);
}

void dec_ref(void *pa) {
  int index = pa_index(pa);
  if (index == -1) {
    return;
  }
  acquire(&ref_lock.lock);
  ref_lock.page_refcount[index]--;
  int current_count = ref_lock.page_refcount[index];
  release(&ref_lock.lock);
  if (current_count== 0) {
    kfree(pa);
  }
}
