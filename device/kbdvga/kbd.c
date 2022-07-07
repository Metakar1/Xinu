#include "kbd.h"

devcall kbdgetc(struct dentry *devptr) {
	wait(kbdcb.tyisem);
	char c = *kbdcb.tyihead++;
	if (kbdcb.tyihead >= &kbdcb.tyibuff[TY_IBUFLEN]) {
		kbdcb.tyihead = kbdcb.tyibuff;
	}
	return c;
}

devcall kbdread(struct dentry *devptr, char *buff, int32 count) {
	if (count < 0)
		return SYSERR;
	if (count == 0) {
		int32 avail = semcount(kbdcb.tyisem);
		if (avail == 0) {
			return 0;
		}
		else {
			count = avail;
		}
	}

	int32 nread = 0;
	char ch = kbdgetc(devptr);
	buff[nread++] = ch;

	while ((nread < count) && (ch != TY_NEWLINE) && (ch != TY_RETURN)) {
		ch = kbdgetc(devptr);
		buff[nread++] = ch;
	}
	return nread;
}
