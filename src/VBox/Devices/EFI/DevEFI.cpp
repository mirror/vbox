/* $Id$ */
/** @file
 * DevEFI - EFI <-> VirtualBox Integration Framework.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_EFI

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmnvram.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/mp.h>
#include <iprt/list.h>
#ifdef DEBUG
# include <iprt/stream.h>
# define DEVEFI_WITH_VBOXDBG_SCRIPT
#endif

#include "Firmware2/VBoxPkg/Include/DevEFI.h"
#include "VBoxDD.h"
#include "VBoxDD2.h"
#include "../PC/DevFwCommon.h"

/* EFI includes */
#include <ProcessorBind.h>
#include <Common/UefiBaseTypes.h>
#include <Common/PiFirmwareVolume.h>
#include <Common/PiFirmwareFile.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * EFI NVRAM variable.
 */
typedef struct EFIVAR
{
    /** The list node for the variable. */
    RTLISTNODE      ListNode;
    /** The unique sequence number of the variable.
     * This is used to find pCurVar when restoring saved state and therefore only
     * set when saving. */
    uint32_t        idUniqueSavedState;
    /** The value attributess. */
    uint32_t        fAttributes;
    /** The variable name length (not counting the terminator char). */
    uint32_t        cchName;
    /** The size of the value. This cannot be zero. */
    uint32_t        cbValue;
    /** The vendor UUID scoping the variable name. */
    RTUUID          uuid;
    /** The variable name. */
    char            szName[EFI_VARIABLE_NAME_MAX];
    /** The variable value bytes.  */
    uint8_t         abValue[EFI_VARIABLE_VALUE_MAX];
} EFIVAR;
/** Pointer to an EFI NVRAM variable. */
typedef EFIVAR *PEFIVAR;
/** Pointer to an EFI NVRAM variable pointer. */
typedef PEFIVAR *PPEFIVAR;

/**
 * NVRAM state.
 */
typedef struct NVRAMDESC
{
    /** The current operation. */
    EFIVAROP        enmOp;
    /** The current status. */
    uint32_t        u32Status;
    /** The current  */
    uint32_t        offOpBuffer;
    /** The current number of variables. */
    uint32_t        cVariables;
    /** The list of variables. */
    RTLISTANCHOR    VarList;

    /** The unique variable sequence ID, for the saved state only.
     * @todo It's part of this structure for hysterical raisins, consider remove it
     *       when changing the saved state format the next time. */
    uint32_t        idUniqueCurVar;
    /** Variable buffered used both when adding and querying NVRAM variables.
     * When querying a variable, a copy of it is stored in this buffer and read
     * from it.  When adding, updating or deleting a variable, this buffer is used
     * to set up the parameters before taking action. */
    EFIVAR          VarOpBuf;
    /** The current variable. This is only used by EFI_VARIABLE_OP_QUERY_NEXT,
     * the attribute readers work against the copy in VarOpBuf. */
    PEFIVAR         pCurVar;
} NVRAMDESC;


/**
 * The EFI device state structure.
 */
typedef struct DEVEFI
{
    /** Pointer back to the device instance. */
    PPDMDEVINS              pDevIns;
    /** EFI message buffer. */
    char                    szMsg[VBOX_EFI_DEBUG_BUFFER];
    /** EFI message buffer index. */
    uint32_t                iMsg;
    /** EFI panic message buffer. */
    char                    szPanicMsg[2048];
    /** EFI panic message buffer index. */
    uint32_t                iPanicMsg;
    /** The system EFI ROM data. */
    uint8_t                *pu8EfiRom;
    /** The size of the system EFI ROM. */
    uint64_t                cbEfiRom;
    /** The name of the EFI ROM file. */
    char                   *pszEfiRomFile;
    /** Thunk page pointer. */
    uint8_t                *pu8EfiThunk;
    /** First entry point of the EFI firmware. */
    RTGCPHYS                GCEntryPoint0;
    /** Second Entry Point (PeiCore)*/
    RTGCPHYS                GCEntryPoint1;
    /** EFI firmware physical load address. */
    RTGCPHYS                GCLoadAddress;
    /** Current info selector. */
    uint32_t                iInfoSelector;
    /** Current info position. */
    int32_t                 offInfo;

    /** Number of virtual CPUs. (Config) */
    uint32_t                cCpus;
    /** RAM below 4GB (in bytes). (Config) */
    uint32_t                cbBelow4GB;
    /** RAM above 4GB (in bytes). (Config) */
    uint64_t                cbAbove4GB;
    /** The total amount of memory. */
    uint64_t                cbRam;
    /** The size of the RAM hole below 4GB. */
    uint64_t                cbRamHole;

    /** The size of the DMI tables. */
    uint16_t                cbDmiTables;
    /** The DMI tables. */
    uint8_t                 au8DMIPage[0x1000];

    /** I/O-APIC enabled? */
    uint8_t                 u8IOAPIC;

    /** Boot parameters passed to the firmware. */
    char                    szBootArgs[256];

    /** Host UUID (for DMI). */
    RTUUID                  aUuid;

    /** Device properties buffer. */
    R3PTRTYPE(uint8_t *)    pbDeviceProps;
    /** Device properties buffer size. */
    uint32_t                cbDeviceProps;

    /** Virtual machine front side bus frequency. */
    uint64_t                u64FsbFrequency;
    /** Virtual machine time stamp counter frequency. */
    uint64_t                u64TscFrequency;
    /** Virtual machine CPU frequency. */
    uint64_t                u64CpuFrequency;
    /** GOP mode. */
    uint32_t                u32GopMode;
    /** Uga mode horisontal resolution. */
    uint32_t                cxUgaResolution;
    /** Uga mode vertical resolution. */
    uint32_t                cyUgaResolution;


    /** NVRAM state variables. */
    NVRAMDESC               NVRAM;

    /**
     * NVRAM port - LUN\#0.
     */
    struct
    {
        /** The base interface we provide the NVRAM driver. */
        PDMIBASE            IBase;
        /** The NVRAM driver base interface. */
        PPDMIBASE           pDrvBase;
        /** The NVRAM interface provided by the driver. */
        PPDMINVRAMCONNECTOR pNvramDrv;
    } Lun0;
} DEVEFI;
typedef DEVEFI *PDEVEFI;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The saved state version. */
#define EFI_SSM_VERSION 2
/** The saved state version from VBox 4.2. */
#define EFI_SSM_VERSION_4_2 1


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Saved state NVRAMDESC field descriptors. */
static SSMFIELD const g_aEfiNvramDescField[] =
{
        SSMFIELD_ENTRY(       NVRAMDESC, enmOp),
        SSMFIELD_ENTRY(       NVRAMDESC, u32Status),
        SSMFIELD_ENTRY(       NVRAMDESC, offOpBuffer),
        SSMFIELD_ENTRY_IGNORE(NVRAMDESC, VarOpBuf),
        SSMFIELD_ENTRY(       NVRAMDESC, cVariables),
        SSMFIELD_ENTRY_OLD(   idUnquireLast, 4),
        SSMFIELD_ENTRY_IGNORE(NVRAMDESC, VarList),
        SSMFIELD_ENTRY(       NVRAMDESC, idUniqueCurVar),
        SSMFIELD_ENTRY_IGNORE(NVRAMDESC, pCurVar),
        SSMFIELD_ENTRY_TERM()
};

/** Saved state EFIVAR field descriptors. */
static SSMFIELD const g_aEfiVariableDescFields[] =
{
        SSMFIELD_ENTRY_IGNORE(EFIVAR, ListNode),
        SSMFIELD_ENTRY(       EFIVAR, idUniqueSavedState),
        SSMFIELD_ENTRY(       EFIVAR, uuid),
        SSMFIELD_ENTRY(       EFIVAR, szName),
        SSMFIELD_ENTRY_OLD(   cchName, 4),
        SSMFIELD_ENTRY(       EFIVAR, abValue),
        SSMFIELD_ENTRY(       EFIVAR, cbValue),
        SSMFIELD_ENTRY(       EFIVAR, fAttributes),
        SSMFIELD_ENTRY_TERM()
};




/**
 * Flushes the variable list.
 *
 * @param   pThis               The EFI state.
 */
static void nvramFlushDeviceVariableList(PDEVEFI pThis)
{
    while (!RTListIsEmpty(&pThis->NVRAM.VarList))
    {
        PEFIVAR pEfiVar = RTListNodeGetNext(&pThis->NVRAM.VarList, EFIVAR, ListNode);
        RTListNodeRemove(&pEfiVar->ListNode);
        RTMemFree(pEfiVar);
    }

    pThis->NVRAM.pCurVar = NULL;
}

/**
 * This function looks up variable in NVRAM list.
 */
