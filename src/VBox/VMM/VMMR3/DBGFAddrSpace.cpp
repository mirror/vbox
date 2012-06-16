/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Address Space Management.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgf_addr_space     DBGFAddrSpace - Address Space Management
 *
 * What's an address space? It's mainly a convenient way of stuffing
 * module segments and ad-hoc symbols together. It will also help out
 * when the debugger gets extended to deal with user processes later.
 *
 * There are two standard address spaces that will always be present:
 *   - The physical address space.
 *   - The global virtual address space.
 *
 * Additional address spaces will be added and removed at runtime for
 * guest processes. The global virtual address space will be used to
 * track the kernel parts of the OS, or at least the bits of the kernel
 * that is part of all address spaces (mac os x and 4G/4G patched linux).
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/mm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/path.h>
#include <iprt/param.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Address space database node.
 */
typedef struct DBGFASDBNODE
{
    /** The node core for DBGF::AsHandleTree, the key is the address space handle. */
    AVLPVNODECORE   HandleCore;
    /** The node core for DBGF::AsPidTree, the key is the process id. */
    AVLU32NODECORE  PidCore;
    /** The node core for DBGF::AsNameSpace, the string is the address space name. */
    RTSTRSPACECORE  NameCore;

} DBGFASDBNODE;
/** Pointer to an address space database node. */
typedef DBGFASDBNODE *PDBGFASDBNODE;


/**
 * For dbgfR3AsLoadImageOpenData and dbgfR3AsLoadMapOpenData.
 */
typedef struct DBGFR3ASLOADOPENDATA
{
    const char *pszModName;
    RTGCUINTPTR uSubtrahend;
    uint32_t fFlags;
    RTDBGMOD hMod;
} DBGFR3ASLOADOPENDATA;

/**
 * Callback for dbgfR3AsSearchPath and dbgfR3AsSearchEnvPath.
 *
 * @returns VBox status code. If success, then the search is completed.
 * @param   pszFilename     The file name under evaluation.
 * @param   pvUser          The user argument.
 */
typedef int FNDBGFR3ASSEARCHOPEN(const char *pszFilename, void *pvUser);
/** Pointer to a FNDBGFR3ASSEARCHOPEN. */
typedef FNDBGFR3ASSEARCHOPEN *PFNDBGFR3ASSEARCHOPEN;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Locks the address space database for writing. */
#define DBGF_AS_DB_LOCK_WRITE(pVM) \
    do { \
        int rcSem = RTSemRWRequestWrite((pVM)->dbgf.s.hAsDbLock, RT_INDEFINITE_WAIT); \
        AssertRC(rcSem); \
    } while (0)

/** Unlocks the address space database after writing. */
#define DBGF_AS_DB_UNLOCK_WRITE(pVM) \
    do { \
        int rcSem = RTSemRWReleaseWrite((pVM)->dbgf.s.hAsDbLock); \
        AssertRC(rcSem); \
    } while (0)

/** Locks the address space database for reading. */
#define DBGF_AS_DB_LOCK_READ(pVM) \
    do { \
        int rcSem = RTSemRWRequestRead((pVM)->dbgf.s.hAsDbLock, RT_INDEFINITE_WAIT); \
        AssertRC(rcSem); \
    } while (0)

/** Unlocks the address space database after reading. */
#define DBGF_AS_DB_UNLOCK_READ(pVM) \
    do { \
        int rcSem = RTSemRWReleaseRead((pVM)->dbgf.s.hAsDbLock); \
        AssertRC(rcSem); \
    } while (0)



/**
 * Initializes the address space parts of DBGF.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 */
int dbgfR3AsInit(PVM pVM)
{
    /*
     * Create the semaphore.
     */
    int rc = RTSemRWCreate(&pVM->dbgf.s.hAsDbLock);
    AssertRCReturn(rc, rc);

    /*
     * Create the standard address spaces.
     */
    RTDBGAS hDbgAs;
    rc = RTDbgAsCreate(&hDbgAs, 0, RTGCPTR_MAX, "Global");
    AssertRCReturn(rc, rc);
    rc = DBGFR3AsAdd(pVM, hDbgAs, NIL_RTPROCESS);
    AssertRCReturn(rc, rc);
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_GLOBAL)] = hDbgAs;

    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_KERNEL)] = hDbgAs;

    rc = RTDbgAsCreate(&hDbgAs, 0, RTGCPHYS_MAX, "Physical");
    AssertRCReturn(rc, rc);
    rc = DBGFR3AsAdd(pVM, hDbgAs, NIL_RTPROCESS);
    AssertRCReturn(rc, rc);
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_PHYS)] = hDbgAs;

    rc = RTDbgAsCreate(&hDbgAs, 0, RTRCPTR_MAX, "HyperRawMode");
    AssertRCReturn(rc, rc);
    rc = DBGFR3AsAdd(pVM, hDbgAs, NIL_RTPROCESS);
    AssertRCReturn(rc, rc);
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_RC)] = hDbgAs;
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_RC_AND_GC_GLOBAL)] = hDbgAs;

    rc = RTDbgAsCreate(&hDbgAs, 0, RTR0PTR_MAX, "HyperRing0");
    AssertRCReturn(rc, rc);
    rc = DBGFR3AsAdd(pVM, hDbgAs, NIL_RTPROCESS);
    AssertRCReturn(rc, rc);
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_R0)] = hDbgAs;

    return VINF_SUCCESS;
}


