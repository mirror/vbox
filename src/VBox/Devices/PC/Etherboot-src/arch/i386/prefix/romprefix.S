#ifdef VBOX
#include "etherboot-asm.h"
#endif /* VBOX */

/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

/* Define DELAYED_INT when NO_DELAYED_INT is not defined.
 * This allows positive tests instead of tests that contain
 * double negatives, and become confusing.
 */
#ifndef NO_DELAYED_INT
#define DELAYED_INT
#endif

/* We need some unique magic ID, if we defer startup thru the INT18H or INT19H
 * handler. This way, we can check if we have already been installed.
 */
#ifndef MAGIC
#define MAGIC		0xE44C
#endif

/* Hook into INT18H or INT19H handler */
#ifdef	BOOT_INT18H
#define BOOT_INT	0x18
#else
#define BOOT_INT	0x19
#endif

#define BOOT_INT_VEC	BOOT_INT*4
#define SCRATCHVEC	0x300

/* Prefix exit codes.  We store these on the stack so that we will
 * know how to return control to the BIOS when Etherboot exits.
 */
#define EXIT_VIA_LRET		0x0
#define EXIT_VIA_INT_18		0x1
#define EXIT_VIA_BOOT_INT	0x2

	.text
	.code16
	.arch i386
	.org 0
#ifdef VBOX
#ifdef __WIN32__
	.section .prefix, "ax"
#else /* __WIN32__ */
	.section .prefix, "ax", @progbits
#endif /* __WIN32__ */
	.globl GSYM(_prefix)
GSYM(_prefix):
#else /* !VBOX */
	.section ".prefix", "ax", @progbits
	.globl _prefix
_prefix:
#endif /* !VBOX */
	.word 0xAA55			/* BIOS extension signature */
size:	.byte 0				/* number of 512 byte blocks */
					/* = number of 256 word blocks */
					/* filled in by makerom program */
	jmp	over			/* skip over checksum */
	.byte 0				/* checksum */
	jmp	legacyentry		/* alternate entry point +6 */
					/* used by mknbi-rom */

#ifdef	PCI_PNP_HEADER
mfgstr:
	.asciz	"Etherboot"

#ifdef	PXE_EXPORT
	.org	0x16
#ifdef VBOX
	.word	UNDIROMID - GSYM(_prefix)
#else /* !VBOX */
	.word	UNDIROMID - _prefix
#endif /* !VBOX */
#endif	/* PXE_EXPORT */

	.org	0x18
#ifdef VBOX
	.word	PCI - GSYM(_prefix)
	.word	PnP - GSYM(_prefix)
#else /* !VBOX */
	.word	PCI - _prefix
	.word	PnP - _prefix
#endif /* !VBOX */

PCI:
	.ascii	"PCIR"
	.word 0x0000			/* vendor ID, filled in by makerom */
	.word 0x0000			/* device ID, filled in by makerom */
	.word 0x0000			/* pointer to vital product data */
	.word 0x0018			/* PCI data structure length */
	.byte 0x00			/* PCI data structure revision */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.word 0x0000			/* Image length same as offset 02h */
	.word 0x0001			/* revision level of code/data */
	.byte 0x00			/* code type */
	.byte 0x80			/* indicator (last PCI data structure) */
	.word 0x0000			/* reserved */

PnP:
	.ascii	"$PnP"
	.byte 0x01			/* structure revision */
	.byte 0x02			/* length (in 16 byte increments) */
	.word 0x0000			/* offset of next header */
	.byte 0x00			/* Reserved */
	.byte 0x00			/* checksum filled by makerom */
	.long 0x00000000		/* Device identifier */
#ifdef VBOX
	.word mfgstr - GSYM(_prefix)
#else /* !VBOX */
	.word mfgstr - _prefix
#endif /* !VBOX */
	.word 0x0			/* pointer to product name */
					/* filled by makerom */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.byte 0x14			/* device indicator */
	.word 0x0000			/* boot connection vector */
	.word 0x0000			/* disconnect vector */
#ifdef VBOX
	.word pnpentry - GSYM(_prefix)
#else /* !VBOX */
	.word pnpentry - _prefix
#endif /* !VBOX */
	.word 0x0000			/* reserved */
	.word 0x0000			/* static resource information vector */
#ifdef	PXE_EXPORT
UNDIROMID:
	.ascii	"UNDI"
	.byte	UNDIROMID_end - UNDIROMID /* length of structure */
	.byte	0			/* Checksum */
	.byte	0			/* Structure revision */
	.byte	0,1,2			/* PXE version 2.1.0 */
