/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Register Methods.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vmm/mm.h>
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/uint128.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Locks the register database for writing. */
#define DBGF_REG_DB_LOCK_WRITE(pVM) \
    do { \
        int rcSem = RTSemRWRequestWrite((pVM)->dbgf.s.hRegDbLock, RT_INDEFINITE_WAIT); \
        AssertRC(rcSem); \
    } while (0)

/** Unlocks the register database after writing. */
#define DBGF_REG_DB_UNLOCK_WRITE(pVM) \
    do { \
        int rcSem = RTSemRWReleaseWrite((pVM)->dbgf.s.hRegDbLock); \
        AssertRC(rcSem); \
    } while (0)

/** Locks the register database for reading. */
#define DBGF_REG_DB_LOCK_READ(pVM) \
    do { \
        int rcSem = RTSemRWRequestRead((pVM)->dbgf.s.hRegDbLock, RT_INDEFINITE_WAIT); \
        AssertRC(rcSem); \
    } while (0)

/** Unlocks the register database after reading. */
#define DBGF_REG_DB_UNLOCK_READ(pVM) \
    do { \
        int rcSem = RTSemRWReleaseRead((pVM)->dbgf.s.hRegDbLock); \
        AssertRC(rcSem); \
    } while (0)


/** The max length of a set, register or sub-field name. */
#define DBGF_REG_MAX_NAME       40


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Register set registration record type.
 */
typedef enum DBGFREGSETTYPE
{
    /** Invalid zero value. */
    DBGFREGSETTYPE_INVALID = 0,
    /** CPU record. */
    DBGFREGSETTYPE_CPU,
    /** Device record. */
    DBGFREGSETTYPE_DEVICE,
    /** End of valid record types. */
    DBGFREGSETTYPE_END
} DBGFREGSETTYPE;


/**
 * Register set registration record.
 */
typedef struct DBGFREGSET
{
    /** String space core. */
    RTSTRSPACECORE          Core;
    /** The registration record type. */
    DBGFREGSETTYPE          enmType;
    /** The user argument for the callbacks. */
    union
    {
        /** The CPU view. */
        PVMCPU              pVCpu;
        /** The device view. */
        PPDMDEVINS          pDevIns;
        /** The general view. */
        void               *pv;
    } uUserArg;

    /** The register descriptors. */
    PCDBGFREGDESC           paDescs;
    /** The number of register descriptors. */
    uint32_t                cDescs;

    /** Array of lookup records. */
    struct DBGFREGLOOKUP   *paLookupRecs;
    /** The number of lookup records. */
    uint32_t                cLookupRecs;

    /** The register name prefix. */
    char                    szPrefix[1];
} DBGFREGSET;
/** Pointer to a register registration record. */
typedef DBGFREGSET *PDBGFREGSET;
/** Pointer to a const register registration record. */
typedef DBGFREGSET const *PCDBGFREGSET;


/**
 * Register lookup record.
 */
typedef struct DBGFREGLOOKUP
{
    /** The string space core. */
    RTSTRSPACECORE      Core;
    /** Pointer to the set. */
    PCDBGFREGSET        pSet;
    /** Pointer to the register descriptor. */
    PCDBGFREGDESC       pDesc;
    /** If an alias this points to the alias descriptor, NULL if not. */
    PCDBGFREGALIAS      pAlias;
    /** If a sub-field this points to the sub-field descriptor, NULL if not. */
    PCDBGFREGSUBFIELD   pSubField;
} DBGFREGLOOKUP;
/** Pointer to a register lookup record. */
typedef DBGFREGLOOKUP *PDBGFREGLOOKUP;
/** Pointer to a const register lookup record. */
typedef DBGFREGLOOKUP const *PCDBGFREGLOOKUP;


/**
 * Initializes the register database.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 */
int dbgfR3RegInit(PVM pVM)
{
    int rc = VINF_SUCCESS;
    if (!pVM->dbgf.s.fRegDbInitialized)
    {
        rc = RTSemRWCreate(&pVM->dbgf.s.hRegDbLock);
        pVM->dbgf.s.fRegDbInitialized = RT_SUCCESS(rc);
    }
    return rc;
}


/**
 * Terminates the register database.
 *
 * @param   pVM                 The VM handle.
 */
void dbgfR3RegTerm(PVM pVM)
{
    RTSemRWDestroy(pVM->dbgf.s.hRegDbLock);
    pVM->dbgf.s.hRegDbLock = NIL_RTSEMRW;
    pVM->dbgf.s.fRegDbInitialized = false;
}


/**
 * Validates a register name.
 *
 * This is used for prefixes, aliases and field names.
 *
 * @returns true if valid, false if not.
 * @param   pszName             The register name to validate.
 * @param   chDot               Set to '.' if accepted, otherwise 0.
 */
static bool dbgfR3RegIsNameValid(const char *pszName, char chDot)
{
    const char *psz = pszName;
    if (!RT_C_IS_ALPHA(*psz))
        return false;
    char ch;
    while ((ch = *++psz))
        if (   !RT_C_IS_LOWER(ch)
            && !RT_C_IS_DIGIT(ch)
            && ch != '_'
            && ch != chDot)
            return false;
    if (psz - pszName > DBGF_REG_MAX_NAME)
        return false;
    return true;
}


/**
 * Common worker for registering a register set.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   paRegisters         The register descriptors.
 * @param   enmType             The set type.
 * @param   pvUserArg           The user argument for the callbacks.
 * @param   pszPrefix           The name prefix.
 * @param   iInstance           The instance number to be appended to @a
 *                              pszPrefix when creating the set name.
 */
