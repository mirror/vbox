#ifdef VBOX
#include "etherboot-asm.h"
#endif /* VBOX */

/* #defines because ljmp wants a number, probably gas bug */
#ifdef VBOX
/*	.equ	KERN_CODE_SEG,GSYM(_pmcs)-GSYM(_gdt)	*/
#else /* !VBOX */
/*	.equ	KERN_CODE_SEG,_pmcs-_gdt	*/
#endif /* !VBOX */
#define	KERN_CODE_SEG	0x08
#ifdef VBOX
	.equ	KERN_DATA_SEG,_pmds-GSYM(_gdt)
/*	.equ	REAL_CODE_SEG,_rmcs-GSYM(_gdt)	*/
#else /* !VBOX */
	.equ	KERN_DATA_SEG,_pmds-_gdt
/*	.equ	REAL_CODE_SEG,_rmcs-_gdt	*/
#endif /* !VBOX */
#define	REAL_CODE_SEG	0x18
#ifdef VBOX
	.equ	REAL_DATA_SEG,_rmds-GSYM(_gdt)
	.equ	FLAT_CODE_SEG,_pmcs2-GSYM(_gdt)
	.equ	FLAT_DATA_SEG,_pmds2-GSYM(_gdt)
#else /* !VBOX */
	.equ	REAL_DATA_SEG,_rmds-_gdt
	.equ	FLAT_CODE_SEG,_pmcs2-_gdt
	.equ	FLAT_DATA_SEG,_pmds2-_gdt
#endif /* !VBOX */
	.equ	CR0_PE,1
#ifdef CONFIG_X86_64
#ifdef VBOX
	.equ	LM_CODE_SEG,  _lmcs-GSYM(_gdt)
	.equ	LM_DATA_SEG,  _lmds-GSYM(_gdt)
#else /* !VBOX */
	.equ	LM_CODE_SEG,  _lmcs-_gdt
	.equ	LM_DATA_SEG,  _lmds-_gdt
#endif /* !VBOX */
#endif

	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

#ifdef	GAS291
#define DATA32 data32;
#define ADDR32 addr32;
#define	LJMPI(x)	ljmp	x
#else
#define DATA32 data32
#define ADDR32 addr32
/* newer GAS295 require #define	LJMPI(x)	ljmp	*x */
#define	LJMPI(x)	ljmp	x
#endif

#define BOCHSBP xchgw %bx, %bx

#include "callbacks.h"
#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

/*
 * NOTE: if you write a subroutine that is called from C code (gcc/egcs),
 * then you only have to take care of %ebx, %esi, %edi and %ebp.  These
 * registers must not be altered under any circumstance.  All other registers
 * may be clobbered without any negative side effects.  If you don't follow
 * this rule then you'll run into strange effects that only occur on some
 * gcc versions (because the register allocator may use different registers).
 *
 * All the data32 prefixes for the ljmp instructions are necessary, because
 * the assembler emits code with a relocation address of 0.  This means that
 * all destinations are initially negative, which the assembler doesn't grok,
 * because for some reason negative numbers don't fit into 16 bits. The addr32
 * prefixes are there for the same reasons, because otherwise the memory
 * references are only 16 bit wide.  Theoretically they are all superfluous.
 * One last note about prefixes: the data32 prefixes on all call _real_to_prot
 * instructions could be removed if the _real_to_prot function is changed to
 * deal correctly with 16 bit return addresses.  I tried it, but failed.
 */

/**************************************************************************
 * START
 *
 * This file is no longer enterered from the top.  init.S will jump to
 * either _in_call or _rm_in_call, depending on the processor mode
 * when init.S was entered.
 **************************************************************************/
	.text
	.arch i386
	.code32

/**************************************************************************
_IN_CALL - make a call in to Etherboot.
**************************************************************************/

