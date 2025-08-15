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

int refer[PHYSTOP/ PGSIZE];
struct spinlock rf_lock;

int addrefer(uint64 pa){
  int ans;
  acquire(&rf_lock);
  ans = refer[pa/PGSIZE];
  ans++;
  refer[pa/PGSIZE] = ans;
  release(&rf_lock);
  return ans;
}

int subrefer(uint64 pa){
  int ans;
  acquire(&rf_lock);
  ans = refer[pa/PGSIZE];
  ans--;
  refer[pa/PGSIZE] = ans;
  release(&rf_lock);
  return ans;
}

int getrefer(uint64 pa){
  return refer[pa/PGSIZE];
}

int cowpage(pagetable_t pagetable, uint64 va) {
  if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  if((*pte & PTE_U) == 0)
    return -1;
  if((*pte & PTE_COW) == 0)
    return -1;
  return 0;
}
int cowalloc(pagetable_t pagetable, uint64 va) {

  pte_t* pte = walk(pagetable, va, 0);
  uint64 pa = PTE2PA(*pte);

  char* mem = kalloc();
  if(mem == 0)
    return 0;

  memmove(mem, (void*)pa, PGSIZE);
  *pte &= ~PTE_V;

  kfree((void*)PGROUNDDOWN(pa));
  if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW) != 0) {
    kfree(mem);
    *pte |= PTE_V;
    return 0;
  }
  return 1;
}
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&rf_lock, "refer");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  memset(refer, 1, PHYSTOP / PGSIZE);
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

  if(subrefer((uint64)pa) > 0)
    return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    refer[(uint64)r / PGSIZE] = 1;
  }
  return (void*)r;
}
