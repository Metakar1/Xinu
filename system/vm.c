#include <xinu.h>
#include <vm.h>

uint32 free_page_stack[32768];
uint32 free_page_stack_top = 0;

uint32 get_page() {
	if (free_page_stack_top == 0) {
		panic("out of memory");
	}
	return free_page_stack[--free_page_stack_top];
}

void release_page(uint32 page) {
	free_page_stack[free_page_stack_top++] = page;
}

void page_dir_init(page_directory_entry_t *page) {
	memset(page, 0, 4096);

	for (int i = 0; i < 1024; i++)
		page[i].read_write = 1;
}

void page_table_init(page_table_entry_t *page) {
	memset(page, 0, 4096);

	for (int i = 0; i < 1024; i++)
		page[i].read_write = 1;
}

uint32 vm2pm(void *address) {
	page_table_entry_t *page_table_vm = (page_table_entry_t *)((2 << 22) | (PAGE_DIR(address) << 12));
	return page_table_vm[PAGE_TABLE(address)].address << 12;
}

char *map_child_pm(uint32 nbytes, uint32 first_time, uint32 is_stack) {
	static uint32 used_page = 1016 * 1024;
	intmask       mask;

	mask = disable();
	if (nbytes == 0) {
		restore(mask);
		return (char *)SYSERR;
	}

	if (first_time == 1) {
		used_page = 1016 * 1024;
	}

	nbytes = (uint32)roundpg(nbytes);
	uint32 page_required = nbytes / 4096;
	if (page_required > free_page_stack_top) {
		restore(mask);
		return (char *)SYSERR;
	}

	page_directory_entry_t *page_dir = (page_directory_entry_t *)((2 << 22) | (2 << 12));

	for (uint32 i = used_page; i < used_page + page_required; i++) {
		uint32 page_table_id = i / 1024;
		uint32 page_id = i % 1024;

		if (page_dir[page_table_id].present == 0) {
			page_dir[page_table_id].address = get_page() >> 12;
			page_dir[page_table_id].present = 1;
			page_dir[page_table_id].user_supervisor = 0;
			page_table_init((page_table_entry_t *)((2 << 22) | (page_table_id << 12)));
		}

		page_table_entry_t *page_table = (page_table_entry_t *)((2 << 22) | (page_table_id << 12));
		page_table[page_id].address = get_page() >> 12;
		page_table[page_id].present = 1;
		page_table[page_id].user_supervisor = 0;
		uint32 va = (page_table_id << 22) | (page_id << 12);
		invlpg(va);
	}

	char  *return_address;
	uint32 page_table_id, page_id;
	if (is_stack == 1) {
		page_table_id = (used_page + page_required - 1) / 1024;
		page_id = (used_page + page_required - 1) % 1024;
		return_address = (char *)((page_table_id << 22) | (page_id << 12) | 4092);
	}
	else {
		page_table_id = used_page / 1024;
		page_id = used_page % 1024;
		return_address = (char *)((page_table_id << 22) | (page_id << 12));
	}

	used_page += page_required;
	restore(mask);
	return return_address;
}

void unmap_child_pm() {
	intmask mask; /* Saved interrupt mask		*/
	mask = disable();

	page_directory_entry_t *page_dir = (page_directory_entry_t *)((2 << 22) | (2 << 12));
	for (uint32 i = 1016; i < 1024; i++) {
		if (page_dir[i].present)
			release_page(vm2pm((void *)((2 << 22) | (i << 12))));
	}

	restore(mask);
}

/* Memory bounds */

void *minheap; /* Start of heap			*/
void *maxheap; /* Highest valid heap address		*/

/* Memory map structures */

uint32 bootsign = 1; /* Boot signature of the boot loader	*/

struct mbootinfo *bootinfo = (struct mbootinfo *)1;
/* Base address of the multiboot info	*/
/*  provided by GRUB, initialized just	*/
/*  to guarantee it is in the DATA	*/
/*  segment and not the BSS		*/