/**
 * Callback used by dbgfR3AsTerm / RTAvlPVDestroy to release an address space.
 *
 * @returns 0.
 * @param   pNode           The address space database node.
 * @param   pvIgnore        NULL.
 */
static DECLCALLBACK(int) dbgfR3AsTermDestroyNode(PAVLPVNODECORE pNode, void *pvIgnore)
{
    PDBGFASDBNODE pDbNode = (PDBGFASDBNODE)pNode;
    RTDbgAsRelease((RTDBGAS)pDbNode->HandleCore.Key);
    pDbNode->HandleCore.Key = NIL_RTDBGAS;
    /* Don't bother freeing it here as MM will free it soon and MM is much at
       it when doing it wholesale instead of piecemeal. */
    NOREF(pvIgnore);
    return 0;
}


/**
 * Terminates the address space parts of DBGF.
 *
 * @param   pVM             Pointer to the VM.
 */
void dbgfR3AsTerm(PVM pVM)
{
    /*
     * Create the semaphore.
     */
    int rc = RTSemRWDestroy(pVM->dbgf.s.hAsDbLock);
    AssertRC(rc);
    pVM->dbgf.s.hAsDbLock = NIL_RTSEMRW;

    /*
     * Release all the address spaces.
     */
    RTAvlPVDestroy(&pVM->dbgf.s.AsHandleTree, dbgfR3AsTermDestroyNode, NULL);
    for (size_t i = 0; i < RT_ELEMENTS(pVM->dbgf.s.ahAsAliases); i++)
    {
        RTDbgAsRelease(pVM->dbgf.s.ahAsAliases[i]);
        pVM->dbgf.s.ahAsAliases[i] = NIL_RTDBGAS;
    }
}


/**
 * Relocates the RC address space.
 *
 * @param   pVM             Pointer to the VM.
 * @param   offDelta        The relocation delta.
 */
void dbgfR3AsRelocate(PVM pVM, RTGCUINTPTR offDelta)
{
    /** @todo */
    NOREF(pVM); NOREF(offDelta);
}


/**
 * Adds the address space to the database.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   hDbgAs          The address space handle. The reference of the
 *                          caller will NOT be consumed.
 * @param   ProcId          The process id or NIL_RTPROCESS.
 */
VMMR3DECL(int) DBGFR3AsAdd(PVM pVM, RTDBGAS hDbgAs, RTPROCESS ProcId)
{
    /*
     * Input validation.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    const char *pszName = RTDbgAsName(hDbgAs);
    if (!pszName)
        return VERR_INVALID_HANDLE;
    uint32_t cRefs = RTDbgAsRetain(hDbgAs);
    if (cRefs == UINT32_MAX)
        return VERR_INVALID_HANDLE;

    /*
     * Allocate a tracking node.
     */
    int rc = VERR_NO_MEMORY;
    PDBGFASDBNODE pDbNode = (PDBGFASDBNODE)MMR3HeapAlloc(pVM, MM_TAG_DBGF_AS, sizeof(*pDbNode));
    if (pDbNode)
    {
        pDbNode->HandleCore.Key     = hDbgAs;
        pDbNode->PidCore.Key        = ProcId;
        pDbNode->NameCore.pszString = pszName;
        pDbNode->NameCore.cchString = strlen(pszName);
        DBGF_AS_DB_LOCK_WRITE(pVM);
        if (RTStrSpaceInsert(&pVM->dbgf.s.AsNameSpace, &pDbNode->NameCore))
        {
            if (RTAvlPVInsert(&pVM->dbgf.s.AsHandleTree, &pDbNode->HandleCore))
            {
                DBGF_AS_DB_UNLOCK_WRITE(pVM);
                return VINF_SUCCESS;
            }

            /* bail out */
            RTStrSpaceRemove(&pVM->dbgf.s.AsNameSpace, pszName);
        }
        DBGF_AS_DB_UNLOCK_WRITE(pVM);
        MMR3HeapFree(pDbNode);
    }
    RTDbgAsRelease(hDbgAs);
    return rc;
}


/**
 * Delete an address space from the database.
 *
 * The address space must not be engaged as any of the standard aliases.
 *
 * @returns VBox status code.
 * @retval  VERR_SHARING_VIOLATION if in use as an alias.
 * @retval  VERR_NOT_FOUND if not found in the address space database.
 *
 * @param   pVM             Pointer to the VM.
 * @param   hDbgAs          The address space handle. Aliases are not allowed.
 */
