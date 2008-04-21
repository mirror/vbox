/* $Id$ */
/** @file
 * IPRT - Binary Image Loader, Extended Features.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include "internal/ldr.h"
#include "internal/ldrMZ.h"


/**
 * Open part with reader.
 *
 * @returns iprt status code.
 * @param   pReader     The loader reader instance which will provide the raw image bits.
 * @param   phMod       Where to store the handle.
 */
int rtldrOpenWithReader(PRTLDRREADER pReader, PRTLDRMOD phMod)
{
    /*
     * Read and verify the file signature.
     */
    union
    {
        char        ach[4];
        uint16_t    au16[2];
        uint32_t    u32;
    } uSign;
    int rc = pReader->pfnRead(pReader, &uSign, sizeof(uSign), 0);
    if (RT_FAILURE(rc))
        return rc;
#ifndef LDR_WITH_KLDR
    if (    uSign.au16[0] != IMAGE_DOS_SIGNATURE
        &&  uSign.u32     != IMAGE_NT_SIGNATURE
        &&  uSign.u32     != IMAGE_ELF_SIGNATURE
        &&  uSign.au16[0] != IMAGE_LX_SIGNATURE)
    {
        Log(("rtldrOpenWithReader: %s: unknown magic %#x / '%.4s\n", pReader->pfnLogName(pReader), uSign.u32, &uSign.ach[0]));
        return VERR_INVALID_EXE_SIGNATURE;
    }
#endif
    uint32_t offHdr = 0;
    if (uSign.au16[0] == IMAGE_DOS_SIGNATURE)
    {
        rc = pReader->pfnRead(pReader, &offHdr, sizeof(offHdr), RT_OFFSETOF(IMAGE_DOS_HEADER, e_lfanew));
        if (RT_FAILURE(rc))
            return rc;

        if (offHdr <= sizeof(IMAGE_DOS_HEADER))
        {
            Log(("rtldrOpenWithReader: %s: no new header / invalid offset %#RX32\n", pReader->pfnLogName(pReader), offHdr));
            return VERR_INVALID_EXE_SIGNATURE;
        }
        rc = pReader->pfnRead(pReader, &uSign, sizeof(uSign), offHdr);
        if (RT_FAILURE(rc))
            return rc;
        if (    uSign.u32     != IMAGE_NT_SIGNATURE
            &&  uSign.au16[0] != IMAGE_LX_SIGNATURE
            &&  uSign.au16[0] != IMAGE_LE_SIGNATURE
            &&  uSign.au16[0] != IMAGE_NE_SIGNATURE)
        {
            Log(("rtldrOpenWithReader: %s: unknown new magic %#x / '%.4s\n", pReader->pfnLogName(pReader), uSign.u32, &uSign.ach[0]));
            return VERR_INVALID_EXE_SIGNATURE;
        }
    }

    /*
     * Create image intepreter instance depending on the signature.
     */
    if (uSign.u32 == IMAGE_NT_SIGNATURE)
#ifdef LDR_WITH_PE
        rc = rtldrPEOpen(pReader, offHdr, phMod);
#else
        rc = VERR_PE_EXE_NOT_SUPPORTED;
#endif
    else if (uSign.u32 == IMAGE_ELF_SIGNATURE)
#if defined(LDR_WITH_ELF)
        rc = rtldrELFOpen(pReader, phMod);
#else
        rc = VERR_ELF_EXE_NOT_SUPPORTED;
#endif
    else if (uSign.au16[0] == IMAGE_LX_SIGNATURE)
#ifdef LDR_WITH_LX
        rc = rtldrLXOpen(pReader, offHdr, phMod);
#else
        rc = VERR_LX_EXE_NOT_SUPPORTED;
#endif
    else if (uSign.au16[0] == IMAGE_LE_SIGNATURE)
#ifdef LDR_WITH_LE
        rc = rtldrLEOpen(pReader, phMod);
#else
        rc = VERR_LE_EXE_NOT_SUPPORTED;
#endif
    else if (uSign.au16[0] == IMAGE_NE_SIGNATURE)
#ifdef LDR_WITH_NE
        rc = rtldrNEOpen(pReader, phMod);
#else
        rc = VERR_NE_EXE_NOT_SUPPORTED;
#endif
    else if (uSign.au16[0] == IMAGE_DOS_SIGNATURE)
#ifdef LDR_WITH_MZ
        rc = rtldrMZOpen(pReader, phMod);
#else
        rc = VERR_MZ_EXE_NOT_SUPPORTED;
#endif
    else if (/*   uSign.u32 == IMAGE_AOUT_A_SIGNATURE
             || uSign.u32 == IMAGE_AOUT_Z_SIGNATURE*/ /** @todo find the aout magics in emx or binutils. */
             0)
#ifdef LDR_WITH_AOUT
        rc = rtldrAOUTOpen(pReader, phMod);
#else
        rc = VERR_AOUT_EXE_NOT_SUPPORTED;
#endif
    else
    {
#ifndef LDR_WITH_KLDR
        Log(("rtldrOpenWithReader: %s: the format isn't implemented %#x / '%.4s\n", pReader->pfnLogName(pReader), uSign.u32, &uSign.ach[0]));
#endif
        rc = VERR_INVALID_EXE_SIGNATURE;
    }

#ifdef LDR_WITH_KLDR
    /* Try kLdr if it's a format we don't recognize. */
    if (rc <= VERR_INVALID_EXE_SIGNATURE && rc > VERR_BAD_EXE_FORMAT)
        rc = rtldrkLdrOpen(pReader, phMod);
#endif

    LogFlow(("rtldrOpenWithReader: %s: returns %Rrc *phMod=%p\n", pReader->pfnLogName(pReader), rc, *phMod));
    return rc;
}