static int dbgfR3RegRegisterCommon(PVM pVM, PCDBGFREGDESC paRegisters, DBGFREGSETTYPE enmType, void *pvUserArg, const char *pszPrefix, uint32_t iInstance)
{
    /*
     * Validate input.
     */
    /* The name components. */
    AssertMsgReturn(dbgfR3RegIsNameValid(pszPrefix, 0), ("%s\n", pszPrefix), VERR_INVALID_NAME);
    const char  *psz             = RTStrEnd(pszPrefix, RTSTR_MAX);
    bool const   fNeedUnderscore = RT_C_IS_DIGIT(psz[-1]);
    size_t const cchPrefix       = psz - pszPrefix + fNeedUnderscore;
    AssertMsgReturn(cchPrefix < RT_SIZEOFMEMB(DBGFREGSET, szPrefix) - 4 - 1, ("%s\n", pszPrefix), VERR_INVALID_NAME);

    AssertMsgReturn(iInstance <= 9999, ("%d\n", iInstance), VERR_INVALID_NAME);

    /* The descriptors. */
    uint32_t cLookupRecs = 0;
    uint32_t iDesc;
    for (iDesc = 0; paRegisters[iDesc].pszName != NULL; iDesc++)
    {
        AssertMsgReturn(dbgfR3RegIsNameValid(paRegisters[iDesc].pszName, 0), ("%s (#%u)\n", paRegisters[iDesc].pszName, iDesc), VERR_INVALID_NAME);

        if (enmType == DBGFREGSETTYPE_CPU)
            AssertMsgReturn((unsigned)paRegisters[iDesc].enmReg == iDesc && iDesc < (unsigned)DBGFREG_END,
                            ("%d iDesc=%d\n", paRegisters[iDesc].enmReg, iDesc),
                            VERR_INVALID_PARAMETER);
        else
            AssertReturn(paRegisters[iDesc].enmReg == DBGFREG_END, VERR_INVALID_PARAMETER);
        AssertReturn(   paRegisters[iDesc].enmType > DBGFREGVALTYPE_INVALID
                     && paRegisters[iDesc].enmType < DBGFREGVALTYPE_END, VERR_INVALID_PARAMETER);
        AssertMsgReturn(!(paRegisters[iDesc].fFlags & ~DBGFREG_FLAGS_READ_ONLY),
                        ("%#x (#%u)\n", paRegisters[iDesc].fFlags, iDesc),
                        VERR_INVALID_PARAMETER);
        AssertPtrReturn(paRegisters[iDesc].pfnGet, VERR_INVALID_PARAMETER);
        AssertPtrReturn(paRegisters[iDesc].pfnSet, VERR_INVALID_PARAMETER);

        uint32_t        iAlias    = 0;
        PCDBGFREGALIAS  paAliases = paRegisters[iDesc].paAliases;
        if (paAliases)
        {
            AssertPtrReturn(paAliases, VERR_INVALID_PARAMETER);
            for (; paAliases[iAlias].pszName; iAlias++)
            {
                AssertMsgReturn(dbgfR3RegIsNameValid(paAliases[iAlias].pszName, 0), ("%s (%s)\n", paAliases[iAlias].pszName, paRegisters[iDesc].pszName), VERR_INVALID_NAME);
                AssertReturn(   paAliases[iAlias].enmType > DBGFREGVALTYPE_INVALID
                             && paAliases[iAlias].enmType < DBGFREGVALTYPE_END, VERR_INVALID_PARAMETER);
            }
        }

        uint32_t          iSubField   = 0;
        PCDBGFREGSUBFIELD paSubFields = paRegisters[iDesc].paSubFields;
        if (paSubFields)
        {
            AssertPtrReturn(paSubFields, VERR_INVALID_PARAMETER);
            for (; paSubFields[iSubField].pszName; iSubField++)
            {
                AssertMsgReturn(dbgfR3RegIsNameValid(paSubFields[iSubField].pszName, '.'), ("%s (%s)\n", paSubFields[iSubField].pszName, paRegisters[iDesc].pszName), VERR_INVALID_NAME);
                AssertReturn(paSubFields[iSubField].iFirstBit + paSubFields[iSubField].cBits <= 128, VERR_INVALID_PARAMETER);
                AssertReturn(paSubFields[iSubField].cBits + paSubFields[iSubField].cShift <= 128, VERR_INVALID_PARAMETER);
                AssertPtrNullReturn(paSubFields[iSubField].pfnGet, VERR_INVALID_POINTER);
                AssertPtrNullReturn(paSubFields[iSubField].pfnSet, VERR_INVALID_POINTER);
            }
        }

        cLookupRecs += (1 + iAlias) * (1 + iSubField);
    }

    /* Check the instance number of the CPUs. */
    AssertReturn(enmType != DBGFREGSETTYPE_CPU || iInstance < pVM->cCpus, VERR_INVALID_CPU_ID);

    /*
     * Allocate a new record and all associated lookup records.
     */
    size_t cbRegSet = RT_OFFSETOF(DBGFREGSET, szPrefix[cchPrefix + 4 + 1]);
    cbRegSet = RT_ALIGN_Z(cbRegSet, 32);
    size_t const offLookupRecArray = cbRegSet;
    cbRegSet += cLookupRecs * sizeof(DBGFREGLOOKUP);

    PDBGFREGSET pRegSet = (PDBGFREGSET)MMR3HeapAllocZ(pVM, MM_TAG_DBGF_REG, cbRegSet);
    if (!pRegSet)
        return VERR_NO_MEMORY;

    /*
     * Initialize the new record.
     */
    pRegSet->Core.pszString = pRegSet->szPrefix;
    pRegSet->enmType        = enmType;
    pRegSet->uUserArg.pv    = pvUserArg;
    pRegSet->paDescs        = paRegisters;
    pRegSet->cDescs         = iDesc;
    pRegSet->cLookupRecs    = cLookupRecs;
    pRegSet->paLookupRecs   = (PDBGFREGLOOKUP)((uintptr_t)pRegSet + offLookupRecArray);
    if (fNeedUnderscore)
        RTStrPrintf(pRegSet->szPrefix, cchPrefix + 4 + 1, "%s_%u", pszPrefix, iInstance);
    else
        RTStrPrintf(pRegSet->szPrefix, cchPrefix + 4 + 1, "%s%u", pszPrefix, iInstance);


    /*
     * Initialize the lookup records.
     */
    char szName[DBGF_REG_MAX_NAME * 3 + 16];
    strcpy(szName, pRegSet->szPrefix);
    char *pszReg = strchr(szName, '\0');
    *pszReg++ = '.';

    int             rc = VINF_SUCCESS;
    PDBGFREGLOOKUP  pLookupRec = &pRegSet->paLookupRecs[0];
    for (iDesc = 0; paRegisters[iDesc].pszName != NULL && RT_SUCCESS(rc); iDesc++)
    {
        PCDBGFREGALIAS  pCurAlias  = NULL;
        PCDBGFREGALIAS  pNextAlias = paRegisters[iDesc].paAliases;
        const char     *pszRegName = paRegisters[iDesc].pszName;
        while (RT_SUCCESS(rc))
        {
            size_t cchReg = strlen(pszRegName);
            memcpy(pszReg, pszRegName, cchReg + 1);
            pLookupRec->Core.pszString = MMR3HeapStrDup(pVM, MM_TAG_DBGF_REG, szName);
            if (!pLookupRec->Core.pszString)
                rc = VERR_NO_STR_MEMORY;
            pLookupRec->pSet      = pRegSet;
            pLookupRec->pDesc     = &paRegisters[iDesc];
            pLookupRec->pAlias    = pCurAlias;
            pLookupRec->pSubField = NULL;
            pLookupRec++;

            PCDBGFREGSUBFIELD paSubFields = paRegisters[iDesc].paSubFields;
            if (paSubFields)
            {
                char *pszSub = &pszReg[cchReg];
                *pszSub++ = '.';
                for (uint32_t iSubField = 0; paSubFields[iSubField].pszName && RT_SUCCESS(rc); iSubField++)
                {
                    strcpy(pszSub, paSubFields[iSubField].pszName);
                    pLookupRec->Core.pszString = MMR3HeapStrDup(pVM, MM_TAG_DBGF_REG, szName);
                    if (!pLookupRec->Core.pszString)
                        rc = VERR_NO_STR_MEMORY;
                    pLookupRec->pSet      = pRegSet;
                    pLookupRec->pDesc     = &paRegisters[iDesc];
                    pLookupRec->pAlias    = pCurAlias;
                    pLookupRec->pSubField = &paSubFields[iSubField];
                    pLookupRec++;
                }
            }

            /* next */
            pCurAlias = pNextAlias++;
            if (!pCurAlias)
                break;
            pszRegName = pCurAlias->pszName;
            if (!pszRegName)
                break;
        }
    }
    Assert(pLookupRec == &pRegSet->paLookupRecs[pRegSet->cLookupRecs]);

    if (RT_SUCCESS(rc))
    {
        /*
         * Insert the record into the register set string space and optionally into
         * the CPU register set cache.
         */
        DBGF_REG_DB_LOCK_WRITE(pVM);

        bool fInserted = RTStrSpaceInsert(&pVM->dbgf.s.RegSetSpace, &pRegSet->Core);
        if (fInserted)
        {
            if (enmType == DBGFREGSETTYPE_CPU)
                pVM->aCpus[iInstance].dbgf.s.pRegSet = pRegSet;
            pVM->dbgf.s.cRegs += pRegSet->cDescs;

            PDBGFREGLOOKUP  paLookupRecs = pRegSet->paLookupRecs;
            uint32_t        iLookupRec   = pRegSet->cLookupRecs;
            while (iLookupRec-- > 0)
            {
                bool fInserted2 = RTStrSpaceInsert(&pVM->dbgf.s.RegSpace, &paLookupRecs[iLookupRec].Core);
                AssertMsg(fInserted2, ("'%s'", paLookupRecs[iLookupRec].Core.pszString));
            }

            DBGF_REG_DB_UNLOCK_WRITE(pVM);
            return VINF_SUCCESS;
        }

        DBGF_REG_DB_UNLOCK_WRITE(pVM);
        rc = VERR_DUPLICATE;
    }

    /*
     * Bail out.
     */
    for (uint32_t i = 0; i < pRegSet->cLookupRecs; i++)
        MMR3HeapFree((char *)pRegSet->paLookupRecs[i].Core.pszString);
    MMR3HeapFree(pRegSet);

    return rc;
}