/* There are two 32-bit entry points: _in_call and _in_call_far, for
 * near calls and far calls respectively.  Both should be called with
 * flat physical addresses.  They will result in a call to the C
 * routine in_call(); see there for API details.
 *
 * Note that this routine makes fairly heavy use of the stack and no
 * use of fixed data areas.  This is because it must be re-entrant;
 * there may be more than one concurrent call in to Etherboot.
 */

#define IC_OFFSET_VA_LIST_PTR ( 0 )
#define IC_OFFSET_VA_LIST_PTR_E ( IC_OFFSET_VA_LIST_PTR + 4 )
#define IC_OFFSET_REGISTERS ( IC_OFFSET_VA_LIST_PTR_E )
#define IC_OFFSET_REGISTERS_E ( IC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define IC_OFFSET_SEG_REGS ( IC_OFFSET_REGISTERS_E )
#define IC_OFFSET_SEG_REGS_E ( IC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define IC_OFFSET_GDT ( IC_OFFSET_SEG_REGS_E )
#define IC_OFFSET_GDT_E ( IC_OFFSET_GDT + 8 )
#define IC_OFFSET_FLAGS ( IC_OFFSET_GDT_E )
#define IC_OFFSET_FLAGS_E ( IC_OFFSET_FLAGS + 4 )
#define IC_OFFSET_RETADDR ( IC_OFFSET_FLAGS_E )
#define IC_OFFSET_RETADDR_E ( IC_OFFSET_RETADDR + 8 )
#define IC_OFFSET_ORIG_STACK ( IC_OFFSET_RETADDR )
#define IC_OFFSET_OPCODE ( IC_OFFSET_ORIG_STACK + 8 )
#define IC_OFFSET_OPCODE_E ( IC_OFFSET_OPCODE + 4 )
#define IC_OFFSET_VA_LIST ( IC_OFFSET_OPCODE_E )

	.code32
#ifdef VBOX
	.globl GSYM(_in_call)
	.globl GSYM(_in_call_far)
GSYM(_in_call):
#else /* !VBOX */
	.globl _in_call
	.globl _in_call_far
_in_call:
#endif /* !VBOX */
	/* Expand to far return address */
	pushl	%eax			/* Store %eax */
	xorl	%eax, %eax
	movw	%cs, %ax
	xchgl	%eax, 4(%esp)		/* 4(%esp) = %cs, %eax = ret addr */
	xchgl	%eax, 0(%esp)		/* 0(%esp) = ret addr, restore %eax */
#ifdef VBOX
GSYM(_in_call_far):
#else /* !VBOX */
_in_call_far:
#endif /* !VBOX */
	/* Store flags */
	pushfl
	/* Store the GDT */
	subl	$8, %esp
	sgdt	0(%esp)
	/* Store segment register values */
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs
	/* Store general-purpose register values */
	pushal
	/* Replace %esp in store with physical %esp value on entry */
	leal	(IC_OFFSET_ORIG_STACK - IC_OFFSET_REGISTERS)(%esp), %eax
	movl	%eax, (IC_OFFSET_REGISTERS - IC_OFFSET_REGISTERS + 12)(%esp)
	/* Store va_list pointer (physical address) */
	leal	(IC_OFFSET_VA_LIST - IC_OFFSET_VA_LIST_PTR_E)(%esp), %eax
	pushl	%eax
	/* IC_OFFSET_*(%esp) are now valid */

	/* Determine the offset between physical and virtual address */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp
#ifdef VBOX
	movl	%ebp, GSYM(virt_offset)(%ebp)
#else /* !VBOX */
	movl	%ebp, virt_offset(%ebp)
#endif /* !VBOX */

	/* Check opcode for EB_USE_INTERNAL_STACK flag */
	movl	IC_OFFSET_OPCODE(%esp), %eax
	testl	$EB_USE_INTERNAL_STACK, %eax
	je	2f
	/* Use internal stack flag set */
	/* Check %esp is not already in internal stack range */
#ifdef VBOX
	leal	GSYM(_stack)(%ebp), %esi	/* %esi = bottom of internal stack */
	leal	GSYM(_estack)(%ebp), %edi	/* %edi = top of internal stack */