VMMR3DECL(int) DBGFR3AsDelete(PVM pVM, RTDBGAS hDbgAs)
{
    /*
     * Input validation. Retain the address space so it can be released outside
     * the lock as well as validated.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    if (hDbgAs == NIL_RTDBGAS)
        return VINF_SUCCESS;
    uint32_t cRefs = RTDbgAsRetain(hDbgAs);
    if (cRefs == UINT32_MAX)
        return VERR_INVALID_HANDLE;
    RTDbgAsRelease(hDbgAs);

    DBGF_AS_DB_LOCK_WRITE(pVM);

    /*
     * You cannot delete any of the aliases.
     */
    for (size_t i = 0; i < RT_ELEMENTS(pVM->dbgf.s.ahAsAliases); i++)
        if (pVM->dbgf.s.ahAsAliases[i] == hDbgAs)
        {
            DBGF_AS_DB_UNLOCK_WRITE(pVM);
            return VERR_SHARING_VIOLATION;
        }

    /*
     * Ok, try remove it from the database.
     */
    PDBGFASDBNODE pDbNode = (PDBGFASDBNODE)RTAvlPVRemove(&pVM->dbgf.s.AsHandleTree, hDbgAs);
    if (!pDbNode)
    {
        DBGF_AS_DB_UNLOCK_WRITE(pVM);
        return VERR_NOT_FOUND;
    }
    RTStrSpaceRemove(&pVM->dbgf.s.AsNameSpace, pDbNode->NameCore.pszString);
    if (pDbNode->PidCore.Key != NIL_RTPROCESS)
        RTAvlU32Remove(&pVM->dbgf.s.AsPidTree, pDbNode->PidCore.Key);

    DBGF_AS_DB_UNLOCK_WRITE(pVM);

    /*
     * Free the resources.
     */
    RTDbgAsRelease(hDbgAs);
    MMR3HeapFree(pDbNode);

    return VINF_SUCCESS;
}


/**
 * Changes an alias to point to a new address space.
 *
 * Not all the aliases can be changed, currently it's only DBGF_AS_GLOBAL
 * and DBGF_AS_KERNEL.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   hAlias          The alias to change.
 * @param   hAliasFor       The address space hAlias should be an alias for.
 *                          This can be an alias. The caller's reference to
 *                          this address space will NOT be consumed.
 */
VMMR3DECL(int) DBGFR3AsSetAlias(PVM pVM, RTDBGAS hAlias, RTDBGAS hAliasFor)
{
    /*
     * Input validation.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertMsgReturn(DBGF_AS_IS_ALIAS(hAlias), ("%p\n", hAlias), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!DBGF_AS_IS_FIXED_ALIAS(hAlias), ("%p\n", hAlias), VERR_INVALID_PARAMETER);
    RTDBGAS hRealAliasFor = DBGFR3AsResolveAndRetain(pVM, hAliasFor);
    if (hRealAliasFor == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;

    /*
     * Make sure the handle has is already in the database.
     */
    int rc = VERR_NOT_FOUND;
    DBGF_AS_DB_LOCK_WRITE(pVM);
    if (RTAvlPVGet(&pVM->dbgf.s.AsHandleTree, hRealAliasFor))
    {
        /*
         * Update the alias table and release the current address space.
         */
        RTDBGAS hAsOld;
        ASMAtomicXchgHandle(&pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(hAlias)], hRealAliasFor, &hAsOld);
        uint32_t cRefs = RTDbgAsRelease(hAsOld);
        Assert(cRefs > 0); Assert(cRefs != UINT32_MAX); NOREF(cRefs);
        rc = VINF_SUCCESS;
    }
    DBGF_AS_DB_UNLOCK_WRITE(pVM);

    return rc;
}


/**
 * @callback_method_impl{FNPDMR3ENUM}
 */
static DECLCALLBACK(int) dbgfR3AsLazyPopulateR0Callback(PVM pVM, const char *pszFilename, const char *pszName,
                                                        RTUINTPTR ImageBase, size_t cbImage, bool fRC, void *pvArg)
{
    NOREF(pVM); NOREF(cbImage);

    /* Only ring-0 modules. */
    if (!fRC)
    {
        RTDBGMOD hDbgMod;
        int rc = RTDbgModCreateFromImage(&hDbgMod, pszFilename, pszName, 0 /*fFlags*/);
        if (RT_SUCCESS(rc))
        {
            rc = RTDbgAsModuleLink((RTDBGAS)pvArg, hDbgMod, ImageBase, 0 /*fFlags*/);
            if (RT_FAILURE(rc))
                LogRel(("DBGF: Failed to link module \"%s\" into DBGF_AS_R0 at %RTptr: %Rrc\n",
                        pszName, ImageBase, rc));
        }
        else
            LogRel(("DBGF: RTDbgModCreateFromImage failed with rc=%Rrc for module \"%s\" (%s)\n",
                    rc, pszName, pszFilename));
    }
    return VINF_SUCCESS;
}


/**
 * Lazily populates the specified address space.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   hAlias              The alias.
 */
static void dbgfR3AsLazyPopulate(PVM pVM, RTDBGAS hAlias)
{
    DBGF_AS_DB_LOCK_WRITE(pVM);
    uintptr_t iAlias = DBGF_AS_ALIAS_2_INDEX(hAlias);
    if (!pVM->dbgf.s.afAsAliasPopuplated[iAlias])
    {
        RTDBGAS hAs = pVM->dbgf.s.ahAsAliases[iAlias];
        if (hAlias == DBGF_AS_R0)
            PDMR3LdrEnumModules(pVM, dbgfR3AsLazyPopulateR0Callback, hAs);
        /** @todo what do we do about DBGF_AS_RC?  */

        pVM->dbgf.s.afAsAliasPopuplated[iAlias] = true;
    }
    DBGF_AS_DB_UNLOCK_WRITE(pVM);
}