/**
 * Registers a set of registers for a CPU.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pVCpu           The virtual CPU handle.
 * @param   paRegisters     The register descriptors.
 */
VMMR3_INT_DECL(int) DBGFR3RegRegisterCpu(PVM pVM, PVMCPU pVCpu, PCDBGFREGDESC paRegisters)
{
    if (!pVM->dbgf.s.fRegDbInitialized)
    {
        int rc = dbgfR3RegInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    return dbgfR3RegRegisterCommon(pVM, paRegisters, DBGFREGSETTYPE_CPU, pVCpu, "cpu", pVCpu->idCpu);
}


/**
 * Registers a set of registers for a device.
 *
 * @returns VBox status code.
 * @param   enmReg              The register identifier.
 * @param   enmType             The register type.  This is for sort out
 *                              aliases.  Pass DBGFREGVALTYPE_INVALID to get
 *                              the standard name.
 */
VMMR3DECL(int) DBGFR3RegRegisterDevice(PVM pVM, PCDBGFREGDESC paRegisters, PPDMDEVINS pDevIns, const char *pszPrefix, uint32_t iInstance)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(paRegisters, VERR_INVALID_POINTER);
    AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPrefix, VERR_INVALID_POINTER);

    return dbgfR3RegRegisterCommon(pVM, paRegisters, DBGFREGSETTYPE_DEVICE, pDevIns, pszPrefix, iInstance);
}


