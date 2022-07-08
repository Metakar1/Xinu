#include <xinu.h>

extern "C" {

char   sector_buff[512];
uint32 seek_offset;

devcall diskread(struct dentry *devptr, /* Entry in device switch table	*/
                 char          *buff,   /* Buffer of characters		*/
                 int32          count   /* Count of character to read	*/
) {
	uint32 read_count = 0;
	uint32 offset_in_sector;
	uint32 sector;
	while (read_count < count) {
		offset_in_sector = seek_offset % 512;
		sector = seek_offset / 512;

		outb(0x1f3, sector & 0xff);
		outb(0x1f4, (sector >> 8) & 0xff);
		outb(0x1f5, (sector >> 16) & 0xff);
		outb(0x1f6, 0xe0 | ((sector >> 24) & 0x0f));
		outb(0x1f2, 1);
		outb(0x1f7, 0x20);

		while ((inb(0x1f7) & 8) == 0) {};
		insw(0x1f0, (int32)sector_buff, 256);

		while (offset_in_sector < 512 && read_count < count) {
			buff[read_count] = sector_buff[offset_in_sector];
			read_count++;
			offset_in_sector++;
			seek_offset++;
		}
	}
	return OK;
}

devcall diskgetc(struct dentry *devptr /* Entry in device switch table	*/
) {
	int32 offset_in_sector = seek_offset % 512;
	int32 sector = seek_offset / 512;

	outb(0x1f3, sector & 0xff);
	outb(0x1f4, (sector >> 8) & 0xff);
	outb(0x1f5, (sector >> 16) & 0xff);
	outb(0x1f6, 0xe0 | ((sector >> 24) & 0x0f));
	outb(0x1f2, 1);
	outb(0x1f7, 0x20);

	while ((inb(0x1f7) & 8) == 0) {};
	insw(0x1f0, (int32)sector_buff, 256);
	seek_offset++;
	return sector_buff[offset_in_sector];
}

devcall diskseek(struct dentry *devptr, /* Entry in device switch table */
                 uint32         offset  /* Byte position in the file	*/
) {
	seek_offset = offset;
	return OK;
}
}
