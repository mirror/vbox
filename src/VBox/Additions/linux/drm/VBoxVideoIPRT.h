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

#include <asm/atomic.h>
#include <asm/io.h>
#include <iprt/cdefs.h>
#include <iprt/list.h>
#include <iprt/stdarg.h>
#include <iprt/stdint.h>

#include <linux/string.h>

/** @name VirtualBox error macros
 * @{ */

#define VINF_SUCCESS                        0
#define VERR_INVALID_PARAMETER              (-2)
#define VERR_INVALID_POINTER                (-6)
#define VERR_NO_MEMORY                      (-8)
#define VERR_NOT_IMPLEMENTED                (-12)
#define VERR_INVALID_FUNCTION               (-36)
#define VERR_NOT_SUPPORTED                  (-37)
#define VERR_TOO_MUCH_DATA                  (-42)
#define VERR_INVALID_STATE                  (-79)
#define VERR_OUT_OF_RESOURCES               (-80)
#define VERR_ALREADY_EXISTS                 (-105)
#define VERR_INTERNAL_ERROR                 (-225)

#define RT_SUCCESS_NP(rc)   ( (int)(rc) >= VINF_SUCCESS )
#define RT_SUCCESS(rc)      ( likely(RT_SUCCESS_NP(rc)) )
#define RT_FAILURE(rc)      ( unlikely(!RT_SUCCESS_NP(rc)) )

/** @}  */

/** @name VirtualBox helper functions
 * @{ */

#define RT_ZERO(x) memset(&(x), 0, sizeof(x))
#define ASMAtomicCmpXchgBool(b, new_val, old_val) \
	(cmpxchg(b, old_val, new_val) == old_val)
#define ASMAtomicWriteBool(b, val) xchg(b, val)
#define ASMCompilerBarrier() mb()

/** @}  */

/** @name VirtualBox assertions
 * @{ */

#define Assert(a) WARN_ON(a)
#define AssertPtr(a) WARN_ON(!(a))
#define AssertReturnVoid(a) do { if (WARN_ON(!(a))) return; } while(0)
#define AssertRC(a) WARN_ON(RT_FAILURE(a))
#define AssertPtrNullReturnVoid(a) do {} while(0)
#define AssertPtrReturnVoid(a) do { if (WARN_ON(!(a))) return; } while(0)

extern int RTASSERTVAR[1];
#define AssertCompile(expr) \
    extern int RTASSERTVAR[1] __attribute__((__unused__)), \
    RTASSERTVAR[(expr) ? 1 : 0] __attribute__((__unused__))
#define AssertCompileSize(type, size) \
    AssertCompile(sizeof(type) == (size))

/** @}  */

/** @name Port I/O helpers
 * @{ */
/** Write an 8-bit value to an I/O port. */
#define VBVO_PORT_WRITE_U8(Port, Value) \
    outb(Value, Port)
/** Write a 16-bit value to an I/O port. */
#define VBVO_PORT_WRITE_U16(Port, Value) \
    outw(Value, Port)
/** Write a 32-bit value to an I/O port. */
#define VBVO_PORT_WRITE_U32(Port, Value) \
    outl(Value, Port)
/** Read an 8-bit value from an I/O port. */
#define VBVO_PORT_READ_U8(Port) \
    inb(Port)
/** Read a 16-bit value from an I/O port. */
#define VBVO_PORT_READ_U16(Port) \
    inw(Port)
/** Read a 32-bit value from an I/O port. */
#define VBVO_PORT_READ_U32(Port) \
    inl(Port)

/** @}  */

#endif /* ___VBox_VBoxVideoIPRT_h */