/**
 * Clears the register value variable.
 *
 * @param   pValue              The variable to clear.
 */
DECLINLINE(void) dbgfR3RegValClear(PDBGFREGVAL pValue)
{
    pValue->au64[0] = 0;
    pValue->au64[1] = 0;
}


/**
 * Performs a cast between register value types.
 *
 * @retval  VINF_SUCCESS
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 *
 * @param   pValue              The value to cast (input + output).
 * @param   enmFromType         The input value.
 * @param   enmToType           The desired output value.
 */
static int dbgfR3RegValCast(PDBGFREGVAL pValue, DBGFREGVALTYPE enmFromType, DBGFREGVALTYPE enmToType)
{
    DBGFREGVAL const InVal = *pValue;
    dbgfR3RegValClear(pValue);

    /* Note! No default cases here as gcc warnings about missing enum values
             are desired. */
    switch (enmFromType)
    {
        case DBGFREGVALTYPE_U8:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.u8; return VINF_SUCCESS;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.u8; return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.u8; return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.u8; return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U128:   pValue->u128.s.Lo = InVal.u8; return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u8; return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                  return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_U16:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.u16;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.u16;  return VINF_SUCCESS;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.u16;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.u16;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U128:   pValue->u128.s.Lo = InVal.u16;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u16;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                    return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_U32:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.u32;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.u32;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.u32;  return VINF_SUCCESS;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.u32;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_U128:   pValue->u128.s.Lo = InVal.u32;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u32;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                    return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_U64:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.u64;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.u64;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.u64;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.u64;  return VINF_SUCCESS;
                case DBGFREGVALTYPE_U128:   pValue->u128.s.Lo = InVal.u64;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u64;  return VINF_DBGF_ZERO_EXTENDED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                    return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_U128:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.u128.s.Lo;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.u128.s.Lo;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.u128.s.Lo;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.u128.s.Lo;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U128:   pValue->u128      = InVal.u128;       return VINF_SUCCESS;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u64;        return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                          return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_LRD:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = (uint8_t)InVal.lrd;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = (uint16_t)InVal.lrd;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = (uint32_t)InVal.lrd;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = (uint64_t)InVal.lrd;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U128:
                    pValue->u128.s.Lo = (uint64_t)InVal.lrd;
                    pValue->u128.s.Hi = (uint64_t)InVal.lrd / _4G / _4G;
                    return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.lrd;  return VINF_SUCCESS;
                case DBGFREGVALTYPE_DTR:                                    return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_DTR:
            switch (enmToType)
            {
                case DBGFREGVALTYPE_U8:     pValue->u8        = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U16:    pValue->u16       = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U32:    pValue->u32       = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U64:    pValue->u64       = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_U128:   pValue->u128.s.Lo = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.dtr.u64Base;  return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_DTR:    pValue->dtr       = InVal.dtr;          return VINF_SUCCESS;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_INVALID:
        case DBGFREGVALTYPE_END:
        case DBGFREGVALTYPE_32BIT_HACK:
            break;
    }

    AssertMsgFailed(("%d / %d\n", enmFromType, enmToType));
    return VERR_DBGF_UNSUPPORTED_CAST;
}


/**
 * Worker for the CPU register queries.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The virtual CPU ID.
 * @param   enmReg              The register to query.
 * @param   enmType             The desired return type.
 * @param   pValue              Where to return the register value.
 */
static DECLCALLBACK(int) dbgfR3RegCpuQueryWorker(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, DBGFREGVALTYPE enmType, PDBGFREGVAL pValue)
{
    int rc = VINF_SUCCESS;
    DBGF_REG_DB_LOCK_READ(pVM);

    /*
     * Look up the register set of the specified CPU.
     */
    PDBGFREGSET pSet = pVM->aCpus[idCpu].dbgf.s.pRegSet;
    if (RT_LIKELY(pSet))
    {
        /*
         * Look up the register and get the register value.
         */
        if (RT_LIKELY(pSet->cDescs > (size_t)enmReg))
        {
            PCDBGFREGDESC pDesc = &pSet->paDescs[enmReg];

            pValue->au64[0] = pValue->au64[1] = 0;
            rc = pDesc->pfnGet(pSet->uUserArg.pv, pDesc, pValue);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Do the cast if the desired return type doesn't match what
                 * the getter returned.
                 */
                if (pDesc->enmType == enmType)
                    rc = VINF_SUCCESS;
                else
                    rc = dbgfR3RegValCast(pValue, pDesc->enmType, enmType);
            }
        }
        else
            rc = VERR_DBGF_REGISTER_NOT_FOUND;
    }
    else
        rc = VERR_INVALID_CPU_ID;

    DBGF_REG_DB_UNLOCK_READ(pVM);
    return rc;
}


