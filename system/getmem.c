/* getmem.c - getmem */

#include <xinu.h>
#include <vm.h>

/*------------------------------------------------------------------------
 *  getmem  -  Allocate heap storage, returning lowest word address
 *------------------------------------------------------------------------
 */
char *getmem(uint32 nbytes /* Size of memory requested	*/
) {
	intmask mask; /* Saved interrupt mask		*/

	mask = disable();
	if (nbytes == 0) {
		restore(mask);
		return (char *)SYSERR;
	}

	nbytes = (uint32)roundpg(nbytes); /* Use page size multiples	*/

	uint32 page_required = nbytes / 4096;
	uint32 page_consecutive = 0;
	uint32 page_consecutive_start = 0;

	/* Find consecutive pages */
	page_directory_entry_t *page_dir = (page_directory_entry_t *)((2 << 22) | (2 << 12));
	for (uint32 i = 3; i <= 1015; i++) {
		if (page_dir[i].present == 0) {
			if (page_consecutive == 0)
				page_consecutive_start = i * 1024;
			page_consecutive += 1024;

			if (page_consecutive >= page_required)
				break;
			continue;
		}

		page_table_entry_t *page_table = (page_table_entry_t *)((2 << 22) | (i << 12));
		for (uint32 j = 0; j < 1024; j++) {
			if (page_table[j].present == 0) {
				if (page_consecutive == 0)
					page_consecutive_start = i * 1024 + j;
				page_consecutive += 1;
			}
			else {
				page_consecutive = 0;
				continue;
			}

			if (page_consecutive >= page_required)
				break;
		}
	}

	/* Allocate pages */
	if (page_consecutive >= page_required) {
		for (uint32 i = page_consecutive_start; i < page_consecutive_start + page_required; i++) {
			uint32 page_table_id = i / 1024;
			uint32 page_id = i % 1024;

			if (page_dir[page_table_id].present == 0) {
				page_dir[page_table_id].address = get_page() >> 12;
				page_dir[page_table_id].present = 1;
				page_dir[page_table_id].user_supervisor = 1;
				page_table_init((page_table_entry_t *)((2 << 22) | (page_table_id << 12)));
			}

			page_table_entry_t *page_table = (page_table_entry_t *)((2 << 22) | (page_table_id << 12));
			page_table[page_id].address = get_page() >> 12;
			page_table[page_id].present = 1;
			page_table[page_id].user_supervisor = 1;
		}

		uint32 page_table_id = page_consecutive_start / 1024;
		uint32 page_id = page_consecutive_start % 1024;
		restore(mask);
		return (char *)((page_table_id << 22) | (page_id << 12));
	}

	restore(mask);
	return (char *)SYSERR;
}
