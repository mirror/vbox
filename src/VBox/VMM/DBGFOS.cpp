/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Guest OS Diggers.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/thread.h>
#include <iprt/assert.h>



/**
 * Registers a guest OS digger.
 *
 * This will instantiate an instance of the digger and add it
 * to the list for us in the next call to DBGFR3OSDetect().
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 * @param   pReg    The registration structure.
 */
DBGFR3DECL(int) DBGFR3OSRegister(PVM pVM, PDBGFOSREG pReg)
{
    /*
     * Validate intput.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pReg, VERR_INVALID_POINTER);
    AssertReturn(pReg->u32Magic == DBGFOSREG_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pReg->u32EndMagic == DBGFOSREG_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(!pReg->fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(pReg->cbData < _2G, VERR_INVALID_PARAMETER);
    AssertReturn(!pReg->szName[0], VERR_INVALID_NAME);
    AssertReturn(memchr(&pReg->szName[0], '\0', sizeof(pReg->szName)), VERR_INVALID_NAME);
    AssertPtrReturn(pReg->pfnConstruct, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pReg->pfnDestruct, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnProbe, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnInit, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnRefresh, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnTerm, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnQueryVersion, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnQueryInterface, VERR_INVALID_POINTER);
    for (PDBGFOS pOS = pVM->dbgf.s.pOSHead; pOS; pOS = pOS->pNext)
        AssertMsgReturn(!strcmp(pOS->pReg->szName, pReg->szName), ("%s\n", pReg->szName), VERR_ALREADY_EXISTS);

    /*
     * Allocate a new structure, call the constructor and link it into the list.
     */
    PDBGFOS pOS = (PDBGFOS)MMR3HeapAllocZ(pVM, MM_TAG_DBGF_OS, RT_OFFSETOF(DBGFOS, abData[pReg->cbData]));
    AssertReturn(pOS, VERR_NO_MEMORY);
    pOS->pReg = pReg;

    int rc = pOS->pReg->pfnConstruct(pVM, pOS->abData);
    if (RT_SUCCESS(rc))
    {
        pOS->pNext = pVM->dbgf.s.pOSHead;
        pVM->dbgf.s.pOSHead = pOS;
    }
    else
    {
        if (pOS->pReg->pfnDestruct)
            pOS->pReg->pfnDestruct(pVM, pOS->abData);
        MMR3HeapFree(pOS);
    }

    return VINF_SUCCESS;
}


/**
 * Internal cleanup routine called by DBGFR3Term().
 *
 * @param   pVM     Pointer to the shared VM structure.
 */
void dbgfR3OSTerm(PVM pVM)
{
    /*
     * Terminate the current one.
     */
    if (pVM->dbgf.s.pCurOS)
    {
        pVM->dbgf.s.pCurOS->pReg->pfnTerm(pVM, pVM->dbgf.s.pCurOS->abData);
        pVM->dbgf.s.pCurOS = NULL;
    }

    /*
     * Destroy all the instances.
     */
    while (pVM->dbgf.s.pOSHead)
    {
        PDBGFOS pOS = pVM->dbgf.s.pOSHead;
        pVM->dbgf.s.pOSHead = pOS->pNext;
        if (pOS->pReg->pfnDestruct)
            pOS->pReg->pfnDestruct(pVM, pOS->abData);
        MMR3HeapFree(pOS);
    }
}


/**
 * Detectes the guest OS and try dig out symbols and useful stuff.
 *
 * When called the 2nd time, symbols will be updated that if the OS
 * is the same.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if successfully detected.
 * @retval  VINF_DBGF_OS_NOT_DETCTED if we cannot figure it out.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pszName     Where to store the OS name. Empty string if not detected.
 * @param   cchName     Size of the buffer.
 */
