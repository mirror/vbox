/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/bus/Pci.h,v 1.21 2001/11/01 23:35:33 dawes Exp $ */
/*
 * Copyright 1998 by Concurrent Computer Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Concurrent Computer
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Concurrent Computer Corporation makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * CONCURRENT COMPUTER CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CONCURRENT COMPUTER CORPORATION BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Copyright 1998 by Metro Link Incorporated
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Metro Link
 * Incorporated not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Metro Link Incorporated makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * METRO LINK INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL METRO LINK INCORPORATED BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * This file is derived in part from the original xf86_PCI.h that included
 * following copyright message:
 *
 * Copyright 1995 by Robin Cutshaw <robin@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of the above listed copyright holder(s)
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.  The above listed
 * copyright holder(s) make(s) no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM(S) ALL WARRANTIES WITH REGARD 
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE 
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY 
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER 
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING 
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*
 * This file has the private Pci definitions.  The public ones are imported
 * from xf86Pci.h.  Drivers should not use this file.
 */
#ifndef _PCI_H
#define _PCI_H 1
#include "Xarch.h"
#include "Xfuncproto.h"
#include "xf86Pci.h"

/*
 * Global Definitions
 */
#define MAX_PCI_DEVICES 64	/* Max number of devices accomodated */
				/* by xf86scanpci		     */
#if defined(i386) || defined(__i386__) || defined(__i386)
/* Q&D stopgap to deal with mainboards whose PCI space is smaller */
#define MAX_PCI_BUSES   128	/* Max number of PCI buses           */
#else
#define MAX_PCI_BUSES   256	/* Max number of PCI buses           */
#endif

#define PCI_NOT_FOUND   0xffffffff

/*
 *   
 */
#define PCI_MAKE_TAG(b,d,f)  ((((b) & 0xff) << 16) | \
			      (((d) & 0x1f) << 11) | \
			      (((f) & 0x7) << 8))

#define PCI_BUS_FROM_TAG(tag)  (((tag) & 0x00ff0000) >> 16)
#define PCI_DEV_FROM_TAG(tag)  (((tag) & 0x0000f800) >> 11)
#define PCI_FUNC_FROM_TAG(tag) (((tag) & 0x00000700) >> 8)

#define PCI_DFN_FROM_TAG(tag) (((tag) & 0x0000ff00) >> 8)

/*
 * Debug Macros/Definitions
 */
/* #define DEBUGPCI  2 */    /* Disable/enable trace in PCI code */

#if defined(DEBUGPCI)

# define PCITRACE(lvl,printfargs) \
	if (lvl > xf86Verbose) { \
		ErrorF printfargs; \
	}

#else /* !defined(DEBUGPCI) */

# define PCITRACE(lvl,printfargs)

#endif /* !defined(DEBUGPCI) */

/*
 * PCI Config mechanism definitions
 */
#define PCI_EN 0x80000000

#define	PCI_CFGMECH1_ADDRESS_REG	0xCF8
#define	PCI_CFGMECH1_DATA_REG		0xCFC

#define PCI_CFGMECH1_MAXDEV	32

#define PCI_CFGMECH1_TYPE1_CFGADDR(b,d,f,o) (PCI_EN | PCI_MAKE_TAG(b,d,f) | ((o) & 0xff) | 1)
#define PCI_CFGMECH1_TYPE0_CFGADDR(d,f,o) (PCI_EN | PCI_MAKE_TAG(0,d,f) | ((o) & 0xff))

/*
 * Select architecture specific PCI init function
 */