/**
 * Resolves the address space handle into a real handle if it's an alias.
 *
 * @returns Real address space handle. NIL_RTDBGAS if invalid handle.
 *
 * @param   pVM             Pointer to the VM.
 * @param   hAlias          The possibly address space alias.
 *
 * @remarks Doesn't take any locks.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsResolve(PVM pVM, RTDBGAS hAlias)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);
    AssertCompileNS(NIL_RTDBGAS == (RTDBGAS)0);

    uintptr_t   iAlias = DBGF_AS_ALIAS_2_INDEX(hAlias);
    if (iAlias < DBGF_AS_COUNT)
        ASMAtomicReadHandle(&pVM->dbgf.s.ahAsAliases[iAlias], &hAlias);
    return hAlias;
}


/**
 * Resolves the address space handle into a real handle if it's an alias,
 * and retains whatever it is.
 *
 * @returns Real address space handle. NIL_RTDBGAS if invalid handle.
 *
 * @param   pVM             Pointer to the VM.
 * @param   hAlias          The possibly address space alias.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsResolveAndRetain(PVM pVM, RTDBGAS hAlias)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);
    AssertCompileNS(NIL_RTDBGAS == (RTDBGAS)0);

    uint32_t    cRefs;
    uintptr_t   iAlias = DBGF_AS_ALIAS_2_INDEX(hAlias);
    if (iAlias < DBGF_AS_COUNT)
    {
        if (DBGF_AS_IS_FIXED_ALIAS(hAlias))
        {
            /* Perform lazy address space population. */
            if (!pVM->dbgf.s.afAsAliasPopuplated[iAlias])
                dbgfR3AsLazyPopulate(pVM, hAlias);

            /* Won't ever change, no need to grab the lock. */
            hAlias = pVM->dbgf.s.ahAsAliases[iAlias];
            cRefs = RTDbgAsRetain(hAlias);
        }
        else
        {
            /* May change, grab the lock so we can read it safely. */
            DBGF_AS_DB_LOCK_READ(pVM);
            hAlias = pVM->dbgf.s.ahAsAliases[iAlias];
            cRefs = RTDbgAsRetain(hAlias);
            DBGF_AS_DB_UNLOCK_READ(pVM);
        }
    }
    else
        /* Not an alias, just retain it. */
        cRefs = RTDbgAsRetain(hAlias);

    return cRefs != UINT32_MAX ? hAlias : NIL_RTDBGAS;
}


/**
 * Query an address space by name.
 *
 * @returns Retained address space handle if found, NIL_RTDBGAS if not.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The name.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsQueryByName(PVM pVM, const char *pszName)
{
    /*
     * Validate the input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, NIL_RTDBGAS);
    AssertPtrReturn(pszName, NIL_RTDBGAS);
    AssertReturn(*pszName, NIL_RTDBGAS);

    /*
     * Look it up in the string space and retain the result.
     */
    RTDBGAS hDbgAs = NIL_RTDBGAS;
    DBGF_AS_DB_LOCK_READ(pVM);

    PRTSTRSPACECORE pNode = RTStrSpaceGet(&pVM->dbgf.s.AsNameSpace, pszName);
    if (pNode)
    {
        PDBGFASDBNODE pDbNode = RT_FROM_MEMBER(pNode, DBGFASDBNODE, NameCore);
        hDbgAs = (RTDBGAS)pDbNode->HandleCore.Key;
        uint32_t cRefs = RTDbgAsRetain(hDbgAs);
        if (RT_UNLIKELY(cRefs == UINT32_MAX))
            hDbgAs = NIL_RTDBGAS;
    }
    DBGF_AS_DB_UNLOCK_READ(pVM);

    return hDbgAs;
}


/**
 * Query an address space by process ID.
 *
 * @returns Retained address space handle if found, NIL_RTDBGAS if not.
 *
 * @param   pVM         Pointer to the VM.
 * @param   ProcId      The process ID.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsQueryByPid(PVM pVM, RTPROCESS ProcId)
{
    /*
     * Validate the input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, NIL_RTDBGAS);
    AssertReturn(ProcId != NIL_RTPROCESS, NIL_RTDBGAS);

    /*
     * Look it up in the PID tree and retain the result.
     */
    RTDBGAS hDbgAs = NIL_RTDBGAS;
    DBGF_AS_DB_LOCK_READ(pVM);

    PAVLU32NODECORE pNode = RTAvlU32Get(&pVM->dbgf.s.AsPidTree, ProcId);
    if (pNode)
    {
        PDBGFASDBNODE pDbNode = RT_FROM_MEMBER(pNode, DBGFASDBNODE, PidCore);
        hDbgAs = (RTDBGAS)pDbNode->HandleCore.Key;
        uint32_t cRefs = RTDbgAsRetain(hDbgAs);
        if (RT_UNLIKELY(cRefs == UINT32_MAX))
            hDbgAs = NIL_RTDBGAS;
    }
    DBGF_AS_DB_UNLOCK_READ(pVM);

    return hDbgAs;
}


/**
 * Searches for the file in the path.
 *
 * The file is first tested without any path modification, then we walk the path
 * looking in each directory.
 *
 * @returns VBox status code.
 * @param   pszFilename     The file to search for.
 * @param   pszPath         The search path.
 * @param   pfnOpen         The open callback function.
 * @param   pvUser          User argument for the callback.
 */