DBGFR3DECL(int) DBGFR3OSDetect(PVM pVM, char *pszName, size_t cchName)
{
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrNullReturn(pszName, VERR_INVALID_POINTER);

    /*
     * Cycle thru the detection routines.
     */
    if (pszName && cchName)
        *pszName = '\0';
    PDBGFOS const pOldOS = pVM->dbgf.s.pCurOS;
    pVM->dbgf.s.pCurOS = NULL;

    for (PDBGFOS pNewOS = pVM->dbgf.s.pOSHead; pNewOS; pNewOS = pNewOS->pNext)
        if (pNewOS->pReg->pfnProbe(pVM, pNewOS->abData))
        {
            int rc;
            pVM->dbgf.s.pCurOS = pNewOS;
            if (pOldOS == pNewOS)
                rc = pNewOS->pReg->pfnRefresh(pVM, pNewOS->abData);
            else
            {
                if (pOldOS)
                    pOldOS->pReg->pfnTerm(pVM, pNewOS->abData);
                rc = pNewOS->pReg->pfnInit(pVM, pNewOS->abData);
            }
            if (pszName && cchName)
                strncat(pszName, pNewOS->pReg->szName, cchName);
            return rc;
        }

    /* not found */
    if (pOldOS)
        pOldOS->pReg->pfnTerm(pVM, pOldOS->abData);
    return VINF_DBGF_OS_NOT_DETCTED;
}


/**
 * Queries the name and/or version string for the guest OS.
 *
 * It goes without saying that this querying is done using the current
 * guest OS digger and not additions or user configuration.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pszName         Where to store the OS name. Optional.
 * @param   cchName         The size of the name buffer.
 * @param   pszVersion      Where to store the version string. Optional.
 * @param   cchVersion      The size of the version buffer.
 */
DBGFR3DECL(int) DBGFR3OSNameAndVersion(PVM pVM, char *pszName, size_t cchName, char *pszVersion, size_t cchVersion)
{
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrNullReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszVersion, VERR_INVALID_POINTER);

    /*
     * Initialize the output up front.
     */
    if (pszName && cchName)
        *pszName = '\0';
    if (pszVersion && cchVersion)
        *pszVersion = '\0';

    /*
     * Any known OS?
     */
    if (pVM->dbgf.s.pCurOS)
    {
        int rc = VINF_SUCCESS;
        if (pszName && cchName)
        {
            size_t cch = strlen(pVM->dbgf.s.pCurOS->pReg->szName);
            if (cchName > cch)
                memcpy(pszName, pVM->dbgf.s.pCurOS->pReg->szName, cch + 1);
            else
            {
                memcpy(pszName, pVM->dbgf.s.pCurOS->pReg->szName, cchName - 1);
                pszName[cchName - 1] = '\0';
                rc = VINF_BUFFER_OVERFLOW;
            }
        }

        if (pszVersion && cchVersion)
        {
            int rc2 = pVM->dbgf.s.pCurOS->pReg->pfnQueryVersion(pVM, pVM->dbgf.s.pCurOS->abData, pszVersion, cchVersion);
            if (RT_FAILURE(rc2) || rc == VINF_SUCCESS)
                rc = rc2;
        }
        return rc;
    }

    return VERR_DBGF_OS_NOT_DETCTED;
}


/**
 * Query an optional digger interface.
 *
 * @returns Pointer to the digger interface on success, NULL if the interfaces isn't
 *          available or no active guest OS digger.
 * @param   pVM         Pointer to the shared VM structure.
 * @param   enmIf       The interface identifier.
 */
DBGFR3DECL(void *) DBGFR3OSQueryInterface(PVM pVM, DBGFOSINTERFACE enmIf)
{
    AssertMsgReturn(enmIf > DBGFOSINTERFACE_INVALID && enmIf < DBGFOSINTERFACE_END, ("%d\n", enmIf), NULL);
    if (pVM->dbgf.s.pCurOS)
        return pVM->dbgf.s.pCurOS->pReg->pfnQueryInterface(pVM, pVM->dbgf.s.pCurOS->abData, enmIf);
    return NULL;
}

