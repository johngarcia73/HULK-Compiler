.data
.balign 16
string_literal_pointer0:
	.int 13
	.ascii "Hello, World!"
	.byte 0
/* end data */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	callq _hulk_gc_init
	movl $3, %esi
	leaq string_literal_pointer0(%rip), %rdi
	callq _l_to_string
	movq %rax, %rdi
	callq _print
	movl $0, %eax
	leave
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
