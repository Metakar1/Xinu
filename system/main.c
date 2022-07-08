/*  main.c  - main */

#include <xinu.h>
#include "filesystem.h"

process main(void) {
	/* Run the Xinu shell */

	char elf[8192];
	read_file("HELLO.ELF", elf, 8192);
	uint32 offset = get_elf_entrypoint(elf);
	resume(create(elf + offset, INITSTK, 60, "hello.elf", 1, kprintf));

	recvclr();
	resume(create(shell, 8192, 50, "shell", 1, CONSOLE));

	/* Wait for shell to exit and recreate it */

	while (TRUE) {
		receive();
		sleepms(200);
		kprintf("\n\nMain process recreating shell\n\n");
		resume(create(shell, 4096, 20, "shell", 1, CONSOLE));
	}
	return OK;
}
