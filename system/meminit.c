/* meminit.c - memory bounds and free list init */

#include <tss.h>
#include <xinu.h>

/* Segment table structures */

/* Segment Descriptor */

struct __attribute__((__packed__)) sd {
	unsigned short sd_lolimit;
	unsigned short sd_lobase;
	unsigned char  sd_midbase;
	unsigned char  sd_access;
	unsigned char  sd_hilim_fl;
	unsigned char  sd_hibase;
};

#define NGD               8 /* Number of global descriptor entries	*/
#define FLAGS_GRANULARITY 0x80
#define FLAGS_SIZE        0x40
#define FLAGS_SETTINGS    (FLAGS_GRANULARITY | FLAGS_SIZE)

tss_entry_t tss_entry;

struct sd gdt_copy[NGD] = {
  /*   sd_lolimit  sd_lobase   sd_midbase  sd_access   sd_hilim_fl sd_hibase */
  /* 0th entry NULL */
    {     0, 0, 0,    0,    0, 0},
 /* 1st, Kernel Code Segment */
    {0xffff, 0, 0, 0x9a, 0xcf, 0},
 /* 2nd, Kernel Data Segment */
    {0xffff, 0, 0, 0x92, 0xcf, 0},
 /* 3rd, Kernel Stack Segment */
    {0xffff, 0, 0, 0x92, 0xcf, 0},
 /* 4th, User Code Segment */
    {0xffff, 0, 0, 0xfa, 0xc0, 0},
 /* 5th, User Data Segment */
    {0xffff, 0, 0, 0xf2, 0xc0, 0},
 /* 6th, User Stack Segment */
    {0xffff, 0, 0, 0xf2, 0xc0, 0},
 /* 7th, Task State Segment */
    {     0, 0, 0,    0,    0, 0},
};

extern struct sd gdt[]; /* Global segment table			*/

/*------------------------------------------------------------------------
 * setsegs  -  Initialize the global segment table
 *------------------------------------------------------------------------
 */
void setsegs() {
	extern int etext;
	struct sd *psd;
	uint32     np, ds_end;

	ds_end = 0xffffffff / PAGE_SIZE; /* End page number of Data segment */

	psd = &gdt_copy[1];                                 /* Kernel code segment: identity map from address
				   0 to etext */
	np = ((int)&etext - 0 + PAGE_SIZE - 1) / PAGE_SIZE; /* Number of code pages */
	psd->sd_lolimit = np;
	psd->sd_hilim_fl = FLAGS_SETTINGS | ((np >> 16) & 0xff);

	psd = &gdt_copy[2]; /* Kernel data segment */
	psd->sd_lolimit = ds_end;
	psd->sd_hilim_fl = FLAGS_SETTINGS | ((ds_end >> 16) & 0xff);

	psd = &gdt_copy[3]; /* Kernel stack segment */
	psd->sd_lolimit = ds_end;
	psd->sd_hilim_fl = FLAGS_SETTINGS | ((ds_end >> 16) & 0xff);

	gdt_copy[7].sd_lolimit = 0xffff & (sizeof(tss_entry) - 1);
	gdt_copy[7].sd_lobase = 0xffff & (uint32)(&tss_entry);
	gdt_copy[7].sd_midbase = 0xff & ((uint32)(&tss_entry) >> 16);
	gdt_copy[7].sd_access = 0x89;
	gdt_copy[7].sd_hilim_fl = 0x00;
	gdt_copy[7].sd_hibase = 0xff & ((uint32)(&tss_entry) >> 24);

	tss_entry.ss0 = 0x18;

	memcpy(gdt, gdt_copy, sizeof(gdt_copy));
}