static int nvramLookupVariableByUuidAndName(PDEVEFI pThis, char *pszVariableName, PCRTUUID pUuid, PPEFIVAR ppEfiVar)
{
    LogFlowFunc(("%RTuuid::'%s'\n", pUuid, pszVariableName));

    int      rc         = VERR_NOT_FOUND;
    uint32_t cVariables = 0;
    PEFIVAR  pEfiVar;
    RTListForEach(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode)
    {
        LogFlowFunc(("pEfiVar:%p\n", pEfiVar));
        cVariables++;
        if (   pEfiVar
            && RTUuidCompare(pUuid, &pEfiVar->uuid) == 0
            && RTStrCmp(pszVariableName, pEfiVar->szName) == 0) /** @todo case sensitive or insensitive? */
        {
            *ppEfiVar = pEfiVar;
            rc = VINF_SUCCESS;
            break;
        }
    }
    Assert(pThis->NVRAM.cVariables >= cVariables);

    LogFlowFunc(("rc=%Rrc pEfiVar=%p\n", rc, *ppEfiVar));
    return rc;
}

/**
 * Creates an device internal list of variables.
 *
 * @returns VBox status code.
 * @param   pThis           The EFI state.
 */
static int nvramLoad(PDEVEFI pThis)
{
    int rc;
    for (uint32_t iVar = 0; iVar < EFI_VARIABLE_MAX; iVar++)
    {
        PEFIVAR pEfiVar = (PEFIVAR)RTMemAllocZ(sizeof(EFIVAR));
        AssertReturn(pEfiVar, VERR_NO_MEMORY);

        pEfiVar->cchName = sizeof(pEfiVar->szName);
        pEfiVar->cbValue = sizeof(pEfiVar->abValue);
        rc = pThis->Lun0.pNvramDrv->pfnVarQueryByIndex(pThis->Lun0.pNvramDrv, iVar,
                                                       &pEfiVar->uuid, &pEfiVar->szName[0], &pEfiVar->cchName,
                                                       &pEfiVar->fAttributes, &pEfiVar->abValue[0], &pEfiVar->cbValue);
        if (RT_SUCCESS(rc))
        {
            /* Some validations. */
            rc = RTStrValidateEncoding(pEfiVar->szName);
            size_t cchName = RTStrNLen(pEfiVar->szName, sizeof(pEfiVar->szName));
            if (cchName != pEfiVar->cchName)
                rc = VERR_INVALID_PARAMETER;
            if (pEfiVar->cbValue == 0)
                rc = VERR_NO_DATA;
            if (RT_FAILURE(rc))
                LogRel(("EFI/nvramLoad: Bad variable #%u: cbValue=%#x cchName=%#x (strlen=%#x) szName=%.*Rhxs\n",
                        pEfiVar->cbValue, pEfiVar->cchName, cchName, pEfiVar->cchName + 1, pEfiVar->szName));
        }
        if (RT_FAILURE(rc))
        {
            RTMemFree(pEfiVar);
            if (rc == VERR_NOT_FOUND)
                rc = VINF_SUCCESS;
            AssertRC(rc);
            return rc;
        }

        /* Append it. */
        RTListAppend((PRTLISTNODE)&pThis->NVRAM.VarList, &pEfiVar->ListNode);
        pThis->NVRAM.cVariables++;
    }

    AssertLogRelMsgFailed(("EFI: Too many variables.\n"));
    return VERR_TOO_MUCH_DATA;
}


/**
 * Let the NVRAM driver store the internal NVRAM variable list.
 *
 * @returns VBox status code.
 * @param   pThis               The EFI state.
 */
static int nvramStore(PDEVEFI pThis)
{
    int rc = pThis->Lun0.pNvramDrv->pfnVarStoreSeqBegin(pThis->Lun0.pNvramDrv, pThis->NVRAM.cVariables);
    if (RT_SUCCESS(rc))
    {
        uint32_t    idxVar  = 0;
        PEFIVAR     pEfiVar;
        RTListForEach(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode)
        {
            int rc2 = pThis->Lun0.pNvramDrv->pfnVarStoreSeqPut(pThis->Lun0.pNvramDrv, idxVar,
                                                               &pEfiVar->uuid, pEfiVar->szName,  pEfiVar->cchName,
                                                               pEfiVar->fAttributes, pEfiVar->abValue, pEfiVar->cbValue);
            if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rc))
            {
                LogRel(("EFI: pfnVarStoreVarByIndex failed: %Rrc\n", rc));
                rc = rc2;
            }
            idxVar++;
        }
        Assert((pThis->NVRAM.cVariables == idxVar));
        rc = pThis->Lun0.pNvramDrv->pfnVarStoreSeqEnd(pThis->Lun0.pNvramDrv, rc);
    }
    else
        LogRel(("EFI: pfnVarStoreBegin failed: %Rrc\n", rc));
    return rc;
}

/**
 * EFI_VARIABLE_OP_QUERY and EFI_VARIABLE_OP_QUERY_NEXT worker that copies the
 * variable into the VarOpBuf, set pCurVar and u32Status.
 *
 * @param   pThis               The EFI state.
 * @param   pEfiVar             The resulting variable. NULL if not found / end.
 */
static void nvramWriteVariableOpQueryCopyResult(PDEVEFI pThis, PEFIVAR pEfiVar)
{
    RT_ZERO(pThis->NVRAM.VarOpBuf.szName);
    RT_ZERO(pThis->NVRAM.VarOpBuf.abValue);
    if (pEfiVar)
    {
        pThis->NVRAM.VarOpBuf.uuid        = pEfiVar->uuid;
        pThis->NVRAM.VarOpBuf.cchName     = pEfiVar->cchName;
        memcpy(pThis->NVRAM.VarOpBuf.szName, pEfiVar->szName, pEfiVar->cchName); /* no need for + 1. */
        pThis->NVRAM.VarOpBuf.fAttributes = pEfiVar->fAttributes;
        pThis->NVRAM.VarOpBuf.cbValue     = pEfiVar->cbValue;
        memcpy(pThis->NVRAM.VarOpBuf.abValue, pEfiVar->abValue, pEfiVar->cbValue);
        pThis->NVRAM.pCurVar              = pEfiVar;
        pThis->NVRAM.u32Status            = EFI_VARIABLE_OP_STATUS_OK;
        LogFlow(("EFI: Variable query -> %RTuuid::'%s' abValue=%.*Rhxs\n", &pThis->NVRAM.VarOpBuf.uuid,
                 pThis->NVRAM.VarOpBuf.szName, pThis->NVRAM.VarOpBuf.cbValue, pThis->NVRAM.VarOpBuf.abValue));
    }
    else
    {
        pThis->NVRAM.VarOpBuf.fAttributes = 0;
        pThis->NVRAM.VarOpBuf.cbValue     = 0;
        pThis->NVRAM.VarOpBuf.cchName     = 0;
        pThis->NVRAM.pCurVar              = NULL;
        pThis->NVRAM.u32Status            = EFI_VARIABLE_OP_STATUS_NOT_FOUND;
        LogFlow(("EFI: Variable query -> NOT_FOUND\n"));
    }
}

/**
 * Implements EFI_VARIABLE_PARAM + EFI_VARIABLE_OP_QUERY.
 *
 * @returns IOM strict status code.
 * @param   pThis               The EFI state.
 */
static int nvramWriteVariableOpQuery(PDEVEFI pThis)
{
    LogRel(("EFI: Querying Variable %RTuuid::'%s'\n",
            &pThis->NVRAM.VarOpBuf.uuid, pThis->NVRAM.VarOpBuf.szName));

    PEFIVAR pEfiVar;
    int rc = nvramLookupVariableByUuidAndName(pThis,
                                              pThis->NVRAM.VarOpBuf.szName,
                                              &pThis->NVRAM.VarOpBuf.uuid,
                                              &pEfiVar);
    nvramWriteVariableOpQueryCopyResult(pThis, RT_SUCCESS(rc) ? pEfiVar : NULL);
    return VINF_SUCCESS;
}

/**
 * Implements EFI_VARIABLE_PARAM + EFI_VARIABLE_OP_QUERY_NEXT.
 *
 * This simply walks the list.
 *
 * @returns IOM strict status code.
 * @param   pThis               The EFI state.
 */
static int nvramWriteVariableOpQueryNext(PDEVEFI pThis)
{
    Log(("EFI: Querying next variable...\n"));
    PEFIVAR pEfiVar = pThis->NVRAM.pCurVar;
    if (pEfiVar)
        pEfiVar = RTListGetNext(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode);
    nvramWriteVariableOpQueryCopyResult(pThis, pEfiVar);
    return VINF_SUCCESS;
}

/**
 * Implements EFI_VARIABLE_PARAM + EFI_VARIABLE_OP_ADD.
 *
 * @returns IOM strict status code.
 * @param   pThis               The EFI state.
 */