#else /* !VBOX */
	leal	_stack(%ebp), %esi	/* %esi = bottom of internal stack */
	leal	_estack(%ebp), %edi	/* %edi = top of internal stack */
#endif /* !VBOX */
	cmpl	%esi, %esp
	jb	1f
	cmpl	%edi, %esp
	jbe	2f
1:	/* %esp not currently in internal stack range */
	movl	%esp, %esi		/* %esi = original stack */
	movl	$IC_OFFSET_OPCODE_E, %ecx /* %ecx = length to transfer */
	subl	%ecx, %edi		/* %edi = internal stack pos */
	movl	%edi, %esp		/*  = new %esp */
	rep movsb			/* Copy data to internal stack */
2:

	/* Switch to virtual addresses */
#ifdef VBOX
	call	GSYM(_phys_to_virt)
#else /* !VBOX */
	call	_phys_to_virt
#endif /* !VBOX */

	/* Fixup the va_list pointer */
#ifdef VBOX
	movl	GSYM(virt_offset), %ebp
#else /* !VBOX */
	movl	virt_offset, %ebp
#endif /* !VBOX */
	subl	%ebp, IC_OFFSET_VA_LIST_PTR(%esp)

	/* Call to C code */
#ifdef VBOX
	call	GSYM(i386_in_call)
#else /* !VBOX */
	call	i386_in_call
#endif /* !VBOX */

	/* Set %eax (return code from C) in registers structure on
	 * stack, so that we return it to the caller.
	 */
	movl	%eax, (IC_OFFSET_REGISTERS + 28)(%esp)

	/* Calculate physical continuation address */
#ifdef VBOX
	movl	GSYM(virt_offset), %ebp
#else /* !VBOX */
	movl	virt_offset, %ebp
#endif /* !VBOX */
	movzwl	(IC_OFFSET_SEG_REGS + 0)(%esp), %eax	/* %cs */
	movzwl	(IC_OFFSET_SEG_REGS + 2)(%esp), %ebx	/* %ss */
	pushl	%eax			/* Continuation segment */
	leal	1f(%ebp), %eax
	pushl	%eax			/* Continuation offset */

	/* Restore caller's GDT */
	cli				/* Temporarily disable interrupts */
	lgdt	(8+IC_OFFSET_GDT)(%esp)
	/* Reset %ss and adjust %esp */
	movw	%bx, %ss
	addl	%ebp, %esp
	lret				/* Reload %cs:eip, flush prefetch */
1:

	/* Skip va_list ptr */
	popl	%eax
	/* Reload general-purpose registers to be returned */
	popal
	/* Reload segment registers as passed in from caller */
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	addl	$(4+8), %esp	/* Skip %cs, %ss and GDT (already reloaded) */
	/* Restore flags (including revert of interrupt status) */
	popfl

	/* Restore physical %esp from entry.  It will only be
	 * different if EB_USE_INTERNAL_STACK was specified.
	 */
	movl	( 12 + IC_OFFSET_REGISTERS - IC_OFFSET_RETADDR )(%esp), %esp

	/* Check for EB_SKIP_OPCODE */
	pushfl
	testl	$EB_SKIP_OPCODE, 12(%esp)
	jnz	1f
	/* Normal return */
	popfl
	lret
1:	/* Return and skip opcode */
	popfl
	lret	$4

/**************************************************************************
RELOCATE_TO - relocate etherboot to the specified address
**************************************************************************/
#ifdef VBOX
	.globl GSYM(relocate_to)
GSYM(relocate_to):
#else /* !VBOX */
	.globl relocate_to
relocate_to:
#endif /* !VBOX */
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi

	/* Compute the virtual destination address */
	movl	16(%esp), %edi	# dest
#ifdef VBOX
	subl	GSYM(virt_offset), %edi
#else /* !VBOX */
	subl	virt_offset, %edi