/**
 * Queries a 8-bit CPU register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu8                 Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryU8(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint8_t *pu8)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    DBGFREGVAL Value;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryWorker, 5, pVM, idCpu, enmReg, DBGFREGVALTYPE_U8, &Value);
    if (RT_SUCCESS(rc))
        *pu8 = Value.u8;
    else
        *pu8 = 0;

    return rc;
}


/**
 * Queries a 16-bit CPU register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu16                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryU16(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint16_t *pu16)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    DBGFREGVAL Value;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryWorker, 5, pVM, idCpu, enmReg, DBGFREGVALTYPE_U16, &Value);
    if (RT_SUCCESS(rc))
        *pu16 = Value.u16;
    else
        *pu16 = 0;

    return rc;
}


/**
 * Queries a 32-bit CPU register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu32                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryU32(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint32_t *pu32)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    DBGFREGVAL Value;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryWorker, 5, pVM, idCpu, enmReg, DBGFREGVALTYPE_U32, &Value);
    if (RT_SUCCESS(rc))
        *pu32 = Value.u32;
    else
        *pu32 = 0;

    return rc;
}


/**
 * Queries a 64-bit CPU register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu64                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryU64(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t *pu64)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    DBGFREGVAL Value;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryWorker, 5, pVM, idCpu, enmReg, DBGFREGVALTYPE_U64, &Value);
    if (RT_SUCCESS(rc))
        *pu64 = Value.u64;
    else
        *pu64 = 0;

    return rc;
}


/**
 * Wrapper around CPUMQueryGuestMsr for dbgfR3RegCpuQueryBatchWorker.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 *
 * @param   pVCpu               The current CPU.
 * @param   pReg                The where to store the register value and
 *                              size.
 * @param   idMsr               The MSR to get.
 */
static void dbgfR3RegGetMsrBatch(PVMCPU pVCpu, PDBGFREGENTRY pReg, uint32_t idMsr)
{
    pReg->enmType = DBGFREGVALTYPE_U64;
    int rc = CPUMQueryGuestMsr(pVCpu, idMsr, &pReg->Val.u64);
    if (RT_FAILURE(rc))
    {
        AssertMsg(rc == VERR_CPUM_RAISE_GP_0, ("%Rrc\n", rc));
        pReg->Val.u64 = 0;
    }
}


static DECLCALLBACK(int) dbgfR3RegCpuQueryBatchWorker(PVM pVM, VMCPUID idCpu, PDBGFREGENTRY paRegs, size_t cRegs)
{
#if 0
    PVMCPU    pVCpu = &pVM->aCpus[idCpu];
    PCCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

    PDBGFREGENTRY pReg = paRegs - 1;
    while (cRegs-- > 0)
    {
        pReg++;
        pReg->Val.au64[0] = 0;
        pReg->Val.au64[1] = 0;

        DBGFREG const enmReg = pReg->enmReg;
        AssertMsgReturn(enmReg >= 0 && enmReg <= DBGFREG_END, ("%d (%#x)\n", enmReg, enmReg), VERR_DBGF_REGISTER_NOT_FOUND);
        if (enmReg != DBGFREG_END)
        {
            PCDBGFREGDESC pDesc = &g_aDbgfRegDescs[enmReg];
            if (!pDesc->pfnGet)
            {
                PCRTUINT128U pu = (PCRTUINT128U)((uintptr_t)pCtx + pDesc->offCtx);
                pReg->enmType = pDesc->enmType;
                switch (pDesc->enmType)
                {
                    case DBGFREGVALTYPE_U8:     pReg->Val.u8   = pu->au8[0];   break;
                    case DBGFREGVALTYPE_U16:    pReg->Val.u16  = pu->au16[0];  break;
                    case DBGFREGVALTYPE_U32:    pReg->Val.u32  = pu->au32[0];  break;
                    case DBGFREGVALTYPE_U64:    pReg->Val.u64  = pu->au64[0];  break;
                    case DBGFREGVALTYPE_U128:
                        pReg->Val.au64[0] = pu->au64[0];
                        pReg->Val.au64[1] = pu->au64[1];
                        break;
                    case DBGFREGVALTYPE_LRD:
                        pReg->Val.au64[0] = pu->au64[0];
                        pReg->Val.au16[5] = pu->au16[5];
                        break;
                    default:
                        AssertMsgFailedReturn(("%s %d\n", pDesc->pszName, pDesc->enmType), VERR_INTERNAL_ERROR_3);
                }
            }
            else
            {
                int rc = pDesc->pfnGet(pVCpu, pDesc, pCtx, &pReg->Val.u);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
    }
    return VINF_SUCCESS;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Query a batch of registers.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   paRegs              Pointer to an array of @a cRegs elements.  On
 *                              input the enmReg members indicates which
 *                              registers to query.  On successful return the
 *                              other members are set.  DBGFREG_END can be used
 *                              as a filler.
 * @param   cRegs               The number of entries in @a paRegs.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryBatch(PVM pVM, VMCPUID idCpu, PDBGFREGENTRY paRegs, size_t cRegs)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    if (!cRegs)
        return VINF_SUCCESS;
    AssertReturn(cRegs < _1M, VERR_OUT_OF_RANGE);
    AssertPtrReturn(paRegs, VERR_INVALID_POINTER);
    size_t iReg = cRegs;
    while (iReg-- > 0)
    {
        DBGFREG enmReg = paRegs[iReg].enmReg;
        AssertMsgReturn(enmReg < DBGFREG_END && enmReg >= DBGFREG_AL, ("%d (%#x)", enmReg, enmReg), VERR_DBGF_REGISTER_NOT_FOUND);
    }

    return VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryBatchWorker, 4, pVM, idCpu, paRegs, cRegs);
}


/**
 * Query a all registers for a Virtual CPU.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   paRegs              Pointer to an array of @a cRegs elements.
 *                              These will be filled with the CPU register
 *                              values. Overflowing entries will be set to
 *                              DBGFREG_END.  The returned registers can be
 *                              accessed by using the DBGFREG values as index.
 * @param   cRegs               The number of entries in @a paRegs.  The
 *                              recommended value is DBGFREG_ALL_COUNT.
 */
VMMR3DECL(int) DBGFR3RegCpuQueryAll(PVM pVM, VMCPUID idCpu, PDBGFREGENTRY paRegs, size_t cRegs)
{
    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    if (!cRegs)
        return VINF_SUCCESS;
    AssertReturn(cRegs < _1M, VERR_OUT_OF_RANGE);
    AssertPtrReturn(paRegs, VERR_INVALID_POINTER);

    /*
     * Convert it into a batch query (lazy bird).
     */
    unsigned iReg = 0;
    while (iReg < cRegs && iReg < DBGFREG_ALL_COUNT)
    {
        paRegs[iReg].enmReg = (DBGFREG)iReg;
        iReg++;
    }
    while (iReg < cRegs)
        paRegs[iReg++].enmReg = DBGFREG_END;

    return VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegCpuQueryBatchWorker, 4, pVM, idCpu, paRegs, cRegs);
}



