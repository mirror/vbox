/* $Id$ */
/** @file
 * BS3Kit - bs3-fpustate-1, C code template.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


#ifdef BS3_INSTANTIATING_CMN

///**
// * Wrapper around Bs3TestFailedF that prefixes the error with g_usBs3TestStep
// * and g_pszTestMode.
// */
//# define bs3CpuBasic2_FailedF BS3_CMN_NM(bs3CpuBasic2_FailedF)
//BS3_DECL_NEAR(void) bs3CpuBasic2_FailedF(const char *pszFormat, ...)
//{
//}

#endif /* BS3_INSTANTIATING_CMN */


/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE
# if TMPL_MODE == BS3_MODE_PE32 \
  || TMPL_MODE == BS3_MODE_PP32 \
  || TMPL_MODE == BS3_MODE_PAE32 \
  || TMPL_MODE == BS3_MODE_LM64
//  || TMPL_MODE == BS3_MODE_RM

BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_InitState)(X86FXSTATE BS3_FAR *pFxState);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_Restore)(X86FXSTATE const BS3_FAR *pFxState);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_Save)(X86FXSTATE BS3_FAR *pFxState);

BS3_DECL_FAR(uint8_t) TMPL_NM(bs3FpuState1_Corruption)(uint8_t bMode)
{
    /* We don't need to test that many modes, probably.  */

    uint8_t             abBuf[sizeof(X86FXSTATE)*2 + 32];
    uint8_t BS3_FAR    *pbTmp = &abBuf[0x10 - (((uintptr_t)abBuf) & 0x0f)];
    X86FXSTATE BS3_FAR *pExpected = (X86FXSTATE BS3_FAR *)pbTmp;
    X86FXSTATE BS3_FAR *pChecking = pExpected + 1;
    uint32_t            iLoops;

    /** @todo flexible wrt fxsave support? do we care?  */
    /* First, make 100% sure we don't trap accessing the FPU state and that we can use fxsave/fxrstor. */
    g_usBs3TestStep = 1;
    ASMSetCR0((ASMGetCR0() & ~(X86_CR0_TS | X86_CR0_EM)) | X86_CR0_MP);
    ASMSetCR4(ASMGetCR4() | X86_CR4_OSFXSR /*| X86_CR4_OSXMMEEXCPT*/);

    /* Second, come up with a distinct state. We do that from assembly. */
    g_usBs3TestStep = 2;
    Bs3MemSet(abBuf, 0x42, sizeof(abBuf));
    TMPL_NM(bs3FpuState1_InitState)(pExpected);

    /* Check that we can keep it consistent for a while. */
    g_usBs3TestStep = 3;
    for (iLoops = 0; iLoops < _4M; iLoops++) /** @todo adjust counter. will hardcode for now and do timers later so day... */
    {
        TMPL_NM(bs3FpuState1_Save)(pChecking);
        if (Bs3MemCmp(pExpected, pChecking, sizeof(*pExpected)) != 0)
        {
            Bs3TestFailedF("State differs after %RU32 save loops\n", iLoops);
            return 1;
        }
    }

    return 0;
}
# endif
#endif /* BS3_INSTANTIATING_MODE */