#endif /* !VBOX */


	/* Compute the new value of virt_offset */
	movl	16(%esp), %ebp	# virt_offset
#ifdef VBOX
	subl	$GSYM(_text), %ebp
#else /* !VBOX */
	subl	$_text, %ebp
#endif /* !VBOX */

	/* Fixup the gdt */
#ifdef VBOX
	pushl	$GSYM(_pmcs)
#else /* !VBOX */
	pushl	$_pmcs
#endif /* !VBOX */
	pushl	%ebp		# virt_offset
#ifdef VBOX
	call	GSYM(set_seg_base)
#else /* !VBOX */
	call	set_seg_base
#endif /* !VBOX */
	addl	$8, %esp

	/* Fixup gdtarg */
#ifdef VBOX
	leal	GSYM(_gdt)(%ebp), %eax
	movl	%eax, GSYM(gdtarg) +2
#else /* !VBOX */
	leal	_gdt(%ebp), %eax
	movl	%eax, gdtarg +2
#endif /* !VBOX */

	/* Fixup virt_offset */
#ifdef VBOX
	movl	%ebp, GSYM(virt_offset)
#else /* !VBOX */
	movl	%ebp, virt_offset
#endif /* !VBOX */

	/* Load the move parameters */
#ifdef VBOX
	movl	$GSYM(_text), %esi
	movl	$GSYM(_end), %ecx
#else /* !VBOX */
	movl	$_text, %esi
	movl	$_end, %ecx
#endif /* !VBOX */
	subl	%esi, %ecx

	/* Move etherboot uses %esi, %edi, %ecx */
	rep
	movsb

	/* Reload the gdt */
	cs
#ifdef VBOX
	lgdt	GSYM(gdtarg)
#else /* !VBOX */
	lgdt	gdtarg
#endif /* !VBOX */

	/* Reload %cs */
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	/* Restore the callee save registers */
	popl	%edi
	popl	%esi
	popl	%ebp

	/* return */
	ret

/**************************************************************************
XSTART32 - Transfer control to the kernel just loaded
**************************************************************************/
#ifdef VBOX
	.globl GSYM(xstart32)
GSYM(xstart32):
#else /* !VBOX */
	.globl xstart32
xstart32:
#endif /* !VBOX */
	/* Save the callee save registers */
#ifdef VBOX
	movl	%ebp, GSYM(os_regs) + 32
	movl	%esi, GSYM(os_regs) + 36
	movl	%edi, GSYM(os_regs) + 40
	movl	%ebx, GSYM(os_regs) + 44
#else /* !VBOX */
	movl	%ebp, os_regs + 32
	movl	%esi, os_regs + 36
	movl	%edi, os_regs + 40
	movl	%ebx, os_regs + 44
#endif /* !VBOX */

	/* save the return address */
	popl	%eax
#ifdef VBOX
	movl	%eax, GSYM(os_regs) + 48
#else /* !VBOX */
	movl	%eax, os_regs + 48
#endif /* !VBOX */

	/* save the stack pointer */
#ifdef VBOX
	movl	%esp, GSYM(os_regs) + 52
#else /* !VBOX */
	movl	%esp, os_regs + 52
#endif /* !VBOX */

	/* Get the new destination address */
	popl	%ecx

	/* Store the physical address of xend on the stack */
#ifdef VBOX
	movl	$GSYM(xend32), %ebx
	addl	GSYM(virt_offset), %ebx
#else /* !VBOX */
	movl	$xend32, %ebx
	addl	virt_offset, %ebx
#endif /* !VBOX */
	pushl	%ebx

	/* Store the destination address on the stack */
	pushl	$FLAT_CODE_SEG
	pushl	%ecx

	/* Cache virt_offset */
#ifdef VBOX
	movl	GSYM(virt_offset), %ebp
#else /* !VBOX */
	movl	virt_offset, %ebp
#endif /* !VBOX */

	/* Switch to using physical addresses */
#ifdef VBOX
	call	GSYM(_virt_to_phys)