static int nvramWriteVariableOpAdd(PDEVEFI pThis)
{
    LogRel(("EFI: Adding variable %RTuuid::'%s'\n", &pThis->NVRAM.VarOpBuf.uuid, pThis->NVRAM.VarOpBuf.szName));
    LogFlowFunc(("fAttributes=%#x abValue=%.*Rhxs\n", pThis->NVRAM.VarOpBuf.fAttributes,
                 pThis->NVRAM.VarOpBuf.cbValue, pThis->NVRAM.VarOpBuf.abValue));

    /*
     * Validate and adjust the input a little before we start.
     */
    int rc = RTStrValidateEncoding(pThis->NVRAM.VarOpBuf.szName);
    if (RT_FAILURE(rc))
        LogRel(("EFI: Badly encoded variable name: %.*Rhxs\n", pThis->NVRAM.VarOpBuf.cchName + 1, pThis->NVRAM.VarOpBuf.szName));
    if (RT_FAILURE(rc))
    {
        pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
        return VINF_SUCCESS;
    }
    pThis->NVRAM.VarOpBuf.cchName = RTStrNLen(pThis->NVRAM.VarOpBuf.szName, sizeof(pThis->NVRAM.VarOpBuf.szName));

    /*
     * Look it up and see what to do.
     */
    PEFIVAR pEfiVar;
    rc = nvramLookupVariableByUuidAndName(pThis,
                                          pThis->NVRAM.VarOpBuf.szName,
                                          &pThis->NVRAM.VarOpBuf.uuid,
                                          &pEfiVar);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("Old abValue=%.*Rhxs\n", pEfiVar->cbValue, pEfiVar->abValue));
#if 0 /** @todo Implement read-only EFI variables. */
        if (pEfiVar->fAttributes & EFI_VARIABLE_XXXXXXX)
        {
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_RO;
            break;
        }
#endif

        if (pThis->NVRAM.VarOpBuf.cbValue == 0)
        {
            /*
             * Delete it.
             */
            LogFlow(("nvramWriteVariableOpAdd: Delete\n"));
            RTListNodeRemove(&pEfiVar->ListNode);
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_OK;
            pThis->NVRAM.cVariables--;

            if (pThis->NVRAM.pCurVar == pEfiVar)
                pThis->NVRAM.pCurVar = NULL;
            RTMemFree(pEfiVar);
            pEfiVar = NULL;
        }
        else
        {
            /*
             * Update/replace it. (The name and UUID are unchanged, of course.)
             */
            LogFlow(("nvramWriteVariableOpAdd: Replace\n"));
            pEfiVar->fAttributes   = pThis->NVRAM.VarOpBuf.fAttributes;
            pEfiVar->cbValue       = pThis->NVRAM.VarOpBuf.cbValue;
            memcpy(pEfiVar->abValue, pThis->NVRAM.VarOpBuf.abValue, pEfiVar->cbValue);
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_OK;
        }
    }
    else if (pThis->NVRAM.VarOpBuf.cbValue == 0)
    {
        /* delete operation, but nothing to delete. */
        LogFlow(("nvramWriteVariableOpAdd: Delete (not found)\n"));
        pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_OK;
    }
    else if (pThis->NVRAM.cVariables < EFI_VARIABLE_MAX)
    {
        /*
         * Add a new variable.
         */
        LogFlow(("nvramWriteVariableOpAdd: New\n"));
        pEfiVar = (PEFIVAR)RTMemAllocZ(sizeof(EFIVAR));
        if (pEfiVar)
        {
            pEfiVar->uuid          = pThis->NVRAM.VarOpBuf.uuid;
            pEfiVar->cchName       = pThis->NVRAM.VarOpBuf.cchName;
            memcpy(pEfiVar->szName, pThis->NVRAM.VarOpBuf.szName, pEfiVar->cchName); /* The buffer is zeroed, so skip '\0'. */
            pEfiVar->fAttributes   = pThis->NVRAM.VarOpBuf.fAttributes;
            pEfiVar->cbValue       = pThis->NVRAM.VarOpBuf.cbValue;
            memcpy(pEfiVar->abValue, pThis->NVRAM.VarOpBuf.abValue, pEfiVar->cbValue);

            RTListAppend(&pThis->NVRAM.VarList, &pEfiVar->ListNode);
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_OK;
            pThis->NVRAM.cVariables++;
        }
        else
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
    }
    else
    {
        /*
         * Too many variables.
         */
        static unsigned s_cWarnings = 0;
        if (s_cWarnings++ < 5)
            LogRel(("EFI: Too many variables.\n"));
        pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
        Log(("nvramWriteVariableOpAdd: Too many variabled.\n"));
    }

    LogFunc(("cVariables=%u u32Status=%#x\n", pThis->NVRAM.cVariables, pThis->NVRAM.u32Status));
    return VINF_SUCCESS;
}

/**
 * Implements EFI_VARIABLE_PARAM writes.
 *
 * @returns IOM strict status code.
 * @param   pThis               The EFI state.
 * @param   u32Value            The value being written.
 */
