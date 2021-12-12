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

struct page_refer_node{
  struct spinlock lock;
  int refer[(PHYSTOP-KERNBASE)/PGSIZE];
};
struct page_refer_node page_refer;


void page_refer_add(uint64 pa){
  acquire(&page_refer.lock);
  pa = (pa-KERNBASE)/PGSIZE;
  page_refer.refer[pa]+=1;
  printf("add:%d\n",page_refer.refer[pa]);
  release(&page_refer.lock);
}

void page_refer_minus(uint64 pa){
  acquire(&page_refer.lock);
  pa = (pa-KERNBASE)/PGSIZE;
  page_refer.refer[pa]-=1;
  printf("minus:%d\n",page_refer.refer[pa]);
  release(&page_refer.lock);
}

int get_page_refer(uint64 pa){
  int a;
  acquire(&page_refer.lock);
  pa = (pa-KERNBASE)/PGSIZE;
  a = page_refer.refer[pa];
  printf("get:%d\n",page_refer.refer[pa]);
  release(&page_refer.lock);
  return a;
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
  for(int i=0;i<(PHYSTOP-KERNBASE)/PGSIZE;i++){
    page_refer.refer[i] = 1;
  }
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

// Free the page of physical memory pointed at by v,
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

  acquire(&page_refer.lock);
    page_refer.refer[((uint64)r-KERNBASE)/PGSIZE] -= 1;
    if(page_refer.refer[((uint64)r-KERNBASE)/PGSIZE] > 0){
      release(&page_refer.lock);
      return;
    }
  release(&page_refer.lock);

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


  acquire(&page_refer.lock);
  if(r)
    page_refer.refer[((uint64)r-KERNBASE)/PGSIZE] = 1;
  release(&page_refer.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