#else /* !VBOX */
	call	_virt_to_phys
#endif /* !VBOX */

	/* Save the target stack pointer */
#ifdef VBOX
	movl	%esp, GSYM(os_regs) + 12(%ebp)
	leal	GSYM(os_regs)(%ebp), %esp
#else /* !VBOX */
	movl	%esp, os_regs + 12(%ebp)
	leal	os_regs(%ebp), %esp
#endif /* !VBOX */

	/* Store the pointer to os_regs */
	movl	%esp, os_regs_ptr(%ebp)

	/* Load my new registers */
	popal
	movl	(-32 + 12)(%esp), %esp

	/* Jump to the new kernel
	 * The lret switches to a flat code segment
	 */
	lret

	.balign 4
#ifdef VBOX
	.globl GSYM(xend32)
GSYM(xend32):
#else /* !VBOX */
	.globl xend32
xend32:
#endif /* !VBOX */
	/* Fixup %eflags */
	nop
	cli
	cld

	/* Load %esp with &os_regs + virt_offset */
	.byte	0xbc /* movl $0, %esp */
os_regs_ptr:
	.long	0

	/* Save the result registers */
	addl	$32, %esp
	pushal

	/* Compute virt_offset */
	movl	%esp, %ebp
#ifdef VBOX
	subl	$GSYM(os_regs), %ebp
#else /* !VBOX */
	subl	$os_regs, %ebp
#endif /* !VBOX */

	/* Load the stack pointer and convert it to physical address */
	movl	52(%esp), %esp
	addl	%ebp, %esp

	/* Enable the virtual addresses */
#ifdef VBOX
	leal	GSYM(_phys_to_virt)(%ebp), %eax
#else /* !VBOX */
	leal	_phys_to_virt(%ebp), %eax
#endif /* !VBOX */
	call	*%eax

	/* Restore the callee save registers */
#ifdef VBOX
	movl	GSYM(os_regs) + 32, %ebp
	movl	GSYM(os_regs) + 36, %esi
	movl	GSYM(os_regs) + 40, %edi
	movl	GSYM(os_regs) + 44, %ebx
	movl	GSYM(os_regs) + 48, %edx
	movl	GSYM(os_regs) + 52, %esp
#else /* !VBOX */
	movl	os_regs + 32, %ebp
	movl	os_regs + 36, %esi
	movl	os_regs + 40, %edi
	movl	os_regs + 44, %ebx
	movl	os_regs + 48, %edx
	movl	os_regs + 52, %esp
#endif /* !VBOX */

	/* Get the C return value */
#ifdef VBOX
	movl	GSYM(os_regs) + 28, %eax
#else /* !VBOX */
	movl	os_regs + 28, %eax
#endif /* !VBOX */

	jmpl	*%edx

#ifdef CONFIG_X86_64
	.arch	sledgehammer
/**************************************************************************
XSTART_lm - Transfer control to the kernel just loaded in long mode
**************************************************************************/
#ifdef VBOX
	.globl GSYM(xstart_lm)
GSYM(xstart_lm):
#else /* !VBOX */
	.globl xstart_lm
xstart_lm:
#endif /* !VBOX */
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	/* Cache virt_offset && (virt_offset & 0xfffff000) */
#ifdef VBOX
	movl	GSYM(virt_offset), %ebp
#else /* !VBOX */
	movl	virt_offset, %ebp
#endif /* !VBOX */
	movl	%ebp, %ebx
	andl	$0xfffff000, %ebx

	/* Switch to using physical addresses */
#ifdef VBOX
	call	GSYM(_virt_to_phys)
#else /* !VBOX */
	call	_virt_to_phys
