#ifdef VBOX
#include "etherboot-asm.h"
#endif /* VBOX */
#include "callbacks.h"
	.equ CR0_PE, 1
	
	.text
	.arch i386
#ifdef VBOX
#ifdef __WIN32__
	.section .prefix, "ax"
#else /* __WIN32__ */
	.section .prefix, "ax", @progbits
#endif /* __WIN32__ */
#else /* !VBOX */
	.section ".prefix", "ax", @progbits
#endif /* !VBOX */

#undef CODE16
#if defined(PCBIOS)
#define	CODE16
#endif

/* We have two entry points: "conventional" (at the start of the file)
 * and "callback" (at _entry, 2 bytes in).  The "callback" entry
 * should be used if the caller wishes to provide a specific opcode.
 * It is equivalent to a call to in_call.  Using the "conventional"
 * entry point is equivalent to using the "callback" entry point with
 * an opcode of EB_OPCODE_MAIN.
 *
 * Both entry points can be called in either 16-bit real or 32-bit
 * protected mode with flat physical addresses.  We detect which mode
 * the processor is in and call either in_call or rm_in_call as
 * appropriate.  Note that the mode detection code must therefore be
 * capable of doing the same thing in either mode, even though the
 * machine code instructions will be interpreted differently.
 *
 * The decompressor will be invoked if necessary to decompress
 * Etherboot before attempting to jump to it.
 */

/******************************************************************************
 * Entry points and mode detection code
 ******************************************************************************
 */

	.code32
/* "Conventional" entry point: caller provides no opcode */
#ifdef VBOX
	.globl	GSYM(_start)
GSYM(_start):
#else /* !VBOX */
	.globl	_start
_start:
#endif /* !VBOX */
	/* Set flag to indicate conventional entry point used */
	pushl	$0			/* "pushw $0" in 16-bit code */
	/* Fall through to "callback" entry point */
	
/* "Callback" entry point */
#ifdef VBOX
	.globl	GSYM(_entry)
GSYM(_entry):
#else /* !VBOX */
	.globl	_entry
_entry:
#endif /* !VBOX */
	
#ifdef CODE16
	/* CPU mode detection code */
	pushl	%eax			/* "pushw %ax" in 16-bit code */
	pushw	%ax			/* "pushl %eax" in 16-bit code */
	movl	%cr0, %eax		/* Test protected mode bit */
	testb	$CR0_PE, %al
	popw	%ax			/* "popl %eax" in 16-bit code */
	popl	%eax			/* "popw %eax" in 16-bit code */
	jz	rmode
#endif /* CODE16 */

/******************************************************************************
 * Entered in protected mode
 ******************************************************************************
 */
		
	.code32
pmode:
	cmpl	$0, 0(%esp)		/* Conventional entry point used? */
	jne	1f
	/* Entered via conventional entry point: set up stack */
	xchgl	%eax, 4(%esp)		/* %eax = return addr, store %eax */
	movl	%eax, 0(%esp)		/* 0(%esp) = return address */
	movl	$(EB_OPCODE_MAIN|EB_USE_INTERNAL_STACK|EB_SKIP_OPCODE), %eax
	xchgl	%eax, 4(%esp)		/* 4(%esp) = opcode, restore %eax */
1:
	/* Run decompressor if necessary */
	pushl	%eax
#ifdef VBOX
	movl	$GSYM(decompress), %eax
#else /* !VBOX */
	movl	$decompress, %eax
#endif /* !VBOX */
	testl	%eax, %eax
	jz	1f
#ifdef VBOX
	call	GSYM(decompress)
#else /* !VBOX */
	call	decompress
#endif /* !VBOX */
1:	popl	%eax

	/* Make in_call to Etherboot */
#ifdef VBOX
	jmp	GSYM(_prefix_in_call)
#else /* !VBOX */
	jmp	_prefix_in_call
#endif /* !VBOX */

/******************************************************************************
 * Entered in real mode
 ******************************************************************************
 */
	
#ifdef CODE16
	.code16
rmode:
	pushw	%ax			/* Padding */
	pushw	%bp
	movw	%sp, %bp
	cmpw	$0, 6(%bp)		/* Conventional entry point used? */
	jne	1f
	/* Entered via conventional entry point: set up stack */
	pushw	%ax
	movw	6(%bp), %ax
	movw	%ax, 2(%bp)		/* Move return address down */
	movl	$(EB_OPCODE_MAIN|EB_USE_INTERNAL_STACK|EB_SKIP_OPCODE), 4(%bp)
	popw	%ax
	popw	%bp
	jmp	2f