static int dbgfR3AsSearchPath(const char *pszFilename, const char *pszPath, PFNDBGFR3ASSEARCHOPEN pfnOpen, void *pvUser)
{
    char szFound[RTPATH_MAX];

    /* Check the filename length. */
    size_t const    cchFilename = strlen(pszFilename);
    if (cchFilename >= sizeof(szFound))
        return VERR_FILENAME_TOO_LONG;
    const char     *pszName = RTPathFilename(pszFilename);
    if (!pszName)
        return VERR_IS_A_DIRECTORY;
    size_t const    cchName = strlen(pszName);

    /*
     * Try default location first.
     */
    memcpy(szFound, pszFilename, cchFilename + 1);
    int rc = pfnOpen(szFound, pvUser);
    if (RT_SUCCESS(rc))
        return rc;

    /*
     * Walk the search path.
     */
    const char *psz = pszPath;
    while (*psz)
    {
        /* Skip leading blanks - no directories with leading spaces, thank you. */
        while (RT_C_IS_BLANK(*psz))
            psz++;

        /* Find the end of this element. */
        const char *pszNext;
        const char *pszEnd = strchr(psz, ';');
        if (!pszEnd)
            pszEnd = pszNext = strchr(psz, '\0');
        else
            pszNext = pszEnd + 1;
        if (pszEnd != psz)
        {
            size_t const cch = pszEnd - psz;
            if (cch + 1 + cchName < sizeof(szFound))
            {
                /** @todo RTPathCompose, RTPathComposeN(). This code isn't right
                 * for 'E:' on DOS systems. It may also create unwanted double slashes. */
                memcpy(szFound, psz, cch);
                szFound[cch] = '/';
                memcpy(szFound + cch + 1, pszName, cchName + 1);
                int rc2 = pfnOpen(szFound, pvUser);
                if (RT_SUCCESS(rc2))
                    return rc2;
                if (    rc2 != rc
                    &&  (   rc == VERR_FILE_NOT_FOUND
                         || rc == VERR_OPEN_FAILED))
                    rc = rc2;
            }
        }

        /* advance */
        psz = pszNext;
    }

    /*
     * Walk the path once again, this time do a depth search.
     */
    /** @todo do a depth search using the specified path. */

    /* failed */
    return rc;
}


/**
 * Same as dbgfR3AsSearchEnv, except that the path is taken from the environment.
 *
 * If the environment variable doesn't exist, the current directory is searched
 * instead.
 *
 * @returns VBox status code.
 * @param   pszFilename     The filename.
 * @param   pszEnvVar       The environment variable name.
 * @param   pfnOpen         The open callback function.
 * @param   pvUser          User argument for the callback.
 */
static int dbgfR3AsSearchEnvPath(const char *pszFilename, const char *pszEnvVar, PFNDBGFR3ASSEARCHOPEN pfnOpen, void *pvUser)
{
    int     rc;
    char   *pszPath = RTEnvDupEx(RTENV_DEFAULT, pszEnvVar);
    if (pszPath)
    {
        rc = dbgfR3AsSearchPath(pszFilename, pszPath, pfnOpen, pvUser);
        RTStrFree(pszPath);
    }
    else
        rc = dbgfR3AsSearchPath(pszFilename, ".", pfnOpen, pvUser);
    return rc;
}


/**
 * Same as dbgfR3AsSearchEnv, except that the path is taken from the DBGF config
 * (CFGM).
 *
 * Nothing is done if the CFGM variable isn't set.
 *
 * @returns VBox status code.
 * @param   pszFilename     The filename.
 * @param   pszCfgValue     The name of the config variable (under /DBGF/).
 * @param   pfnOpen         The open callback function.
 * @param   pvUser          User argument for the callback.
 */
static int dbgfR3AsSearchCfgPath(PVM pVM, const char *pszFilename, const char *pszCfgValue, PFNDBGFR3ASSEARCHOPEN pfnOpen, void *pvUser)
{
    char *pszPath;
    int rc = CFGMR3QueryStringAllocDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "/DBGF"), pszCfgValue, &pszPath, NULL);
    if (RT_FAILURE(rc))
        return rc;
    if (!pszPath)
        return VERR_FILE_NOT_FOUND;
    rc = dbgfR3AsSearchPath(pszFilename, pszPath, pfnOpen, pvUser);
    MMR3HeapFree(pszPath);
    return rc;
}


/**
 * Callback function used by DBGFR3AsLoadImage.
 *
 * @returns VBox status code.
 * @param   pszFilename     The filename under evaluation.
 * @param   pvUser          Use arguments (DBGFR3ASLOADOPENDATA).
 */
static DECLCALLBACK(int) dbgfR3AsLoadImageOpen(const char *pszFilename, void *pvUser)
{
    DBGFR3ASLOADOPENDATA *pData = (DBGFR3ASLOADOPENDATA *)pvUser;
    return RTDbgModCreateFromImage(&pData->hMod, pszFilename, pData->pszModName, pData->fFlags);
}