#if (defined(__powerpc__) || defined(__mips__) || defined(__sh__) || defined(__mc68000__) || defined(__arm__) || defined(__s390__) || defined(__hppa__)) && defined(linux)
# define ARCH_PCI_INIT linuxPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__powerpc__) && defined(__OpenBSD__)
# define  ARCH_PCI_INIT freebsdPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__powerpc__)
# define ARCH_PCI_INIT ppcPciInit
# if !defined(PowerMAX_OS)
#  define INCLUDE_XF86_MAP_PCI_MEM
# endif
#elif defined(__sparc__) && (defined(linux) || defined(sun))
# define ARCH_PCI_INIT sparcPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__alpha__) && defined(linux)
# define ARCH_PCI_INIT axpPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__i386__) && defined(linux)
# define ARCH_PCI_INIT ix86PciInit
# define ARCH_PCI_OS_INIT linuxPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__ia64__) && defined(linux)
# define ARCH_PCI_INIT linuxPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#elif defined(__alpha__) && defined(__FreeBSD__)
# define ARCH_PCI_INIT freebsdPciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#else
# define ARCH_PCI_INIT ix86PciInit
# define INCLUDE_XF86_MAP_PCI_MEM
#endif
extern void ARCH_PCI_INIT(void);
#if defined(ARCH_PCI_OS_INIT)
extern void ARCH_PCI_OS_INIT(void);
#endif

/*
 * Table of functions used to access a specific PCI bus domain
 * (e.g. a primary PCI bus and all of its secondaries)
 */
typedef struct pci_bus_funcs {
	CARD32  (*pciReadLong)(PCITAG, int);
	void    (*pciWriteLong)(PCITAG, int, CARD32);
        void    (*pciSetBitsLong)(PCITAG, int, CARD32, CARD32);
	ADDRESS (*pciAddrHostToBus)(PCITAG, PciAddrType, ADDRESS);
	ADDRESS (*pciAddrBusToHost)(PCITAG, PciAddrType, ADDRESS);
} pciBusFuncs_t;

/*
 * pciBusInfo_t - One structure per defined PCI bus 
 */
typedef struct pci_bus_info {
	unsigned char  configMech;   /* PCI config type to use      */
	unsigned char  numDevices;   /* Range of valid devnums      */
	unsigned char  secondary;    /* Boolean: bus is a secondary */
	unsigned char  primary_bus;  /* Top level (primary) parent  */
	unsigned long  ppc_io_base;  /* PowerPC I/O spc membase     */
	unsigned long  ppc_io_size;  /* PowerPC I/O spc size        */
	pciBusFuncs_t  funcs;        /* PCI access functions        */
	void          *pciBusPriv;   /* Implementation private data */
} pciBusInfo_t;

/* configMech values */
#define PCI_CFG_MECH_UNKNOWN 0 /* Not yet known  */
#define PCI_CFG_MECH_1       1 /* Most machines  */
#define PCI_CFG_MECH_2       2 /* Older PC's     */
#define PCI_CFG_MECH_OTHER   3 /* Something else */

/* Generic PCI service functions and helpers */
PCITAG        pciGenFindFirst(void);
PCITAG        pciGenFindNext(void);
CARD32        pciCfgMech1Read(PCITAG tag, int offset);
void          pciCfgMech1Write(PCITAG tag, int offset, CARD32 val);
void          pciCfgMech1SetBits(PCITAG tag, int offset, CARD32 mask,
				 CARD32 val);
CARD32        pciByteSwap(CARD32);
Bool          pciMfDev(int, int);
CARD32        pciReadLongNULL(PCITAG tag, int offset);
void          pciWriteLongNULL(PCITAG tag, int offset, CARD32 val);
void          pciSetBitsLongNULL(PCITAG tag, int offset, CARD32 mask,
				 CARD32 val);
ADDRESS       pciAddrNOOP(PCITAG tag, PciAddrType type, ADDRESS);

extern PCITAG (*pciFindFirstFP)(void);
extern PCITAG (*pciFindNextFP)(void);

extern CARD32 pciDevid;    
extern CARD32 pciDevidMask;

extern int    pciBusNum;
extern int    pciDevNum;
extern int    pciFuncNum;
extern PCITAG pciDeviceTag;

extern pciBusInfo_t  *pciBusInfo[];

extern pciBusFuncs_t pciNOOPFuncs;

#endif /* _PCI_H */