1:	/* Entered via callback entry point: do nothing */
	popw	%bp
	popw	%ax
2:
	/* Preserve registers */
	pushw	%ds
	pushl	%eax
	
	/* Run decompressor if necessary.  Decompressor is 32-bit
	 * code, so we must switch to pmode first.  Save and restore
	 * GDT over transition to pmode.
	 */
#ifdef VBOX
	movl	$GSYM(decompress), %eax
#else /* !VBOX */
	movl	$decompress, %eax
#endif /* !VBOX */
	testl	%eax, %eax
	jz	1f
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	subw	$8, %sp
	pushw	%bp
	movw	%sp, %bp
	sgdt	2(%bp)
	pushw	%ss			/* Store params for _prot_to_real */
	pushw	%cs
#ifdef VBOX
	HACK_DATA32 call	GSYM(_prefix_real_to_prot)
#else /* !VBOX */
	call	_prefix_real_to_prot
#endif /* !VBOX */
	.code32
#ifdef VBOX
	call	GSYM(decompress)
	call	GSYM(_prefix_prot_to_real)
#else /* !VBOX */
	call	decompress
	call	_prefix_prot_to_real
#endif /* !VBOX */
	.code16
	popw	%ax			/* skip */
	popw	%ax			/* skip */
	lgdt	2(%bp)
	popw	%bp
	addw	$8, %sp
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
1:

	/* Set rm_etherboot_location */
	xorl	%eax, %eax
	movw	%cs, %ax
	movw	%ax, %ds
	shll	$4, %eax
#ifdef VBOX
	addl	$GSYM(_prefix_size), %eax
	HACK_ADDR32 movl	%eax, GSYM(_prefix_rm_etherboot_location)
#else /* !VBOX */
	addl	$_prefix_size, %eax
	movl	%eax, _prefix_rm_etherboot_location
#endif /* !VBOX */

	/* Restore registers */
	popl	%eax
	popw	%ds

	/* Make real-mode in_call to Etherboot */
#ifdef VBOX
	HACK_DATA32 jmp	GSYM(_prefix_rm_in_call)
#else /* !VBOX */
	jmp	_prefix_rm_in_call
#endif /* !VBOX */
#endif /* CODE16 */

/******************************************************************************
 * Utility routines that can be called by the "prefix".
 ******************************************************************************
 */

#ifdef CODE16

/* Prelocate code: either to an area at the top of free base memory.
 * Switch stacks to use the stack within the resulting
 * Etherboot image.
 *
 * On entry, %cs:0000 must be the start of the prefix: this is used to
 * locate the code to be copied.
 *
 * This routine takes a single word parameter: the number of bytes to
 * be transferred from the old stack to the new stack (excluding the
 * return address and this parameter itself, which will always be
 * copied).  If this value is negative, the stacks will not be
 * switched.
 *
 * Control will "return" to the appropriate point in the relocated
 * image.
 */

#define PRELOC_PRESERVE ( 20 )
#define PRELOC_OFFSET_RETADDR ( PRELOC_PRESERVE )
#define PRELOC_OFFSET_RETADDR_E ( PRELOC_OFFSET_RETADDR + 4 )
#define PRELOC_OFFSET_COPY ( PRELOC_OFFSET_RETADDR_E )
#define PRELOC_OFFSET_COPY_E ( PRELOC_OFFSET_COPY + 2 )

#define PRELOC_ALWAYS_COPY ( PRELOC_OFFSET_COPY_E )
	
	.code16
#ifdef VBOX
	.globl	GSYM(prelocate)
GSYM(prelocate):
#else /* !VBOX */
	.globl	prelocate
prelocate:
#endif /* !VBOX */
	/* Pad to allow for expansion of return address */
	pushw	%ax
	
	/* Preserve registers */
	pushaw
	pushw	%ds
	pushw	%es
	
	/* Claim an area of base memory from the BIOS and put the
	 * payload there.
	 */
	movw	$0x40, %bx
	movw	%bx, %es
	movw	%es:(0x13), %bx		/* FBMS in kb to %ax */
	shlw	$6, %bx			/* ... in paragraphs */
#ifdef VBOX
	subw	$GSYM(_image_size_pgh), %bx	/* Subtract space for image */
#else /* !VBOX */
	subw	$_image_size_pgh, %bx	/* Subtract space for image */
