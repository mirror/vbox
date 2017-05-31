/** @file
 * VirtualBox Video driver, common code - iprt and VirtualBox macros and
 * definitions.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___VBox_VBoxVideoIPRT_h
#define ___VBox_VBoxVideoIPRT_h

#include <asm/io.h>
#include <iprt/cdefs.h>
#include <iprt/stdarg.h>
#include <iprt/stdint.h>
#include <iprt/types.h>

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

#define ASMCompilerBarrier() mb()

/** @}  */

/** @name VirtualBox assertions
 * @{ */

#define Assert(a) WARN_ON_ONCE(!(a))
#define AssertPtr(a) WARN_ON_ONCE(!(a))
#define AssertReturnVoid(a) do { if (WARN_ON_ONCE(!(a))) return; } while(0)
#define AssertRC(a) WARN_ON_ONCE(RT_FAILURE(a))
#define AssertPtrNullReturnVoid(a) do {} while(0)
#define AssertPtrReturnVoid(a) do { if (WARN_ON_ONCE(!(a))) return; } while(0)

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

/** @name types for VirtualBox OS-independent code
 * @{ */

/* HGSMI uses 32 bit offsets and sizes. */
typedef uint32_t HGSMISIZE;
typedef uint32_t HGSMIOFFSET;

/** @}  */

#endif /* ___VBox_VBoxVideoIPRT_h */