static int nvramWriteVariableParam(PDEVEFI pThis, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;
    switch (pThis->NVRAM.enmOp)
    {
        case EFI_VM_VARIABLE_OP_START:
            switch (u32Value)
            {
                case EFI_VARIABLE_OP_QUERY:
                    rc = nvramWriteVariableOpQuery(pThis);
                    break;

                case EFI_VARIABLE_OP_QUERY_NEXT:
                    rc = nvramWriteVariableOpQueryNext(pThis);
                    break;

                case EFI_VARIABLE_OP_ADD:
                    rc = nvramWriteVariableOpAdd(pThis);
                    break;

                default:
                    pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
                    LogRel(("EFI: Unknown  EFI_VM_VARIABLE_OP_START value %#x\n", u32Value));
                    break;
            }
            break;

        case EFI_VM_VARIABLE_OP_GUID:
            Log2(("EFI_VM_VARIABLE_OP_GUID[%#x]=%#x\n", pThis->NVRAM.offOpBuffer, u32Value));
            if (pThis->NVRAM.offOpBuffer < sizeof(pThis->NVRAM.VarOpBuf.uuid))
                pThis->NVRAM.VarOpBuf.uuid.au8[pThis->NVRAM.offOpBuffer++] = (uint8_t)u32Value;
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_GUID write (%#x).\n", u32Value));
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            break;

        case EFI_VM_VARIABLE_OP_ATTRIBUTE:
            Log2(("EFI_VM_VARIABLE_OP_ATTRIBUTE=%#x\n", u32Value));
            pThis->NVRAM.VarOpBuf.fAttributes = u32Value;
            break;

        case EFI_VM_VARIABLE_OP_NAME:
            Log2(("EFI_VM_VARIABLE_OP_NAME[%#x]=%#x\n", pThis->NVRAM.offOpBuffer, u32Value));
            if (pThis->NVRAM.offOpBuffer < pThis->NVRAM.VarOpBuf.cchName)
                pThis->NVRAM.VarOpBuf.szName[pThis->NVRAM.offOpBuffer++] = (uint8_t)u32Value;
            else if (u32Value == 0)
                Assert(pThis->NVRAM.VarOpBuf.szName[sizeof(pThis->NVRAM.VarOpBuf.szName) - 1] == 0);
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_NAME write (%#x).\n", u32Value));
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            break;

        case EFI_VM_VARIABLE_OP_NAME_LENGTH:
            Log2(("EFI_VM_VARIABLE_OP_NAME_LENGTH=%#x\n", u32Value));
            RT_ZERO(pThis->NVRAM.VarOpBuf.szName);
            if (u32Value < sizeof(pThis->NVRAM.VarOpBuf.szName))
                pThis->NVRAM.VarOpBuf.cchName = u32Value;
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_NAME_LENGTH write (%#x, max %#x).\n",
                        u32Value, sizeof(pThis->NVRAM.VarOpBuf.szName) - 1));
                pThis->NVRAM.VarOpBuf.cchName = sizeof(pThis->NVRAM.VarOpBuf.szName) - 1;
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            Assert(pThis->NVRAM.offOpBuffer == 0);
            break;

        case EFI_VM_VARIABLE_OP_NAME_UTF16:
        {
            Log2(("EFI_VM_VARIABLE_OP_NAME_UTF16[%#x]=%#x\n", pThis->NVRAM.offOpBuffer, u32Value));
            /* Currently simplifying this to UCS2, i.e. no surrogates. */
            if (pThis->NVRAM.offOpBuffer == 0)
                RT_ZERO(pThis->NVRAM.VarOpBuf.szName);
            size_t cbUtf8 = RTStrCpSize(u32Value);
            if (pThis->NVRAM.offOpBuffer + cbUtf8 < sizeof(pThis->NVRAM.VarOpBuf.szName))
            {
                RTStrPutCp(&pThis->NVRAM.VarOpBuf.szName[pThis->NVRAM.offOpBuffer], u32Value);
                pThis->NVRAM.offOpBuffer += cbUtf8;
            }
            else if (u32Value == 0)
                Assert(pThis->NVRAM.VarOpBuf.szName[sizeof(pThis->NVRAM.VarOpBuf.szName) - 1] == 0);
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_NAME_UTF16 write (%#x).\n", u32Value));
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            break;
        }

        case EFI_VM_VARIABLE_OP_VALUE:
            Log2(("EFI_VM_VARIABLE_OP_VALUE[%#x]=%#x\n", pThis->NVRAM.offOpBuffer, u32Value));
            if (pThis->NVRAM.offOpBuffer < pThis->NVRAM.VarOpBuf.cbValue)
                pThis->NVRAM.VarOpBuf.abValue[pThis->NVRAM.offOpBuffer++] = (uint8_t)u32Value;
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_VALUE write (%#x).\n", u32Value));
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            break;

        case EFI_VM_VARIABLE_OP_VALUE_LENGTH:
            Log2(("EFI_VM_VARIABLE_OP_VALUE_LENGTH=%#x\n", u32Value));
            RT_ZERO(pThis->NVRAM.VarOpBuf.abValue);
            if (u32Value <= sizeof(pThis->NVRAM.VarOpBuf.abValue))
                pThis->NVRAM.VarOpBuf.cbValue = u32Value;
            else
            {
                LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_VALUE_LENGTH write (%#x, max %#x).\n",
                        u32Value, sizeof(pThis->NVRAM.VarOpBuf.abValue)));
                pThis->NVRAM.VarOpBuf.cbValue = sizeof(pThis->NVRAM.VarOpBuf.abValue);
                pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            }
            Assert(pThis->NVRAM.offOpBuffer == 0);
            break;

        default:
            pThis->NVRAM.u32Status = EFI_VARIABLE_OP_STATUS_ERROR;
            LogRel(("EFI: Unexpected variable operation %#x\n", pThis->NVRAM.enmOp));
            break;
    }
    return VINF_SUCCESS;
}

/**
 * Implements EFI_VARIABLE_OP reads.
 *
 * @returns IOM strict status code.
 * @param   pThis               The EFI state.
 * @param   u32Value            The value being written.
 */
static int nvramReadVariableOp(PDEVEFI pThis,  uint32_t *pu32, unsigned cb)
{
    switch (pThis->NVRAM.enmOp)
    {
        case EFI_VM_VARIABLE_OP_START:
            *pu32 = pThis->NVRAM.u32Status;
            break;

        case EFI_VM_VARIABLE_OP_GUID:
            if (pThis->NVRAM.offOpBuffer < sizeof(pThis->NVRAM.VarOpBuf.uuid) && cb == 1)
                *pu32 = pThis->NVRAM.VarOpBuf.uuid.au8[pThis->NVRAM.offOpBuffer++];
            else
            {
                if (cb == 1)
                    LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_GUID read.\n"));
                else
                    LogRel(("EFI: Invalid EFI_VM_VARIABLE_OP_GUID read size (%d).\n", cb));
                *pu32 = UINT32_MAX;
            }
            break;

        case EFI_VM_VARIABLE_OP_ATTRIBUTE:
            *pu32 = pThis->NVRAM.VarOpBuf.fAttributes;
            break;

        case EFI_VM_VARIABLE_OP_NAME:
            /* allow reading terminator char */
            if (pThis->NVRAM.offOpBuffer <= pThis->NVRAM.VarOpBuf.cchName && cb == 1)
                *pu32 = pThis->NVRAM.VarOpBuf.szName[pThis->NVRAM.offOpBuffer++];
            else
            {
                if (cb == 1)
                    LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_NAME read.\n"));
                else
                    LogRel(("EFI: Invalid EFI_VM_VARIABLE_OP_NAME read size (%d).\n", cb));
                *pu32 = UINT32_MAX;
            }
            break;

        case EFI_VM_VARIABLE_OP_NAME_LENGTH:
            *pu32 = pThis->NVRAM.VarOpBuf.cchName;
            break;

        case EFI_VM_VARIABLE_OP_NAME_UTF16:
            /* Lazy bird: ASSUME no surrogate pairs. */
            if (pThis->NVRAM.offOpBuffer < pThis->NVRAM.VarOpBuf.cchName)
            {
                char const *psz1 = &pThis->NVRAM.VarOpBuf.szName[pThis->NVRAM.offOpBuffer];
                char const *psz2 = psz2;
                RTUNICP Cp;
                RTStrGetCpEx(&psz2, &Cp);
                *pu32 = Cp;
                pThis->NVRAM.offOpBuffer += psz2 - psz1;
            }
            else if (pThis->NVRAM.offOpBuffer == pThis->NVRAM.VarOpBuf.cchName)
            {
                *pu32 = 0;
                pThis->NVRAM.offOpBuffer++;
            }
            else
            {
                if (cb == 1)
                    LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_NAME_UTF16 read.\n"));
                else
                    LogRel(("EFI: Invalid EFI_VM_VARIABLE_OP_NAME_UTF16 read size (%d).\n", cb));
                *pu32 = UINT32_MAX;
            }
            break;

        case EFI_VM_VARIABLE_OP_NAME_LENGTH_UTF16:
            /* Lazy bird: ASSUME no surrogate pairs. */
            *pu32 = RTStrUniLen(pThis->NVRAM.VarOpBuf.szName);
            break;

        case EFI_VM_VARIABLE_OP_VALUE:
            if (pThis->NVRAM.offOpBuffer < pThis->NVRAM.VarOpBuf.cbValue && cb == 1)
                *pu32 = pThis->NVRAM.VarOpBuf.abValue[pThis->NVRAM.offOpBuffer++];
            else
            {
                if (cb == 1)
                    LogRel(("EFI: Out of bounds EFI_VM_VARIABLE_OP_VALUE read.\n"));
                else
                    LogRel(("EFI: Invalid EFI_VM_VARIABLE_OP_VALUE read size (%d).\n", cb));
                *pu32 = UINT32_MAX;
            }
            break;

        case EFI_VM_VARIABLE_OP_VALUE_LENGTH:
            *pu32 = pThis->NVRAM.VarOpBuf.cbValue;
            break;

        default:
            *pu32 = UINT32_MAX;
            break;
    }
    return VINF_SUCCESS;
}

/**
 * @implement_callback_method{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) efiInfoNvram(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    PDMCritSectEnter(pDevIns->pCritSectRoR3, VERR_IGNORED);

    pHlp->pfnPrintf(pHlp, "NVRAM variables: %u\n", pThis->NVRAM.cVariables);
    PEFIVAR pEfiVar;
    RTListForEach(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode)
    {
        pHlp->pfnPrintf(pHlp, "%RTuuid::'%s' = %.*Rhxs (attr=%#x)\n",
                        &pEfiVar->uuid, pEfiVar->szName, pEfiVar->cbValue, pEfiVar->abValue, pEfiVar->fAttributes);
    }

    PDMCritSectLeave(pDevIns->pCritSectRoR3);
}



/**
 * Gets the info item size.
 *
 * @returns Size in bytes, UINT32_MAX on error.
 * @param   pThis               .
 */
static uint32_t efiInfoSize(PDEVEFI pThis)
{
    switch (pThis->iInfoSelector)
    {
        case EFI_INFO_INDEX_VOLUME_BASE:
        case EFI_INFO_INDEX_VOLUME_SIZE:
        case EFI_INFO_INDEX_TEMPMEM_BASE:
        case EFI_INFO_INDEX_TEMPMEM_SIZE:
        case EFI_INFO_INDEX_STACK_BASE:
        case EFI_INFO_INDEX_STACK_SIZE:
        case EFI_INFO_INDEX_GOP_MODE:
        case EFI_INFO_INDEX_UGA_VERTICAL_RESOLUTION:
        case EFI_INFO_INDEX_UGA_HORISONTAL_RESOLUTION:
            return 4;
        case EFI_INFO_INDEX_BOOT_ARGS:
            return (uint32_t)RTStrNLen(pThis->szBootArgs, sizeof(pThis->szBootArgs)) + 1;
        case EFI_INFO_INDEX_DEVICE_PROPS:
            return pThis->cbDeviceProps;
        case EFI_INFO_INDEX_FSB_FREQUENCY:
        case EFI_INFO_INDEX_CPU_FREQUENCY:
        case EFI_INFO_INDEX_TSC_FREQUENCY:
            return 8;
    }
    return UINT32_MAX;
}


/**
 * efiInfoNextByte for a uint64_t value.
 *
 * @returns Next (current) byte.
 * @param   pThis               The EFI instance data.
 * @param   u64                 The value.
 */
static uint8_t efiInfoNextByteU64(PDEVEFI pThis, uint64_t u64)
{
    uint64_t off = pThis->offInfo;
    if (off >= 4)
        return 0;
    return (uint8_t)(off >> (off * 8));
}

/**
 * efiInfoNextByte for a uint32_t value.
 *
 * @returns Next (current) byte.
 * @param   pThis               The EFI instance data.
 * @param   u32                 The value.
 */
static uint8_t efiInfoNextByteU32(PDEVEFI pThis, uint32_t u32)
{
    uint32_t off = pThis->offInfo;
    if (off >= 4)
        return 0;
    return (uint8_t)(off >> (off * 8));
}

/**
 * efiInfoNextByte for a buffer.
 *
 * @returns Next (current) byte.
 * @param   pThis               The EFI instance data.
 * @param   pvBuf               The buffer.
 * @param   cbBuf               The buffer size.
 */
static uint8_t efiInfoNextByteBuf(PDEVEFI pThis, void const *pvBuf, size_t cbBuf)
{
    uint32_t off = pThis->offInfo;
    if (off >= cbBuf)
        return 0;
    return ((uint8_t const *)pvBuf)[off];
}

/**
 * Gets the next info byte.
 *
 * @returns Next (current) byte.
 * @param   pThis               The EFI instance data.
 */
static uint8_t efiInfoNextByte(PDEVEFI pThis)
{
    switch (pThis->iInfoSelector)
    {

        case EFI_INFO_INDEX_VOLUME_BASE:        return efiInfoNextByteU64(pThis, pThis->GCLoadAddress);
        case EFI_INFO_INDEX_VOLUME_SIZE:        return efiInfoNextByteU64(pThis, pThis->cbEfiRom);
        case EFI_INFO_INDEX_TEMPMEM_BASE:       return efiInfoNextByteU32(pThis, VBOX_EFI_TOP_OF_STACK); /* just after stack */
        case EFI_INFO_INDEX_TEMPMEM_SIZE:       return efiInfoNextByteU32(pThis, _512K);
        case EFI_INFO_INDEX_FSB_FREQUENCY:      return efiInfoNextByteU64(pThis, pThis->u64FsbFrequency);
        case EFI_INFO_INDEX_TSC_FREQUENCY:      return efiInfoNextByteU64(pThis, pThis->u64TscFrequency);
        case EFI_INFO_INDEX_CPU_FREQUENCY:      return efiInfoNextByteU64(pThis, pThis->u64CpuFrequency);
        case EFI_INFO_INDEX_BOOT_ARGS:          return efiInfoNextByteBuf(pThis, pThis->szBootArgs, sizeof(pThis->szBootArgs));
        case EFI_INFO_INDEX_DEVICE_PROPS:       return efiInfoNextByteBuf(pThis, pThis->pbDeviceProps, pThis->cbDeviceProps);
        case EFI_INFO_INDEX_GOP_MODE:           return efiInfoNextByteU32(pThis, pThis->u32GopMode);
        case EFI_INFO_INDEX_UGA_HORISONTAL_RESOLUTION:  return efiInfoNextByteU32(pThis, pThis->cxUgaResolution);
        case EFI_INFO_INDEX_UGA_VERTICAL_RESOLUTION:    return efiInfoNextByteU32(pThis, pThis->cyUgaResolution);

        /* Keep in sync with value in EfiThunk.asm */
        case EFI_INFO_INDEX_STACK_BASE:         return efiInfoNextByteU32(pThis,  VBOX_EFI_TOP_OF_STACK - _128K); /* 2M - 128 K */
        case EFI_INFO_INDEX_STACK_SIZE:         return efiInfoNextByteU32(pThis, _128K);

        default:
            PDMDevHlpDBGFStop(pThis->pDevIns, RT_SRC_POS, "%#x", pThis->iInfoSelector);
            return 0;
    }
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) efiIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    Log4(("EFI in: %x %x\n", Port, cb));

    switch (Port)
    {
        case EFI_INFO_PORT:
            if (pThis->offInfo == -1 && cb == 4)
            {
                pThis->offInfo = 0;
                uint32_t cbInfo = *pu32 = efiInfoSize(pThis);
                if (cbInfo == UINT32_MAX)
                    return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "iInfoSelector=%#x (%d)\n",
                                             pThis->iInfoSelector, pThis->iInfoSelector);
            }
            else
            {
                if (cb != 1)
                    return VERR_IOM_IOPORT_UNUSED;
                *pu32 = efiInfoNextByte(pThis);
                pThis->offInfo++;
            }
            return VINF_SUCCESS;

        case EFI_PANIC_PORT:
#ifdef IN_RING3
           LogRel(("EFI panic port read!\n"));
           /* Insert special code here on panic reads */
           return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "EFI Panic: panic port read!\n");
#else
           /* Reschedule to R3 */
           return VINF_IOM_R3_IOPORT_READ;
#endif

        case EFI_VARIABLE_OP:
            return nvramReadVariableOp(pThis, pu32, cb);

        case EFI_VARIABLE_PARAM:
            *pu32 = UINT32_MAX;
            return VINF_SUCCESS;
    }

    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) efiIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    int     rc    = VINF_SUCCESS;
    Log4(("efi: out %x %x %d\n", Port, u32, cb));

    switch (Port)
    {
        case EFI_INFO_PORT:
            Log2(("EFI_INFO_PORT: iInfoSelector=%#x\n", u32));
            pThis->iInfoSelector = u32;
            pThis->offInfo       = -1;
            break;

        case EFI_DEBUG_PORT:
        {
            /* The raw version. */
            switch (u32)
            {
                case '\r': Log3(("efi: <return>\n")); break;
                case '\n': Log3(("efi: <newline>\n")); break;
                case '\t': Log3(("efi: <tab>\n")); break;
                default:   Log3(("efi: %c (%02x)\n", u32, u32)); break;
            }
            /* The readable, buffered version. */
            if (u32 == '\n' || u32 == '\r')
            {
                pThis->szMsg[pThis->iMsg] = '\0';
                if (pThis->iMsg)
                {
                    Log(("efi: %s\n", pThis->szMsg));
#ifdef DEVEFI_WITH_VBOXDBG_SCRIPT
                    const char *pszVBoxDbg = strstr(pThis->szMsg, "VBoxDbg> ");
                    if (pszVBoxDbg)
                    {
                        pszVBoxDbg += sizeof("VBoxDbg> ") - 1;

                        PRTSTREAM pStrm;
                        int rc2 = RTStrmOpen("./DevEFI.VBoxDbg", "a", &pStrm);
                        if (RT_SUCCESS(rc2))
                        {
                            RTStrmPutStr(pStrm, pszVBoxDbg);
                            RTStrmPutCh(pStrm, '\n');
                            RTStrmClose(pStrm);
                        }
                    }
#endif
                }
                pThis->iMsg = 0;
            }
            else
            {
                if (pThis->iMsg >= sizeof(pThis->szMsg)-1)
                {
                    pThis->szMsg[pThis->iMsg] = '\0';
                    Log(("efi: %s\n", pThis->szMsg));
                    pThis->iMsg = 0;
                }
                pThis->szMsg[pThis->iMsg] = (char )u32;
                pThis->szMsg[++pThis->iMsg] = '\0';
            }
            break;
        }

        case EFI_PANIC_PORT:
        {
            switch (u32)
            {
                case EFI_PANIC_CMD_BAD_ORG: /* Legacy */
                case EFI_PANIC_CMD_THUNK_TRAP:
                    LogRel(("EFI Panic: Unexpected trap!!\n"));
#ifdef VBOX_STRICT
                    return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "EFI Panic: Unexpected trap during early bootstrap!\n");
#else
                    AssertReleaseMsgFailed(("Unexpected trap during early EFI bootstrap!!\n"));
#endif
                    break;

                case EFI_PANIC_CMD_START_MSG:
                    pThis->iPanicMsg = 0;
                    pThis->szPanicMsg[0] = '\0';
                    break;

                case EFI_PANIC_CMD_END_MSG:
                    LogRel(("EFI Panic: %s\n", pThis->szPanicMsg));
#ifdef VBOX_STRICT
                    return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "EFI Panic: %s\n", pThis->szPanicMsg);
#else
                    return VERR_INTERNAL_ERROR;
#endif

                default:
                    if (    u32 >= EFI_PANIC_CMD_MSG_FIRST
                        &&  u32 <= EFI_PANIC_CMD_MSG_LAST)
                    {
                        /* Add the message char to the buffer. */
                        uint32_t i = pThis->iPanicMsg;
                        if (i + 1 < sizeof(pThis->szPanicMsg))
                        {
                            char ch = EFI_PANIC_CMD_MSG_GET_CHAR(u32);
                            if (    ch == '\n'
                                &&  i > 0
                                &&  pThis->szPanicMsg[i - 1] == '\r')
                                i--;
                            pThis->szPanicMsg[i] = ch;
                            pThis->szPanicMsg[i + 1] = '\0';
                            pThis->iPanicMsg = i + 1;
                        }
                    }
                    else
                        Log(("EFI: Unknown panic command: %#x (cb=%d)\n", u32, cb));
                    break;
            }
            break;
        }

        case EFI_VARIABLE_OP:
        {
            /* clear buffer index */
            if (u32 >= (uint32_t)EFI_VM_VARIABLE_OP_MAX)
            {
                Log(("EFI: Invalid variable op %#x\n", u32));
                u32 = EFI_VM_VARIABLE_OP_ERROR;
            }
            pThis->NVRAM.offOpBuffer = 0;
            pThis->NVRAM.enmOp = (EFIVAROP)u32;
            Log2(("EFI_VARIABLE_OP: enmOp=%#x (%d)\n", u32));
            break;
        }

        case EFI_VARIABLE_PARAM:
            rc = nvramWriteVariableParam(pThis, u32);
            break;

        default:
            Log(("EFI: Write to reserved port %RTiop: %#x (cb=%d)\n", Port, u32, cb));
            break;
    }
    return rc;
}

