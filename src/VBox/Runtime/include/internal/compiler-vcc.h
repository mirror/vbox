/* $Id$ */
/** @file
 * IPRT - Internal header for the Visual C++ Compiler Support Code.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef IPRT_INCLUDED_INTERNAL_compiler_vcc_h
#define IPRT_INCLUDED_INTERNAL_compiler_vcc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

/** @name Special sections.
 * @{
 */

#ifdef IPRT_COMPILER_VCC_WITH_C_INIT_TERM_SECTIONS
# pragma section(".CRT$XIA",  read, long)   /* start C initializers */
# pragma section(".CRT$XIAA", read, long)
# pragma section(".CRT$XIZ",  read, long)

# pragma section(".CRT$XPA",  read, long)   /* start early C terminators  */
# pragma section(".CRT$XPAA", read, long)
# pragma section(".CRT$XPZ",  read, long)

# pragma section(".CRT$XTA",  read, long)   /* start C terminators  */
# pragma section(".CRT$XTAA", read, long)
# pragma section(".CRT$XTZ",  read, long)
# define IPRT_COMPILER_TERM_CALLBACK(a_fn) \
    __declspec(allocate(".CRT$XTAA")) PFNRT RT_CONCAT(g_rtVccTermCallback_, a_fn) = a_fn
#endif

#ifdef IPRT_COMPILER_VCC_WITH_CPP_INIT_SECTIONS
# pragma warning(disable:5247)  /* warning C5247: section '.CRT$XCA' is reserved for C++ dynamic initialization. Manually creating the section will interfere with C++ dynamic initialization and may lead to undefined behavior */
# pragma warning(disable:5248)  /* warning C5248: section '.CRT$XCA' is reserved for C++ dynamic initialization. Variables manually put into the section may be optimized out and their order relative to compiler generated dynamic initializers is unspecified */
# pragma section(".CRT$XCA",  read, long)   /* start C++ initializers */
# pragma section(".CRT$XCAA", read, long)
# pragma section(".CRT$XCZ",  read, long)
#endif

#ifdef IPRT_COMPILER_VCC_WITH_RTC_INIT_TERM_SECTIONS
# pragma section(".rtc$IAA",  read, long)   /* start RTC initializers */
# pragma section(".rtc$IZZ",  read, long)

# pragma section(".rtc$TAA",  read, long)   /* start RTC terminators */
# pragma section(".rtc$TZZ",  read, long)
#endif

#ifdef IPRT_COMPILER_VCC_WITH_TLS_CALLBACK_SECTIONS
# pragma section(".CRT$XLA",  read, long)   /* start TLS callback */
# pragma section(".CRT$XLAA", read, long)
# pragma section(".CRT$XLZ",  read, long)

/** @todo what about .CRT$XDA? Dynamic TLS initializers. */
#endif

#ifdef IPRT_COMPILER_VCC_WITH_TLS_DATA_SECTIONS
# pragma section(".tls",      read, long)   /* start TLS callback */
# pragma section(".tls$ZZZ",  read, long)

/** @todo what about .CRT$XDA? Dynamic TLS initializers. */
#endif

/** @} */


RT_C_DECLS_BEGIN

extern unsigned _fltused;

void rtVccInitSecurityCookie(void) RT_NOEXCEPT;
void rtVccWinInitProcExecPath(void) RT_NOEXCEPT;
int  rtVccInitializersRunInit(void) RT_NOEXCEPT;
void rtVccInitializersRunTerm(void) RT_NOEXCEPT;
void rtVccTermRunAtExit(void) RT_NOEXCEPT;


RT_C_DECLS_END


#endif /* !IPRT_INCLUDED_INTERNAL_compiler_vcc_h */

