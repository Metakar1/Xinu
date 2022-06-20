/* create.c - create, newpid */

#include <xinu.h>
#include <tss.h>
#include <vm.h>

local int newpid();

extern void ret_k2u();

extern tss_entry_t tss_entry;
/*------------------------------------------------------------------------
 *  create  -  Create a process to start running a function on x86
 *------------------------------------------------------------------------
 */
pid32 create(void  *funcaddr, /* Address of the function	*/
             uint32 ssize,    /* Stack size in bytes		*/
             pri16  priority, /* Process priority > 0		*/
             char  *name,     /* Name (for debugging)		*/
             uint32 nargs,    /* Number of args that follow	*/
             ...) {
	uint32          savsp, *pushsp;
	intmask         mask;  /* Interrupt mask		*/
	pid32           pid;   /* Stores new process id	*/
	struct procent *prptr; /* Pointer to proc. table entry */
	int32           i;
	uint32         *a;                         /* Points to list of args	*/
	uint32         *ksaddr = (uint32 *)SYSERR; /* Kernel stack address		*/
	uint32         *usaddr;                    /* User stack address */

	mask = disable();
	if (ssize < MINSTK)
		ssize = MINSTK;
	ssize = (uint32)roundpg(ssize);
	if ((priority < 1) || ((pid = newpid()) == SYSERR)
	    || ((ksaddr = (uint32 *)map_child_pm(8192, 1, 1)) == (uint32 *)SYSERR)
	    || ((usaddr = (uint32 *)map_child_pm(ssize, 0, 1)) == (uint32 *)SYSERR)) {
		if (ksaddr != (uint32 *)SYSERR)
			freestk(ksaddr, ssize);
		restore(mask);
		return SYSERR;
	}

	page_directory_entry_t *new_process_page_dir = (page_directory_entry_t *)map_child_pm(4096, 0, 0);
	page_dir_init(new_process_page_dir);

	page_table_entry_t *new_process_page_table[2];
	new_process_page_table[0] = new_process_page_table[1] = NULL;

	/* Code and data */
	for (uint32 i = 0; i < (uint32)roundpg(((void *)&end)); i += 4096) {
		uint32 page_table_id = PAGE_DIR(i);
		uint32 page_id = PAGE_TABLE(i);

		if (new_process_page_table[page_table_id] == NULL) {
			new_process_page_table[page_table_id] = (page_table_entry_t *)map_child_pm(4096, 0, 0);
			new_process_page_dir[page_table_id].address = vm2pm((void *)new_process_page_table[page_table_id]) >> 12;
			new_process_page_dir[page_table_id].present = 1;
			new_process_page_dir[page_table_id].user_supervisor = 1;
			page_table_init(new_process_page_table[page_table_id]);
		}

		if ((uint32)&text <= i && i < (uint32)&etext)
			new_process_page_table[page_table_id][page_id].read_write = 0;
		new_process_page_table[page_table_id][page_id].address = i >> 12;
		new_process_page_table[page_table_id][page_id].present = 1;
		new_process_page_table[page_table_id][page_id].user_supervisor = 1;
	}

	/* Kernel stack */
	for (uint32 i = 0; i < 8192; i += 4096) {
		uint32 page_table_id = PAGE_DIR(roundpg(((void *)&end)) + i);
		uint32 page_id = PAGE_TABLE(roundpg(((void *)&end)) + i);

		if (new_process_page_table[page_table_id] == NULL) {
			new_process_page_table[page_table_id] = (page_table_entry_t *)map_child_pm(4096, 0, 0);
			new_process_page_dir[page_table_id].address = vm2pm((void *)new_process_page_table[page_table_id]) >> 12;
			new_process_page_dir[page_table_id].present = 1;
			page_table_init(new_process_page_table[page_table_id]);
		}

		new_process_page_table[page_table_id][page_id].address = vm2pm((void *)ksaddr + 4 - 8192 + i) >> 12;
		new_process_page_table[page_table_id][page_id].present = 1;
	}

	/* User stack */
	uint32 filled_ssize = 0;
	for (uint32 i = 1015; i > 2; i--) {
		page_table_entry_t *new_process_page_table = (page_table_entry_t *)map_child_pm(4096, 0, 0);
		new_process_page_dir[i].address = vm2pm((void *)new_process_page_table) >> 12;
		new_process_page_dir[i].present = 1;
		new_process_page_dir[i].user_supervisor = 1;
		page_table_init(new_process_page_table);

		for (int32 j = 1023; j > -1; j--) {
			new_process_page_table[j].address = vm2pm((void *)usaddr + 4 - 4096 - filled_ssize) >> 12;
			new_process_page_table[j].present = 1;
			new_process_page_table[j].user_supervisor = 1;

			filled_ssize += 4096;
			if (filled_ssize >= ssize)
				break;
		}

		if (filled_ssize >= ssize)
			break;
	}

	new_process_page_dir[2].address = vm2pm(new_process_page_dir) >> 12;
	new_process_page_dir[2].present = 1;
	new_process_page_dir[2].user_supervisor = 1;

	prcount++;
	prptr = &proctab[pid];

	/* Initialize process table entry for new process */
	prptr->prstate = PR_SUSP; /* Initial state is suspended	*/
	prptr->prprio = priority;
	// prptr->prstkbase = (char *)ksaddr;
	// prptr->prustkbase = (char *)usaddr;
	prptr->prstkbase = (char *)(((void *)&end) + 8192 - 4);
	prptr->prustkbase = (char *)(0xfe000000 - 4);
	prptr->prustkbase_in_prt = (char *)usaddr;
	prptr->prstklen = ssize;
	prptr->prname[PNMLEN - 1] = NULLCH;
	for (i = 0; i < PNMLEN - 1 && (prptr->prname[i] = name[i]) != NULLCH; i++)
		;
	prptr->prsem = -1;
	prptr->prparent = (pid32)getpid();
	prptr->prhasmsg = FALSE;
	prptr->prpgdir = vm2pm(new_process_page_dir);

	/* Set up stdin, stdout, and stderr descriptors for the shell	*/
	prptr->prdesc[0] = CONSOLE;
	prptr->prdesc[1] = CONSOLE;
	prptr->prdesc[2] = CONSOLE;

	/* Initialize stack as if the process was called		*/

	uint32 *kernel_stack_vm = (uint32 *)(((void *)&end) + 8192 - 4);
	uint32 *user_stack_vm = (uint32 *)(0xfe000000 - 4);
	*usaddr = *ksaddr = STACKMAGIC;
	savsp = (uint32)kernel_stack_vm;

	/* Push arguments */
	a = (uint32 *)(&nargs + 1);  /* Start of args		*/
	a += nargs - 1;              /* Last argument		*/
	for (; nargs > 0; nargs--) { /* Machine dependent; copy args	*/
		*--usaddr = *a--;
		--user_stack_vm;
	}

	*--ksaddr = (long)INITRET; /* Push on return address	*/
	--kernel_stack_vm;
	*--usaddr = (long)INITRET;
	--user_stack_vm;
	prptr->prustkptr = (char *)user_stack_vm;
	prptr->prustkptr_in_prt = (char *)usaddr;
	prptr->prksesp = kernel_stack_vm;

	/* The following entries on the stack must match what ctxsw	*/
	/*   expects a saved process state to contain: ret address,	*/
	/*   ebp, interrupt mask, flags, registers, and an old SP	*/

	/* A fake part used for returning from interrupt */
	/* %ss */
	*--ksaddr = 0x33;
	--kernel_stack_vm;
	/* %esp */
	*--ksaddr = (uint32)user_stack_vm;
	--kernel_stack_vm;
	/* eflags */
	*--ksaddr = 0x200;
	--kernel_stack_vm;
	/* %cs */
	*--ksaddr = 0x23;
	--kernel_stack_vm;

	/* %eip or funcaddr */
	*--ksaddr = (long)funcaddr; /* Make the stack look like it's*/
	--kernel_stack_vm;
	*--ksaddr = (long)ret_k2u;
	--kernel_stack_vm;
	/*   half-way through a call to	*/
	/*   ctxsw that "returns" to the*/
	/*   new process		*/
	*--ksaddr = savsp; /* This will be register ebp	*/
	--kernel_stack_vm;
	/*   for process exit		*/
	savsp = (uint32)kernel_stack_vm; /* Start of frame for ctxsw	*/
	*--ksaddr = 0x0;                 /* New process runs with	*/
	--kernel_stack_vm;
	/*   interrupts enabled		*/

	/* Basically, the following emulates an x86 "pushal" instruction*/

	*--ksaddr = 0; /* %eax */
	--kernel_stack_vm;
	*--ksaddr = 0; /* %ecx */
	--kernel_stack_vm;
	*--ksaddr = 0; /* %edx */
	--kernel_stack_vm;
	*--ksaddr = 0; /* %ebx */
	--kernel_stack_vm;
	*--ksaddr = 0; /* %esp; value filled in below	*/
	--kernel_stack_vm;
	pushsp = ksaddr;   /* Remember this location	*/
	*--ksaddr = savsp; /* %ebp (while finishing ctxsw)	*/
	--kernel_stack_vm;
	*--ksaddr = 0; /* %esi */
	--kernel_stack_vm;
	*--ksaddr = 0; /* %edi */
	--kernel_stack_vm;
	*pushsp = (unsigned long)(prptr->prstkptr = (char *)kernel_stack_vm);
	restore(mask);
	return pid;
}

/*------------------------------------------------------------------------
 *  newpid  -  Obtain a new (free) process ID
 *------------------------------------------------------------------------
 */
local pid32 newpid(void) {
	uint32       i;           /* Iterate through all processes*/
	static pid32 nextpid = 1; /* Position in table to try or	*/
	/*   one beyond end of table	*/

	/* Check all NPROC slots */

	for (i = 0; i < NPROC; i++) {
		nextpid %= NPROC; /* Wrap around to beginning */
		if (proctab[nextpid].prstate == PR_FREE) {
			return nextpid++;
		}
		else {
			nextpid++;
		}
	}
	kprintf("newpid error\n");
	return (pid32)SYSERR;
}
