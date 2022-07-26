/* $Id$ */
/** @file
 * IPRT - No-CRT - Basic allocators, Windows.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>

#include "internal/compiler-vcc.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Variable descriptor. */
typedef struct RTC_VAR_DESC_T
{
    int32_t     offFrame;
    uint32_t    cbVar;
    const char *pszName;
} RTC_VAR_DESC_T;

/** Frame descriptor. */
typedef struct RTC_FRAME_DESC_T
{
    uint32_t                cVars;
    RTC_VAR_DESC_T const   *paVars;
} RTC_FRAME_DESC_T;

#define VARIABLE_MARKER_PRE     0xcccccccc
#define VARIABLE_MARKER_POST    0xcccccccc


/**
 * Alloca allocation entry.
 * @note For whatever reason the pNext and cb members are misaligned on 64-bit
 *       targets.  32-bit targets OTOH adds padding to keep the structure size
 *       and pNext + cb offsets the same.
 */
#pragma pack(4)
typedef struct RTC_ALLOC_ENTRY
{
    uint32_t            uGuard1;
    RTC_ALLOC_ENTRY    *pNext;
#if ARCH_BITS == 32
    uint32_t            pNextPad;
#endif
    size_t              cb;
#if ARCH_BITS == 32
    uint32_t            cbPad;
#endif
    uint32_t            auGuard2[3];
} RTC_ALLOC_ENTRY;
#pragma pack()

#define ALLOCA_FILLER_BYTE      0xcc
#define ALLOCA_FILLER_32        0xcccccccc


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
extern "C" void __fastcall _RTC_CheckStackVars(uint8_t *pbFrame, RTC_VAR_DESC_T const *pVar); /* nocrt-stack.asm */
extern "C" uintptr_t __security_cookie;


DECLASM(void) _RTC_StackVarCorrupted(uint8_t *pbFrame, RTC_VAR_DESC_T const *pVar)
{
    RTAssertMsg2("\n\n!!Stack corruption!!\n\n"
                 "%p LB %#x - %s\n",
                 pbFrame + pVar->offFrame, pVar->cbVar, pVar->pszName);
    RT_BREAKPOINT();
}


DECLASM(void) _RTC_SecurityCookieMismatch(uintptr_t uCookie)
{
    RTAssertMsg2("\n\n!!Stack cookie corruption!!\n\n"
                 "expected %p, found %p\n",
                 __security_cookie, uCookie);
    RT_BREAKPOINT();
}


extern "C" void __cdecl _RTC_UninitUse(const char *pszVar)
{
    RTAssertMsg2("\n\n!!Used uninitialized variable %s at %p!!\n\n",
                 pszVar ? pszVar : "", ASMReturnAddress());
    RT_BREAKPOINT();
}


/** @todo reimplement in assembly (feeling too lazy right now). */
extern "C" void __fastcall _RTC_CheckStackVars2(uint8_t *pbFrame, RTC_VAR_DESC_T const *pVar, RTC_ALLOC_ENTRY *pHead)
{
    while (pHead)
    {
        if (   pHead->uGuard1     == ALLOCA_FILLER_32
#if 1 && ARCH_BITS == 32
            && pHead->pNextPad    == ALLOCA_FILLER_32
            && pHead->cbPad       == ALLOCA_FILLER_32
#endif
            && pHead->auGuard2[0] == ALLOCA_FILLER_32
            && pHead->auGuard2[1] == ALLOCA_FILLER_32
            && pHead->auGuard2[2] == ALLOCA_FILLER_32
            && *(uint32_t const *)((uint8_t const *)pHead + pHead->cb - sizeof(uint32_t)) == ALLOCA_FILLER_32)
        { /* likely */ }
        else
        {
            RTAssertMsg2("\n\n!!Stack corruption (alloca)!!\n\n"
                         "%p LB %#x\n",
                         pHead, pHead->cb);
            RT_BREAKPOINT();
        }
        pHead = pHead->pNext;
    }

    _RTC_CheckStackVars(pbFrame, pVar);
}

