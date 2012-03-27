/* $Id$ */
/** @file
 * IPRT - Kernel debug information, Ring-0 Driver, Solaris Code.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <sys/kobj.h>
#include <sys/thread.h>
#include <sys/ctf_api.h>
#include <sys/modctl.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTDBGKRNLINFOINT
{
    /** Magic value (). */
    uint32_t volatile   u32Magic;
    /** The number of threads referencing this object. */
    uint32_t volatile   cRefs;
    /** Pointer to the genunix CTF handle. */
    ctf_file_t         *pGenUnixCTF;
    /** Pointer to the genunix module handle. */
    modctl_t           *pGenUnixMod;
} RTDBGKRNLINFOINT;
typedef struct RTDBGKRNLINFOINT *PRTDBGKRNLINFOINT;
/** Magic value for RTDBGKRNLINFOINT::u32Magic. (John Carmack) */
#define RTDBGKRNLINFO_MAGIC       UINT32_C(0x19700820)


RTR0DECL(int) RTR0DbgKrnlInfoOpen(PRTDBGKRNLINFO phKrnlInfo, uint32_t fFlags)
{
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phKrnlInfo, VERR_INVALID_POINTER);
    RT_ASSERT_PREEMPTIBLE();

    PRTDBGKRNLINFOINT pThis = (PRTDBGKRNLINFOINT)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    pThis->pGenUnixMod = mod_hold_by_name("genunix");
    if (RT_LIKELY(pThis->pGenUnixMod))
    {
        /*
         * Hold mod_lock as ctf_modopen may update the module with uncompressed CTF data.
         */
        int err;
        mutex_enter(&mod_lock);
        pThis->pGenUnixCTF = ctf_modopen(pThis->pGenUnixMod->mod_mp, &err);
        mutex_exit(&mod_lock);

        if (RT_LIKELY(pThis->pGenUnixCTF))
        {
            pThis->u32Magic       = RTDBGKRNLINFO_MAGIC;
            pThis->cRefs          = 1;

            *phKrnlInfo = pThis;
            return VINF_SUCCESS;
        }
        else
        {
            LogRel(("RTR0DbgKrnlInfoOpen: ctf_modopen failed. err=%d\n", err));
            rc = VERR_INTERNAL_ERROR_2;
        }
    }
    else
    {
        LogRel(("RTR0DbgKrnlInfoOpen: mod_hold_by_name failed for genunix.\n"));
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


RTR0DECL(uint32_t) RTR0DbgKrnlInfoRetain(RTDBGKRNLINFO hKrnlInfo)
{
    PRTDBGKRNLINFOINT pThis = hKrnlInfo;
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs && cRefs < 100000);
    NOREF(cRefs);
}

static void rtR0DbgKrnlInfoDestroy(RTDBGKRNLINFO hKrnlInfo)
{
    PRTDBGKRNLINFOINT pThis = hKrnlInfo;
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pThis->u32Magic == RTDBGKRNLINFO_MAGIC);
    RT_ASSERT_PREEMPTIBLE();

    ctf_close(pThis->pGenUnixCTF);
    mod_release_mod(pThis->pGenUnixMod);
    RTMemFree(pThis);
}


RTR0DECL(uint32_t) RTR0DbgKrnlInfoRelease(RTDBGKRNLINFO hKrnlInfo)
{
    PRTDBGKRNLINFOINT pThis = hKrnlInfo;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), UINT32_MAX);
    RT_ASSERT_PREEMPTIBLE();

    if (ASMAtomicDecU32(&pThis->cRefs) == 0)
        rtR0DbgKrnlInfoDestroy(pThis);
}


RTR0DECL(int) RTR0DbgKrnlInfoQueryMember(RTDBGKRNLINFO hKrnlInfo, const char *pszStructure,
                                               const char *pszMember, size_t *poffMember)
{
    PRTDBGKRNLINFOINT pThis = hKrnlInfo;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertPtrReturn(pszMember, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszStructure, VERR_INVALID_PARAMETER);
    AssertPtrReturn(poffMember,  VERR_INVALID_PARAMETER);
    RT_ASSERT_PREEMPTIBLE();

    int rc = VERR_NOT_FOUND;
	ctf_id_t TypeIdent = ctf_lookup_by_name(pThis->pGenUnixCTF, pszStructure);
	if (TypeIdent != CTF_ERR)
	{
		ctf_membinfo_t MemberInfo;
        RT_ZERO(MemberInfo);
		if (ctf_member_info(pThis->pGenUnixCTF, TypeIdent, pszMember, &MemberInfo) != CTF_ERR)
		{
			*poffMember = (MemberInfo.ctm_offset >> 3);
			return VINF_SUCCESS;
		}
	}

	return rc;
}


RTR0DECL(int) RTR0DbgKrnlInfoQuerySymbol(RTDBGKRNLINFO hKrnlInfo, const char *pszModule,
                                               const char *pszSymbol, void **ppvSymbol)
{
    PRTDBGKRNLINFOINT pThis = hKrnlInfo;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertPtrReturn(pszSymbol, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvSymbol, VERR_INVALID_PARAMETER);
    RT_ASSERT_PREEMPTIBLE();

    NOREF(pszModule);
	*ppvSymbol = kobj_getsymvalue(pszSymbol, 1 /* only kernel */);
    if (*ppvSymbol)
        return VINF_SUCCESS;

    return VERR_SYMBOL_NOT_FOUND;
}