/**
 * Gets the name of a register.
 *
 * @returns Pointer to read-only register name (lower case).  NULL if the
 *          parameters are invalid.
 *
 * @param   pVM                 The VM handle.
 * @param   enmReg              The register identifier.
 * @param   enmType             The register type.  This is for sort out
 *                              aliases.  Pass DBGFREGVALTYPE_INVALID to get
 *                              the standard name.
 */
VMMR3DECL(const char *) DBGFR3RegCpuName(PVM pVM, DBGFREG enmReg, DBGFREGVALTYPE enmType)
{
    AssertReturn(enmReg >= DBGFREG_AL && enmReg < DBGFREG_END, NULL);
    AssertReturn(enmType >= DBGFREGVALTYPE_INVALID && enmType < DBGFREGVALTYPE_END, NULL);
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);

    PCDBGFREGSET    pSet    = pVM->aCpus[0].dbgf.s.pRegSet;
    if (RT_UNLIKELY(!pSet))
        return NULL;

    PCDBGFREGDESC   pDesc   = &pSet->paDescs[enmReg];
    PCDBGFREGALIAS  pAlias  = pDesc->paAliases;
    if (   pAlias
        && pDesc->enmType != enmType
        && enmType != DBGFREGVALTYPE_INVALID)
    {
        while (pAlias->pszName)
        {
            if (pAlias->enmType == enmType)
                return pAlias->pszName;
            pAlias++;
        }
    }

    return pDesc->pszName;
}


/**
 * Fold the string to lower case and copy it into the destination buffer.
 *
 * @returns Number of folder characters, -1 on overflow.
 * @param   pszSrc              The source string.
 * @param   cchSrc              How much to fold and copy.
 * @param   pszDst              The output buffer.
 * @param   cbDst               The size of the output buffer.
 */
static ssize_t dbgfR3RegCopyToLower(const char *pszSrc, size_t cchSrc, char *pszDst, size_t cbDst)
{
    ssize_t cchFolded = 0;
    char    ch;
    while (cchSrc-- > 0 && (ch = *pszSrc++))
    {
        if (RT_UNLIKELY(cbDst <= 1))
            return -1;
        cbDst--;

        char chLower = RT_C_TO_LOWER(ch);
        cchFolded += chLower != ch;
        *pszDst++ = chLower;
    }
    if (RT_UNLIKELY(!cbDst))
        return -1;
    *pszDst = '\0';
    return cchFolded;
}


/**
 * Resolves the register name.
 *
 * @returns Lookup record.
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default CPU ID set.
 * @param   pszReg              The register name.
 */
static PCDBGFREGLOOKUP dbgfR3RegResolve(PVM pVM, VMCPUID idDefCpu, const char *pszReg)
{
    DBGF_REG_DB_LOCK_READ(pVM);

    /* Try looking up the name without any case folding or cpu prefixing. */
    PCDBGFREGLOOKUP pLookupRec = (PCDBGFREGLOOKUP)RTStrSpaceGet(&pVM->dbgf.s.RegSpace, pszReg);
    if (!pLookupRec)
    {
        char szName[DBGF_REG_MAX_NAME * 4 + 16];

        /* Lower case it and try again. */
        ssize_t cchFolded = dbgfR3RegCopyToLower(pszReg, RTSTR_MAX, szName, sizeof(szName) - DBGF_REG_MAX_NAME);
        if (cchFolded > 0)
            pLookupRec = (PCDBGFREGLOOKUP)RTStrSpaceGet(&pVM->dbgf.s.RegSpace, szName);
        if (   !pLookupRec
            && cchFolded >= 0
            && idDefCpu != VMCPUID_ANY)
        {
            /* Prefix it with the specified CPU set. */
            size_t cchCpuSet = RTStrPrintf(szName, sizeof(szName), "cpu%u.", idDefCpu);
            dbgfR3RegCopyToLower(pszReg, RTSTR_MAX, &szName[cchCpuSet], sizeof(szName) - cchCpuSet);
            pLookupRec = (PCDBGFREGLOOKUP)RTStrSpaceGet(&pVM->dbgf.s.RegSpace, szName);
        }
    }

    DBGF_REG_DB_UNLOCK_READ(pVM);
    return pLookupRec;
}


