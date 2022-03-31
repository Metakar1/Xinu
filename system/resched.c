/* resched.c - resched, resched_cntl */

#include <xinu.h>

struct	defer	Defer;

/*------------------------------------------------------------------------
 *  resched  -  Reschedule processor to highest priority eligible process
 *------------------------------------------------------------------------
 */
void	resched(void)		/* Assumes interrupts are disabled	*/
{
	struct procent *ptold;	/* Ptr to table entry for old process	*/
	struct procent *ptnew;	/* Ptr to table entry for new process	*/
	static uint32 last_time = 0;

	/* If rescheduling is deferred, record attempt and return */

	if (Defer.ndefers > 0) {
		Defer.attempt = TRUE;
		return;
	}

	/* Point to process table entry for the current (old) process */

	ptold = &proctab[currpid];
	ptold->preempt = (preempt <= 0 ? 0 : preempt);

	kprintf("Process %s ran %dms before reschedule.\n", ptold->prname, clktime * 1000 + count1000 - last_time);

	if (ptold->prstate == PR_CURR) {
		/* Old process will no longer remain current */

		ptold->prstate = PR_READY;
		if (currpid == 0)
			insert(currpid, readylist, -1);
		else
			insert(currpid, readylist, ptold->preempt);
	}

	/* Force context switch to highest priority ready process */

	currpid = dequeue(readylist);
	ptnew = &proctab[currpid];
	ptnew->prstate = PR_CURR;

	if (ptnew->preempt == 0) {
		switch (ptnew->prprio) {
			case 30:
				ptnew->preempt = T3;
				break;
			case 40:
				ptnew->preempt = T2;
				break;
			case 50:
				ptnew->preempt = T1;
				break;
			default:
				ptnew->preempt = QUANTUM;
		}
	}
	last_time = clktime * 1000 + count1000;
	preempt = ptnew->preempt;		/* Reset time slice for process	*/
	ctxsw(&ptold->prstkptr, &ptnew->prstkptr);

	/* Old process returns here when resumed */

	return;
}

/*------------------------------------------------------------------------
 *  resched_cntl  -  Control whether rescheduling is deferred or allowed
 *------------------------------------------------------------------------
 */
status	resched_cntl(		/* Assumes interrupts are disabled	*/
	  int32	defer		/* Either DEFER_START or DEFER_STOP	*/
	)
{
	switch (defer) {

	    case DEFER_START:	/* Handle a deferral request */

		if (Defer.ndefers++ == 0) {
			Defer.attempt = FALSE;
		}
		return OK;

	    case DEFER_STOP:	/* Handle end of deferral */
		if (Defer.ndefers <= 0) {
			return SYSERR;
		}
		if ( (--Defer.ndefers == 0) && Defer.attempt ) {
			resched();
		}
		return OK;

	    default:
		return SYSERR;
	}
}