#ifdef VBOX
	.word	UNDILoader - GSYM(_prefix)	/* Offset to loader routine */
#else /* !VBOX */
	.word	UNDILoader - _prefix	/* Offset to loader routine */
#endif /* !VBOX */
	.word	UNDIStackSize		/* Stack segment size */
	.word	UNDIDataSize		/* Data segment size */
	.word	UNDICodeSize		/* Code segment size */
	.ascii	"PCIR"

	/* The code segment contains our pxe_stack_t plus the PXE and
	 * RM callback interfaces.  We don't actually use a data
	 * segment, but we put a nonzero value here to avoid confusing
	 * things.  16k of stack space should be enough.
	 *
	 * When we claim our own memory, we fill out the data segment
	 * with the address and size of the real-mode stack, so that
	 * NBPs will free that area of memory for us.  When the UNDI
	 * loader is used to initialise us, we will never need a
	 * real-mode stack because we will only ever be called via the
	 * PXE API, hence our stack is already in base memory.
	 */
#ifdef VBOX
	.equ	UNDICodeSize, GSYM(_pxe_stack_size)
	.equ	UNDIDataSize, GSYM(_real_mode_stack_size)
	.equ	UNDIStackSize, GSYM(_real_mode_stack_size)
#else /* !VBOX */
	.globl	UNDICodeSize
	.equ	UNDICodeSize, _pxe_stack_size
	.globl	UNDIDataSize
	.equ	UNDIDataSize, _real_mode_stack_size
	.globl	UNDIStackSize
	.equ	UNDIStackSize, _real_mode_stack_size
#endif /* !VBOX */
UNDIROMID_end:
#endif	/* PXE_EXPORT */

#endif	/* PCI_PNP_HEADER */

/*
 *	Explicitly specify DI is wrt ES to avoid problems with some BIOSes
 *	Discovered by Eric Biederman
 *	In addition, some BIOSes don't point DI to the string $PnP so
 *	we need another #define to take care of that.
 */
over:
#ifdef	DEBUG_ROMPREFIX
	call	print_bcv
#endif
/* Omit this test for ISA cards anyway */
#ifdef	PCI_PNP_HEADER
/* Accept old name too for backward compatibility */
#if	!defined(BBS_BUT_NOT_PNP_COMPLIANT) && !defined(PNP_BUT_NOT_BBS_COMPLIANT)
	cmpw	$'$'+'P'*256,%es:0(%di)
	jne	notpnp
	cmpw	$'n'+'P'*256,%es:2(%di)
	jne	notpnp
#endif	/* BBS_BUT_NOT_PNP_COMPLIANT */
	movw	$0x20,%ax
	lret
#endif	/* PCI_PNP_HEADER */
notpnp:
#ifdef	DEBUG_ROMPREFIX
	call	print_notpnp
#endif
#ifdef	DELAYED_INT
	pushw	%ax
	pushw	%ds
	xorw	%ax,%ax
	movw	%ax,%ds			/* access first 64kB segment */
	movw	SCRATCHVEC+4, %ax	/* check if already installed */
	cmpw	$MAGIC, %ax		/* check magic word */
	jz	installed
	movw	BOOT_INT_VEC, %ax	/* hook into INT18H or INT19H */
	movw	%ax, SCRATCHVEC
	movw	BOOT_INT_VEC+2, %ax
	movw	%ax, SCRATCHVEC+2
#ifdef VBOX
	movw	$start_int - GSYM(_prefix), %ax
#else /* !VBOX */
	movw	$start_int - _prefix, %ax
#endif /* !VBOX */
	movw	%ax, BOOT_INT_VEC
	movw	%cs,%ax
	movw	%ax, BOOT_INT_VEC+2
	movw	$MAGIC, %ax		/* set magic word */
	movw	%ax, SCRATCHVEC+4
#ifdef	DEBUG_ROMPREFIX
	call	print_installed
#endif
installed:
	popw	%ds
	popw	%ax
	movw	$0x20,%ax
	lret

start_int:				/* clobber magic id, so that we will */
#ifdef	DEBUG_ROMPREFIX
	call	print_start_int
