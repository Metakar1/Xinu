#include "kbd.h"

struct kbdcblk kbdcb;
extern void    vgainit();

devcall kbdvgainit() {
	set_evec(0x21, (uint32)kbddisp);

	kbdcb.tyihead = kbdcb.tyitail = &kbdcb.tyibuff[0];
	kbdcb.tyisem = semcreate(0);
	kbdcb.tyicursor = 0;

	vgainit();
	return OK;
}