#endif /* !VBOX */

	/* Initialize the page tables */
	/* Level 4 */
	leal	0x23 + pgt_level3(%ebx), %eax
	leal	pgt_level4(%ebx), %edi
	movl	%eax, (%edi)

	/* Level 3 */
	leal	0x23 + pgt_level2(%ebx), %eax
	leal	pgt_level3(%ebx), %edi
	movl	%eax, 0x00(%edi)
	addl	$4096, %eax
	movl	%eax, 0x08(%edi)
	addl	$4096, %eax
	movl	%eax, 0x10(%edi)
	addl	$4096, %eax
	movl	%eax, 0x18(%edi)

	/* Level 2 */
	movl	$0xe3, %eax
	leal	pgt_level2(%ebx), %edi
	leal	16384(%edi), %esi
pgt_level2_loop:
	movl	%eax, (%edi)
	addl	$8, %edi
	addl	$0x200000, %eax
	cmp	%esi, %edi
	jne	pgt_level2_loop

	/* Point at the x86_64 page tables */
	leal	pgt_level4(%ebx), %edi
	movl	%edi, %cr3


	/* Setup for the return from 64bit mode */
	/* 64bit align the stack */
	movl	%esp, %ebx		/* original stack pointer + 16 */
	andl	$0xfffffff8, %esp

	/* Save original stack pointer + 16 */
	pushl	%ebx

	/* Save virt_offset */
	pushl	%ebp

	/* Setup for the jmp to 64bit long mode */
	leal	start_lm(%ebp), %eax
	movl	%eax, 0x00 + start_lm_addr(%ebp)
	movl	$LM_CODE_SEG, %eax
	movl	%eax, 0x04 + start_lm_addr(%ebp)

	/* Setup for the jump out of 64bit long mode */
	leal	end_lm(%ebp), %eax
	movl	%eax, 0x00 + end_lm_addr(%ebp)
	movl	$FLAT_CODE_SEG, %eax
	movl	%eax, 0x04 + end_lm_addr(%ebp)

	/* Enable PAE mode */
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Enable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Start paging, entering 32bit compatiblity mode */
	movl	%cr0, %eax
	orl	$CR0_PG, %eax
	movl	%eax, %cr0

	/* Enter 64bit long mode */
	ljmp	*start_lm_addr(%ebp)
	.code64
start_lm:
	/* Load 64bit data segments */
	movl	$LM_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss

	andq	$0xffffffff, %rbx
	/* Get the address to jump to */
	movl	20(%rbx), %edx
	andq	$0xffffffff, %rdx

	/* Get the argument pointer */
	movl	24(%rbx), %ebx
	andq	$0xffffffff, %rbx

	/* Jump to the 64bit code */
	call	*%rdx

	/* Preserve the result */
	movl	%eax, %edx

	/* Fixup %eflags */
	cli
	cld

	/* Switch to 32bit compatibility mode */
	ljmp	*end_lm_addr(%rip)

	.code32
end_lm:
	/* Disable paging */
	movl	%cr0, %eax
	andl	$~CR0_PG, %eax
	movl	%eax, %cr0

	/* Disable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	andl	$~EFER_LME, %eax
	wrmsr

	/* Disable PAE */
	movl	%cr4, %eax
	andl	$~X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Compute virt_offset */
	popl	%ebp

	/* Compute the original stack pointer + 16 */
	popl	%ebx
	movl	%ebx, %esp

	/* Enable the virtual addresses */
#ifdef VBOX
	leal	GSYM(_phys_to_virt)(%ebp), %eax
#else /* !VBOX */
	leal	_phys_to_virt(%ebp), %eax
#endif /* !VBOX */
	call	*%eax

	/* Restore the callee save registers */
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp

	/* Get the C return value */
	movl	%edx, %eax

	/* Return */
	ret

	.arch i386
#endif /* CONFIG_X86_64 */

/**************************************************************************
SETJMP - Save stack context for non-local goto
**************************************************************************/
#ifdef VBOX
	.globl	GSYM(setjmp)
GSYM(setjmp):
#else /* !VBOX */
	.globl	setjmp
setjmp:
#endif /* !VBOX */
	movl	4(%esp),%ecx		/* jmpbuf */
	movl	0(%esp),%edx		/* return address */
	movl	%edx,0(%ecx)
	movl	%ebx,4(%ecx)
	movl	%esp,8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	$0,%eax
	ret