#endif /* !VBOX */
	shrw	$6, %bx			/* Round down to nearest kb */
	movw	%bx, %es:(0x13)		/* ...and claim memory from BIOS */
	shlw	$6, %bx

	/* At this point %bx contains the segment address for the
	 * start of the image (image = prefix + runtime).
	 */

	/* Switch stacks */
	movw	%ss, %ax
	movw	%ax, %ds
	movw	%sp, %si		/* %ds:si = current %ss:sp */
	movw	%ss:PRELOC_OFFSET_COPY(%si), %cx
	testw	%cx, %cx
	js	1f
#ifdef VBOX
	leaw	GSYM(_stack_offset_pgh)(%bx), %ax /* %ax = new %ss */
#else /* !VBOX */
	leaw	_stack_offset_pgh(%bx), %ax /* %ax = new %ss */
#endif /* !VBOX */
	movw	%ax, %es
#ifdef VBOX
	movw	$GSYM(_stack_size), %di
#else /* !VBOX */
	movw	$_stack_size, %di
#endif /* !VBOX */
	addw	$PRELOC_ALWAYS_COPY, %cx
	subw	%cx, %di		/* %es:di = new %ss:sp */
	movw	%ax, %ss		/* Set new %ss:sp */
	movw	%di, %sp
	cld
	rep movsb			/* Copy stack contents */
1:	

	/* Do the image copy backwards, since if there's overlap with
	 * a forward copy then it means we're going to get trashed
	 * during the copy anyway...
	 */
	pushal				/* Preserve 32-bit registers */
	movw	%bx, %es		/* Destination base for copy */
	pushw	%cs
	popw	%ds			/* Source base for copy */
#ifdef VBOX
	movl	$GSYM(_verbatim_size)-1, %ecx	/* Offset to last byte */
#else /* !VBOX */
	movl	$_verbatim_size-1, %ecx	/* Offset to last byte */
#endif /* !VBOX */
	movl	%ecx, %esi
	movl	%ecx, %edi
	incl	%ecx			/* Length */
	std				/* Backwards copy of binary */
	ADDR32 rep movsb
	cld
	popal				/* Restore 32-bit registers */

	/* Store (%bx<<4) as image_basemem to be picked up by
	 * basemem.c.  Also store image_size, since there's no other
	 * way that we can later know how much memory we allocated.
	 * (_zfile_size is unavailable when rt2 is linked).
	 */
	pushl	%eax
	xorl	%eax, %eax
	movw	%bx, %ax
	shll	$4, %eax
#ifdef VBOX
	HACK_ADDR32 movl	%eax, %es:GSYM(_prefix_image_basemem)
	HACK_ADDR32 movl	$GSYM(_image_size), %es:GSYM(_prefix_image_basemem_size)
#else /* !VBOX */
	movl	%eax, %es:_prefix_image_basemem
	movl	$_image_size, %es:_prefix_image_basemem_size
#endif /* !VBOX */
	popl	%eax

	/* Expand original near return address into far return to new
	 * code location.
	 */
	movw	%sp, %bp
	xchgw	%bx, (PRELOC_OFFSET_RETADDR+2)(%bp)
	movw	%bx, (PRELOC_OFFSET_RETADDR+0)(%bp)

	/* Restore registers and return */
	popw	%es
	popw	%ds
	popaw
	lret				/* Jump to relocated code */

	/* Utility routine to free base memory allocated by prelocate.
	 * Ensure that said memory is not in use (e.g. for the CPU
	 * stack) before calling this routine.
	 */
#ifdef VBOX
	.globl GSYM(deprelocate)
GSYM(deprelocate):
#else /* !VBOX */
	.globl deprelocate
deprelocate:	
#endif /* !VBOX */
	/* Claim an area of base memory from the BIOS and put the
	 * payload there.
	 */
	pushw	%ax
	pushw	%es
	movw	$0x40, %ax
	movw	%ax, %es
	movw	%es:(0x13), %ax		/* FBMS in kb to %ax */
	shlw	$6, %ax			/* ... in paragraphs */
#ifdef VBOX
	addw	$GSYM(_image_size_pgh)+0x40-1, %ax /* Add space for image and... */
#else /* !VBOX */
	addw	$_image_size_pgh+0x40-1, %ax /* Add space for image and... */
#endif /* !VBOX */
	shrw	$6, %ax			/* ...round up to nearest kb */
	movw	%ax, %es:(0x13)		/* Give memory back to BIOS */
	popw	%es
	popw	%ax
	ret
	
#endif /* CODE16 */