/**
 * On CPU worker for the register queries, used by dbgfR3RegNmQueryWorker.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   pLookupRec          The register lookup record.
 * @param   enmType             The desired return type.
 * @param   pValue              Where to return the register value.
 * @param   penmType            Where to store the register value type.
 *                              Optional.
 */
static DECLCALLBACK(int) dbgfR3RegNmQueryWorkerOnCpu(PVM pVM, PCDBGFREGLOOKUP pLookupRec, DBGFREGVALTYPE enmType,
                                                     PDBGFREGVAL pValue, PDBGFREGVALTYPE penmType)
{
    PCDBGFREGDESC       pDesc        = pLookupRec->pDesc;
    PCDBGFREGSET        pSet         = pLookupRec->pSet;
    PCDBGFREGSUBFIELD   pSubField    = pLookupRec->pSubField;
    DBGFREGVALTYPE      enmValueType = pDesc->enmType;
    int                 rc;

    /*
     * Get the register or sub-field value.
     */
    dbgfR3RegValClear(pValue);
    if (!pSubField)
    {
        rc = pDesc->pfnGet(pSet->uUserArg.pv, pDesc, pValue);
        if (   pLookupRec->pAlias
            && pLookupRec->pAlias->enmType != enmValueType
            && RT_SUCCESS(rc))
        {
            rc = dbgfR3RegValCast(pValue, enmValueType, pLookupRec->pAlias->enmType);
            enmValueType = pLookupRec->pAlias->enmType;
        }
    }
    else
    {
        if (pSubField->pfnGet)
        {
            rc = pSubField->pfnGet(pSet->uUserArg.pv, pSubField, &pValue->u128);
            enmValueType = DBGFREGVALTYPE_U128;
        }
        else
        {
            rc = pDesc->pfnGet(pSet->uUserArg.pv, pDesc, pValue);
            if (   pLookupRec->pAlias
                && pLookupRec->pAlias->enmType != enmValueType
                && RT_SUCCESS(rc))
            {
                rc = dbgfR3RegValCast(pValue, enmValueType, pLookupRec->pAlias->enmType);
                enmValueType = pLookupRec->pAlias->enmType;
            }
            if (RT_SUCCESS(rc))
            {
                rc = dbgfR3RegValCast(pValue, enmValueType, DBGFREGVALTYPE_U128);
                if (RT_SUCCESS(rc))
                {
                    RTUInt128AssignShiftLeft(&pValue->u128, -pSubField->iFirstBit);
                    RTUInt128AssignAndNFirstBits(&pValue->u128, pSubField->cBits);
                    if (pSubField->cShift)
                        RTUInt128AssignShiftLeft(&pValue->u128, pSubField->cShift);
                }
            }
        }
        if (RT_SUCCESS(rc))
        {
            unsigned const cBits = pSubField->cBits + pSubField->cShift;
            if (cBits <= 8)
                enmValueType = DBGFREGVALTYPE_U8;
            else if (cBits <= 16)
                enmValueType = DBGFREGVALTYPE_U16;
            else if (cBits <= 32)
                enmValueType = DBGFREGVALTYPE_U32;
            else if (cBits <= 64)
                enmValueType = DBGFREGVALTYPE_U64;
            else
                enmValueType = DBGFREGVALTYPE_U128;
            rc = dbgfR3RegValCast(pValue, DBGFREGVALTYPE_U128, enmValueType);
        }
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Do the cast if the desired return type doesn't match what
         * the getter returned.
         */
        if (   enmValueType == enmType
            || enmType == DBGFREGVALTYPE_END)
        {
            rc = VINF_SUCCESS;
            if (penmType)
                *penmType = enmValueType;
        }
        else
        {
            rc = dbgfR3RegValCast(pValue, enmValueType, enmType);
            if (penmType)
                *penmType = RT_SUCCESS(rc) ? enmType : enmValueType;
        }
    }

    return rc;
}


/**
 * Worker for the register queries.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The virtual CPU ID for the default CPU register
 *                              set.
 * @param   pszReg              The register to query.
 * @param   enmType             The desired return type.
 * @param   pValue              Where to return the register value.
 * @param   penmType            Where to store the register value type.
 *                              Optional.
 */
