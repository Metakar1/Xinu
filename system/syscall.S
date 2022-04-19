	.text
	.globl syscall_handler
syscall_handler:
	pushal
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs

	movl $(0x2 << 3), %esi
	movw %si, %ds
	movw %si, %es
	movw %si, %fs
	movw %si, %gs

	movl %esp, %edi
.LPUSH_ARGS:
	cmpl $0, %ecx
	jle .LSYSCALL_FUNC
	decl %ecx
	movl (%edx, %ecx, 4), %esi
	pushl %esi
	jmp .LPUSH_ARGS

.LSYSCALL_FUNC:
	call *%eax
	movl %eax, (%ebx)
	movl %edi, %esp

	popl %gs
	popl %fs
	popl %es
	popl %ds
	popal

	iret