.data
.balign 8
Object:
	.byte 0
/* end data */

.data
.balign 8
Range:
	.byte 0
/* end data */

.data
.balign 8
next:
	.byte 0
/* end data */

.data
.balign 8
current:
	.byte 0
/* end data */

.text
.balign 16
.globl __Range_next
__Range_next:
	endbr64
	movsd 24(%rdi), %xmm0
	addsd ".Lfp0"(%rip), %xmm0
	movsd %xmm0, 24(%rdi)
	movsd 16(%rdi), %xmm1
	ucomisd %xmm0, %xmm1
	seta %al
	movzbl %al, %eax
	ret
.type __Range_next, @function
.size __Range_next, .-__Range_next
/* end function __Range_next */

.text
.balign 16
.globl __Range_current
__Range_current:
	endbr64
	movsd 24(%rdi), %xmm0
	ret
.type __Range_current, @function
.size __Range_current, .-__Range_current
/* end function __Range_current */

