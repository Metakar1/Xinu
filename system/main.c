/*  main.c  - main */

#include <xinu.h>

void busy_wait(void) {
	while (1);
}

void wait_sleep(void) {
	uint32 count = 0;
	while (++count < 130000);
	sleepms(15);
	while (1);
}

process	main(void)
{
	// resume(create(busy_wait, 1024, 50, "T1", 0));
	// resume(create(busy_wait, 1024, 40, "T2", 0));
	// resume(create(busy_wait, 1024, 30, "T3", 0));
	// resume(create(busy_wait, 1024, 40, "T4", 0));
	resume(create(wait_sleep, 1024, 50, "T1", 0));
	return OK;
}
