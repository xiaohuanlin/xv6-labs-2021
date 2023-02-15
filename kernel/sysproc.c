#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "printf.h"

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
  backtrace();
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

uint64
sys_sigalarm(void)
{
  int n;
  uint64 virtual_handler;
  if(argint(0, &n) < 0)
    return -1;
  if(argaddr(1, &virtual_handler) < 0)
    return -1;

  myproc()->target_ticks = n;
  myproc()->handler = virtual_handler;
  return 0;
}

uint64
pop_from_stack(uint64 sp, uint64* addr)
{
  struct proc *p = myproc();
  sp += sizeof(uint64);
  // sp += (16 - sp % 16) % 16;
  if (copyin(p->pagetable, (char*)addr, sp, sizeof(uint64)) < 0)
    panic("stack size is not enough");
  return sp;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  // get handler stack top from previous frame pointer
  uint64 sp = p->trapframe->s0 - sizeof(uint64);

  // get return address
  uint64 ra;
  sp = pop_from_stack(sp, &ra);

  // restore registers
  sp = pop_from_stack(sp, &p->trapframe->t6);
  sp = pop_from_stack(sp, &p->trapframe->t5);
  sp = pop_from_stack(sp, &p->trapframe->t4);
  sp = pop_from_stack(sp, &p->trapframe->t3);
  sp = pop_from_stack(sp, &p->trapframe->s11);
  sp = pop_from_stack(sp, &p->trapframe->s10);
  sp = pop_from_stack(sp, &p->trapframe->s9);
  sp = pop_from_stack(sp, &p->trapframe->s8);
  sp = pop_from_stack(sp, &p->trapframe->s7);
  sp = pop_from_stack(sp, &p->trapframe->s6);
  sp = pop_from_stack(sp, &p->trapframe->s5);
  sp = pop_from_stack(sp, &p->trapframe->s4);
  sp = pop_from_stack(sp, &p->trapframe->s3);
  sp = pop_from_stack(sp, &p->trapframe->s2);
  sp = pop_from_stack(sp, &p->trapframe->a7);
  sp = pop_from_stack(sp, &p->trapframe->a6);
  sp = pop_from_stack(sp, &p->trapframe->a5);
  sp = pop_from_stack(sp, &p->trapframe->a4);
  sp = pop_from_stack(sp, &p->trapframe->a3);
  sp = pop_from_stack(sp, &p->trapframe->a2);
  sp = pop_from_stack(sp, &p->trapframe->a1);
  sp = pop_from_stack(sp, &p->trapframe->a0);
  sp = pop_from_stack(sp, &p->trapframe->s1);
  sp = pop_from_stack(sp, &p->trapframe->s0);
  sp = pop_from_stack(sp, &p->trapframe->t2);
  sp = pop_from_stack(sp, &p->trapframe->t1);
  sp = pop_from_stack(sp, &p->trapframe->t0);
  sp = pop_from_stack(sp, &p->trapframe->tp);
  sp = pop_from_stack(sp, &p->trapframe->gp);
  sp = pop_from_stack(sp, &p->trapframe->sp);
  sp = pop_from_stack(sp, &p->trapframe->ra);

  // set epc
  p->trapframe->epc = ra;
  p->executing = 0;
  return 0;
}