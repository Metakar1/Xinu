/* xsh_test.c - xsh_test */

#include <xinu.h>
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------
 * xhs_test - display test string for keyboard and screen driver lab
 *------------------------------------------------------------------------
 */
shellcmd xsh_test(int nargs, char *args[]) {
	syscall_printf("12345678\t123456789\n123\t\t123456789\n\tabababababhbhjbjbjhASASAS\n\n\n\n\n\n\n\n\n\nab\bA\n");
	return 0;
}