/**************************************************************************
LONGJMP - Non-local jump to a saved stack context
**************************************************************************/
#ifdef VBOX
	.globl	GSYM(longjmp)
GSYM(longjmp):
#else /* !VBOX */
	.globl	longjmp
longjmp:
#endif /* !VBOX */
	movl	4(%esp),%edx		/* jumpbuf */
	movl	8(%esp),%eax		/* result */
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	cmpl	$0,%eax
	jne	1f
	movl	$1,%eax
1:	movl	%ecx,0(%esp)
	ret

/**************************************************************************
_VIRT_TO_PHYS - Transition from virtual to physical addresses
		Preserves all preservable registers and flags
**************************************************************************/
#ifdef VBOX
	.globl GSYM(_virt_to_phys)
GSYM(_virt_to_phys):
#else /* !VBOX */
	.globl _virt_to_phys
_virt_to_phys:
#endif /* !VBOX */
	pushfl
	pushl	%ebp
	pushl	%eax
#ifdef VBOX
	movl	GSYM(virt_offset), %ebp	/* Load virt_offset */
#else /* !VBOX */
	movl	virt_offset, %ebp	/* Load virt_offset */
#endif /* !VBOX */
	addl	%ebp, 12(%esp)		/* Adjust the return address */

	/* reload the code segment */
	pushl	$FLAT_CODE_SEG
	leal	1f(%ebp), %eax
	pushl	%eax
	lret

1:
	/* reload other segment registers */
	movl	$FLAT_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	addl	%ebp, %esp		/* Adjust the stack pointer */
	movl	%eax, %fs
	movl	%eax, %gs

	popl	%eax
	popl	%ebp
	popfl
	ret

/**************************************************************************
_PHYS_TO_VIRT - Transition from using physical to virtual addresses
		Preserves all preservable registers and flags
**************************************************************************/
#ifdef VBOX
	.globl GSYM(_phys_to_virt)
GSYM(_phys_to_virt):
#else /* !VBOX */
	.globl _phys_to_virt
_phys_to_virt:
#endif /* !VBOX */
	pushfl
	pushl	%ebp
	pushl	%eax

	call	1f
1:	popl	%ebp
	subl	$1b, %ebp

	/* Fixup the gdt */
#ifdef VBOX
	leal	GSYM(_pmcs)(%ebp), %eax
#else /* !VBOX */
	leal	_pmcs(%ebp), %eax
#endif /* !VBOX */
	pushl	%eax
	pushl	%ebp
#ifdef VBOX
	call	GSYM(set_seg_base)
#else /* !VBOX */
	call	set_seg_base
#endif /* !VBOX */
	addl	$8, %esp

	/* Fixup gdtarg */
#ifdef VBOX
	leal	GSYM(_gdt)(%ebp), %eax
	movl	%eax, (GSYM(gdtarg)+2)(%ebp)
#else /* !VBOX */
	leal	_gdt(%ebp), %eax
	movl	%eax, (gdtarg+2)(%ebp)
#endif /* !VBOX */

	/* Load the global descriptor table */
	cli
#ifdef VBOX
	lgdt	%cs:GSYM(gdtarg)(%ebp)
#else /* !VBOX */
	lgdt	%cs:gdtarg(%ebp)
#endif /* !VBOX */
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment regsters */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	subl	%ebp, %esp	/* Adjust the stack pointer */
	movl	%eax, %fs
	movl	%eax, %gs

	subl	%ebp, 12(%esp)	/* Adjust the return address */
	popl	%eax
	popl	%ebp
	popfl
	ret

/**************************************************************************
SET_SEG_BASE - Set the base address of a segment register
**************************************************************************/
#ifdef VBOX
	.globl GSYM(set_seg_base)
GSYM(set_seg_base):
#else /* !VBOX */
	.globl set_seg_base