uint32 vminit() {
	struct memblk    *memptr;            /* Ptr to memory block		*/
	struct mbmregion *mmap_addr;         /* Ptr to mmap entries		*/
	struct mbmregion *mmap_addrend;      /* Ptr to end of mmap region	*/
	struct memblk    *next_memptr;       /* Ptr to next memory block	*/
	uint32            next_block_length; /* Size of next memory block	*/

	mmap_addr = (struct mbmregion *)NULL;
	mmap_addrend = (struct mbmregion *)NULL;

	/* Initialize the free list */
	memptr = &memlist;
	memptr->mnext = (struct memblk *)NULL;
	memptr->mlength = 0;

	/* Initialize the memory counters */
	/*    Heap starts at the end of Xinu image */
	minheap = ((void *)&end) + 8192;
	maxheap = minheap;

	/* Check if Xinu was loaded using the multiboot specification	*/
	/*   and a memory map was included				*/
	if (bootsign != MULTIBOOT_SIGNATURE) {
		panic("could not find multiboot signature");
	}
	if (!(bootinfo->flags & MULTIBOOT_BOOTINFO_MMAP)) {
		panic("no mmap found in boot info");
	}

	/* Get base address of mmap region (passed by GRUB) */
	mmap_addr = (struct mbmregion *)bootinfo->mmap_addr;

	/* Calculate address that follows the mmap block */
	mmap_addrend = (struct mbmregion *)((uint8 *)mmap_addr + bootinfo->mmap_length);

	/* Read mmap blocks and initialize the Xinu free memory list	*/
	while (mmap_addr < mmap_addrend) {
		/* If block is not usable, skip to next block */
		if (mmap_addr->type != MULTIBOOT_MMAP_TYPE_USABLE) {
			mmap_addr = (struct mbmregion *)((uint8 *)mmap_addr + mmap_addr->size + 4);
			continue;
		}

		if ((uint32)maxheap < ((uint32)mmap_addr->base_addr + (uint32)mmap_addr->length)) {
			maxheap = (void *)((uint32)mmap_addr->base_addr + (uint32)mmap_addr->length);
		}

		/* Ignore memory blocks within the Xinu image */
		if ((mmap_addr->base_addr + mmap_addr->length) < ((uint32)minheap)) {
			mmap_addr = (struct mbmregion *)((uint8 *)mmap_addr + mmap_addr->size + 4);
			continue;
		}

		/* The block is usable, so add it to Xinu's memory list */

		/* This block straddles the end of the Xinu image */
		if ((mmap_addr->base_addr <= (uint32)minheap) && ((mmap_addr->base_addr + mmap_addr->length) > (uint32)minheap)) {
			/* This is the first free block, base address is the minheap */
			next_memptr = (struct memblk *)roundpg(minheap);

			/* Subtract Xinu image from length of block */
			next_block_length = (uint32)truncpg(mmap_addr->base_addr + mmap_addr->length - (uint32)minheap);
		}
		else {
			/* Handle a free memory block other than the first one */
			next_memptr = (struct memblk *)roundpg(mmap_addr->base_addr);

			/* Initialize the length of the block */
			next_block_length = (uint32)truncpg(mmap_addr->length);
		}

		/* Add then new block to the free list */
		if (next_block_length != 0) {
			for (uint32 i = 0; i < next_block_length; i += 4096)
				release_page(((uint32)next_memptr) + i);
		}

		/* Move to the next mmap block */
		mmap_addr = (struct mbmregion *)((uint8 *)mmap_addr + mmap_addr->size + 4);
	}

	page_directory_entry_t *null_page_dir = (page_directory_entry_t *)get_page();
	page_dir_init(null_page_dir);

	for (uint32 i = 0; i < (uint32)roundpg(minheap); i += 4096) {
		uint32              page_table_id = PAGE_DIR(i);
		uint32              page_id = PAGE_TABLE(i);
		page_table_entry_t *null_page_table = (page_table_entry_t *)(null_page_dir[page_table_id].address << 12);

		if (null_page_dir[page_table_id].present == 0) {
			null_page_table = (page_table_entry_t *)get_page();
			null_page_dir[page_table_id].address = ((uint32)null_page_table) >> 12;
			null_page_dir[page_table_id].present = 1;
			page_table_init(null_page_table);
		}

		if ((uint32)&text <= i && i < (uint32)&etext)
			null_page_table[page_id].read_write = 0;
		null_page_table[page_id].address = i >> 12;
		null_page_table[page_id].present = 1;
	}

	null_page_dir[2].address = ((uint32)null_page_dir) >> 12;
	null_page_dir[2].present = 1;

	proctab[NULLPROC].prpgdir = (uint32)null_page_dir;

	return (uint32)null_page_dir;
}