/**
 * Load symbols from an executable module into the specified address space.
 *
 * If an module exist at the specified address it will be replaced by this
 * call, otherwise a new module is created.
 *
 * @returns VBox status code.
 *
 * @param   pVM             Pointer to the VM.
 * @param   hDbgAs          The address space.
 * @param   pszFilename     The filename of the executable module.
 * @param   pszModName      The module name. If NULL, then then the file name
 *                          base is used (no extension or nothing).
 * @param   pModAddress     The load address of the module.
 * @param   iModSeg         The segment to load, pass NIL_RTDBGSEGIDX to load
 *                          the whole image.
 * @param   fFlags          Flags reserved for future extensions, must be 0.
 */
VMMR3DECL(int) DBGFR3AsLoadImage(PVM pVM, RTDBGAS hDbgAs, const char *pszFilename, const char *pszModName, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, uint32_t fFlags)
{
    /*
     * Validate input
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pVM, pModAddress), VERR_INVALID_PARAMETER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);
    RTDBGAS hRealAS = DBGFR3AsResolveAndRetain(pVM, hDbgAs);
    if (hRealAS == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;

    /*
     * Do the work.
     */
    DBGFR3ASLOADOPENDATA Data;
    Data.pszModName = pszModName;
    Data.uSubtrahend = 0;
    Data.fFlags = 0;
    Data.hMod = NIL_RTDBGMOD;
    int rc = dbgfR3AsSearchCfgPath(pVM, pszFilename, "ImagePath", dbgfR3AsLoadImageOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_IMAGE_PATH", dbgfR3AsLoadImageOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchCfgPath(pVM, pszFilename, "Path", dbgfR3AsLoadImageOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_PATH", dbgfR3AsLoadImageOpen, &Data);
    if (RT_SUCCESS(rc))
    {
        rc = DBGFR3AsLinkModule(pVM, hRealAS, Data.hMod, pModAddress, iModSeg, 0);
        if (RT_FAILURE(rc))
            RTDbgModRelease(Data.hMod);
    }

    RTDbgAsRelease(hRealAS);
    return rc;
}


/**
 * Callback function used by DBGFR3AsLoadMap.
 *
 * @returns VBox status code.
 * @param   pszFilename     The filename under evaluation.
 * @param   pvUser          Use arguments (DBGFR3ASLOADOPENDATA).
 */
static DECLCALLBACK(int) dbgfR3AsLoadMapOpen(const char *pszFilename, void *pvUser)
{
    DBGFR3ASLOADOPENDATA *pData = (DBGFR3ASLOADOPENDATA *)pvUser;
    return RTDbgModCreateFromMap(&pData->hMod, pszFilename, pData->pszModName, pData->uSubtrahend, pData->fFlags);
}


/**
 * Load symbols from a map file into a module at the specified address space.
 *
 * If an module exist at the specified address it will be replaced by this
 * call, otherwise a new module is created.
 *
 * @returns VBox status code.
 *
 * @param   pVM             Pointer to the VM.
 * @param   hDbgAs          The address space.
 * @param   pszFilename     The map file.
 * @param   pszModName      The module name. If NULL, then then the file name
 *                          base is used (no extension or nothing).
 * @param   pModAddress     The load address of the module.
 * @param   iModSeg         The segment to load, pass NIL_RTDBGSEGIDX to load
 *                          the whole image.
 * @param   uSubtrahend     Value to to subtract from the symbols in the map
 *                          file. This is useful for the linux System.map and
 *                          /proc/kallsyms.
 * @param   fFlags          Flags reserved for future extensions, must be 0.
 */