#endif
	xorw	%ax,%ax			/* not inadvertendly end up in an */
	movw	%ax,%ds			/* endless loop */
	movw	%ax, SCRATCHVEC+4
	movw	SCRATCHVEC+2, %ax	/* restore original INT19h handler */
	movw	%ax, BOOT_INT_VEC+2
	movw	SCRATCHVEC, %ax
	movw	%ax, BOOT_INT_VEC
	pushl	%eax			/* padding */
	pushw	$EXIT_VIA_BOOT_INT
	jmp	invoke
#endif	/* DELAYED_INT */




legacyentry:
#ifdef	DEBUG_ROMPREFIX
	call	print_legacyentry
#endif
	pushw	$EXIT_VIA_LRET
	jmp	invoke



#ifdef PCI_PNP_HEADER
pnpentry:
#ifdef	DEBUG_ROMPREFIX
	call	print_bev
#endif
	pushl	%eax			/* padding */
	pushw	$EXIT_VIA_INT_18
	jmp	invoke
#endif /* PCI_PNP_HEADER */


invoke:
	/* Remember original stack location for later */
	movw	%sp,%di
	pushw	%ss
	pushw	%di
	/* Store ROM segment and size on stack */
	pushw	%ax
	pushw	%ds
	pushw	%cs
#ifdef VBOX
	movzbw	%cs:(size-GSYM(_prefix)), %ax
#else /* !VBOX */
	movzbw	%cs:(size-_prefix), %ax
#endif /* !VBOX */
	shlw	$9, %ax			/* 512-byte blocks */
	pushw	%ax
	/* Relocate to free base memory, switch stacks */
	pushw	$18			/* Preserve ROM length and CS & original AX and DS & original stack location & exit code & far ret addr */
#ifdef VBOX
	call	GSYM(prelocate)
#else /* !VBOX */
	call	prelocate
#endif /* !VBOX */
	/* We are now running in RAM */
	popw	%ax			/* discard stack length */
	movw	%cs, %ax
	movw	%ax, %ds
#ifdef VBOX
	HACK_ADDR32 popw	%ds:(GSYM(_prefix_rom)+2)	/* ROM size */
	HACK_ADDR32 popw	%ds:(GSYM(_prefix_rom)+0)	/* ROM segment */
#else /* !VBOX */
	popw	%ds:(_prefix_rom+2)	/* ROM size */
	popw	%ds:(_prefix_rom+0)	/* ROM segment */
#endif /* !VBOX */
	popw	%ds			/* Original %ds */
	popw	%ax			/* Original %ax */
	pushw	%ax			/* 4-byte alignment */
	pushl	$12			/* Preserve original stack location & exit code & far ret addr */
	pushw	$0			/* Set null return address */
#ifdef VBOX
	jmp	GSYM(_start)
#else /* !VBOX */
	jmp	_start
#endif /* !VBOX */


#ifdef VBOX
#ifdef __WIN32__
	.section .text16, "ax"
#else /* __WIN32__ */
	.section .text16, "ax", @progbits
#endif /* __WIN32__ */
	.globl	GSYM(prefix_exit)
GSYM(prefix_exit):
#else /* !VBOX */
	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
#endif /* !VBOX */
	popw	%ax			/* padding */
	popw	%di			/* Load original SP into temp register */
	addw	$6,%di			/* Calculate SP value before call to Etherboot */
	popw	%si			/* Load original SS into temp register */
	popw	%ax			/* %ax = exit code */
	popw	%bx			/* Save potential return address across stack switch */
	popw	%cx
	movw	%si, %ss		/* Restore original SS:SP before call to Etherboot */
	movw	%di, %sp
	cmpw	$EXIT_VIA_LRET, %ax
	jne	1f
	pushw	%cx			/* Restore return address */
	pushw	%bx
	/* Exit via LRET */
	lret
1:	cmpw	$EXIT_VIA_BOOT_INT, %ax
	jne	2f
	/* Exit via int BOOT_INT */
	int	$BOOT_INT		/* Try original vector */
2:	/* Exit via int $0x18 */
	int	$0x18			/* As per BIOS Boot Spec, next dev */
#ifdef VBOX
	.globl	GSYM(prefix_exit_end)
GSYM(prefix_exit_end):
#else /* !VBOX */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous
#endif /* !VBOX */



#ifdef	PXE_EXPORT

#include "callbacks.h"
#define PXENV_UNDI_LOADER		0x104d

#ifdef VBOX
	.section .prefix
#else /* !VBOX */
	.section ".prefix"
