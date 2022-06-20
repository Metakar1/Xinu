/* freemem.c - freemem */

#include "vm.h"
#include <xinu.h>

/*------------------------------------------------------------------------
 *  freemem  -  Free a memory block, returning the block to the free list
 *------------------------------------------------------------------------
 */
syscall freemem(char  *blkaddr, /* Pointer to memory block	*/
                uint32 nbytes   /* Size of block in bytes	*/
) {
	intmask mask; /* Saved interrupt mask		*/

	mask = disable();
	if (nbytes == 0) {
		restore(mask);
		return SYSERR;
	}

	nbytes = (uint32)roundpg(nbytes); /* Use page size multiples	*/

	uint32                  min_page_table_id = PAGE_DIR(blkaddr), max_page_table_id = PAGE_DIR(blkaddr);
	page_directory_entry_t *page_dir = (page_directory_entry_t *)((2 << 22) | (2 << 12));

	/* Release page */
	for (uint32 i = (uint32)blkaddr; i < (uint32)blkaddr + nbytes; i += 4096) {
		release_page(vm2pm((void *)i));

		uint32              page_table_id = PAGE_DIR(i);
		uint32              page_id = PAGE_TABLE(i);
		page_table_entry_t *page_table = (page_table_entry_t *)((2 << 22) | (page_table_id << 12));
		page_table[page_id].user_supervisor = 0;
		page_table[page_id].present = 0;

		max_page_table_id = page_table_id;
	}

	/* Release empty page table */
	for (uint32 i = min_page_table_id; i <= max_page_table_id; i++) {
		page_table_entry_t *page_table = (page_table_entry_t *)((2 << 22) | (i << 12));

		int flag = 0;
		for (uint32 j = 0; j < 1024; j++)
			if (page_table[j].present == 1) {
				flag = 1;
				break;
			}

		if (flag == 0) {
			release_page(vm2pm((void *)page_table));
			page_dir[i].user_supervisor = 0;
			page_dir[i].present = 0;
		}
	}

	restore(mask);
	return OK;
}
