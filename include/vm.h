#pragma once

#include "kernel.h"

/*----------------------------------------------------------------------
 * roundpg, truncpg - Round or truncate address to page size
 *----------------------------------------------------------------------
 */
#define roundpg(x)    (char *)((4095 + (uint32)(x)) & (~4095))
#define truncpg(x)    (char *)(((uint32)(x)) & (~4095))
#define PAGE_DIR(x)   (((uint32)(x) >> 22) & 1023)
#define PAGE_TABLE(x) (((uint32)(x) >> 12) & 1023)
#define invlpg(va)                                            \
	do {                                                      \
		asm volatile("invlpg (%0)" : : "r"((va)) : "memory"); \
	} while (0);

typedef struct {
	uint32 present         : 1;
	uint32 read_write      : 1;
	uint32 user_supervisor : 1;
	uint32 write_through   : 1;
	uint32 cache_disable   : 1;
	uint32 accessed        : 1;
	uint32 available_1     : 1;
	uint32 page_size       : 1;
	uint32 available_0     : 4;
	uint32 address         : 20;
} page_directory_entry_t;

typedef struct {
	uint32 present         : 1;
	uint32 read_write      : 1;
	uint32 user_supervisor : 1;
	uint32 write_through   : 1;
	uint32 cache_disable   : 1;
	uint32 accessed        : 1;
	uint32 dirty           : 1;
	uint32 page_attribute  : 1;
	uint32 global          : 1;
	uint32 available       : 3;
	uint32 address         : 20;
} page_table_entry_t;

uint32 get_page();
void   release_page(uint32 page);
void   page_dir_init(page_directory_entry_t *page);
void   page_table_init(page_table_entry_t *page);
uint32 vm2pm(void *address);
char  *map_child_pm(uint32 nbytes, uint32 first_time, uint32 is_stack);
void   unmap_child_pm();