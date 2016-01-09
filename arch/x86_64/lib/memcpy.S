/* Copyright 2002 Andi Kleen */
	
/*
 * memcpy - Copy a memory block.
 *
 * Input:	
 * rdi destination
 * rsi source
 * rdx count
 * 
 * Output:
 * rax original destination
 */	

 // #define FIX_ALIGNMENT
 	.globl __memcpy
	.globl memcpy
	.p2align
__memcpy:
memcpy:		
	pushq %rbx
	movq %rdi,%rax

#ifdef FIX_ALIGNMENT
	movl %edi,%ecx
	andl $7,%ecx
	jnz  .Lbad_alignment	
.Lafter_bad_alignment:
#endif

	movq %rdx,%rcx
	movl $64,%ebx
	shrq $6,%rcx
	jz .Lhandle_tail
	
.Lloop_64:
	movq (%rsi),%r11
	movq 8(%rsi),%r8
	movq 2*8(%rsi),%r9
	movq 3*8(%rsi),%r10
	movq %r11,(%rdi)
	movq %r8,1*8(%rdi)
	movq %r9,2*8(%rdi)
	movq %r10,3*8(%rdi)
		
	movq 4*8(%rsi),%r11
	movq 5*8(%rsi),%r8
	movq 6*8(%rsi),%r9
	movq 7*8(%rsi),%r10
	movq %r11,4*8(%rdi)
	movq %r8,5*8(%rdi)
	movq %r9,6*8(%rdi)
	movq %r10,7*8(%rdi)

	addq %rbx,%rsi	
	addq %rbx,%rdi
	decl %ecx
	jnz  .Lloop_64

.Lhandle_tail:
	movl %edx,%ecx
	andl $63,%ecx
	shrl $3,%ecx
	jz   .Lhandle_7
	movl $8,%ebx
.Lloop_8: 
	movq (%rsi),%r8
	movq %r8,(%rdi) 
	addq %rbx,%rdi
	addq %rbx,%rsi
	decl %ecx
	jnz  .Lloop_8

.Lhandle_7:
	movl %edx,%ecx
	andl $7,%ecx
	jz .Lende
.Lloop_1:
	movb (%rsi),%r8b
	movb %r8b,(%rdi) 
	incq %rdi
	incq %rsi
	decl %ecx
	jnz .Lloop_1
	
.Lende: 	
	sfence
	popq %rbx
	ret


#ifdef FIX_ALIGNMENT
	/* align destination */
	/* This is simpleminded. For bigger blocks it may make sense to align
	   src and dst to their aligned subset and handle the rest separately */
.Lbad_alignment:
	movl $8,%r9d
	subl %ecx,%r9d
	movl %r9d,%ecx
	subq %r9,%rdx
	js   .Lsmall_alignment
	jz   .Lsmall_alignment
.Lalign_1:
	movb (%rsi),%r8b
	movb %r8b,(%rdi) 
	incq %rdi
	incq %rsi
	decl %ecx
	jnz  .Lalign_1
	jmp .Lafter_bad_alignment
.Lsmall_alignment:
	addq %r9,%rdx
	jmp .Lhandle_7
#endif	