static DECLCALLBACK(int) dbgfR3RegNmQueryWorker(PVM pVM, VMCPUID idDefCpu, const char *pszReg, DBGFREGVALTYPE enmType,
                                                PDBGFREGVAL pValue, PDBGFREGVALTYPE penmType)
{
    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idDefCpu < pVM->cCpus || idDefCpu == VMCPUID_ANY, VERR_INVALID_CPU_ID);
    AssertPtrReturn(pszReg, VERR_INVALID_POINTER);

    Assert(enmType > DBGFREGVALTYPE_INVALID && enmType <= DBGFREGVALTYPE_END);
    AssertPtr(pValue);

    /*
     * Resolve the register and call the getter on the relevant CPU.
     */
    PCDBGFREGLOOKUP pLookupRec = dbgfR3RegResolve(pVM, idDefCpu, pszReg);
    if (pLookupRec)
    {
        if (pLookupRec->pSet->enmType == DBGFREGSETTYPE_CPU)
            idDefCpu = pLookupRec->pSet->uUserArg.pVCpu->idCpu;
        return VMR3ReqCallWait(pVM, idDefCpu, (PFNRT)dbgfR3RegNmQueryWorkerOnCpu, 5, pVM, pLookupRec, enmType, pValue, penmType);
    }
    return VERR_DBGF_REGISTER_NOT_FOUND;
}


/**
 * Queries a descriptor table register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pValue              Where to store the register value.
 * @param   penmType            Where to store the register value type.
 */
VMMR3DECL(int) DBGFR3RegNmQuery(PVM pVM, VMCPUID idDefCpu, const char *pszReg, PDBGFREGVAL pValue, PDBGFREGVALTYPE penmType)
{
    return dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_END, pValue, penmType);
}


/**
 * Queries a 8-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu8                 Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryU8(PVM pVM, VMCPUID idDefCpu, const char *pszReg, uint8_t *pu8)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_U8, &Value, NULL);
    if (RT_SUCCESS(rc))
        *pu8 = Value.u8;
    else
        *pu8 = 0;
    return rc;
}


/**
 * Queries a 16-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu16                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryU16(PVM pVM, VMCPUID idDefCpu, const char *pszReg, uint16_t *pu16)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_U16, &Value, NULL);
    if (RT_SUCCESS(rc))
        *pu16 = Value.u16;
    else
        *pu16 = 0;
    return rc;
}


/**
 * Queries a 32-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu32                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryU32(PVM pVM, VMCPUID idDefCpu, const char *pszReg, uint32_t *pu32)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_U32, &Value, NULL);
    if (RT_SUCCESS(rc))
        *pu32 = Value.u32;
    else
        *pu32 = 0;
    return rc;
}


/**
 * Queries a 64-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu64                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryU64(PVM pVM, VMCPUID idDefCpu, const char *pszReg, uint64_t *pu64)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_U64, &Value, NULL);
    if (RT_SUCCESS(rc))
        *pu64 = Value.u64;
    else
        *pu64 = 0;
    return rc;
}


/**
 * Queries a 128-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu128               Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryU128(PVM pVM, VMCPUID idDefCpu, const char *pszReg, PRTUINT128U pu128)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_U128, &Value, NULL);
    if (RT_SUCCESS(rc))
        *pu128 = Value.u128;
    else
        pu128->s.Hi = pu128->s.Lo = 0;
    return rc;
}


/**
 * Queries a long double register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   plrd                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryLrd(PVM pVM, VMCPUID idDefCpu, const char *pszReg, long double *plrd)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_LRD, &Value, NULL);
    if (RT_SUCCESS(rc))
        *plrd = Value.lrd;
    else
        *plrd = 0;
    return rc;
}


/**
 * Queries a descriptor table register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_REGISTER_NOT_FOUND
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idDefCpu            The default target CPU ID, VMCPUID_ANY if not
 *                              applicable.
 * @param   pszReg              The register that's being queried.  Except for
 *                              CPU registers, this must be on the form
 *                              "set.reg[.sub]".
 * @param   pu64Base            Where to store the register base value.
 * @param   pu32Limit           Where to store the register limit value.
 */
VMMR3DECL(int) DBGFR3RegNmQueryXdtr(PVM pVM, VMCPUID idDefCpu, const char *pszReg, uint64_t *pu64Base, uint32_t *pu32Limit)
{
    DBGFREGVAL Value;
    int rc = dbgfR3RegNmQueryWorker(pVM, idDefCpu, pszReg, DBGFREGVALTYPE_DTR, &Value, NULL);
    if (RT_SUCCESS(rc))
    {
        *pu64Base  = Value.dtr.u64Base;
        *pu32Limit = Value.dtr.u32Limit;
    }
    else
    {
        *pu64Base  = 0;
        *pu32Limit = 0;
    }
    return rc;
}


/// @todo VMMR3DECL(int) DBGFR3RegNmQueryBatch(PVM pVM,VMCPUID idDefCpu, DBGFREGENTRYNM paRegs, size_t cRegs);


/**
 * Gets the number of registers returned by DBGFR3RegNmQueryAll.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pcRegs              Where to return the register count.
 */
VMMR3DECL(int) DBGFR3RegNmQueryAllCount(PVM pVM, size_t *pcRegs)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    *pcRegs = pVM->dbgf.s.cRegs;
    return VINF_SUCCESS;
}


VMMR3DECL(int) DBGFR3RegNmQueryAll(PVM pVM, DBGFREGENTRYNM paRegs, size_t cRegs)
{
    return VERR_NOT_IMPLEMENTED;
}


/// @todo VMMR3DECL(int) DBGFR3RegNmPrintf(PVM pVM, VMCPUID idDefCpu, char pszBuf, size_t cbBuf, const char *pszFormat, ...);
/// @todo VMMR3DECL(int) DBGFR3RegNmPrintfV(PVM pVM, VMCPUID idDefCpu, char pszBuf, size_t cbBuf, const char *pszFormat, ...);

