/** @file
 * VirtualBox Video driver, common code - iprt and VirtualBox macros and
 * definitions.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_VBoxVideoIPRT_h
#define ___VBox_VBoxVideoIPRT_h

#ifndef RT_OS_OS2
# include <iprt/asm.h>
#endif
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/list.h>
#include <iprt/stdarg.h>
#include <iprt/stdint.h>
#include <iprt/string.h>
#include <iprt/types.h>

#if !defined VBOX_XPDM_MINIPORT && !defined VBOX_GUESTR3XORGMOD
# include <iprt/asm-amd64-x86.h>
#endif

#ifdef VBOX_XPDM_MINIPORT
# include <iprt/nt/miniport.h>
# include <ntddvdeo.h> /* sdk, clean */
# include <iprt/nt/Video.h>
#elif defined VBOX_GUESTR3XORGMOD
# include <compiler.h>
#endif

/** @name Port I/O helpers
 * @{ */

#ifdef VBOX_XPDM_MINIPORT

/** Write an 8-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U8(Port, Value) \
    VideoPortWritePortUchar((PUCHAR)Port, Value)
/** Write a 16-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U16(Port, Value) \
    VideoPortWritePortUshort((PUSHORT)Port, Value)
/** Write a 32-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U32(Port, Value) \
    VideoPortWritePortUlong((PULONG)Port, Value)
/** Read an 8-bit value from an I/O port. */
# define VBVO_PORT_READ_U8(Port) \
    VideoPortReadPortUchar((PUCHAR)Port)
/** Read a 16-bit value from an I/O port. */
# define VBVO_PORT_READ_U16(Port) \
    VideoPortReadPortUshort((PUSHORT)Port)
/** Read a 32-bit value from an I/O port. */
# define VBVO_PORT_READ_U32(Port) \
    VideoPortReadPortUlong((PULONG)Port)
    
#elif defined(VBOX_GUESTR3XORGMOD)

/** Write an 8-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U8(Port, Value) \
    outb(Port, Value)
/** Write a 16-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U16(Port, Value) \
    outw(Port, Value)
/** Write a 32-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U32(Port, Value) \
    outl(Port, Value)
/** Read an 8-bit value from an I/O port. */
# define VBVO_PORT_READ_U8(Port) \
    inb(Port)
/** Read a 16-bit value from an I/O port. */
# define VBVO_PORT_READ_U16(Port) \
    inw(Port)
/** Read a 32-bit value from an I/O port. */
# define VBVO_PORT_READ_U32(Port) \
    inl(Port)

#else  /** @todo make these explicit */

/** Write an 8-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U8(Port, Value) \
    ASMOutU8(Port, Value)
/** Write a 16-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U16(Port, Value) \
    ASMOutU16(Port, Value)
/** Write a 32-bit value to an I/O port. */
# define VBVO_PORT_WRITE_U32(Port, Value) \
    ASMOutU32(Port, Value)
/** Read an 8-bit value from an I/O port. */
# define VBVO_PORT_READ_U8(Port) \
    ASMInU8(Port)
/** Read a 16-bit value from an I/O port. */
# define VBVO_PORT_READ_U16(Port) \
    ASMInU16(Port)
/** Read a 32-bit value from an I/O port. */
# define VBVO_PORT_READ_U32(Port) \
    ASMInU32(Port)
#endif

/** @}  */

#endif /* ___VBox_VBoxVideoIPRT_h */
