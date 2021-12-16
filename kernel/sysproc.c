#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void* sys_mmap(){
  uint64 addr = 0;
  int length = 0;
  int prot = 0;
  int flags = 0;
  int fd = 0;
  int offset = 0;
  struct proc* p = 0;
  int i = 0;

  if(argaddr(0, &addr) < 0 || argint(1,&length) < 0 || argint(2,&prot) < 0 || argint(3,&flags) < 0 || argint(4,&fd) < 0 || argint(5,&offset) < 0){
    return 0;
  }
  p = myproc();

  for(i =0;i<16;i++){
    if(p->vma[i].valid == 0){
      p->vma[i].addr = p->vma_start-length+1;
      p->vma_start -= length;
      p->vma[i].file = p->ofile[fd];
      p->vma[i].length = length;
      p->vma[i].prot = prot;
      p->vma[i].flags = flags;
      p->vma[i].offset = offset;
      p->vma[i].valid = 1;
      break;
    }
  }
  
  filedup(p->vma[i].file);

  //struct proc* p = myproc();
  return (void*)p->vma[i].addr;
}

uint64 sys_munmap(){
  uint64 addr;
  int length;
  struct proc* p = 0;
  int i  = 0;
  if(argaddr(0,&addr) < 0 || argint(1,&length) < 0){
    return -1;
  }

  p = myproc();

  for(i = 0;i<16;i++){
    if(p->vma[i].valid == 1 && addr >= p->vma[i].addr && addr <= p->vma[i].addr + p->vma[i].length + p->vma[i].addr -1){
      break;
    }
  }
  if(p->vma[i].flags == MAP_SHARED){
    filewrite(p->vma[i].file,addr,length);
  }
  uvmunmap(p->pagetable,PGROUNDDOWN(addr),length/PGSIZE,1);
  p->vma[i].length -= length;
  if(p->vma[i].length == 0){
    fileclose(p->vma[i].file);
    p->vma[i].valid = 0;
  }
  return 0;
}