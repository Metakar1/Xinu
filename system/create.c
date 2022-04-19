/* create.c - create, newpid */

#include <xinu.h>
#include <tss.h>

local	int newpid();

extern void ret_k2u();

extern tss_entry_t tss_entry;
/*------------------------------------------------------------------------
 *  create  -  Create a process to start running a function on x86
 *------------------------------------------------------------------------
 */
pid32	create(
	  void		*funcaddr,	/* Address of the function	*/
	  uint32	ssize,		/* Stack size in bytes		*/
	  pri16		priority,	/* Process priority > 0		*/
	  char		*name,		/* Name (for debugging)		*/
	  uint32	nargs,		/* Number of args that follow	*/
	  ...
	)
{
	uint32		savsp, *pushsp;
	intmask 	mask;    	/* Interrupt mask		*/
	pid32		pid;		/* Stores new process id	*/
	struct	procent	*prptr;		/* Pointer to proc. table entry */
	int32		i;
	uint32		*a;		/* Points to list of args	*/
	uint32		*ksaddr = (uint32 *)SYSERR;		/* Kernel stack address		*/
	uint32      *usaddr;	/* User stack address */

	mask = disable();
	if (ssize < MINSTK)
		ssize = MINSTK;
	ssize = (uint32) roundmb(ssize);
	if ((priority < 1) || ((pid=newpid()) == SYSERR) ||
		 ((ksaddr = (uint32 *)getstk(ssize)) == (uint32 *)SYSERR) ||
		 ((usaddr = (uint32 *)getstk(ssize)) == (uint32 *)SYSERR)) {
		if (ksaddr != (uint32 *)SYSERR)
			freestk(ksaddr, ssize);
		restore(mask);
		return SYSERR;
	}

	prcount++;
	prptr = &proctab[pid];

	/* Initialize process table entry for new process */
	prptr->prstate = PR_SUSP;	/* Initial state is suspended	*/
	prptr->prprio = priority;
	prptr->prstkbase = (char *)ksaddr;
	prptr->prstklen = ssize;
	prptr->prname[PNMLEN-1] = NULLCH;
	for (i=0 ; i<PNMLEN-1 && (prptr->prname[i]=name[i])!=NULLCH; i++)
		;
	prptr->prsem = -1;
	prptr->prparent = (pid32)getpid();
	prptr->prhasmsg = FALSE;

	/* Set up stdin, stdout, and stderr descriptors for the shell	*/
	prptr->prdesc[0] = CONSOLE;
	prptr->prdesc[1] = CONSOLE;
	prptr->prdesc[2] = CONSOLE;

	/* Initialize stack as if the process was called		*/

	*usaddr = *ksaddr = STACKMAGIC;
	savsp = (uint32)ksaddr;

	/* Push arguments */
	a = (uint32 *)(&nargs + 1);	/* Start of args		*/
	a += nargs - 1;			/* Last argument		*/
	for ( ; nargs > 0 ; nargs--) {	/* Machine dependent; copy args	*/
		*--ksaddr = *a;	/* onto created process's stack	*/
		*--usaddr = *a--;
	}
	*--ksaddr = (long)INITRET;	/* Push on return address	*/
	*--usaddr = (long)INITRET;
	prptr->prksesp = ksaddr;

	/* The following entries on the stack must match what ctxsw	*/
	/*   expects a saved process state to contain: ret address,	*/
	/*   ebp, interrupt mask, flags, registers, and an old SP	*/

	/* A fake part used for returning from interrupt */
	/* %ss */
	*--ksaddr = 0x33;
	/* %esp */
	*--ksaddr = (uint32)usaddr;
	/* eflags */
	*--ksaddr = 0x200;
	/* %cs */
	*--ksaddr = 0x23;

	/* %eip or funcaddr */
	*--ksaddr = (long)funcaddr;	/* Make the stack look like it's*/
	*--ksaddr = (long)ret_k2u;
					/*   half-way through a call to	*/
					/*   ctxsw that "returns" to the*/
					/*   new process		*/
	*--ksaddr = savsp;		/* This will be register ebp	*/
					/*   for process exit		*/
	savsp = (uint32) ksaddr;		/* Start of frame for ctxsw	*/
	*--ksaddr = 0x0;		/* New process runs with	*/
					/*   interrupts enabled		*/

	/* Basically, the following emulates an x86 "pushal" instruction*/

	*--ksaddr = 0;			/* %eax */
	*--ksaddr = 0;			/* %ecx */
	*--ksaddr = 0;			/* %edx */
	*--ksaddr = 0;			/* %ebx */
	*--ksaddr = 0;			/* %esp; value filled in below	*/
	pushsp = ksaddr;			/* Remember this location	*/
	*--ksaddr = savsp;		/* %ebp (while finishing ctxsw)	*/
	*--ksaddr = 0;			/* %esi */
	*--ksaddr = 0;			/* %edi */
	*pushsp = (unsigned long) (prptr->prstkptr = (char *)ksaddr);
	restore(mask);
	return pid;
}

/*------------------------------------------------------------------------
 *  newpid  -  Obtain a new (free) process ID
 *------------------------------------------------------------------------
 */
local	pid32	newpid(void)
{
	uint32	i;			/* Iterate through all processes*/
	static	pid32 nextpid = 1;	/* Position in table to try or	*/
					/*   one beyond end of table	*/

	/* Check all NPROC slots */

	for (i = 0; i < NPROC; i++) {
		nextpid %= NPROC;	/* Wrap around to beginning */
		if (proctab[nextpid].prstate == PR_FREE) {
			return nextpid++;
		} else {
			nextpid++;
		}
	}kprintf("newpid error\n");
	return (pid32) SYSERR;
}
