.data
.balign 16
string_literal_pointer0:
	.int 3
	.ascii "..."
	.byte 0
/* end data */

.data
.balign 16
string_literal_pointer1:
	.int 4
	.ascii "Woof"
	.byte 0
/* end data */

.data
.balign 16
string_literal_pointer2:
	.int 4
	.ascii "Meow"
	.byte 0
/* end data */

.data
.balign 16
string_literal_pointer3:
	.int 3
	.ascii "Rex"
	.byte 0
/* end data */

.data
.balign 16
string_literal_pointer4:
	.int 8
	.ascii "Whiskers"
	.byte 0
/* end data */

.data
.balign 16
string_literal_pointer5:
	.int 5
	.ascii "Buddy"
	.byte 0
/* end data */

.text
.balign 16
.globl __Animal_sound
__Animal_sound:
	endbr64
	leaq string_literal_pointer0(%rip), %rax
	ret
.type __Animal_sound, @function
.size __Animal_sound, .-__Animal_sound
/* end function __Animal_sound */

.text
.balign 16
.globl __Dog_sound
__Dog_sound:
	endbr64
	leaq string_literal_pointer1(%rip), %rax
	ret
.type __Dog_sound, @function
.size __Dog_sound, .-__Dog_sound
/* end function __Dog_sound */

.text
.balign 16
.globl __Cat_sound
__Cat_sound:
	endbr64
	leaq string_literal_pointer2(%rip), %rax
	ret
.type __Cat_sound, @function
.size __Cat_sound, .-__Cat_sound
/* end function __Cat_sound */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	callq _hulk_gc_init
	movl $0, %edi
	callq _hulk_alloc
	movq %rax, %rdi
	callq __Dog_sound
	movq %rax, %rdi
	movl $3, %esi
	callq _l_to_string
	movq %rax, %rdi
	callq _print
	movl $0, %edi
	callq _hulk_alloc
	movq %rax, %rdi
	callq __Cat_sound
	movq %rax, %rdi
	movl $3, %esi
	callq _l_to_string
	movq %rax, %rdi
	callq _print
	movl $0, %edi
	callq _hulk_alloc
	movq %rax, %rdi
	callq __Animal_sound
	movq %rax, %rdi
	movl $3, %esi
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