/**
 * Gets the size of the loaded image.
 * This is only supported for modules which has been opened using RTLdrOpen() and RTLdrOpenBits().
 *
 * @returns image size (in bytes).
 * @returns ~(size_t)0 on if not opened by RTLdrOpen().
 * @param   hLdrMod     Handle to the loader module.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(size_t) RTLdrSize(RTLDRMOD hLdrMod)
{
    LogFlow(("RTLdrSize: hLdrMod=%RTldrm\n", hLdrMod));

    /*
     * Validate input.
     */
    AssertMsgReturn(rtldrIsValid(hLdrMod), ("hLdrMod=%p\n", hLdrMod), ~(size_t)0);
    PRTLDRMODINTERNAL pMod = (PRTLDRMODINTERNAL)hLdrMod;
    AssertMsgReturn(pMod->eState == LDR_STATE_OPENED, ("eState=%d\n", pMod->eState), ~(size_t)0);

    /*
     * Do it.
     */
    size_t cb = pMod->pOps->pfnGetImageSize(pMod);
    LogFlow(("RTLdrSize: returns %zu\n", cb));
    return cb;
}


/**
 * Loads the image into a buffer provided by the user and applies fixups
 * for the given base address.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The load module handle.
 * @param   pvBits          Where to put the bits.
 *                          Must be as large as RTLdrSize() suggests.
 * @param   BaseAddress     The base address.
 * @param   pfnGetImport    Callback function for resolving imports one by one.
 * @param   pvUser          User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrGetBits(RTLDRMOD hLdrMod, void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    LogFlow(("RTLdrGetBits: hLdrMod=%RTldrm pvBits=%p BaseAddress=%RTptr pfnGetImport=%p pvUser=%p\n",
             hLdrMod, pvBits, BaseAddress, pfnGetImport, pvUser));

    /*
     * Validate input.
     */
    AssertMsgReturn(rtldrIsValid(hLdrMod), ("hLdrMod=%p\n", hLdrMod), VERR_INVALID_HANDLE);
    AssertMsgReturn(VALID_PTR(pvBits), ("pvBits=%p\n", pvBits), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pfnGetImport), ("pfnGetImport=%p\n", pfnGetImport), VERR_INVALID_PARAMETER);
    PRTLDRMODINTERNAL pMod = (PRTLDRMODINTERNAL)hLdrMod;
    AssertMsgReturn(pMod->eState == LDR_STATE_OPENED, ("eState=%d\n", pMod->eState), VERR_WRONG_ORDER);

    /*
     * Do it.
     */
    int rc = pMod->pOps->pfnGetBits(pMod, pvBits, BaseAddress, pfnGetImport, pvUser);
    LogFlow(("RTLdrGetBits: returns %Rrc\n",rc));
    return rc;
}


/**
 * Relocates bits after getting them.
 * Useful for code which moves around a bit.
 *
 * @returns iprt status code.
 * @param   hLdrMod             The loader module handle.
 * @param   pvBits              Where the image bits are.
 *                              Must've been passed to RTLdrGetBits().
 * @param   NewBaseAddress      The new base address.
 * @param   OldBaseAddress      The old base address.
 * @param   pfnGetImport        Callback function for resolving imports one by one.
 * @param   pvUser              User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrRelocate(RTLDRMOD hLdrMod, void *pvBits, RTUINTPTR NewBaseAddress, RTUINTPTR OldBaseAddress,
                          PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    LogFlow(("RTLdrRelocate: hLdrMod=%RTldrm pvBits=%p NewBaseAddress=%RTptr OldBaseAddress=%RTptr pfnGetImport=%p pvUser=%p\n",
             hLdrMod, pvBits, NewBaseAddress, OldBaseAddress, pfnGetImport, pvUser));

    /*
     * Validate input.
     */
    AssertMsgReturn(rtldrIsValid(hLdrMod), ("hLdrMod=%p\n", hLdrMod), VERR_INVALID_HANDLE);
    AssertMsgReturn(VALID_PTR(pvBits), ("pvBits=%p\n", pvBits), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pfnGetImport), ("pfnGetImport=%p\n", pfnGetImport), VERR_INVALID_PARAMETER);
    PRTLDRMODINTERNAL pMod = (PRTLDRMODINTERNAL)hLdrMod;
    AssertMsgReturn(pMod->eState == LDR_STATE_OPENED, ("eState=%d\n", pMod->eState), VERR_WRONG_ORDER);

    /*
     * Do it.
     */
    int rc = pMod->pOps->pfnRelocate(pMod, pvBits, NewBaseAddress, OldBaseAddress, pfnGetImport, pvUser);
    LogFlow(("RTLdrRelocate: returns %Vrc\n", rc));
    return rc;
}