set_seg_base:
#endif /* !VBOX */
	pushl	%eax
	pushl	%ebx
	movl	12(%esp), %eax		/* %eax = base address */
	movl	16(%esp), %ebx		/* %ebx = &code_descriptor */
	movw	%ax, (0+2)(%ebx)	/* CS base bits 0-15 */
	movw	%ax, (8+2)(%ebx)	/* DS base bits 0-15 */
	shrl	$16, %eax
	movb	%al, (0+4)(%ebx)	/* CS base bits 16-23 */
	movb	%al, (8+4)(%ebx)	/* DS base bits 16-23 */
	movb	%ah, (0+7)(%ebx)	/* CS base bits 24-31 */
	movb	%ah, (8+7)(%ebx)	/* DS base bits 24-31 */
	popl	%ebx
	popl	%eax
	ret

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.data
	.align	4

#ifdef VBOX
	.globl	GSYM(_gdt)
	.globl	GSYM(gdtarg)
GSYM(_gdt):
GSYM(gdtarg):
	.word	_gdt_end - GSYM(_gdt) - 1	/* limit */
	.long	GSYM(_gdt)			/* addr */
#else /* !VBOX */
	.globl	_gdt
	.globl	gdtarg
_gdt:
gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
	.long	_gdt			/* addr */
#endif /* !VBOX */
	.word	0

#ifdef VBOX
	.globl	GSYM(_pmcs)
GSYM(_pmcs):
#else /* !VBOX */
	.globl	_pmcs
_pmcs:
#endif /* !VBOX */
	/* 32 bit protected mode code segment */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds:
	/* 32 bit protected mode data segment */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

_rmcs:
	/* 16 bit real mode code segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x9b,0x00,(0>>24)

_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x93,0x00,(0>>24)

_pmcs2:
	/* 32 bit protected mode code segment, base 0 */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds2:
	/* 32 bit protected mode data segment, base 0 */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

#ifdef CONFIG_X86_64
_lmcs:
	/* 64bit long mode code segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x9f, 0xaf , 0x00
_lmds:
	/* 64bit long mode data segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x93, 0xcf, 0x00
#endif
_gdt_end:

	/* The initial register contents */
	.balign 4
#ifdef VBOX
	.globl GSYM(initial_regs)
GSYM(initial_regs):
#else /* !VBOX */
	.globl initial_regs
initial_regs:
#endif /* !VBOX */
	.fill 8, 4, 0

	/* The virtual address offset  */
#ifdef VBOX
	.globl GSYM(virt_offset)
GSYM(virt_offset):
#else /* !VBOX */
	.globl virt_offset
virt_offset:
#endif /* !VBOX */
	.long	0

#ifdef VBOX
#ifndef __WIN32__
/* Setting up the stack doesn't work with the PE tools provided by mingw
 * (symptom: image is too large, contains 0-filled bss and stack, causing
 * the stack to be overwritten during decompression).  Set up the stack
 * in the linker script instead.  */
	.section .stack
        .p2align 3
        /* allocate a 4K stack in the stack segment */
	.globl	GSYM(_stack)
GSYM(_stack):
	.space 4096
	.globl	GSYM(_estack)
GSYM(_estack):
#endif /* __WIN32__ */
#else /* !VBOX */
	.section ".stack"
	.p2align 3
	/* allocate a 4K stack in the stack segment */
	.globl	_stack
_stack:
	.space 4096
	.globl	_estack
_estack:
#endif /* !VBOX */
#ifdef CONFIG_X86_64
#ifdef VBOX
	.section .bss
#else /* !VBOX */
	.section ".bss"
#endif /* !VBOX */
	.p2align 12
	/* Include a dummy space in case we are loaded badly aligned */
	.space 4096
	/* Reserve enough space for a page table convering 4GB with 2MB pages */
pgt_level4:
	.space 4096
pgt_level3:
	.space 4096
pgt_level2:
	.space 16384
start_lm_addr:
	.space	8
end_lm_addr:
	.space	8
#endif
