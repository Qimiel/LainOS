.set IRQ_BASE, 0x20



.section .text

.extern _ZN2os21hardwarecommunication16InterruptManager15handleInterruptEhj

.global _ZN2os21hardwarecommunication16InterruptManager22IgnoreInterruptRequestEv



.macro HandleExceptionNoError num
.global _ZN2os21hardwarecommunication16InterruptManager19HandleException\num\()Ev
_ZN2os21hardwarecommunication16InterruptManager19HandleException\num\()Ev:
	movl $\num, (interruptnumber)
	pushl $0
	jmp int_bottom
.endm



.macro HandleExceptionWithError num
.global _ZN2os21hardwarecommunication16InterruptManager19HandleException\num\()Ev
_ZN2os21hardwarecommunication16InterruptManager19HandleException\num\()Ev:
	movl $\num, (interruptnumber)
	jmp int_bottom
.endm



.macro HandleInterruptRequest num
.global _ZN2os21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev
_ZN2os21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev:
	movl $\num + IRQ_BASE, (interruptnumber)
	pushl $0
	jmp int_bottom
.endm


HandleExceptionNoError 0x00
HandleExceptionNoError 0x01
HandleExceptionNoError 0x02
HandleExceptionNoError 0x03
HandleExceptionNoError 0x04
HandleExceptionNoError 0x05
HandleExceptionNoError 0x06
HandleExceptionNoError 0x07
HandleExceptionWithError 0x08
HandleExceptionNoError 0x09
HandleExceptionWithError 0x0A
HandleExceptionWithError 0x0B
HandleExceptionWithError 0x0C
HandleExceptionWithError 0x0D
HandleExceptionWithError 0x0E
HandleExceptionNoError 0x0F
HandleExceptionNoError 0x10
HandleExceptionWithError 0x11
HandleExceptionNoError 0x12
HandleExceptionNoError 0x13



HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x02
HandleInterruptRequest 0x03
HandleInterruptRequest 0x04
HandleInterruptRequest 0x05
HandleInterruptRequest 0x06
HandleInterruptRequest 0x07
HandleInterruptRequest 0x08
HandleInterruptRequest 0x09
HandleInterruptRequest 0x0A
HandleInterruptRequest 0x0B
HandleInterruptRequest 0x0C
HandleInterruptRequest 0x0D
HandleInterruptRequest 0x0E
HandleInterruptRequest 0x0F
HandleInterruptRequest 0x31

HandleInterruptRequest 0x80


int_bottom:

	# save registers
	#pusha
	#pushl %ds
	#pushl %es
	#pushl %fs
	#pushl %gs

	pushl %ebp
	pushl %edi
	pushl %esi

	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	
	#call C++ handler
	pushl %esp
	pushl (interruptnumber)
	call _ZN2os21hardwarecommunication16InterruptManager15handleInterruptEhj
	# addl $5, %esp	
	mov %eax, %esp # switch the stack

	# restore registers	

	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	
	popl %esi
	popl %edi
	popl %ebp

	#popl %gs
	#popl %fs
	#popl %es
	#popl %ds
	#popa	

	add $4, %esp

.global _ZN2os21hardwarecommunication16InterruptManager15InterruptIgnoreEv
_ZN2os21hardwarecommunication16InterruptManager22IgnoreInterruptRequestEv:

	iret

.data
	interruptnumber: .long 0
	