/**
 * Gets the address of a named exported symbol.
 *
 * This function differs from the plain one in that it can deal with
 * both GC and HC address sizes, and that it can calculate the symbol
 * value relative to any given base address.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pvBits          Optional pointer to the loaded image.
 *                          Set this to NULL if no RTLdrGetBits() processed image bits are available.
 *                          Not supported for RTLdrLoad() images and must be NULL.
 * @param   BaseAddress     Image load address.
 *                          Not supported for RTLdrLoad() images and must be 0.
 * @param   pszSymbol       Symbol name.
 * @param   pValue          Where to store the symbol value.
 */
RTDECL(int) RTLdrGetSymbolEx(RTLDRMOD hLdrMod, const void *pvBits, RTUINTPTR BaseAddress, const char *pszSymbol, RTUINTPTR *pValue)
{
    LogFlow(("RTLdrGetSymbolEx: hLdrMod=%RTldrm pvBits=%p BaseAddress=%RTptr pszSymbol=%p:{%s} pValue\n",
             hLdrMod, pvBits, BaseAddress, pszSymbol, pszSymbol, pValue));

    /*
     * Validate input.
     */
    AssertMsgReturn(rtldrIsValid(hLdrMod), ("hLdrMod=%p\n", hLdrMod), VERR_INVALID_HANDLE);
    AssertMsgReturn(!pvBits || VALID_PTR(pvBits), ("pvBits=%p\n", pvBits), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pszSymbol, ("pszSymbol=%p\n", pszSymbol), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pValue), ("pValue=%p\n", pvBits), VERR_INVALID_PARAMETER);
    PRTLDRMODINTERNAL pMod = (PRTLDRMODINTERNAL)hLdrMod;
    //AssertMsgReturn(pMod->eState == LDR_STATE_OPENED, ("eState=%d\n", pMod->eState), VERR_WRONG_ORDER);

    /*
     * Do it.
     */
    int rc;
    if (pMod->pOps->pfnGetSymbolEx)
        rc = pMod->pOps->pfnGetSymbolEx(pMod, pvBits, BaseAddress, pszSymbol, pValue);
    else if (!BaseAddress && !pvBits)
    {
        void *pvValue;
        rc = pMod->pOps->pfnGetSymbol(pMod, pszSymbol, &pvValue);
        if (RT_SUCCESS(rc))
            *pValue = (uintptr_t)pvValue;
    }
    else
        AssertMsgFailedReturn(("BaseAddress=%RTptr pvBits=%p\n", BaseAddress, pvBits), VERR_INVALID_FUNCTION);
    LogFlow(("RTLdrGetSymbolEx: returns %Rrc *pValue=%p\n", rc, *pValue));
    return rc;
}


/**
 * Enumerates all symbols in a module.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   fFlags          Flags indicating what to return and such.
 * @param   pvBits          Optional pointer to the loaded image.
 *                          Set this to NULL if no RTLdrGetBits() processed image bits are available.
 * @param   BaseAddress     Image load address.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrEnumSymbols(RTLDRMOD hLdrMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    LogFlow(("RTLdrEnumSymbols: hLdrMod=%RTldrm fFlags=%#x pvBit=%p BaseAddress=%RTptr pfnCallback=%p pvUser=%p\n",
             hLdrMod, fFlags, pvBits, BaseAddress, pfnCallback, pvUser));

    /*
     * Validate input.
     */
    AssertMsgReturn(rtldrIsValid(hLdrMod), ("hLdrMod=%p\n", hLdrMod), VERR_INVALID_HANDLE);
    AssertMsgReturn(!pvBits || VALID_PTR(pvBits), ("pvBits=%p\n", pvBits), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pfnCallback), ("pfnCallback=%p\n", pfnCallback), VERR_INVALID_PARAMETER);
    PRTLDRMODINTERNAL pMod = (PRTLDRMODINTERNAL)hLdrMod;
    //AssertMsgReturn(pMod->eState == LDR_STATE_OPENED, ("eState=%d\n", pMod->eState), VERR_WRONG_ORDER);

    /*
     * Do it.
     */
    int rc = pMod->pOps->pfnEnumSymbols(pMod, fFlags, pvBits, BaseAddress, pfnCallback, pvUser);
    LogFlow(("RTLdrEnumSymbols: returns %Rrc\n", rc));
    return rc;
}