VMMR3DECL(int) DBGFR3AsLoadMap(PVM pVM, RTDBGAS hDbgAs, const char *pszFilename, const char *pszModName,
                               PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, RTGCUINTPTR uSubtrahend, uint32_t fFlags)
{
    /*
     * Validate input
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pVM, pModAddress), VERR_INVALID_PARAMETER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);
    RTDBGAS hRealAS = DBGFR3AsResolveAndRetain(pVM, hDbgAs);
    if (hRealAS == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;

    /*
     * Do the work.
     */
    DBGFR3ASLOADOPENDATA Data;
    Data.pszModName = pszModName;
    Data.uSubtrahend = uSubtrahend;
    Data.fFlags = 0;
    Data.hMod = NIL_RTDBGMOD;
    int rc = dbgfR3AsSearchCfgPath(pVM, pszFilename, "MapPath", dbgfR3AsLoadMapOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_MAP_PATH", dbgfR3AsLoadMapOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchCfgPath(pVM, pszFilename, "Path", dbgfR3AsLoadMapOpen, &Data);
    if (RT_FAILURE(rc))
        rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_PATH", dbgfR3AsLoadMapOpen, &Data);
    if (RT_SUCCESS(rc))
    {
        rc = DBGFR3AsLinkModule(pVM, hRealAS, Data.hMod, pModAddress, iModSeg, 0);
        if (RT_FAILURE(rc))
            RTDbgModRelease(Data.hMod);
    }

    RTDbgAsRelease(hRealAS);
    return rc;
}


/**
 * Wrapper around RTDbgAsModuleLink, RTDbgAsModuleLinkSeg and DBGFR3AsResolve.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   hDbgAs          The address space handle.
 * @param   hMod            The module handle.
 * @param   pModAddress     The link address.
 * @param   iModSeg         The segment to link, NIL_RTDBGSEGIDX for the entire image.
 * @param   fFlags          Flags to pass to the link functions, see RTDBGASLINK_FLAGS_*.
 */
VMMR3DECL(int) DBGFR3AsLinkModule(PVM pVM, RTDBGAS hDbgAs, RTDBGMOD hMod, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, uint32_t fFlags)
{
    /*
     * Input validation.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(DBGFR3AddrIsValid(pVM, pModAddress), VERR_INVALID_PARAMETER);
    RTDBGAS hRealAS = DBGFR3AsResolveAndRetain(pVM, hDbgAs);
    if (hRealAS == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    int rc;
    if (iModSeg == NIL_RTDBGSEGIDX)
        rc = RTDbgAsModuleLink(hRealAS, hMod, pModAddress->FlatPtr, fFlags);
    else
        rc = RTDbgAsModuleLinkSeg(hRealAS, hMod, iModSeg, pModAddress->FlatPtr, fFlags);

    RTDbgAsRelease(hRealAS);
    return rc;
}


/**
 * Adds the module name to the symbol name.
 *
 * @param   pSymbol         The symbol info (in/out).
 * @param   hMod            The module handle.
 */
static void dbgfR3AsSymbolJoinNames(PRTDBGSYMBOL pSymbol, RTDBGMOD hMod)
{
    /* Figure the lengths, adjust them if the result is too long. */
    const char *pszModName = RTDbgModName(hMod);
    size_t      cchModName = strlen(pszModName);
    size_t      cchSymbol  = strlen(pSymbol->szName);
    if (cchModName + 1 + cchSymbol >= sizeof(pSymbol->szName))
    {
        if (cchModName >= sizeof(pSymbol->szName) / 4)
            cchModName = sizeof(pSymbol->szName) / 4;
        if (cchModName + 1 + cchSymbol >= sizeof(pSymbol->szName))
            cchSymbol = sizeof(pSymbol->szName) - cchModName - 2;
        Assert(cchModName + 1 + cchSymbol < sizeof(pSymbol->szName));
    }

    /* Do the moving and copying. */
    memmove(&pSymbol->szName[cchModName + 1], &pSymbol->szName[0], cchSymbol + 1);
    memcpy(&pSymbol->szName[0], pszModName, cchModName);
    pSymbol->szName[cchModName] = '!';
}


/** Temporary symbol conversion function. */
static void dbgfR3AsSymbolConvert(PRTDBGSYMBOL pSymbol, PCDBGFSYMBOL pDbgfSym)
{
    pSymbol->offSeg = pSymbol->Value = pDbgfSym->Value;
    pSymbol->cb = pDbgfSym->cb;
    pSymbol->iSeg = 0;
    pSymbol->fFlags = 0;
    pSymbol->iOrdinal = UINT32_MAX;
    strcpy(pSymbol->szName, pDbgfSym->szName);
}


/**
 * Query a symbol by address.
 *
 * The returned symbol is the one we consider closes to the specified address.
 *
 * @returns VBox status code. See RTDbgAsSymbolByAddr.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   hDbgAs              The address space handle.
 * @param   pAddress            The address to lookup.
 * @param   poffDisp            Where to return the distance between the
 *                              returned symbol and pAddress. Optional.
 * @param   pSymbol             Where to return the symbol information.
 *                              The returned symbol name will be prefixed by
 *                              the module name as far as space allows.
 * @param   phMod               Where to return the module handle. Optional.
 */
VMMR3DECL(int) DBGFR3AsSymbolByAddr(PVM pVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress,
                                    PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod)
{
    /*
     * Implement the special address space aliases the lazy way.
     */
    if (hDbgAs == DBGF_AS_RC_AND_GC_GLOBAL)
    {
        int rc = DBGFR3AsSymbolByAddr(pVM, DBGF_AS_RC, pAddress, poffDisp, pSymbol, phMod);
        if (RT_FAILURE(rc))
            rc = DBGFR3AsSymbolByAddr(pVM, DBGF_AS_GLOBAL, pAddress, poffDisp, pSymbol, phMod);
        return rc;
    }

    /*
     * Input validation.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(DBGFR3AddrIsValid(pVM, pAddress), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(poffDisp, VERR_INVALID_POINTER);
    AssertPtrReturn(pSymbol, VERR_INVALID_POINTER);
    AssertPtrNullReturn(phMod, VERR_INVALID_POINTER);
    if (poffDisp)
        *poffDisp = 0;
    if (phMod)
        *phMod = NIL_RTDBGMOD;
    RTDBGAS hRealAS = DBGFR3AsResolveAndRetain(pVM, hDbgAs);
    if (hRealAS == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;

    /*
     * Do the lookup.
     */
    RTDBGMOD hMod;
    int rc = RTDbgAsSymbolByAddr(hRealAS, pAddress->FlatPtr, RTDBGSYMADDR_FLAGS_LESS_OR_EQUAL, poffDisp, pSymbol, &hMod);
    if (RT_SUCCESS(rc))
    {
        dbgfR3AsSymbolJoinNames(pSymbol, hMod);
        if (!phMod)
            RTDbgModRelease(hMod);
    }
    /* Temporary conversions. */
    else if (hDbgAs == DBGF_AS_GLOBAL)
    {
        DBGFSYMBOL DbgfSym;
        rc = DBGFR3SymbolByAddr(pVM, pAddress->FlatPtr, poffDisp, &DbgfSym);
        if (RT_SUCCESS(rc))
            dbgfR3AsSymbolConvert(pSymbol, &DbgfSym);
    }
    else if (hDbgAs == DBGF_AS_R0)
    {
        RTR0PTR     R0PtrMod;
        char        szNearSym[260];
        RTR0PTR     R0PtrNearSym;
        RTR0PTR     R0PtrNearSym2;
        rc = PDMR3LdrQueryR0ModFromPC(pVM, pAddress->FlatPtr,
                                      pSymbol->szName, sizeof(pSymbol->szName) / 2, &R0PtrMod,
                                      &szNearSym[0],   sizeof(szNearSym),           &R0PtrNearSym,
                                      NULL,            0,                           &R0PtrNearSym2);
        if (RT_SUCCESS(rc))
        {
            pSymbol->offSeg     = pSymbol->Value = R0PtrNearSym;
            pSymbol->cb         = R0PtrNearSym2 > R0PtrNearSym ? R0PtrNearSym2 - R0PtrNearSym : 0;
            pSymbol->iSeg       = 0;
            pSymbol->fFlags     = 0;
            pSymbol->iOrdinal   = UINT32_MAX;
            size_t offName = strlen(pSymbol->szName);
            pSymbol->szName[offName++] = '!';
            size_t cchNearSym = strlen(szNearSym);
            if (cchNearSym + offName >= sizeof(pSymbol->szName))
                cchNearSym = sizeof(pSymbol->szName) - offName - 1;
            strncpy(&pSymbol->szName[offName], szNearSym, cchNearSym);
            pSymbol->szName[offName + cchNearSym] = '\0';
            if (poffDisp)
                *poffDisp = pAddress->FlatPtr - pSymbol->Value;
        }
    }

    return rc;
}


/**
 * Convenience function that combines RTDbgSymbolDup and DBGFR3AsSymbolByAddr.
 *
 * @returns Pointer to the symbol on success. This must be free using
 *          RTDbgSymbolFree(). NULL is returned if not found or any error
 *          occurs.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   hDbgAs              See DBGFR3AsSymbolByAddr.
 * @param   pAddress            See DBGFR3AsSymbolByAddr.
 * @param   poffDisp            See DBGFR3AsSymbolByAddr.
 * @param   phMod               See DBGFR3AsSymbolByAddr.
 */
VMMR3DECL(PRTDBGSYMBOL) DBGFR3AsSymbolByAddrA(PVM pVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress, PRTGCINTPTR poffDisp, PRTDBGMOD phMod)
{
    RTDBGSYMBOL SymInfo;
    int rc = DBGFR3AsSymbolByAddr(pVM, hDbgAs, pAddress, poffDisp, &SymInfo, phMod);
    if (RT_SUCCESS(rc))
        return RTDbgSymbolDup(&SymInfo);
    return NULL;
}


/**
 * Query a symbol by name.
 *
 * The symbol can be prefixed by a module name pattern to scope the search. The
 * pattern is a simple string pattern with '*' and '?' as wild chars. See
 * RTStrSimplePatternMatch().
 *
 * @returns VBox status code. See RTDbgAsSymbolByAddr.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   hDbgAs              The address space handle.
 * @param   pszSymbol           The symbol to search for, maybe prefixed by a
 *                              module pattern.
 * @param   pSymbol             Where to return the symbol information.
 *                              The returned symbol name will be prefixed by
 *                              the module name as far as space allows.
 * @param   phMod               Where to return the module handle. Optional.
 */
VMMR3DECL(int) DBGFR3AsSymbolByName(PVM pVM, RTDBGAS hDbgAs, const char *pszSymbol,
                                    PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod)
{
    /*
     * Implement the special address space aliases the lazy way.
     */
    if (hDbgAs == DBGF_AS_RC_AND_GC_GLOBAL)
    {
        int rc = DBGFR3AsSymbolByName(pVM, DBGF_AS_RC, pszSymbol, pSymbol, phMod);
        if (RT_FAILURE(rc))
            rc = DBGFR3AsSymbolByName(pVM, DBGF_AS_GLOBAL, pszSymbol, pSymbol, phMod);
        return rc;
    }

    /*
     * Input validation.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pSymbol, VERR_INVALID_POINTER);
    AssertPtrNullReturn(phMod, VERR_INVALID_POINTER);
    if (phMod)
        *phMod = NIL_RTDBGMOD;
    RTDBGAS hRealAS = DBGFR3AsResolveAndRetain(pVM, hDbgAs);
    if (hRealAS == NIL_RTDBGAS)
        return VERR_INVALID_HANDLE;


    /*
     * Do the lookup.
     */
    RTDBGMOD hMod;
    int rc = RTDbgAsSymbolByName(hRealAS, pszSymbol, pSymbol, &hMod);
    if (RT_SUCCESS(rc))
    {
        dbgfR3AsSymbolJoinNames(pSymbol, hMod);
        if (!phMod)
            RTDbgModRelease(hMod);
    }
    /* Temporary conversion. */
    else if (hDbgAs == DBGF_AS_GLOBAL)
    {
        DBGFSYMBOL DbgfSym;
        rc = DBGFR3SymbolByName(pVM, pszSymbol, &DbgfSym);
        if (RT_SUCCESS(rc))
            dbgfR3AsSymbolConvert(pSymbol, &DbgfSym);
    }

    return rc;
}

