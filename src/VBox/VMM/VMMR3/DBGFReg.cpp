/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Register Methods.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
    DBGFREGSETTYPE       enmType;
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
    size_t                  cDescs;

    /** The register name prefix. */
    char                    szPrefix[32];
} DBGFREGSET;
/** Pointer to a register registration record. */
typedef DBGFREGSET *PDBGFREGSET;


/**
 * Validates a register name.
 *
 * This is used for prefixes, aliases and field names.
 *
 * @returns true if valid, false if not.
 * @param   pszName             The register name to validate.
 */
static bool dbgfR3RegIsNameValid(const char *pszName)
{
    if (!RT_C_IS_ALPHA(*pszName))
        return false;
    char ch;
    while ((ch = *++pszName))
        if (   !RT_C_IS_LOWER(ch)
            && !RT_C_IS_DIGIT(ch)
            && ch != '_')
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
    AssertMsgReturn(dbgfR3RegIsNameValid(pszPrefix), ("%s\n", pszPrefix), VERR_INVALID_NAME);
    const char  *psz             = RTStrEnd(pszPrefix, RTSTR_MAX);
    bool const   fNeedUnderscore = RT_C_IS_DIGIT(psz[-1]);
    size_t const cchPrefix       = psz - pszPrefix + fNeedUnderscore;
    AssertMsgReturn(cchPrefix < RT_SIZEOFMEMB(DBGFREGSET, szPrefix) - 4 - 1, ("%s\n", pszPrefix), VERR_INVALID_NAME);

    AssertMsgReturn(iInstance <= 9999, ("%d\n", iInstance), VERR_INVALID_NAME);

    /* The descriptors. */
    uint32_t iDesc;
    for (iDesc = 0; paRegisters[iDesc].pszName != NULL; iDesc++)
    {
        AssertMsgReturn(dbgfR3RegIsNameValid(paRegisters[iDesc].pszName), ("%s (#%u)\n", paRegisters[iDesc].pszName, iDesc), VERR_INVALID_NAME);

        if (enmType == DBGFREGSETTYPE_CPU)
            AssertMsgReturn((unsigned)paRegisters[iDesc].enmReg == iDesc && iDesc < (unsigned)DBGFREG_END,
                            ("%d iDesc=%d\n", paRegisters[iDesc].enmReg, iDesc),
                            VERR_INVALID_PARAMETER);
        else
            AssertReturn(paRegisters[iDesc].enmReg == DBGFREG_END, VERR_INVALID_PARAMETER);
        AssertReturn(   paRegisters[iDesc].enmType > DBGFREGVALTYPE_INVALID
                     && paRegisters[iDesc].enmType < DBGFREGVALTYPE_END, VERR_INVALID_PARAMETER);
        AssertMsgReturn(paRegisters[iDesc].fFlags & ~DBGFREG_FLAGS_READ_ONLY,
                        ("%#x (#%u)\n", paRegisters[iDesc].fFlags, iDesc),
                        VERR_INVALID_PARAMETER);
        AssertPtrReturn(paRegisters[iDesc].pfnGet, VERR_INVALID_PARAMETER);
        AssertPtrReturn(paRegisters[iDesc].pfnSet, VERR_INVALID_PARAMETER);

        PCDBGFREGALIAS paAliases = paRegisters[iDesc].paAliases;
        if (paAliases)
        {
            AssertPtrReturn(paAliases, VERR_INVALID_PARAMETER);
            for (uint32_t j = 0; paAliases[j].pszName; j++)
            {
                AssertMsgReturn(dbgfR3RegIsNameValid(paAliases[j].pszName), ("%s (%s)\n", paAliases[j].pszName, paRegisters[iDesc].pszName), VERR_INVALID_NAME);
                AssertReturn(   paAliases[j].enmType > DBGFREGVALTYPE_INVALID
                             && paAliases[j].enmType < DBGFREGVALTYPE_END, VERR_INVALID_PARAMETER);
            }
        }

        PCDBGFREGSUBFIELD paSubFields = paRegisters[iDesc].paSubFields;
        if (paSubFields)
        {
            AssertPtrReturn(paSubFields, VERR_INVALID_PARAMETER);
            for (uint32_t j = 0; paSubFields[j].pszName; j++)
            {
                AssertMsgReturn(dbgfR3RegIsNameValid(paSubFields[j].pszName), ("%s (%s)\n", paSubFields[j].pszName, paRegisters[iDesc].pszName), VERR_INVALID_NAME);
                AssertReturn(paSubFields[j].iFirstBit + paSubFields[j].cBits <= 128, VERR_INVALID_PARAMETER);
                AssertReturn(paSubFields[j].cBits + paSubFields[j].cShift <= 128, VERR_INVALID_PARAMETER);
                AssertPtrNullReturn(paSubFields[j].pfnGet, VERR_INVALID_POINTER);
                AssertPtrNullReturn(paSubFields[j].pfnSet, VERR_INVALID_POINTER);
            }
        }
    }

    /*
     * Allocate a new record.
     */
    PDBGFREGSET pRegRec = (PDBGFREGSET)MMR3HeapAlloc(pVM, MM_TAG_DBGF_REG, RT_OFFSETOF(DBGFREGSET, szPrefix[cchPrefix + 4 + 1]));
    if (!pRegRec)
        return VERR_NO_MEMORY;

    pRegRec->Core.pszString = pRegRec->szPrefix;
    pRegRec->enmType        = enmType;
    pRegRec->uUserArg.pv    = pvUserArg;
    pRegRec->paDescs        = paRegisters;
    pRegRec->cDescs         = iDesc;
    if (fNeedUnderscore)
        RTStrPrintf(pRegRec->szPrefix, cchPrefix + 4 + 1, "%s_%u", pszPrefix, iInstance);
    else
        RTStrPrintf(pRegRec->szPrefix, cchPrefix + 4 + 1, "%s%u", pszPrefix, iInstance);

    DBGF_REG_DB_LOCK_WRITE(pVM);
    bool fInserted = RTStrSpaceInsert(&pVM->dbgf.s.RegSetSpace, &pRegRec->Core);
    DBGF_REG_DB_UNLOCK_WRITE(pVM);
    if (fInserted)
        return VINF_SUCCESS;

    MMR3HeapFree(pRegRec);
    return VERR_DUPLICATE;
}