static DECLCALLBACK(int) efiSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    LogFlow(("efiSaveExec:\n"));

    /*
     * Set variables only used when saving state.
     */
    uint32_t idUniqueSavedState = 0;
    PEFIVAR pEfiVar;
    RTListForEach(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode)
    {
        pEfiVar->idUniqueSavedState = idUniqueSavedState++;
    }
    Assert(idUniqueSavedState == pThis->NVRAM.cVariables);

    pThis->NVRAM.idUniqueCurVar = pThis->NVRAM.pCurVar
                                ? pThis->NVRAM.pCurVar->idUniqueSavedState
                                : UINT32_MAX;

    /*
     * Save the NVRAM state.
     */
    SSMR3PutStructEx(pSSM, &pThis->NVRAM,          sizeof(NVRAMDESC), 0, g_aEfiNvramDescField,     NULL);
    SSMR3PutStructEx(pSSM, &pThis->NVRAM.VarOpBuf, sizeof(EFIVAR),    0, g_aEfiVariableDescFields, NULL);

    /*
     * Save the list variables (we saved the length above).
     */
    RTListForEach(&pThis->NVRAM.VarList, pEfiVar, EFIVAR, ListNode)
    {
        SSMR3PutStructEx(pSSM, pEfiVar, sizeof(EFIVAR), 0, g_aEfiVariableDescFields, NULL);
    }

    return VINF_SUCCESS; /* SSM knows */
}

static DECLCALLBACK(int) efiLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    LogFlow(("efiLoadExec: uVersion=%d uPass=%d\n", uVersion, uPass));

    /*
     * Validate input.
     */
    if (uPass != SSM_PASS_FINAL)
        return VERR_SSM_UNEXPECTED_PASS;
    if (   uVersion != EFI_SSM_VERSION
        && uVersion != EFI_SSM_VERSION_4_2
        )
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /*
     * Kill the current variables before loading anything.
     */
    nvramFlushDeviceVariableList(pThis);

    /*
     * Load the NVRAM state.
     */
    int rc = SSMR3GetStructEx(pSSM, &pThis->NVRAM, sizeof(NVRAMDESC), 0, g_aEfiNvramDescField, NULL);
    AssertRCReturn(rc, rc);
    pThis->NVRAM.pCurVar = NULL;

    rc = SSMR3GetStructEx(pSSM, &pThis->NVRAM.VarOpBuf, sizeof(EFIVAR), 0, g_aEfiVariableDescFields, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Load variables.
     */
    pThis->NVRAM.pCurVar = NULL;
    Assert(RTListIsEmpty(&pThis->NVRAM.VarList));
    RTListInit(&pThis->NVRAM.VarList);
    for (uint32_t i = 0; i < pThis->NVRAM.cVariables; i++)
    {
        PEFIVAR pEfiVar = (PEFIVAR)RTMemAllocZ(sizeof(EFIVAR));
        AssertPtrReturn(pEfiVar, VERR_NO_MEMORY);

        rc = SSMR3GetStructEx(pSSM, pEfiVar, sizeof(EFIVAR), 0, g_aEfiVariableDescFields, NULL);
        if (RT_SUCCESS(rc))
        {
            if (   pEfiVar->cbValue > sizeof(pEfiVar->abValue)
                || pEfiVar->cbValue == 0)
            {
                rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
                LogRel(("EFI: Loaded invalid variable value length %#x\n", pEfiVar->cbValue));
            }
            size_t cchVarName = RTStrNLen(pEfiVar->szName, sizeof(pEfiVar->szName));
            if (cchVarName >= sizeof(pEfiVar->szName))
            {
                rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
                LogRel(("EFI: Loaded variable name is unterminated.\n"));
            }
            if (pEfiVar->cchName > cchVarName) /* No check for 0 here, busted load code in 4.2, so now storing 0 here. */
            {
                rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
                LogRel(("EFI: Loaded invalid variable name length %#x (cchVarName=%#x)\n", pEfiVar->cchName, cchVarName));
            }
            if (RT_SUCCESS(rc))
                pEfiVar->cchName = cchVarName;
        }
        AssertRCReturnStmt(rc, RTMemFree(pEfiVar), rc);

        /* Add it, updating the current variable pointer while we're here. */
        RTListAppend(&pThis->NVRAM.VarList, &pEfiVar->ListNode);
        if (pThis->NVRAM.idUniqueCurVar == pEfiVar->idUniqueSavedState)
            pThis->NVRAM.pCurVar = pEfiVar;
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc(PDMIBASE::pfnQueryInterface)
 */
static DECLCALLBACK(void *) devEfiQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    LogFlowFunc(("ENTER: pIBase: %p, pszIID:%p\n", __FUNCTION__, pInterface, pszIID));
    PDEVEFI  pThis = RT_FROM_MEMBER(pInterface, DEVEFI, Lun0.IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->Lun0.IBase);
    return NULL;
}


/**
 * Write to CMOS memory.
 * This is used by the init complete code.
 */
static void cmosWrite(PPDMDEVINS pDevIns, unsigned off, uint32_t u32Val)
{
    Assert(off < 128);
    Assert(u32Val < 256);

    int rc = PDMDevHlpCMOSWrite(pDevIns, off, u32Val);
    AssertRC(rc);
}

/**
 * Init complete notification.
 *
 * @returns VBOX status code.
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(int) efiInitComplete(PPDMDEVINS pDevIns)
{
    /* PC Bios */
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    uint32_t u32;

    /*
     * Memory sizes.
     */
    uint64_t const offRamHole = _4G - pThis->cbRamHole;
    if (pThis->cbRam > 16 * _1M)
        u32 = (uint32_t)( (RT_MIN(RT_MIN(pThis->cbRam, offRamHole), UINT32_C(0xffe00000)) - 16U * _1M) / _64K );
    else
        u32 = 0;
    cmosWrite(pDevIns, 0x34, u32 & 0xff);
    cmosWrite(pDevIns, 0x35, u32 >> 8);

    /*
     * Number of CPUs.
     */
    cmosWrite(pDevIns, 0x60, pThis->cCpus & 0xff);

    return VINF_SUCCESS;
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) efiReset(PPDMDEVINS pDevIns)
{
    PDEVEFI  pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    int rc;

    LogFlow(("efiReset\n"));

    pThis->iInfoSelector = 0;
    pThis->offInfo       = -1;

    pThis->iMsg = 0;
    pThis->szMsg[0] = '\0';
    pThis->iPanicMsg = 0;
    pThis->szPanicMsg[0] = '\0';

    /*
     * Plan some structures in RAM.
     */
    FwCommonPlantSmbiosAndDmiHdrs(pDevIns, pThis->cbDmiTables);
    if (pThis->u8IOAPIC)
        FwCommonPlantMpsFloatPtr(pDevIns);

    /*
     * Re-shadow the Firmware Volume and make it RAM/RAM.
     */
    uint32_t    cPages = RT_ALIGN_64(pThis->cbEfiRom, PAGE_SIZE) >> PAGE_SHIFT;
    RTGCPHYS    GCPhys = pThis->GCLoadAddress;
    while (cPages > 0)
    {
        uint8_t abPage[PAGE_SIZE];

        /* Read the (original) ROM page and write it back to the RAM page. */
        rc = PDMDevHlpROMProtectShadow(pDevIns, GCPhys, PAGE_SIZE, PGMROMPROT_READ_ROM_WRITE_RAM);
        AssertLogRelRC(rc);

        rc = PDMDevHlpPhysRead(pDevIns, GCPhys, abPage, PAGE_SIZE);
        AssertLogRelRC(rc);
        if (RT_FAILURE(rc))
            memset(abPage, 0xcc, sizeof(abPage));

        rc = PDMDevHlpPhysWrite(pDevIns, GCPhys, abPage, PAGE_SIZE);
        AssertLogRelRC(rc);

        /* Switch to the RAM/RAM mode. */
        rc = PDMDevHlpROMProtectShadow(pDevIns, GCPhys, PAGE_SIZE, PGMROMPROT_READ_RAM_WRITE_RAM);
        AssertLogRelRC(rc);

        /* Advance */
        GCPhys += PAGE_SIZE;
        cPages--;
    }
}

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) efiDestruct(PPDMDEVINS pDevIns)
{
    PDEVEFI  pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);

    nvramStore(pThis);
    nvramFlushDeviceVariableList(pThis);

    if (pThis->pu8EfiRom)
    {
        RTFileReadAllFree(pThis->pu8EfiRom, (size_t)pThis->cbEfiRom);
        pThis->pu8EfiRom = NULL;
    }

    /*
     * Free MM heap pointers (waste of time, but whatever).
     */
    if (pThis->pszEfiRomFile)
    {
        MMR3HeapFree(pThis->pszEfiRomFile);
        pThis->pszEfiRomFile = NULL;
    }

    if (pThis->pu8EfiThunk)
    {
        MMR3HeapFree(pThis->pu8EfiThunk);
        pThis->pu8EfiThunk = NULL;
    }

    if (pThis->pbDeviceProps)
    {
        MMR3HeapFree(pThis->pbDeviceProps);
        pThis->pbDeviceProps = NULL;
        pThis->cbDeviceProps = 0;
    }

    return VINF_SUCCESS;
}

/**
 * Helper that searches for a FFS file of a given type.
 *
 * @returns Pointer to the FFS file header if found, NULL if not.
 *
 * @param   pFfsFile    Pointer to the FFS file header to start searching at.
 * @param   pbEnd       The end of the firmware volume.
 * @param   FileType    The file type to look for.
 * @param   pcbFfsFile  Where to store the FFS file size (includes header).
 */
DECLINLINE(EFI_FFS_FILE_HEADER const *)
efiFwVolFindFileByType(EFI_FFS_FILE_HEADER const *pFfsFile, uint8_t const *pbEnd, EFI_FV_FILETYPE FileType, uint32_t *pcbFile)
{
#define FFS_SIZE(hdr)   RT_MAKE_U32_FROM_U8((hdr)->Size[0], (hdr)->Size[1], (hdr)->Size[2], 0)
    while ((uintptr_t)pFfsFile < (uintptr_t)pbEnd)
    {
        if (pFfsFile->Type == FileType)
        {
            *pcbFile = FFS_SIZE(pFfsFile);
            LogFunc(("Found %RTuuid of type:%d\n", &pFfsFile->Name, FileType));
            return pFfsFile;
        }
        pFfsFile = (EFI_FFS_FILE_HEADER *)((uintptr_t)pFfsFile + RT_ALIGN(FFS_SIZE(pFfsFile), 8));
    }
#undef FFS_SIZE
    return NULL;
}


/**
 * Parse EFI ROM headers and find entry points.
 *
 * @returns VBox status.
 * @param   pThis    The device instance data.
 */
static int efiParseFirmware(PDEVEFI pThis)
{
    EFI_FIRMWARE_VOLUME_HEADER const *pFwVolHdr = (EFI_FIRMWARE_VOLUME_HEADER const *)pThis->pu8EfiRom;

    /*
     * Validate firmware volume header.
     */
    AssertLogRelMsgReturn(pFwVolHdr->Signature == RT_MAKE_U32_FROM_U8('_', 'F', 'V', 'H'),
                          ("%#x, expected %#x\n", pFwVolHdr->Signature, RT_MAKE_U32_FROM_U8('_', 'F', 'V', 'H')),
                          VERR_INVALID_MAGIC);
    AssertLogRelMsgReturn(pFwVolHdr->Revision == EFI_FVH_REVISION,
                          ("%#x, expected %#x\n", pFwVolHdr->Signature, EFI_FVH_REVISION),
                          VERR_VERSION_MISMATCH);
    /** @todo check checksum, see PE spec vol. 3 */
    AssertLogRelMsgReturn(pFwVolHdr->FvLength <= pThis->cbEfiRom,
                          ("%#llx, expected %#llx\n", pFwVolHdr->FvLength, pThis->cbEfiRom),
                          VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(      pFwVolHdr->BlockMap[0].Length > 0
                          &&    pFwVolHdr->BlockMap[0].NumBlocks > 0,
                          ("%#x, %x\n", pFwVolHdr->BlockMap[0].Length, pFwVolHdr->BlockMap[0].NumBlocks),
                          VERR_INVALID_PARAMETER);

    AssertLogRelMsgReturn(!(pThis->cbEfiRom & PAGE_OFFSET_MASK), ("%RX64\n", pThis->cbEfiRom), VERR_INVALID_PARAMETER);

    uint8_t const * const pbFwVolEnd = pThis->pu8EfiRom + pFwVolHdr->FvLength;
    pThis->GCLoadAddress = UINT32_C(0xfffff000) - pThis->cbEfiRom + PAGE_SIZE;

    return VINF_SUCCESS;
}

/**
 * Load EFI ROM file into the memory.
 *
 * @returns VBox status.
 * @param   pThis       The device instance data.
 * @param   pCfg        Configuration node handle for the device.
 */
static int efiLoadRom(PDEVEFI pThis, PCFGMNODE pCfg)
{
    /*
     * Read the entire firmware volume into memory.
     */
    void   *pvFile;
    size_t  cbFile;
    int rc = RTFileReadAllEx(pThis->pszEfiRomFile,
                             0 /*off*/,
                             RTFOFF_MAX /*cbMax*/,
                             RTFILE_RDALL_O_DENY_WRITE,
                             &pvFile,
                             &cbFile);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pThis->pDevIns, rc, RT_SRC_POS,
                                   N_("Loading the EFI firmware volume '%s' failed with rc=%Rrc"),
                                   pThis->pszEfiRomFile, rc);
    pThis->pu8EfiRom = (uint8_t *)pvFile;
    pThis->cbEfiRom  = cbFile;

    /*
     * Validate firmware volume and figure out the load address as well as the SEC entry point.
     */
    rc = efiParseFirmware(pThis);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pThis->pDevIns, rc, RT_SRC_POS,
                                   N_("Parsing the EFI firmware volume '%s' failed with rc=%Rrc"),
                                   pThis->pszEfiRomFile, rc);

    /*
     * Map the firmware volume into memory as shadowed ROM.
     */
    /** @todo fix PGMR3PhysRomRegister so it doesn't mess up in SUPLib when mapping a big ROM image. */
    RTGCPHYS cbQuart = RT_ALIGN_64(pThis->cbEfiRom / 4, PAGE_SIZE);
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress,
                              cbQuart,
                              pThis->pu8EfiRom,
                              cbQuart,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume");
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpROMProtectShadow(pThis->pDevIns, pThis->GCLoadAddress, (uint32_t)cbQuart, PGMROMPROT_READ_RAM_WRITE_IGNORE);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart,
                              cbQuart,
                              pThis->pu8EfiRom + cbQuart,
                              cbQuart,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 2)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart * 2,
                              cbQuart,
                              pThis->pu8EfiRom + cbQuart * 2,
                              cbQuart,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 3)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart * 3,
                              pThis->cbEfiRom - cbQuart * 3,
                              pThis->pu8EfiRom + cbQuart * 3,
                              pThis->cbEfiRom - cbQuart * 3,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 4)");
    if (RT_FAILURE(rc))
        return rc;
    return VINF_SUCCESS;
}

static uint8_t efiGetHalfByte(char ch)
{
    uint8_t val;

    if (ch >= '0' && ch <= '9')
        val = ch - '0';
    else if (ch >= 'A' && ch <= 'F')
        val = ch - 'A' + 10;
    else if(ch >= 'a' && ch <= 'f')
        val = ch - 'a' + 10;
    else
        val = 0xff;

    return val;

}


