/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Info.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF_INFO
#include <VBox/vmm/dbgf.h>

#include <VBox/vmm/mm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) dbgfR3InfoLog_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...);
static DECLCALLBACK(void) dbgfR3InfoLog_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args);
static DECLCALLBACK(void) dbgfR3InfoLogRel_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...);
static DECLCALLBACK(void) dbgfR3InfoLogRel_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args);
static DECLCALLBACK(void) dbgfR3InfoStdErr_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...);
static DECLCALLBACK(void) dbgfR3InfoStdErr_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args);
static DECLCALLBACK(void) dbgfR3InfoHelp(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Logger output. */
static const DBGFINFOHLP g_dbgfR3InfoLogHlp =
{
    dbgfR3InfoLog_Printf,
    dbgfR3InfoLog_PrintfV
};

/** Release logger output. */
static const DBGFINFOHLP g_dbgfR3InfoLogRelHlp =
{
    dbgfR3InfoLogRel_Printf,
    dbgfR3InfoLogRel_PrintfV
};

/** Standard error output. */
static const DBGFINFOHLP g_dbgfR3InfoStdErrHlp =
{
    dbgfR3InfoStdErr_Printf,
    dbgfR3InfoStdErr_PrintfV
};


/**
 * Initialize the info handlers.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
int dbgfR3InfoInit(PVM pVM)
{
    /*
     * Make sure we already didn't initialized in the lazy manner.
     */
    if (RTCritSectIsInitialized(&pVM->dbgf.s.InfoCritSect))
        return VINF_SUCCESS;

    /*
     * Initialize the crit sect.
     */
    int rc = RTCritSectInit(&pVM->dbgf.s.InfoCritSect);
    AssertRCReturn(rc, rc);

    /*
     * Register the 'info help' item.
     */
    rc = DBGFR3InfoRegisterInternal(pVM, "help", "List of info items.", dbgfR3InfoHelp);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Terminate the info handlers.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
int dbgfR3InfoTerm(PVM pVM)
{
    /*
     * Delete the crit sect.
     */
    int rc = RTCritSectDelete(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    return rc;
}


/** Logger output.
 * @copydoc DBGFINFOHLP::pfnPrintf */
static DECLCALLBACK(void) dbgfR3InfoLog_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...)
{
    NOREF(pHlp);
    va_list args;
    va_start(args, pszFormat);
    RTLogPrintfV(pszFormat, args);
    va_end(args);
}

/** Logger output.
 * @copydoc DBGFINFOHLP::pfnPrintfV */
static DECLCALLBACK(void) dbgfR3InfoLog_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args)
{
    NOREF(pHlp);
    RTLogPrintfV(pszFormat, args);
}


/**
 * Gets the logger info helper.
 * The returned info helper will unconditionally write all output to the log.
 *
 * @returns Pointer to the logger info helper.
 */
VMMR3DECL(PCDBGFINFOHLP) DBGFR3InfoLogHlp(void)
{
    return &g_dbgfR3InfoLogHlp;
}


/** Release logger output.
 * @copydoc DBGFINFOHLP::pfnPrintf */
static DECLCALLBACK(void) dbgfR3InfoLogRel_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...)
{
    NOREF(pHlp);
    va_list args;
    va_start(args, pszFormat);
    RTLogRelPrintfV(pszFormat, args);
    va_end(args);
}

/** Release logger output.
 * @copydoc DBGFINFOHLP::pfnPrintfV */
static DECLCALLBACK(void) dbgfR3InfoLogRel_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args)
{
    NOREF(pHlp);
    RTLogRelPrintfV(pszFormat, args);
}


/** Standard error output.
 * @copydoc DBGFINFOHLP::pfnPrintf */
static DECLCALLBACK(void) dbgfR3InfoStdErr_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...)
{
    NOREF(pHlp);
    va_list args;
    va_start(args, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, args);
    va_end(args);
}

/** Standard error output.
 * @copydoc DBGFINFOHLP::pfnPrintfV */
static DECLCALLBACK(void) dbgfR3InfoStdErr_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args)
{
    NOREF(pHlp);
    RTStrmPrintfV(g_pStdErr, pszFormat, args);
}


/**
 * Gets the release logger info helper.
 * The returned info helper will unconditionally write all output to the release log.
 *
 * @returns Pointer to the release logger info helper.
 */
VMMR3DECL(PCDBGFINFOHLP) DBGFR3InfoLogRelHlp(void)
{
    return &g_dbgfR3InfoLogRelHlp;
}


/**
 * Handle registration worker.
 * This allocates the structure, initializes the common fields and inserts into the list.
 * Upon successful return the we're inside the crit sect and the caller must leave it.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   fFlags      The flags.
 * @param   ppInfo      Where to store the created
 */
static int dbgfR3InfoRegister(PVM pVM, const char *pszName, const char *pszDesc, uint32_t fFlags, PDBGFINFO *ppInfo)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertMsgReturn(!(fFlags & ~(DBGFINFO_FLAGS_RUN_ON_EMT)), ("fFlags=%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize.
     */
    int rc;
    size_t cchName = strlen(pszName) + 1;
    PDBGFINFO pInfo = (PDBGFINFO)MMR3HeapAlloc(pVM, MM_TAG_DBGF_INFO, RT_OFFSETOF(DBGFINFO, szName[cchName]));
    if (pInfo)
    {
        pInfo->enmType = DBGFINFOTYPE_INVALID;
        pInfo->fFlags = fFlags;
        pInfo->pszDesc = pszDesc;
        pInfo->cchName = cchName - 1;
        memcpy(pInfo->szName, pszName, cchName);

        /* lazy init */
        rc = VINF_SUCCESS;
        if (!RTCritSectIsInitialized(&pVM->dbgf.s.InfoCritSect))
            rc = dbgfR3InfoInit(pVM);
        if (RT_SUCCESS(rc))
        {
            /*
             * Insert in alphabetical order.
             */
            rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
            AssertRC(rc);
            PDBGFINFO pPrev = NULL;
            PDBGFINFO pCur;
            for (pCur = pVM->dbgf.s.pInfoFirst; pCur; pPrev = pCur, pCur = pCur->pNext)
                if (strcmp(pszName, pCur->szName) < 0)
                    break;
            pInfo->pNext = pCur;
            if (pPrev)
                pPrev->pNext = pInfo;
            else
                pVM->dbgf.s.pInfoFirst = pInfo;

            *ppInfo = pInfo;
            return VINF_SUCCESS;
        }
        MMR3HeapFree(pInfo);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Register a info handler owned by a device.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   pfnHandler  The handler function to be called to display the info.
 * @param   pDevIns     The device instance owning the info.
 */
VMMR3DECL(int) DBGFR3InfoRegisterDevice(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler, PPDMDEVINS pDevIns)
{
    LogFlow(("DBGFR3InfoRegisterDevice: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p pDevIns=%p\n",
             pszName, pszName, pszDesc, pszDesc, pfnHandler, pDevIns));

    /*
     * Validate the specific stuff.
     */
    if (!pfnHandler)
    {
        AssertMsgFailed(("No handler\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (!pDevIns)
    {
        AssertMsgFailed(("No pDevIns\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register
     */
    PDBGFINFO pInfo;
    int rc = dbgfR3InfoRegister(pVM, pszName, pszDesc, 0, &pInfo);
    if (RT_SUCCESS(rc))
    {
        pInfo->enmType = DBGFINFOTYPE_DEV;
        pInfo->u.Dev.pfnHandler = pfnHandler;
        pInfo->u.Dev.pDevIns = pDevIns;
        RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    }

    return rc;
}


/**
 * Register a info handler owned by a driver.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   pfnHandler  The handler function to be called to display the info.
 * @param   pDrvIns     The driver instance owning the info.
 */
VMMR3DECL(int) DBGFR3InfoRegisterDriver(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDRV pfnHandler, PPDMDRVINS pDrvIns)
{
    LogFlow(("DBGFR3InfoRegisterDriver: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p pDrvIns=%p\n",
             pszName, pszName, pszDesc, pszDesc, pfnHandler, pDrvIns));

    /*
     * Validate the specific stuff.
     */
    if (!pfnHandler)
    {
        AssertMsgFailed(("No handler\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (!pDrvIns)
    {
        AssertMsgFailed(("No pDrvIns\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register
     */
    PDBGFINFO pInfo;
    int rc = dbgfR3InfoRegister(pVM, pszName, pszDesc, 0, &pInfo);
    if (RT_SUCCESS(rc))
    {
        pInfo->enmType = DBGFINFOTYPE_DRV;
        pInfo->u.Drv.pfnHandler = pfnHandler;
        pInfo->u.Drv.pDrvIns = pDrvIns;
        RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    }

    return rc;
}


/**
 * Register a info handler owned by an internal component.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   pfnHandler  The handler function to be called to display the info.
 */
VMMR3DECL(int) DBGFR3InfoRegisterInternal(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler)
{
    return DBGFR3InfoRegisterInternalEx(pVM, pszName, pszDesc, pfnHandler, 0);
}


/**
 * Register a info handler owned by an internal component.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   pfnHandler  The handler function to be called to display the info.
 * @param   fFlags      Flags, see the DBGFINFO_FLAGS_*.
 */
VMMR3DECL(int) DBGFR3InfoRegisterInternalEx(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler, uint32_t fFlags)
{
    LogFlow(("DBGFR3InfoRegisterInternal: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p fFlags=%x\n",
             pszName, pszName, pszDesc, pszDesc, pfnHandler, fFlags));

    /*
     * Validate the specific stuff.
     */
    if (!pfnHandler)
    {
        AssertMsgFailed(("No handler\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register
     */
    PDBGFINFO pInfo;
    int rc = dbgfR3InfoRegister(pVM, pszName, pszDesc, fFlags, &pInfo);
    if (RT_SUCCESS(rc))
    {
        pInfo->enmType = DBGFINFOTYPE_INT;
        pInfo->u.Int.pfnHandler = pfnHandler;
        RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    }

    return rc;
}


/**
 * Register a info handler owned by an external component.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   pszDesc     The description of the info and any arguments the handler may take.
 * @param   pfnHandler  The handler function to be called to display the info.
 * @param   pvUser      User argument to be passed to the handler.
 */
VMMR3DECL(int) DBGFR3InfoRegisterExternal(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLEREXT pfnHandler, void *pvUser)
{
    LogFlow(("DBGFR3InfoRegisterExternal: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p pvUser=%p\n",
             pszName, pszName, pszDesc, pszDesc, pfnHandler, pvUser));

    /*
     * Validate the specific stuff.
     */
    if (!pfnHandler)
    {
        AssertMsgFailed(("No handler\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register
     */
    PDBGFINFO pInfo;
    int rc = dbgfR3InfoRegister(pVM, pszName, pszDesc, 0, &pInfo);
    if (RT_SUCCESS(rc))
    {
        pInfo->enmType = DBGFINFOTYPE_EXT;
        pInfo->u.Ext.pfnHandler = pfnHandler;
        pInfo->u.Ext.pvUser = pvUser;
        RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    }

    return rc;
}


/**
 * Deregister one(/all) info handler(s) owned by a device.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pDevIns     Device instance.
 * @param   pszName     The identifier of the info. If NULL all owned by the device.
 */
VMMR3DECL(int) DBGFR3InfoDeregisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName)
{
    LogFlow(("DBGFR3InfoDeregisterDevice: pDevIns=%p pszName=%p:{%s}\n", pDevIns, pszName, pszName));

    /*
     * Validate input.
     */
    if (!pDevIns)
    {
        AssertMsgFailed(("!pDevIns\n"));
        return VERR_INVALID_PARAMETER;
    }
    size_t cchName = pszName ? strlen(pszName) : 0;

    /*
     * Enumerate the info handlers and free the requested entries.
     */
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    rc = VERR_FILE_NOT_FOUND;
    PDBGFINFO pPrev = NULL;
    PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst;
    if (pszName)
    {
        /*
         * Free a specific one.
         */
        for (; pInfo; pPrev = pInfo, pInfo = pInfo->pNext)
            if (    pInfo->enmType == DBGFINFOTYPE_DEV
                &&  pInfo->u.Dev.pDevIns == pDevIns
                &&  pInfo->cchName == cchName
                &&  !strcmp(pInfo->szName, pszName))
            {
                if (pPrev)
                    pPrev->pNext = pInfo->pNext;
                else
                    pVM->dbgf.s.pInfoFirst = pInfo->pNext;
                MMR3HeapFree(pInfo);
                rc = VINF_SUCCESS;
                break;
            }
    }
    else
    {
        /*
         * Free all owned by the driver.
         */
        for (; pInfo; pPrev = pInfo, pInfo = pInfo->pNext)
            if (    pInfo->enmType == DBGFINFOTYPE_DEV
                &&  pInfo->u.Dev.pDevIns == pDevIns)
            {
                if (pPrev)
                    pPrev->pNext = pInfo->pNext;
                else
                    pVM->dbgf.s.pInfoFirst = pInfo->pNext;
                MMR3HeapFree(pInfo);
                pInfo = pPrev;
            }
        rc = VINF_SUCCESS;
    }
    int rc2 = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc2);
    AssertRC(rc);
    LogFlow(("DBGFR3InfoDeregisterDevice: returns %Rrc\n", rc));
    return rc;
}

/**
 * Deregister one(/all) info handler(s) owned by a driver.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pDrvIns     Driver instance.
 * @param   pszName     The identifier of the info. If NULL all owned by the driver.
 */
VMMR3DECL(int) DBGFR3InfoDeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName)
{
    LogFlow(("DBGFR3InfoDeregisterDriver: pDrvIns=%p pszName=%p:{%s}\n", pDrvIns, pszName, pszName));

    /*
     * Validate input.
     */
    if (!pDrvIns)
    {
        AssertMsgFailed(("!pDrvIns\n"));
        return VERR_INVALID_PARAMETER;
    }
    size_t cchName = pszName ? strlen(pszName) : 0;

    /*
     * Enumerate the info handlers and free the requested entries.
     */
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    rc = VERR_FILE_NOT_FOUND;
    PDBGFINFO pPrev = NULL;
    PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst;
    if (pszName)
    {
        /*
         * Free a specific one.
         */
        for (; pInfo; pPrev = pInfo, pInfo = pInfo->pNext)
            if (    pInfo->enmType == DBGFINFOTYPE_DRV
                &&  pInfo->u.Drv.pDrvIns == pDrvIns
                &&  pInfo->cchName == cchName
                &&  !strcmp(pInfo->szName, pszName))
            {
                if (pPrev)
                    pPrev->pNext = pInfo->pNext;
                else
                    pVM->dbgf.s.pInfoFirst = pInfo->pNext;
                MMR3HeapFree(pInfo);
                rc = VINF_SUCCESS;
                break;
            }
    }
    else
    {
        /*
         * Free all owned by the driver.
         */
        for (; pInfo; pPrev = pInfo, pInfo = pInfo->pNext)
            if (    pInfo->enmType == DBGFINFOTYPE_DRV
                &&  pInfo->u.Drv.pDrvIns == pDrvIns)
            {
                if (pPrev)
                    pPrev->pNext = pInfo->pNext;
                else
                    pVM->dbgf.s.pInfoFirst = pInfo->pNext;
                MMR3HeapFree(pInfo);
                pInfo = pPrev;
            }
        rc = VINF_SUCCESS;
    }
    int rc2 = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc2);
    AssertRC(rc);
    LogFlow(("DBGFR3InfoDeregisterDriver: returns %Rrc\n", rc));
    return rc;
}


/**
 * Internal deregistration helper.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info.
 * @param   enmType     The info owner type.
 */
static int dbgfR3InfoDeregister(PVM pVM, const char *pszName, DBGFINFOTYPE enmType)
{
    /*
     * Validate input.
     */
    if (!pszName)
    {
        AssertMsgFailed(("!pszName\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find the info handler.
     */
    size_t cchName = strlen(pszName);
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    rc = VERR_FILE_NOT_FOUND;
    PDBGFINFO pPrev = NULL;
    PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst;
    for (; pInfo; pPrev = pInfo, pInfo = pInfo->pNext)
        if (    pInfo->cchName == cchName
            &&  !strcmp(pInfo->szName, pszName)
            &&  pInfo->enmType == enmType)
        {
            if (pPrev)
                pPrev->pNext = pInfo->pNext;
            else
                pVM->dbgf.s.pInfoFirst = pInfo->pNext;
            MMR3HeapFree(pInfo);
            rc = VINF_SUCCESS;
            break;
        }
    int rc2 = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc2);
    AssertRC(rc);
    LogFlow(("dbgfR3InfoDeregister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Deregister a info handler owned by an internal component.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info. If NULL all owned by the device.
 */
VMMR3DECL(int) DBGFR3InfoDeregisterInternal(PVM pVM, const char *pszName)
{
    LogFlow(("DBGFR3InfoDeregisterInternal: pszName=%p:{%s}\n", pszName, pszName));
    return dbgfR3InfoDeregister(pVM, pszName, DBGFINFOTYPE_INT);
}


/**
 * Deregister a info handler owned by an external component.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info. If NULL all owned by the device.
 */
VMMR3DECL(int) DBGFR3InfoDeregisterExternal(PVM pVM, const char *pszName)
{
    LogFlow(("DBGFR3InfoDeregisterExternal: pszName=%p:{%s}\n", pszName, pszName));
    return dbgfR3InfoDeregister(pVM, pszName, DBGFINFOTYPE_EXT);
}


/**
 * Worker for DBGFR3Info and DBGFR3InfoEx.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM.
 * @param   idCpu               Which CPU to run EMT bound handlers on.
 *                              VMCPUID_ANY or a valid CPU ID.
 * @param   pszName             What to dump.
 * @param   pszArgs             Arguments, optional.
 * @param   pHlp                Output helper, optional.
 */
static DECLCALLBACK(int) dbgfR3Info(PVM pVM, VMCPUID idCpu, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    if (pHlp)
    {
        if (    !pHlp->pfnPrintf
            ||  !pHlp->pfnPrintfV)
        {
            AssertMsgFailed(("A pHlp member is missing!\n"));
            return VERR_INVALID_PARAMETER;
        }
    }
    else
        pHlp = &g_dbgfR3InfoLogHlp;

    /*
     * Find the info handler.
     */
    size_t cchName = strlen(pszName);
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst;
    for (; pInfo; pInfo = pInfo->pNext)
        if (    pInfo->cchName == cchName
            &&  !memcmp(pInfo->szName, pszName, cchName))
            break;
    if (pInfo)
    {
        /*
         * Found it.
         *      Make a copy of it on the stack so we can leave the crit sect.
         *      Switch on the type and invoke the handler.
         */
        DBGFINFO Info = *pInfo;
        rc = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
        AssertRC(rc);
        rc = VINF_SUCCESS;
        switch (Info.enmType)
        {
            case DBGFINFOTYPE_DEV:
                if (Info.fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                    rc = VMR3ReqCallVoidWait(pVM, idCpu, (PFNRT)Info.u.Dev.pfnHandler, 3, Info.u.Dev.pDevIns, pHlp, pszArgs);
                else
                    Info.u.Dev.pfnHandler(Info.u.Dev.pDevIns, pHlp, pszArgs);
                break;

            case DBGFINFOTYPE_DRV:
                if (Info.fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                    rc = VMR3ReqCallVoidWait(pVM, idCpu, (PFNRT)Info.u.Drv.pfnHandler, 3, Info.u.Drv.pDrvIns, pHlp, pszArgs);
                else
                    Info.u.Drv.pfnHandler(Info.u.Drv.pDrvIns, pHlp, pszArgs);
                break;

            case DBGFINFOTYPE_INT:
                if (Info.fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                    rc = VMR3ReqCallVoidWait(pVM, idCpu, (PFNRT)Info.u.Int.pfnHandler, 3, pVM, pHlp, pszArgs);
                else
                    Info.u.Int.pfnHandler(pVM, pHlp, pszArgs);
                break;

            case DBGFINFOTYPE_EXT:
                if (Info.fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                    rc = VMR3ReqCallVoidWait(pVM, idCpu, (PFNRT)Info.u.Ext.pfnHandler, 3, Info.u.Ext.pvUser, pHlp, pszArgs);
                else
                    Info.u.Ext.pfnHandler(Info.u.Ext.pvUser, pHlp, pszArgs);
                break;

            default:
                AssertMsgFailedReturn(("Invalid info type enmType=%d\n", Info.enmType), VERR_IPE_NOT_REACHED_DEFAULT_CASE);
        }
    }
    else
    {
        rc = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
        AssertRC(rc);
        rc = VERR_FILE_NOT_FOUND;
    }
    return rc;
}

/**
 * Display a piece of info writing to the supplied handler.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pszName     The identifier of the info to display.
 * @param   pszArgs     Arguments to the info handler.
 * @param   pHlp        The output helper functions. If NULL the logger will be used.
 */
VMMR3DECL(int) DBGFR3Info(PVM pVM, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    return dbgfR3Info(pVM, VMCPUID_ANY, pszName, pszArgs, pHlp);
}


/**
 * Display a piece of info writing to the supplied handler.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   idCpu       The CPU to exectue the request on.  Pass NIL_VMCPUID
 *                      to not involve any EMT.
 * @param   pszName     The identifier of the info to display.
 * @param   pszArgs     Arguments to the info handler.
 * @param   pHlp        The output helper functions. If NULL the logger will be used.
 */
VMMR3DECL(int) DBGFR3InfoEx(PVM pVM, VMCPUID idCpu, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    if (idCpu == NIL_VMCPUID)
        return dbgfR3Info(pVM, VMCPUID_ANY, pszName, pszArgs, pHlp);
    return VMR3ReqPriorityCallWait(pVM, idCpu,
                                   (PFNRT)dbgfR3Info, 5, pVM, idCpu, pszName, pszArgs, pHlp);
}


/**
 * Wrapper for DBGFR3Info that outputs to the release log.
 *
 * @returns See DBGFR3Info.
 * @param   pVM                 Pointer to the VM.
 * @param   pszName             See DBGFR3Info.
 * @param   pszArgs             See DBGFR3Info.
 */
VMMR3DECL(int) DBGFR3InfoLogRel(PVM pVM, const char *pszName, const char *pszArgs)
{
    return DBGFR3Info(pVM, pszName, pszArgs, &g_dbgfR3InfoLogRelHlp);
}


/**
 * Wrapper for DBGFR3Info that outputs to standard error.
 *
 * @returns See DBGFR3Info.
 * @param   pVM                 Pointer to the VM.
 * @param   pszName             See DBGFR3Info.
 * @param   pszArgs             See DBGFR3Info.
 */
VMMR3DECL(int) DBGFR3InfoStdErr(PVM pVM, const char *pszName, const char *pszArgs)
{
    return DBGFR3Info(pVM, pszName, pszArgs, &g_dbgfR3InfoStdErrHlp);
}


/**
 * Display several info items.
 *
 * This is intended used by the fatal error dump only.
 *
 * @returns
 * @param   pVM             Pointer to the VM.
 * @param   pszIncludePat   Simple string pattern of info items to include.
 * @param   pszExcludePat   Simple string pattern of info items to exclude.
 * @param   pszSepFmt       Item separator format string.  The item name will be
 *                          given as parameter.
 * @param   pHlp            The output helper functions.  If NULL the logger
 *                          will be used.
 *
 * @threads EMT
 */
VMMR3DECL(int) DBGFR3InfoMulti(PVM pVM, const char *pszIncludePat, const char *pszExcludePat, const char *pszSepFmt,
                               PCDBGFINFOHLP pHlp)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pszIncludePat, VERR_INVALID_POINTER);
    AssertPtrReturn(pszExcludePat, VERR_INVALID_POINTER);
    if (pHlp)
    {
        AssertPtrReturn(pHlp->pfnPrintf, VERR_INVALID_POINTER);
        AssertPtrReturn(pHlp->pfnPrintfV, VERR_INVALID_POINTER);
    }
    else
        pHlp = &g_dbgfR3InfoLogHlp;

    size_t const cchIncludePat = strlen(pszIncludePat);
    size_t const cchExcludePat = strlen(pszExcludePat);
    const char *pszArgs = "";

    /*
     * Enumerate the info handlers and call the ones matching.
     * Note! We won't leave the critical section here...
     */
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
    rc = VWRN_NOT_FOUND;
    for (PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst; pInfo; pInfo = pInfo->pNext)
    {
        if (   RTStrSimplePatternMultiMatch(pszIncludePat, cchIncludePat, pInfo->szName, pInfo->cchName, NULL)
            && !RTStrSimplePatternMultiMatch(pszExcludePat, cchExcludePat, pInfo->szName, pInfo->cchName, NULL))
        {
            pHlp->pfnPrintf(pHlp, pszSepFmt, pInfo->szName);
            rc = VINF_SUCCESS;
            switch (pInfo->enmType)
            {
                case DBGFINFOTYPE_DEV:
                    if (pInfo->fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                        rc = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY, (PFNRT)pInfo->u.Dev.pfnHandler, 3, pInfo->u.Dev.pDevIns, pHlp, pszArgs);
                    else
                        pInfo->u.Dev.pfnHandler(pInfo->u.Dev.pDevIns, pHlp, pszArgs);
                    break;

                case DBGFINFOTYPE_DRV:
                    if (pInfo->fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                        rc = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY, (PFNRT)pInfo->u.Drv.pfnHandler, 3, pInfo->u.Drv.pDrvIns, pHlp, pszArgs);
                    else
                        pInfo->u.Drv.pfnHandler(pInfo->u.Drv.pDrvIns, pHlp, pszArgs);
                    break;

                case DBGFINFOTYPE_INT:
                    if (pInfo->fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                        rc = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY, (PFNRT)pInfo->u.Int.pfnHandler, 3, pVM, pHlp, pszArgs);
                    else
                        pInfo->u.Int.pfnHandler(pVM, pHlp, pszArgs);
                    break;

                case DBGFINFOTYPE_EXT:
                    if (pInfo->fFlags & DBGFINFO_FLAGS_RUN_ON_EMT)
                        rc = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY, (PFNRT)pInfo->u.Ext.pfnHandler, 3, pInfo->u.Ext.pvUser, pHlp, pszArgs);
                    else
                        pInfo->u.Ext.pfnHandler(pInfo->u.Ext.pvUser, pHlp, pszArgs);
                    break;

                default:
                    AssertMsgFailedReturn(("Invalid info type enmType=%d\n", pInfo->enmType), VERR_IPE_NOT_REACHED_DEFAULT_CASE);
            }
        }
    }
    int rc2 = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc2);

    return rc;
}


/**
 * Enumerate all the register info handlers.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pfnCallback     Pointer to callback function.
 * @param   pvUser          User argument to pass to the callback.
 */
VMMR3DECL(int) DBGFR3InfoEnum(PVM pVM, PFNDBGFINFOENUM pfnCallback, void *pvUser)
{
    LogFlow(("DBGFR3InfoLog: pfnCallback=%p pvUser=%p\n", pfnCallback, pvUser));

    /*
     * Validate input.
     */
    if (!pfnCallback)
    {
        AssertMsgFailed(("!pfnCallback\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Enter and enumerate.
     */
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);

    rc = VINF_SUCCESS;
    for (PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst; RT_SUCCESS(rc) && pInfo; pInfo = pInfo->pNext)
        rc = pfnCallback(pVM, pInfo->szName, pInfo->pszDesc, pvUser);

    /*
     * Leave and exit.
     */
    int rc2 = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc2);

    LogFlow(("DBGFR3InfoLog: returns %Rrc\n", rc));
    return rc;
}


/**
 * Info handler, internal version.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) dbgfR3InfoHelp(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    LogFlow(("dbgfR3InfoHelp: pszArgs=%s\n", pszArgs));

    /*
     * Enter and enumerate.
     */
    int rc = RTCritSectEnter(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);

    if (pszArgs && *pszArgs)
    {
        for (PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst; pInfo; pInfo = pInfo->pNext)
        {
            const char *psz = strstr(pszArgs, pInfo->szName);
            if (    psz
                &&  (   psz == pszArgs
                     || RT_C_IS_SPACE(psz[-1]))
                &&  (   !psz[pInfo->cchName]
                     || RT_C_IS_SPACE(psz[pInfo->cchName])))
                pHlp->pfnPrintf(pHlp, "%-16s  %s\n",
                                pInfo->szName, pInfo->pszDesc);
        }
    }
    else
    {
        for (PDBGFINFO pInfo = pVM->dbgf.s.pInfoFirst; pInfo; pInfo = pInfo->pNext)
            pHlp->pfnPrintf(pHlp, "%-16s  %s\n",
                            pInfo->szName, pInfo->pszDesc);
    }

    /*
     * Leave and exit.
     */
    rc = RTCritSectLeave(&pVM->dbgf.s.InfoCritSect);
    AssertRC(rc);
}

