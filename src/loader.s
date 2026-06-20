.set MAGIC, 0x1badb002
.set FLAGS, (1<<0 | 1<<1 | 1<<2)
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
	.align 4
	.long MAGIC
	.long FLAGS
	.long CHECKSUM

	# Multiboot v1 keeps the video-mode fields after these five
	# address fields, even when the address flag is not set.
	.long 0		# header_addr
	.long 0		# load_addr
	.long 0		# load_end_addr
	.long 0		# bss_end_addr
	.long 0		# entry_addr

	.long 0		# graphics mode
	.long 1600
	.long 900
	.long 32


.section .text
.extern kernelMain
.extern callConstructors
.global loader

loader:
	mov $kernel_stack, %esp
	
		
	call callConstructors
	

	push %eax
	push %ebx
	
	#crash
	#push %ecx
	#push %edx
	
	
	call kernelMain
_stop:
	cli
	hlt
	jmp _stop



.section .bss
.space 2*1024*1024	; # 2 MiB
kernel_stack:
