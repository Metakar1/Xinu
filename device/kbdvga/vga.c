#include "kbd.h"

uint32 pos;

#define TEXT_MODE_ROW    25
#define TEXT_MODE_COLUMN 80
#define TEXT_MODE_BASE   ((uint16 *)0xB8000)

void set_cursor_position(uint32 new_pos) {
	outb(0x3D4, 14);
	outb(0x3D5, new_pos >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, new_pos);
	*(TEXT_MODE_BASE + new_pos) = ' ' | 0x0700;
	pos = new_pos;
}

uint32 get_cursor_position() {
	outb(0x3D4, 14);
	pos = inb(0x3D5) << 8;
	outb(0x3D4, 15);
	pos |= inb(0x3D5);
	return pos;
}

// 80 * row + col;
void put_newline() {
	uint32 row = pos / 80;

	if (row == TEXT_MODE_ROW - 1) {
		// scroll
		for (uint32 i = TEXT_MODE_COLUMN; i < TEXT_MODE_ROW * TEXT_MODE_COLUMN; i++)
			*(TEXT_MODE_BASE + i - TEXT_MODE_COLUMN) = *(TEXT_MODE_BASE + i);
		for (uint32 i = (TEXT_MODE_ROW - 1) * TEXT_MODE_COLUMN; i < TEXT_MODE_ROW * TEXT_MODE_COLUMN; i++)
			*(TEXT_MODE_BASE + i) = ' ' | 0x0700;
		set_cursor_position((TEXT_MODE_ROW - 1) * 80 + 0);
	}
	else
		set_cursor_position((row + 1) * 80 + 0);
}

void put_char(char ch) {
	uint32 col = pos % 80;

	*(TEXT_MODE_BASE + pos) = ch | 0x0700;
	if (col == TEXT_MODE_COLUMN - 1)
		put_newline();
	else
		set_cursor_position(pos + 1);
}

void put_tab() {
	uint32 col = pos % 80;
	for (uint32 i = 0; i < 8 - (col % 8); i++)
		put_char(0);
}

void put_backspace() {
	if (pos == 0)
		return;

	set_cursor_position(pos - 1);
}

devcall vgaputc(struct dentry *devptr, char ch) {
	if (ch == '\n') {
		put_newline();
	}
	else if (ch == '\t') {
		put_tab();
	}
	else if (ch == '\b') {
		put_backspace();
	}
	else {
		put_char(ch);
	}
	return OK;
}

void vgaerase1(uint32 c) {
	if (c == '\t') { // tab
		for (uint32 i = 0; i < 8; i++)
			if (*(TEXT_MODE_BASE + pos - 1) == (0 | 0x0700))
				put_backspace();
			else
				break;
	}
	else if (c < TY_BLANK || c == 0177) { // ^A
		put_backspace();
		put_backspace();
	}
	else // single char
		put_backspace();
}

void vgainit() {
	set_cursor_position(0);
	for (uint32 i = 0; i < TEXT_MODE_ROW * TEXT_MODE_COLUMN; i++)
		*(TEXT_MODE_BASE + i) = ' ' | 0x0700;
}