/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Address Space Management.
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
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

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
 * @param   pVM             The VM handle.
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
    rc = RTDbgAsCreate(&hDbgAs, 0, RTRCPTR_MAX, "Global");
    AssertRCReturn(rc, rc);
    rc = DBGFR3AsAdd(pVM, hDbgAs, NIL_RTPROCESS);
    AssertRCReturn(rc, rc);
    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_GLOBAL)] = hDbgAs;

    RTDbgAsRetain(hDbgAs);
    pVM->dbgf.s.ahAsAliases[DBGF_AS_ALIAS_2_INDEX(DBGF_AS_KERNEL)] = hDbgAs;

    rc = RTDbgAsCreate(&hDbgAs, 0, RTRCPTR_MAX, "Physical");
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

    rc = RTDbgAsCreate(&hDbgAs, 0, RTRCPTR_MAX, "HyperRing0");
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
 * @param   pVM             The VM handle.
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
 * @param   pVM             The VM handle.
 * @param   offDelta        The relocation delta.
 */
void dbgfR3AsRelocate(PVM pVM, RTGCUINTPTR offDelta)
{
    /** @todo */
}


/**
 * Adds the address space to the database.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
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
        pDbNode->HandleCore.Key = hDbgAs;
        pDbNode->PidCore.Key = NIL_RTPROCESS;
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
 * @param   pVM             The VM handle.
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
 * @param   pVM             The VM handle.
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
    AssertMsgReturn(!DBGF_AS_IS_ALIAS(hAlias), ("%p\n", hAlias), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!DBGF_AS_IS_FIXED_ALIAS(hAlias), ("%p\n", hAlias), VERR_INVALID_PARAMETER);
    RTDBGAS hRealAliasFor = DBGFR3AsResolveAndRetain(pVM, hAlias);
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
        Assert(cRefs > 0);
        Assert(cRefs != UINT32_MAX);
        rc = VINF_SUCCESS;
    }
    DBGF_AS_DB_UNLOCK_WRITE(pVM);

    return rc;
}


/**
 * Resolves the address space handle into a real handle if it's an alias.
 *
 * @returns Real address space handle. NIL_RTDBGAS if invalid handle.
 *
 * @param   pVM             The VM handle.
 * @param   hAlias          The possibly address space alias.
 *
 * @remarks Doesn't take any locks.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsResolve(PVM pVM, RTDBGAS hAlias)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);
    AssertCompile(NIL_RTDBGAS == (RTDBGAS)0);

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
 * @param   pVM             The VM handle.
 * @param   hAlias          The possibly address space alias.
 */
VMMR3DECL(RTDBGAS) DBGFR3AsResolveAndRetain(PVM pVM, RTDBGAS hAlias)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);
    AssertCompile(NIL_RTDBGAS == (RTDBGAS)0);

    uint32_t    cRefs;
    uintptr_t   iAlias = DBGF_AS_ALIAS_2_INDEX(hAlias);
    if (iAlias < DBGF_AS_COUNT)
    {
        if (DBGF_AS_IS_FIXED_ALIAS(hAlias))
        {
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
 * @param   pVM         The VM handle.
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
 * @param   pVM         The VM handle.
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

        /* Fine the end of this element. */
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
 * It the environment variable doesn't exist, the current directory is searched instead.
 *
 * @returns VBox status code.
 * @param   pszFilename     The filename.
 * @param   pszEnvVar       The environment variable name.
 * @param   pfnOpen         The open callback function.
 * @param   pvUser          User argument for the callback.
 */
static int dbgfR3AsSearchEnvPath(const char *pszFilename, const char *pszEnvVar, PFNDBGFR3ASSEARCHOPEN pfnOpen, void *pvUser)
{
    const char *pszPath = RTEnvGet(pszEnvVar);
    if (!pszPath)
        pszPath = ".";
    return dbgfR3AsSearchPath(pszFilename, pszPath, pfnOpen, pvUser);
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
 * @param   pVM             The VM handle.
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
    int rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_IMAGE_PATH", dbgfR3AsLoadImageOpen, &Data);
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
 * @param   pVM             The VM handle.
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
    int rc = dbgfR3AsSearchEnvPath(pszFilename, "VBOXDBG_MAP_PATH", dbgfR3AsLoadImageOpen, &Data);
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
 * Wrapper around RTDbgAsModuleLink, RTDbgAsModuleLinkSeg and DBGFR3AsResolve.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
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