static int efiParseDeviceString(PDEVEFI  pThis, char *pszDeviceProps)
{
    int         rc = 0;
    uint32_t    iStr, iHex, u32OutLen;
    uint8_t     u8Value = 0;                    /* (shut up gcc) */
    bool        fUpper = true;

    u32OutLen = (uint32_t)RTStrNLen(pszDeviceProps, RTSTR_MAX) / 2 + 1;

    pThis->pbDeviceProps = (uint8_t *)PDMDevHlpMMHeapAlloc(pThis->pDevIns, u32OutLen);
    if (!pThis->pbDeviceProps)
        return VERR_NO_MEMORY;

    for (iStr=0, iHex = 0; pszDeviceProps[iStr]; iStr++)
    {
        uint8_t u8Hb = efiGetHalfByte(pszDeviceProps[iStr]);
        if (u8Hb > 0xf)
            continue;

        if (fUpper)
            u8Value = u8Hb << 4;
        else
            pThis->pbDeviceProps[iHex++] = u8Hb | u8Value;

        Assert(iHex < u32OutLen);
        fUpper = !fUpper;
    }

    Assert(iHex == 0 || fUpper);
    pThis->cbDeviceProps = iHex;

    return rc;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)  efiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDEVEFI     pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    int         rc;

    Assert(iInstance == 0);

    /*
     * Initalize the basic variables so that the destructor always works.
     */
    pThis->pDevIns = pDevIns;
    RTListInit(&pThis->NVRAM.VarList);
    pThis->Lun0.IBase.pfnQueryInterface = devEfiQueryInterface;


    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "EfiRom\0"
                              "RamSize\0"
                              "RamHoleSize\0"
                              "NumCPUs\0"
                              "UUID\0"
                              "IOAPIC\0"
                              "DmiBIOSFirmwareMajor\0"
                              "DmiBIOSFirmwareMinor\0"
                              "DmiBIOSReleaseDate\0"
                              "DmiBIOSReleaseMajor\0"
                              "DmiBIOSReleaseMinor\0"
                              "DmiBIOSVendor\0"
                              "DmiBIOSVersion\0"
                              "DmiSystemFamily\0"
                              "DmiSystemProduct\0"
                              "DmiSystemSerial\0"
                              "DmiSystemSKU\0"
                              "DmiSystemUuid\0"
                              "DmiSystemVendor\0"
                              "DmiSystemVersion\0"
                              "DmiBoardAssetTag\0"
                              "DmiBoardBoardType\0"
                              "DmiBoardLocInChass\0"
                              "DmiBoardProduct\0"
                              "DmiBoardSerial\0"
                              "DmiBoardVendor\0"
                              "DmiBoardVersion\0"
                              "DmiChassisAssetTag\0"
                              "DmiChassisSerial\0"
                              "DmiChassisVendor\0"
                              "DmiChassisVersion\0"
                              "DmiProcManufacturer\0"
                              "DmiProcVersion\0"
                              "DmiOEMVBoxVer\0"
                              "DmiOEMVBoxRev\0"
                              "DmiUseHostInfo\0"
                              "DmiExposeMemoryTable\0"
                              "DmiExposeProcInf\0"
                              "64BitEntry\0"
                              "BootArgs\0"
                              "DeviceProps\0"
                              "GopMode\0"
                              "UgaHorizontalResolution\0"
                              "UgaVerticalResolution\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: Invalid config value(s) for the EFI device"));

    /* CPU count (optional). */
    rc = CFGMR3QueryU32Def(pCfg, "NumCPUs", &pThis->cCpus, 1);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryU8Def(pCfg, "IOAPIC", &pThis->u8IOAPIC, 1);
    if (RT_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

    /*
     * Query the machine's UUID for SMBIOS/DMI use.
     */
    RTUUID  uuid;
    rc = CFGMR3QueryBytes(pCfg, "UUID", &uuid, sizeof(uuid));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"UUID\" failed"));

    /*
     * Convert the UUID to network byte order. Not entirely straightforward as
     * parts are MSB already...
     */
    uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
    uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
    uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
    memcpy(&pThis->aUuid, &uuid, sizeof pThis->aUuid);

    /*
     * RAM sizes
     */
    rc = CFGMR3QueryU64(pCfg, "RamSize", &pThis->cbRam);
    AssertLogRelRCReturn(rc, rc);
    rc = CFGMR3QueryU64(pCfg, "RamHoleSize", &pThis->cbRamHole);
    AssertLogRelRCReturn(rc, rc);
    pThis->cbBelow4GB = RT_MIN(pThis->cbRam, _4G - pThis->cbRamHole);
    pThis->cbAbove4GB = pThis->cbRam - pThis->cbBelow4GB;

    /*
     * Get the system EFI ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "EfiRom", &pThis->pszEfiRomFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszEfiRomFile = (char *)PDMDevHlpMMHeapAlloc(pDevIns, RTPATH_MAX);
        if (!pThis->pszEfiRomFile)
            return VERR_NO_MEMORY;

        rc = RTPathAppPrivateArchTop(pThis->pszEfiRomFile, RTPATH_MAX);
        AssertRCReturn(rc, rc);
        rc = RTPathAppend(pThis->pszEfiRomFile, RTPATH_MAX, "VBoxEFI32.fd");
        AssertRCReturn(rc, rc);
    }
    else if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"EfiRom\" as a string failed"));
    else if (!*pThis->pszEfiRomFile)
    {
        MMR3HeapFree(pThis->pszEfiRomFile);
        pThis->pszEfiRomFile = NULL;
    }

    /*
     * NVRAM processing.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, EFI_SSM_VERSION, sizeof(*pThis), efiSaveExec, efiLoadExec);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->Lun0.IBase, &pThis->Lun0.pDrvBase, "NvramStorage");
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, N_("Can't attach Nvram Storage driver"));

    pThis->Lun0.pNvramDrv = PDMIBASE_QUERY_INTERFACE(pThis->Lun0.pDrvBase, PDMINVRAMCONNECTOR);
    AssertPtrReturn(pThis->Lun0.pNvramDrv, VERR_PDM_MISSING_INTERFACE_BELOW);

    rc = nvramLoad(pThis);
    AssertRCReturn(rc, rc);

    /*
     * Get boot args.
     */
    rc = CFGMR3QueryStringDef(pCfg, "BootArgs", pThis->szBootArgs, sizeof(pThis->szBootArgs), "");
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"BootArgs\" as a string failed"));

    LogRel(("EFI boot args: %s\n", pThis->szBootArgs));

    /*
     * Get device props.
     */
    char *pszDeviceProps;
    rc = CFGMR3QueryStringAllocDef(pCfg, "DeviceProps", &pszDeviceProps, NULL);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"DeviceProps\" as a string failed"));
    if (pszDeviceProps)
    {
        LogRel(("EFI device props: %s\n", pszDeviceProps));
        rc = efiParseDeviceString(pThis, pszDeviceProps);
        MMR3HeapFree(pszDeviceProps);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Configuration error: Cannot parse device properties"));
    }
    else
    {
        pThis->pbDeviceProps = NULL;
        pThis->cbDeviceProps = 0;
    }

    /*
     * CPU frequencies
     */
    /// @todo we need to have VMM API to access TSC increase speed, for now provide reasonable default
    pThis->u64TscFrequency = RTMpGetMaxFrequency(0) * 1000 * 1000;// TMCpuTicksPerSecond(PDMDevHlpGetVM(pDevIns));
    if (pThis->u64TscFrequency == 0)
        pThis->u64TscFrequency = UINT64_C(2500000000);
    /* Multiplier is read from MSR_IA32_PERF_STATUS, and now is hardcoded as 4 */
    pThis->u64FsbFrequency = pThis->u64TscFrequency / 4;
    pThis->u64CpuFrequency = pThis->u64TscFrequency;

    /*
     * GOP graphics
     */
    rc = CFGMR3QueryU32Def(pCfg, "GopMode", &pThis->u32GopMode, 2 /* 1024x768 */);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"GopMode\" as a 32-bit int failed"));
    if (pThis->u32GopMode == UINT32_MAX)
        pThis->u32GopMode = 2; /* 1024x768 */

    /*
     * Uga graphics.
     */
    rc = CFGMR3QueryU32Def(pCfg, "UgaHorizontalResolution", &pThis->cxUgaResolution, 0); AssertRC(rc);
    if (pThis->cxUgaResolution == 0)
        pThis->cxUgaResolution = 1024;  /* 1024x768 */
    rc = CFGMR3QueryU32Def(pCfg, "UgaVerticalResolution", &pThis->cyUgaResolution, 0); AssertRC(rc);
    if (pThis->cyUgaResolution == 0)
        pThis->cyUgaResolution = 768;    /* 1024x768 */

#ifdef DEVEFI_WITH_VBOXDBG_SCRIPT
    /*
     * Zap the debugger script
     */
    RTFileDelete("./DevEFI.VBoxDbg");
#endif

    /*
     * Load firmware volume and thunk ROM.
     */
    rc = efiLoadRom(pThis, pCfg);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register our I/O ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, EFI_PORT_BASE, EFI_PORT_COUNT, NULL,
                                 efiIOPortWrite, efiIOPortRead,
                                 NULL, NULL, "EFI communication ports");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Plant DMI and MPS tables.
     */
    /** @todo XXX I wonder if we really need these tables as there is no SMBIOS header... */
    rc = FwCommonPlantDMITable(pDevIns, pThis->au8DMIPage, VBOX_DMI_TABLE_SIZE, &pThis->aUuid,
                               pDevIns->pCfg, pThis->cCpus, &pThis->cbDmiTables);
    AssertRCReturn(rc, rc);
    if (pThis->u8IOAPIC)
        FwCommonPlantMpsTable(pDevIns,
                              pThis->au8DMIPage + VBOX_DMI_TABLE_SIZE,
                              _4K - VBOX_DMI_TABLE_SIZE, pThis->cCpus);
    rc = PDMDevHlpROMRegister(pDevIns, VBOX_DMI_TABLE_BASE, _4K, pThis->au8DMIPage, _4K,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "DMI tables");

    AssertRCReturn(rc, rc);

    /*
     * Register info handlers.
     */
    rc = PDMDevHlpDBGFInfoRegister(pDevIns, "nvram", "Dumps the NVRAM variables.\n", efiInfoNvram);
    AssertRCReturn(rc, rc);

    /*
     * Call reset to set things up.
     */
    efiReset(pDevIns);

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceEFI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "efi",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Extensible Firmware Interface Device. "
    "LUN#0 - NVRAM port",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64,
    /* fClass */
    PDM_DEVREG_CLASS_ARCH_BIOS,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVEFI),
    /* pfnConstruct */
    efiConstruct,
    /* pfnDestruct */
    efiDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    efiReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete. */
    efiInitComplete,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
