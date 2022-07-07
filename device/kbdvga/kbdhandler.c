#include "kbd.h"

extern void vgaerase1(uint32);

void kbdhandler(void) {
	static uint32  shift;
	static uint8  *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};
	uint32         st, data, c;
	struct dentry *devptr = (struct dentry *)&devtab[CONSOLE];

	st = inb(KBSTATP);
	if ((st & KBS_DIB) == 0)
		return;
	data = inb(KBDATAP);

	if (data == 0xE0) {
		shift |= E0ESC;
		return;
	}
	else if (data & 0x80) {
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return;
	}
	else if (shift & E0ESC) {
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];
	c = charcode[shift & (CTL | SHIFT)][data];
	if (!c)
		return;

	if (c == TY_BACKSP) {
		if (kbdcb.tyicursor > 0) {
			kbdcb.tyicursor--;
			if ((--kbdcb.tyitail) < kbdcb.tyibuff) {
				kbdcb.tyitail += TY_IBUFLEN;
			}
			vgaerase1(*kbdcb.tyitail);
		}
		return;
	}
	else if (c == TY_NEWLINE || c == TY_RETURN) {
		int32 icursor = kbdcb.tyicursor;
		vgaputc(devptr, TY_NEWLINE);
		*kbdcb.tyitail++ = c;
		if (kbdcb.tyitail >= &kbdcb.tyibuff[TY_IBUFLEN]) {
			kbdcb.tyitail = kbdcb.tyibuff;
		}
		kbdcb.tyicursor = 0;
		signaln(kbdcb.tyisem, icursor + 1);
		return;
	}

	int32 avail = semcount(kbdcb.tyisem);
	if (avail < 0)
		avail = 0;
	if ((avail + kbdcb.tyicursor) >= TY_IBUFLEN - 1)
		return;

	if (shift & CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	if ((c < TY_BLANK || c == 0177) && c != '\t') {
		vgaputc(devptr, TY_UPARROW);
		vgaputc(devptr, c + 0100);
		// vgaputc(devptr, 'A');
	}
	else {
		vgaputc(devptr, c);
		// vgaputc(devptr, 'B');
	}

	*kbdcb.tyitail++ = c;
	kbdcb.tyicursor++;
	if (kbdcb.tyitail >= &kbdcb.tyibuff[TY_IBUFLEN]) {
		kbdcb.tyitail = kbdcb.tyibuff;
	}
	return;
}