/**
 * Registers a set of registers for a CPU.
 *
 * @returns VBox status code.
 * @param   pVCpu           The virtual CPU handle.
 * @param   paRegisters     The register descriptors.
 */
VMMR3_INT_DECL(int) DBGFR3RegRegisterDevice(PVMCPU pVCpu, PCDBGFREGDESC paRegisters)
{
    return dbgfR3RegRegisterCommon(pVCpu->pVMR3, paRegisters, DBGFREGSETTYPE_CPU, pVCpu, "cpu", pVCpu->idCpu);
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
                case DBGFREGVALTYPE_80:                                   return VERR_DBGF_UNSUPPORTED_CAST;
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
                case DBGFREGVALTYPE_80:                                     return VERR_DBGF_UNSUPPORTED_CAST;
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
                case DBGFREGVALTYPE_80:                                     return VERR_DBGF_UNSUPPORTED_CAST;
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
                case DBGFREGVALTYPE_80:                                     return VERR_DBGF_UNSUPPORTED_CAST;
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
                case DBGFREGVALTYPE_80:                                           return VERR_DBGF_UNSUPPORTED_CAST;
                case DBGFREGVALTYPE_LRD:    pValue->lrd       = InVal.u64;        return VINF_DBGF_TRUNCATED_REGISTER;
                case DBGFREGVALTYPE_DTR:                                          return VERR_DBGF_UNSUPPORTED_CAST;

                case DBGFREGVALTYPE_32BIT_HACK:
                case DBGFREGVALTYPE_END:
                case DBGFREGVALTYPE_INVALID:
                    break;
            }
            break;

        case DBGFREGVALTYPE_80:
            return VERR_DBGF_UNSUPPORTED_CAST;

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
                case DBGFREGVALTYPE_80:                                     return VERR_DBGF_UNSUPPORTED_CAST;
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
                case DBGFREGVALTYPE_80:                                             return VERR_DBGF_UNSUPPORTED_CAST;
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
 * @retval  VERR_DBGF_INVALID_REGISTER
 * @retval  VERR_DBGF_UNSUPPORTED_CAST
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The virtual CPU ID.
 * @param   enmReg              The register to query.
 * @param   enmType             The desired return type.
 * @param   puValue             Where to return the register value.
 */
static DECLCALLBACK(int) dbgfR3RegCpuQueryWorker(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, DBGFREGVALTYPE enmType, PDBGFREGVAL puValue)
{
    int rc = VINF_SUCCESS;
    DBGF_REG_DB_LOCK_READ(pVM);

    /*
     * Look up the register set of the CPU.
     */
    /** @todo optimize this by adding a cpu register set array to DBGF. */
    char szSetName[16];
    RTStrPrintf(szSetName, sizeof(szSetName), "cpu%u", idCpu);
    PDBGFREGSET pSet = (PDBGFREGSET)RTStrSpaceGet(&pVM->dbgf.s.RegSetSpace, szSetName);
    if (RT_LIKELY(pSet))
    {
        /*
         * Look up the register and get the register value.
         */
        if (RT_LIKELY(pSet->cDescs > (size_t)enmReg))
        {
            PCDBGFREGDESC pDesc = &pSet->paDescs[enmReg];

            puValue->au64[0] = puValue->au64[1] = 0;
            rc = pDesc->pfnGet(pSet->uUserArg.pv, pDesc, puValue);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Do the cast if the desired return type doesn't match what
                 * the getter returned.
                 */
                if (pDesc->enmType == enmType)
                    rc = VINF_SUCCESS;
                else
                    rc = dbgfR3RegValCast(puValue, pDesc->enmType, enmType);
            }
        }
        else
            rc = VERR_DBGF_INVALID_REGISTER;
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
        AssertMsgReturn(enmReg >= 0 && enmReg <= DBGFREG_END, ("%d (%#x)\n", enmReg, enmReg), VERR_DBGF_INVALID_REGISTER);
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
 * @retval  VERR_DBGF_INVALID_REGISTER
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
        AssertMsgReturn(enmReg < DBGFREG_END && enmReg >= DBGFREG_AL, ("%d (%#x)", enmReg, enmReg), VERR_DBGF_INVALID_REGISTER);
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
 * @param   enmReg              The register identifier.
 * @param   enmType             The register type.  This is for sort out
 *                              aliases.  Pass DBGFREGVALTYPE_INVALID to get
 *                              the standard name.
 */
VMMR3DECL(const char *) DBGFR3RegCpuName(DBGFREG enmReg, DBGFREGVALTYPE enmType)
{
    AssertReturn(enmReg >= DBGFREG_AL && enmReg < DBGFREG_END, NULL);
    AssertReturn(enmType >= DBGFREGVALTYPE_INVALID && enmType < DBGFREGVALTYPE_END, NULL);

#if 0 /** @todo need the optimization */
    PCDBGFREGDESC   pDesc  = &g_aDbgfRegDescs[enmReg];
    PCDBGFREGALIAS  pAlias = pDesc->paAliases;
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
#else
    return NULL;
#endif
}

/** @todo Implementing missing APIs. */