#endif /* !VBOX */
UNDILoader:
	/* Loader API is different to the usual PXE API; there is no
	 * opcode on the stack.  We arrange the stack to look like a
	 * normal PXE API call; this makes the Etherboot internals
	 * cleaner and avoids adding an extra API type just for the
	 * PXE loader.
	 */
	pushw	%bx
	movw	%sp, %ax		/* Store original %ss:sp */
	pushw	%ss
	pushw	%ax
	pushl	%eax			/* Space for loader structure ptr */
	pushw	%bp
	movw	%sp, %bp
	movw	16(%bp), %ax		/* Copy loader structure ptr */
	movw	%ax, 2(%bp)
	movw	18(%bp), %ax
	movw	%ax, 4(%bp)
	popw	%bp
	pushw	$PXENV_UNDI_LOADER	/* PXE 'opcode' */
	pushl	%eax			/* dummy return address */
	/* Stack now looks like a normal PXE API call */
	/* Store ROM segment and size on stack */
	pushw	%ax
	pushw	%cs
#ifdef VBOX
	movzbw	%cs:(size-GSYM(_prefix)), %ax
#else /* !VBOX */
	movzbw	%cs:(size-_prefix), %ax
#endif /* !VBOX */
	shlw	$9, %ax			/* 512-byte blocks */
	pushw	%ax
	/* Unpack Etherboot into temporarily claimed base memory */
	pushw	$20			/* Dummy ret, PXE params, orig ss:sp */
#ifdef VBOX
	call	GSYM(prelocate)
#else /* !VBOX */
	call	prelocate
#endif /* !VBOX */
	popw	%ax			/* discard */
#ifdef VBOX
	HACK_ADDR32 popw	%cs:(GSYM(_prefix_rom)+2)	/* ROM size */
	HACK_ADDR32 popw	%cs:(GSYM(_prefix_rom)+0)	/* ROM segment */
#else /* !VBOX */
	popw	%cs:(_prefix_rom+2)	/* ROM size */
	popw	%cs:(_prefix_rom+0)	/* ROM segment */
#endif /* !VBOX */
	popw	%ax			/* Original %ax */
	/* Inhibit automatic deallocation of base memory */
#ifdef VBOX
	HACK_ADDR32 movl	$0, %cs:GSYM(_prefix_image_basemem)
#else /* !VBOX */
	movl	$0, %cs:_prefix_image_basemem
#endif /* !VBOX */
	/* Make PXE API call to Etherboot */
	pushl	$0x201			/* PXE API version */
	/* Need to USE_INTERNAL_STACK, since we will call relocate() */
	pushl	$(EB_OPCODE_PXE|EB_USE_INTERNAL_STACK) /* PXE API call type */
#ifdef VBOX
	call	GSYM(_entry)
#else /* !VBOX */
	call	_entry
#endif /* !VBOX */
	addw	$18, %sp		/* discard */
	popw	%bx			/* Restore original %ss:sp */
	popw	%ss
	movw	%bx, %sp
	popw	%bx
#ifdef VBOX
	call	GSYM(deprelocate)
#else /* !VBOX */
	call	deprelocate
#endif /* !VBOX */
	lret	$2			/* Skip our PXE 'opcode' */
#endif	/* PXE_EXPORT */

#ifdef DEBUG_ROMPREFIX
#ifdef VBOX
	.section .prefix
#else /* !VBOX */
	.section ".prefix"
#endif /* !VBOX */

print_bcv:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.asciz	"ROM detected\r\n"

print_bev:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.asciz	"booting\r\n"

print_notpnp:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.asciz	": Non-PnP BIOS detected!\r\n"

print_legacyentry:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.asciz	"ROM using legacy boot mechanism\r\n"

print_installed:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.ascii	"hooked boot via INT"
#ifdef	BOOT_INT18H
	.asciz	"18\r\n"
#else
	.asciz	"19\r\n"
#endif

print_start_int:
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_message
	popw	%si
	ret
1:	.asciz	"booting via hooked interrupt\r\n"

print_message:
	pushaw
	pushw	%ds
	pushw	%cs
	popw	%ds
	pushw	%si
#ifdef VBOX
	movw	$1f-GSYM(_prefix), %si
#else /* !VBOX */
	movw	$1f-_prefix, %si
#endif /* !VBOX */
	call	print_string
	popw	%si
	call	print_string
	popw	%ds
	popaw
	ret
1:	.asciz	"Etherboot "

print_string:
1:	lodsb
	testb	%al,%al
	je	2f
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	int	$0x10
	jmp	1b
2:	ret

#endif
