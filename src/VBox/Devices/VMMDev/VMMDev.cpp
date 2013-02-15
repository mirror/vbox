/* $Id$ */
/** @file
 * VMMDev - Guest <-> VMM/Host communication device.
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
/* Enable dev_vmm Log3 statements to get IRQ-related logging. */
#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/VMMDev.h>
#include <VBox/vmm/mm.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <VBox/vmm/pgm.h>
#include <VBox/err.h>
#include <VBox/vmm/vm.h> /* for VM_IS_EMT */
#include <VBox/dbg.h>
#include <VBox/version.h>

#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifndef IN_RC
# include <iprt/mem.h>
#endif
#ifdef IN_RING3
# include <iprt/uuid.h>
#endif

#include "VMMDevState.h"
#ifdef VBOX_WITH_HGCM
# include "VMMDevHGCM.h"
#endif
#ifndef VBOX_WITHOUT_TESTING_FEATURES
# include "VMMDevTesting.h"
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define VBOX_GUEST_INTERFACE_VERSION_1_03(s) \
    (   RT_HIWORD((s)->guestInfo.interfaceVersion) == 1 \
     && RT_LOWORD((s)->guestInfo.interfaceVersion) == 3 )

#define VBOX_GUEST_INTERFACE_VERSION_OK(additionsVersion) \
      (   RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
       && RT_LOWORD(additionsVersion) <= RT_LOWORD(VMMDEV_VERSION) )

#define VBOX_GUEST_INTERFACE_VERSION_OLD(additionsVersion) \
      (   (RT_HIWORD(additionsVersion) < RT_HIWORD(VMMDEV_VERSION) \
       || (   RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
           && RT_LOWORD(additionsVersion) <= RT_LOWORD(VMMDEV_VERSION) ) )

#define VBOX_GUEST_INTERFACE_VERSION_TOO_OLD(additionsVersion) \
      ( RT_HIWORD(additionsVersion) < RT_HIWORD(VMMDEV_VERSION) )

#define VBOX_GUEST_INTERFACE_VERSION_NEW(additionsVersion) \
      (   RT_HIWORD(additionsVersion) > RT_HIWORD(VMMDEV_VERSION) \
       || (   RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
           && RT_LOWORD(additionsVersion) >  RT_LOWORD(VMMDEV_VERSION) ) )

/** The saved state version. */
#define VMMDEV_SAVED_STATE_VERSION                              15
/** The saved state version which is missing the guest facility statuses. */
#define VMMDEV_SAVED_STATE_VERSION_MISSING_FACILITY_STATUSES    14
/** The saved state version which is missing the guestInfo2 bits. */
#define VMMDEV_SAVED_STATE_VERSION_MISSING_GUEST_INFO_2         13
/** The saved state version used by VirtualBox 3.0.
 *  This doesn't have the config part. */
#define VMMDEV_SAVED_STATE_VERSION_VBOX_30                      11


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/** @page pg_vmmdev   VMMDev
 *
 * Whenever host wants to inform guest about something an IRQ notification will
 * be raised.
 *
 * VMMDev PDM interface will contain the guest notification method.
 *
 * There is a 32 bit event mask which will be read by guest on an interrupt.  A
 * non zero bit in the mask means that the specific event occurred and requires
 * processing on guest side.
 *
 * After reading the event mask guest must issue a generic request
 * AcknowlegdeEvents.
 *
 * IRQ line is set to 1 (request) if there are unprocessed events, that is the
 * event mask is not zero.
 *
 * After receiving an interrupt and checking event mask, the guest must process
 * events using the event specific mechanism.
 *
 * That is if mouse capabilities were changed, guest will use
 * VMMDev_GetMouseStatus generic request.
 *
 * Event mask is only a set of flags indicating that guest must proceed with a
 * procedure.
 *
 * Unsupported events are therefore ignored. The guest additions must inform
 * host which events they want to receive, to avoid unnecessary IRQ processing.
 * By default no events are signalled to guest.
 *
 * This seems to be fast method. It requires only one context switch for an
 * event processing.
 *
 */

static void vmmdevSetIRQ_Legacy_EMT(VMMDevState *pVMMDevState)
{
    if (!pVMMDevState->fu32AdditionsOk)
    {
        Log(("vmmdevSetIRQ: IRQ is not generated, guest has not yet reported to us.\n"));
        return;
    }

    uint32_t u32IRQLevel = 0;

    /* Filter unsupported events */
    uint32_t u32EventFlags =
        pVMMDevState->u32HostEventFlags
        & pVMMDevState->pVMMDevRAMR3->V.V1_03.u32GuestEventMask;

    Log(("vmmdevSetIRQ: u32EventFlags = 0x%08X, "
         "pVMMDevState->u32HostEventFlags = 0x%08X, "
         "pVMMDevState->pVMMDevRAMR3->u32GuestEventMask = 0x%08X\n",
         u32EventFlags,
         pVMMDevState->u32HostEventFlags,
         pVMMDevState->pVMMDevRAMR3->V.V1_03.u32GuestEventMask));

    /* Move event flags to VMMDev RAM */
    pVMMDevState->pVMMDevRAMR3->V.V1_03.u32HostEvents = u32EventFlags;

    if (u32EventFlags)
    {
        /* Clear host flags which will be delivered to guest. */
        pVMMDevState->u32HostEventFlags &= ~u32EventFlags;
        Log(("vmmdevSetIRQ: pVMMDevState->u32HostEventFlags = 0x%08X\n",
             pVMMDevState->u32HostEventFlags));
        u32IRQLevel = 1;
    }

    /* Set IRQ level for pin 0 */
    /** @todo make IRQ pin configurable, at least a symbolic constant */
    PPDMDEVINS pDevIns = pVMMDevState->pDevIns;
    PDMDevHlpPCISetIrqNoWait(pDevIns, 0, u32IRQLevel);
    Log(("vmmdevSetIRQ: IRQ set %d\n", u32IRQLevel));
}

static void vmmdevMaybeSetIRQ_EMT(VMMDevState *pVMMDevState)
{
    Log3(("vmmdevMaybeSetIRQ_EMT: u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
          pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));

    if (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask)
    {
        pVMMDevState->pVMMDevRAMR3->V.V1_04.fHaveEvents = true;
        PDMDevHlpPCISetIrqNoWait(pVMMDevState->pDevIns, 0, 1);
        Log3(("vmmdevMaybeSetIRQ_EMT: IRQ set.\n"));
    }
}

static void vmmdevNotifyGuest_EMT(VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
    Log3(("VMMDevNotifyGuest_EMT: u32EventMask = 0x%08X.\n", u32EventMask));

    if (VBOX_GUEST_INTERFACE_VERSION_1_03(pVMMDevState))
    {
        Log3(("VMMDevNotifyGuest_EMT: Old additions detected.\n"));

        pVMMDevState->u32HostEventFlags |= u32EventMask;
        vmmdevSetIRQ_Legacy_EMT(pVMMDevState);
    }
    else
    {
        Log3(("VMMDevNotifyGuest_EMT: New additions detected.\n"));

        if (!pVMMDevState->fu32AdditionsOk)
        {
            pVMMDevState->u32HostEventFlags |= u32EventMask;
            Log(("vmmdevNotifyGuest_EMT: IRQ is not generated, guest has not yet reported to us.\n"));
            return;
        }

        const bool fHadEvents =
            (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask) != 0;

        Log3(("VMMDevNotifyGuest_EMT: fHadEvents = %d, u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
              fHadEvents, pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));

        pVMMDevState->u32HostEventFlags |= u32EventMask;

        if (!fHadEvents)
            vmmdevMaybeSetIRQ_EMT (pVMMDevState);
    }
}

void VMMDevCtlSetGuestFilterMask (VMMDevState *pVMMDevState,
                                  uint32_t u32OrMask,
                                  uint32_t u32NotMask)
{
    PDMCritSectEnter(&pVMMDevState->CritSect, VERR_SEM_BUSY);

    const bool fHadEvents =
        (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask) != 0;

    Log(("VMMDevCtlSetGuestFilterMask: u32OrMask = 0x%08X, u32NotMask = 0x%08X, fHadEvents = %d.\n", u32OrMask, u32NotMask, fHadEvents));
    if (fHadEvents)
    {
        if (!pVMMDevState->fNewGuestFilterMask)
            pVMMDevState->u32NewGuestFilterMask = pVMMDevState->u32GuestFilterMask;

        pVMMDevState->u32NewGuestFilterMask |= u32OrMask;
        pVMMDevState->u32NewGuestFilterMask &= ~u32NotMask;
        pVMMDevState->fNewGuestFilterMask = true;
    }
    else
    {
        pVMMDevState->u32GuestFilterMask |= u32OrMask;
        pVMMDevState->u32GuestFilterMask &= ~u32NotMask;
        vmmdevMaybeSetIRQ_EMT (pVMMDevState);
    }
    PDMCritSectLeave(&pVMMDevState->CritSect);
}

void VMMDevNotifyGuest (VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
    PPDMDEVINS pDevIns = pVMMDevState->pDevIns;

    Log3(("VMMDevNotifyGuest: u32EventMask = 0x%08X.\n", u32EventMask));

    /*
     * Drop notifications if the VM is not running yet/anymore.
     */
    VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
    if (    enmVMState != VMSTATE_RUNNING
        &&  enmVMState != VMSTATE_RUNNING_LS)
        return;

    PDMCritSectEnter(&pVMMDevState->CritSect, VERR_SEM_BUSY);
    /* No need to wait for the completion of this request. It is a notification
     * about something, which has already happened.
     */
    vmmdevNotifyGuest_EMT(pVMMDevState, u32EventMask);
    PDMCritSectLeave(&pVMMDevState->CritSect);
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vmmdevBackdoorLog(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);

    if (!pThis->fBackdoorLogDisabled && cb == 1 && Port == RTLOG_DEBUG_PORT)
    {

        /* The raw version. */
        switch (u32)
        {
            case '\r': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <return>\n")); break;
            case '\n': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <newline>\n")); break;
            case '\t': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <tab>\n")); break;
            default:   LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: %c (%02x)\n", u32, u32)); break;
        }

        /* The readable, buffered version. */
        if (u32 == '\n' || u32 == '\r')
        {
            pThis->szMsg[pThis->iMsg] = '\0';
            if (pThis->iMsg)
                LogRelIt(LOG_REL_INSTANCE, RTLOGGRPFLAGS_LEVEL_1, LOG_GROUP_DEV_VMM_BACKDOOR, ("Guest Log: %s\n", pThis->szMsg));
            pThis->iMsg = 0;
        }
        else
        {
            if (pThis->iMsg >= sizeof(pThis->szMsg)-1)
            {
                pThis->szMsg[pThis->iMsg] = '\0';
                LogRelIt(LOG_REL_INSTANCE, RTLOGGRPFLAGS_LEVEL_1, LOG_GROUP_DEV_VMM_BACKDOOR, ("Guest Log: %s\n", pThis->szMsg));
                pThis->iMsg = 0;
            }
            pThis->szMsg[pThis->iMsg] = (char )u32;
            pThis->szMsg[++pThis->iMsg] = '\0';
        }
    }
    return VINF_SUCCESS;
}

#ifdef TIMESYNC_BACKDOOR
/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vmmdevTimesyncBackdoorWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);
        switch (u32)
        {
            case 0:
                pThis->fTimesyncBackdoorLo = false;
                break;
            case 1:
                pThis->fTimesyncBackdoorLo = true;
        }
        return VINF_SUCCESS;

    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for backdoor timesync IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) vmmdevTimesyncBackdoorRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    int rc;
    NOREF(pvUser);
    if (cb == 4)
    {
        VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);
        RTTIMESPEC now;

        if (pThis->fTimesyncBackdoorLo)
            *pu32 = (uint32_t)pThis->hostTime;
        else
        {
            pThis->hostTime = RTTimeSpecGetMilli(PDMDevHlpTMUtcNow(pDevIns, &now));
            *pu32 = (uint32_t)(pThis->hostTime >> 32);
        }
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_IOM_IOPORT_UNUSED;
    return rc;
}
#endif /* TIMESYNC_BACKDOOR */

/**
 * Validates a publisher tag.
 *
 * @returns true / false.
 * @param   pszTag              Tag to validate.
 */
static bool vmmdevReqIsValidPublisherTag(const char *pszTag)
{
    /* Note! This character set is also found in Config.kmk. */
    static char const s_szValidChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz()[]{}+-.,";

    while (*pszTag != '\0')
    {
        if (!strchr(s_szValidChars, *pszTag))
            return false;
        pszTag++;
    }
    return true;
}


/**
 * Validates a build tag.
 *
 * @returns true / false.
 * @param   pszTag              Tag to validate.
 */
static bool vmmdevReqIsValidBuildTag(const char *pszTag)
{
    int cchPrefix;
    if (!strncmp(pszTag, "RC", 2))
        cchPrefix = 2;
    else if (!strncmp(pszTag, "BETA", 4))
        cchPrefix = 4;
    else if (!strncmp(pszTag, "ALPHA", 5))
        cchPrefix = 5;
    else
        return false;

    if (pszTag[cchPrefix] == '\0')
        return true;

    uint8_t u8;
    int rc = RTStrToUInt8Full(&pszTag[cchPrefix], 10, &u8);
    return rc == VINF_SUCCESS;
}

/**
 * Handles VMMDevReq_ReportGuestInfo2.
 *
 * @returns VBox status code that the guest should see.
 * @param   pThis           The VMMDev instance data.
 * @param   pRequestHeader  The header of the request to handle.
 */
static int vmmdevReqHandler_ReportGuestInfo2(VMMDevState *pThis, VMMDevRequestHeader *pRequestHeader)
{
    AssertMsgReturn(pRequestHeader->size == sizeof(VMMDevReportGuestInfo2), ("%u\n", pRequestHeader->size), VERR_INVALID_PARAMETER);
    VBoxGuestInfo2 const *pInfo2 = &((VMMDevReportGuestInfo2 *)pRequestHeader)->guestInfo;

    LogRel(("Guest Additions information report: Version %d.%d.%d r%d '%.*s'\n",
            pInfo2->additionsMajor, pInfo2->additionsMinor, pInfo2->additionsBuild,
            pInfo2->additionsRevision, sizeof(pInfo2->szName), pInfo2->szName));

    /* The interface was introduced in 3.2 and will definitely not be
       backported beyond 3.0 (bird). */
    AssertMsgReturn(pInfo2->additionsMajor >= 3,
                    ("%u.%u.%u\n", pInfo2->additionsMajor, pInfo2->additionsMinor, pInfo2->additionsBuild),
                    VERR_INVALID_PARAMETER);

    /* The version must fit in a full version compression. */
    uint32_t uFullVersion = VBOX_FULL_VERSION_MAKE(pInfo2->additionsMajor, pInfo2->additionsMinor, pInfo2->additionsBuild);
    AssertMsgReturn(   VBOX_FULL_VERSION_GET_MAJOR(uFullVersion) == pInfo2->additionsMajor
                    && VBOX_FULL_VERSION_GET_MINOR(uFullVersion) == pInfo2->additionsMinor
                    && VBOX_FULL_VERSION_GET_BUILD(uFullVersion) == pInfo2->additionsBuild,
                    ("%u.%u.%u\n", pInfo2->additionsMajor, pInfo2->additionsMinor, pInfo2->additionsBuild),
                    VERR_OUT_OF_RANGE);

    /*
     * Validate the name.
     * Be less strict towards older additions (< v4.1.50).
     */
    AssertCompile(sizeof(pThis->guestInfo2.szName) == sizeof(pInfo2->szName));
    AssertReturn(memchr(pInfo2->szName, '\0', sizeof(pInfo2->szName)) != NULL, VERR_INVALID_PARAMETER);
    const char *pszName = pInfo2->szName;

    /* The version number which shouldn't be there. */
    char        szTmp[sizeof(pInfo2->szName)];
    size_t      cchStart = RTStrPrintf(szTmp, sizeof(szTmp), "%u.%u.%u", pInfo2->additionsMajor, pInfo2->additionsMinor, pInfo2->additionsBuild);
    AssertMsgReturn(!strncmp(pszName, szTmp, cchStart), ("%s != %s\n", pszName, szTmp), VERR_INVALID_PARAMETER);
    pszName += cchStart;

    /* Now we can either have nothing or a build tag or/and a publisher tag. */
    if (*pszName != '\0')
    {
        const char *pszRelaxedName = "";
        bool const fStrict = pInfo2->additionsMajor > 4
                          || (pInfo2->additionsMajor == 4 && pInfo2->additionsMinor > 1)
                          || (pInfo2->additionsMajor == 4 && pInfo2->additionsMinor == 1 && pInfo2->additionsBuild >= 50);
        bool fOk = false;
        if (*pszName == '_')
        {
            pszName++;
            strcpy(szTmp, pszName);
            char *pszTag2 = strchr(szTmp, '_');
            if (!pszTag2)
            {
                fOk = vmmdevReqIsValidBuildTag(szTmp)
                   || vmmdevReqIsValidPublisherTag(szTmp);
            }
            else
            {
                *pszTag2++ = '\0';
                fOk = vmmdevReqIsValidBuildTag(szTmp);
                if (fOk)
                {
                    fOk = vmmdevReqIsValidPublisherTag(pszTag2);
                    if (!fOk)
                        pszRelaxedName = szTmp;
                }
            }
        }

        if (!fOk)
        {
            AssertLogRelMsgReturn(!fStrict, ("%s", pszName), VERR_INVALID_PARAMETER);

            /* non-strict mode, just zap the extra stuff. */
            LogRel(("ReportGuestInfo2: Ignoring unparsable version name bits: '%s' -> '%s'.\n", pszName, pszRelaxedName));
            pszName = pszRelaxedName;
        }
    }

    /*
     * Save the info and tell Main or whoever is listening.
     */
    pThis->guestInfo2.uFullVersion  = uFullVersion;
    pThis->guestInfo2.uRevision     = pInfo2->additionsRevision;
    pThis->guestInfo2.fFeatures     = pInfo2->additionsFeatures;
    strcpy(pThis->guestInfo2.szName, pszName);

    if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestInfo2)
        pThis->pDrv->pfnUpdateGuestInfo2(pThis->pDrv, uFullVersion, pszName, pInfo2->additionsRevision, pInfo2->additionsFeatures);

    /* Clear our IRQ in case it was high for whatever reason. */
    PDMDevHlpPCISetIrqNoWait (pThis->pDevIns, 0, 0);

    return VINF_SUCCESS;
}

/**
 * Allocates a new facility status entry, initializing it to inactive.
 *
 * @returns Pointer to a facility status entry on success, NULL on failure
 *          (table full).
 * @param   pThis           The VMMDev instance data.
 * @param   uFacility       The facility type code - VBoxGuestFacilityType.
 * @param   fFixed          This is set when allocating the standard entries
 *                          from the constructor.
 * @param   pTimeSpecNow    Optionally giving the entry timestamp to use (ctor).
 */
static PVMMDEVFACILITYSTATUSENTRY
vmmdevAllocFacilityStatusEntry(VMMDevState *pThis, uint32_t uFacility, bool fFixed, PCRTTIMESPEC pTimeSpecNow)
{
    /* If full, expunge one inactive entry. */
    if (pThis->cFacilityStatuses == RT_ELEMENTS(pThis->aFacilityStatuses))
    {
        uint32_t i = pThis->cFacilityStatuses;
        while (i-- > 0)
        {
            if (   pThis->aFacilityStatuses[i].uStatus == VBoxGuestFacilityStatus_Inactive
                && !pThis->aFacilityStatuses[i].fFixed)
            {
                pThis->cFacilityStatuses--;
                int cToMove = pThis->cFacilityStatuses - i;
                if (cToMove)
                    memmove(&pThis->aFacilityStatuses[i], &pThis->aFacilityStatuses[i + 1],
                            cToMove * sizeof(pThis->aFacilityStatuses[i]));
                RT_ZERO(pThis->aFacilityStatuses[pThis->cFacilityStatuses]);
                break;
            }
        }

        if (pThis->cFacilityStatuses == RT_ELEMENTS(pThis->aFacilityStatuses))
            return NULL;
    }

    /* Find location in array (it's sorted). */
    uint32_t i = pThis->cFacilityStatuses;
    while (i-- > 0)
        if (pThis->aFacilityStatuses[i].uFacility < uFacility)
            break;
    i++;

    /* Move. */
    int cToMove = pThis->cFacilityStatuses - i;
    if (cToMove > 0)
        memmove(&pThis->aFacilityStatuses[i + 1], &pThis->aFacilityStatuses[i],
                cToMove * sizeof(pThis->aFacilityStatuses[i]));
    pThis->cFacilityStatuses++;

    /* Initialize. */
    pThis->aFacilityStatuses[i].uFacility   = uFacility;
    pThis->aFacilityStatuses[i].uStatus     = VBoxGuestFacilityStatus_Inactive;
    pThis->aFacilityStatuses[i].fFixed      = fFixed;
    pThis->aFacilityStatuses[i].fPadding    = 0;
    pThis->aFacilityStatuses[i].fFlags      = 0;
    pThis->aFacilityStatuses[i].uPadding    = 0;
    if (pTimeSpecNow)
        pThis->aFacilityStatuses[i].TimeSpecTS = *pTimeSpecNow;
    else
        RTTimeSpecSetNano(&pThis->aFacilityStatuses[i].TimeSpecTS, 0);

    return &pThis->aFacilityStatuses[i];

}

/**
 * Gets a facility status entry, allocating a new one if not already present.
 *
 * @returns Pointer to a facility status entry on success, NULL on failure
 *          (table full).
 * @param   pThis           The VMMDev instance data.
 * @param   uFacility       The facility type code - VBoxGuestFacilityType.
 */
static PVMMDEVFACILITYSTATUSENTRY vmmdevGetFacilityStatusEntry(VMMDevState *pThis, uint32_t uFacility)
{
    /** @todo change to binary search. */
    uint32_t i = pThis->cFacilityStatuses;
    while (i-- > 0)
    {
        if (pThis->aFacilityStatuses[i].uFacility == uFacility)
            return &pThis->aFacilityStatuses[i];
        if (pThis->aFacilityStatuses[i].uFacility < uFacility)
            break;
    }
    return vmmdevAllocFacilityStatusEntry(pThis, uFacility, false /*fFixed*/, NULL);
}

/**
 * Handles VMMDevReq_ReportGuestStatus.
 *
 * @returns VBox status code that the guest should see.
 * @param   pThis           The VMMDev instance data.
 * @param   pRequestHeader  The header of the request to handle.
 */
static int vmmdevReqHandler_ReportGuestStatus(VMMDevState *pThis, VMMDevRequestHeader *pRequestHeader)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(pRequestHeader->size == sizeof(VMMDevReportGuestStatus), ("%u\n", pRequestHeader->size), VERR_INVALID_PARAMETER);
    VBoxGuestStatus *pStatus = &((VMMDevReportGuestStatus *)pRequestHeader)->guestStatus;
    AssertMsgReturn(   pStatus->facility > VBoxGuestFacilityType_Unknown
                    && pStatus->facility <= VBoxGuestFacilityType_All,
                    ("%d\n", pStatus->facility),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(pStatus->status == (VBoxGuestFacilityStatus)(uint16_t)pStatus->status,
                    ("%#x (%u)\n", pStatus->status, pStatus->status),
                    VERR_OUT_OF_RANGE);

    /*
     * Do the update.
     */
    RTTIMESPEC Now;
    RTTimeNow(&Now);
    if (pStatus->facility == VBoxGuestFacilityType_All)
    {
        uint32_t i = pThis->cFacilityStatuses;
        while (i-- > 0)
        {
            pThis->aFacilityStatuses[i].TimeSpecTS = Now;
            pThis->aFacilityStatuses[i].uStatus    = (uint16_t)pStatus->status;
            pThis->aFacilityStatuses[i].fFlags     = pStatus->flags;
        }
    }
    else
    {
        PVMMDEVFACILITYSTATUSENTRY pEntry = vmmdevGetFacilityStatusEntry(pThis, pStatus->facility);
        if (!pEntry)
        {
            static int g_cLogEntries = 0;
            if (g_cLogEntries++ < 10)
                LogRel(("VMM: Facility table is full - facility=%u status=%u.\n", pStatus->facility, pStatus->status));
            return VERR_OUT_OF_RESOURCES;
        }

        pEntry->TimeSpecTS = Now;
        pEntry->uStatus    = (uint16_t)pStatus->status;
        pEntry->fFlags     = pStatus->flags;
    }

    if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestStatus)
        pThis->pDrv->pfnUpdateGuestStatus(pThis->pDrv, pStatus->facility, pStatus->status, pStatus->flags, &Now);

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_PAGE_SHARING

/**
 * Handles VMMDevReq_RegisterSharedModule.
 *
 * @returns VBox status code that the guest should see.
 * @param   pDevIns         The VMMDev device instance.
 * @param   pReq            Pointer to the request.
 */
static int vmmdevReqHandler_RegisterSharedModule(PPDMDEVINS pDevIns, VMMDevSharedModuleRegistrationRequest *pReq)
{
    /*
     * Basic input validation (more done by GMM).
     */
    AssertMsgReturn(pReq->header.size >= sizeof(VMMDevSharedModuleRegistrationRequest),
                    ("%u\n", pReq->header.size), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pReq->header.size == RT_UOFFSETOF(VMMDevSharedModuleRegistrationRequest, aRegions[pReq->cRegions]),
                    ("%u cRegions=%u\n", pReq->header.size, pReq->cRegions), VERR_INVALID_PARAMETER);

    AssertReturn(memchr(pReq->szName, '\0', sizeof(pReq->szName)), VERR_INVALID_PARAMETER);
    AssertReturn(memchr(pReq->szVersion, '\0', sizeof(pReq->szVersion)), VERR_INVALID_PARAMETER);

    /*
     * Forward the request to the VMM.
     */
    return PGMR3SharedModuleRegister(PDMDevHlpGetVM(pDevIns), pReq->enmGuestOS, pReq->szName, pReq->szVersion,
                                     pReq->GCBaseAddr, pReq->cbModule, pReq->cRegions, pReq->aRegions);
}

/**
 * Handles VMMDevReq_UnregisterSharedModule.
 *
 * @returns VBox status code that the guest should see.
 * @param   pDevIns         The VMMDev device instance.
 * @param   pReq            Pointer to the request.
 */
static int vmmdevReqHandler_UnregisterSharedModule(PPDMDEVINS pDevIns, VMMDevSharedModuleUnregistrationRequest *pReq)
{
    /*
     * Basic input validation.
     */
    AssertMsgReturn(pReq->header.size == sizeof(VMMDevSharedModuleUnregistrationRequest),
                    ("%u\n", pReq->header.size), VERR_INVALID_PARAMETER);

    AssertReturn(memchr(pReq->szName, '\0', sizeof(pReq->szName)), VERR_INVALID_PARAMETER);
    AssertReturn(memchr(pReq->szVersion, '\0', sizeof(pReq->szVersion)), VERR_INVALID_PARAMETER);

    /*
     * Forward the request to the VMM.
     */
    return PGMR3SharedModuleUnregister(PDMDevHlpGetVM(pDevIns), pReq->szName, pReq->szVersion,
                                       pReq->GCBaseAddr, pReq->cbModule);
}

/**
 * Handles VMMDevReq_CheckSharedModules.
 *
 * @returns VBox status code that the guest should see.
 * @param   pDevIns         The VMMDev device instance.
 * @param   pReq            Pointer to the request.
 */
static int vmmdevReqHandler_CheckSharedModules(PPDMDEVINS pDevIns, VMMDevSharedModuleCheckRequest *pReq)
{
    AssertMsgReturn(pReq->header.size == sizeof(VMMDevSharedModuleCheckRequest),
                    ("%u\n", pReq->header.size), VERR_INVALID_PARAMETER);
    return PGMR3SharedModuleCheckAll(PDMDevHlpGetVM(pDevIns));
}

/**
 * Handles VMMDevReq_GetPageSharingStatus.
 *
 * @returns VBox status code that the guest should see.
 * @param   pThis           The VMMDev instance data.
 * @param   pReq            Pointer to the request.
 */
static int vmmdevReqHandler_GetPageSharingStatus(VMMDevState *pThis, VMMDevPageSharingStatusRequest *pReq)
{
    AssertMsgReturn(pReq->header.size == sizeof(VMMDevPageSharingStatusRequest),
                    ("%u\n", pReq->header.size), VERR_INVALID_PARAMETER);

    pReq->fEnabled = false;
    int rc = pThis->pDrv->pfnIsPageFusionEnabled(pThis->pDrv, &pReq->fEnabled);
    if (RT_FAILURE(rc))
        pReq->fEnabled = false;
    return VINF_SUCCESS;
}


/**
 * Handles VMMDevReq_DebugIsPageShared.
 *
 * @returns VBox status code that the guest should see.
 * @param   pDevIns         The VMMDev device instance.
 * @param   pReq            Pointer to the request.
 */
static int vmmdevReqHandler_DebugIsPageShared(PPDMDEVINS pDevIns, VMMDevPageIsSharedRequest *pReq)
{
    AssertMsgReturn(pReq->header.size == sizeof(VMMDevPageIsSharedRequest),
                    ("%u\n", pReq->header.size), VERR_INVALID_PARAMETER);

#ifdef DEBUG
    return PGMR3SharedModuleGetPageState(PDMDevHlpGetVM(pDevIns), pReq->GCPtrPage, &pReq->fShared, &pReq->uPageFlags);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

#endif /* VBOX_WITH_PAGE_SHARING */

/**
 * Port I/O Handler for the generic request interface
 * @see FNIOMIOPORTOUT for details.
 *
 * @todo This function is too long!!  All new request SHALL be implemented as
 *       functions called from the switch!  When making changes, please move the
 *       relevant cases into functions.
 */
static DECLCALLBACK(int) vmmdevRequestHandler(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pThis = (VMMDevState*)pvUser;
    int rcRet = VINF_SUCCESS;
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    /*
     * The caller has passed the guest context physical address
     * of the request structure. Copy the request packet.
     */
    VMMDevRequestHeader *pRequestHeader = NULL;
    VMMDevRequestHeader requestHeader;
    RT_ZERO(requestHeader);

    PDMDevHlpPhysRead(pDevIns, (RTGCPHYS)u32, &requestHeader, sizeof(requestHeader));

    /* the structure size must be greater or equal to the header size */
    if (requestHeader.size < sizeof(VMMDevRequestHeader))
    {
        Log(("VMMDev request header size too small! size = %d\n", requestHeader.size));
        rcRet = VINF_SUCCESS;
        goto l_end; /** @todo shouldn't (/ no need to) write back.*/
    }

    /* check the version of the header structure */
    if (requestHeader.version != VMMDEV_REQUEST_HEADER_VERSION)
    {
        Log(("VMMDev: guest header version (0x%08X) differs from ours (0x%08X)\n", requestHeader.version, VMMDEV_REQUEST_HEADER_VERSION));
        rcRet = VINF_SUCCESS;
        goto l_end; /** @todo shouldn't (/ no need to) write back.*/
    }

    Log2(("VMMDev request issued: %d\n", requestHeader.requestType));

    /* Newer additions starts with VMMDevReq_ReportGuestInfo2, older additions
       started with VMMDevReq_ReportGuestInfo. */
    if (   !pThis->fu32AdditionsOk
        && requestHeader.requestType != VMMDevReq_ReportGuestInfo2
        && requestHeader.requestType != VMMDevReq_ReportGuestInfo
        && requestHeader.requestType != VMMDevReq_WriteCoreDump
        && requestHeader.requestType != VMMDevReq_GetHostVersion) /* Always allow the guest to query the host capabilities. */
    {
        Log(("VMMDev: guest has not yet reported to us. Refusing operation of request #%d!\n",
             requestHeader.requestType));
        requestHeader.rc = VERR_NOT_SUPPORTED;
        static int cRelWarn;
        if (cRelWarn < 10)
        {
            cRelWarn++;
            LogRel(("VMMDev: the guest has not yet reported to us -- refusing operation of request #%d\n",
                    requestHeader.requestType));
        }
        rcRet = VINF_SUCCESS;
        goto l_end;
    }

    /* Check upper limit */
    if (requestHeader.size > VMMDEV_MAX_VMMDEVREQ_SIZE)
    {
        static int cRelWarn;
        if (cRelWarn < 50)
        {
            cRelWarn++;
            LogRel(("VMMDev: request packet too big (%x). Refusing operation.\n", requestHeader.size));
        }
        requestHeader.rc = VERR_NOT_SUPPORTED;
        rcRet = VINF_SUCCESS;
        goto l_end;
    }

    /* Read the entire request packet */
    pRequestHeader = (VMMDevRequestHeader *)RTMemAlloc(requestHeader.size);
    if (!pRequestHeader)
    {
        Log(("VMMDev: RTMemAlloc failed!\n"));
        rcRet = VINF_SUCCESS;
        requestHeader.rc = VERR_NO_MEMORY;
        goto l_end;
    }
    PDMDevHlpPhysRead(pDevIns, (RTGCPHYS)u32, pRequestHeader, requestHeader.size);

    /* which request was sent? */
    switch (pRequestHeader->requestType)
    {
        /*
         * Guest wants to give up a timeslice
         */
        case VMMDevReq_Idle:
        {
            /* just return to EMT telling it that we want to halt */
            rcRet = VINF_EM_HALT;
            break;
        }

        /*
         * Guest is reporting its information
         */
        case VMMDevReq_ReportGuestInfo:
        {
            AssertMsgBreakStmt(pRequestHeader->size == sizeof(VMMDevReportGuestInfo), ("%u\n", pRequestHeader->size),
                               pRequestHeader->rc = VERR_INVALID_PARAMETER);
            VBoxGuestInfo *pGuestInfo = &((VMMDevReportGuestInfo*)pRequestHeader)->guestInfo;

            if (memcmp(&pThis->guestInfo, pGuestInfo, sizeof(*pGuestInfo)) != 0)
            {
                /* make a copy of supplied information */
                pThis->guestInfo = *pGuestInfo;

                /* Check additions version */
                pThis->fu32AdditionsOk = VBOX_GUEST_INTERFACE_VERSION_OK(pThis->guestInfo.interfaceVersion);

                LogRel(("Guest Additions information report: Interface = 0x%08X osType = 0x%08X\n",
                        pThis->guestInfo.interfaceVersion,
                        pThis->guestInfo.osType));
                if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestInfo)
                    pThis->pDrv->pfnUpdateGuestInfo(pThis->pDrv, &pThis->guestInfo);
            }

            if (pThis->fu32AdditionsOk)
            {
                pRequestHeader->rc = VINF_SUCCESS;
                /* Clear our IRQ in case it was high for whatever reason. */
                PDMDevHlpPCISetIrqNoWait (pThis->pDevIns, 0, 0);
            }
            else
                pRequestHeader->rc = VERR_VERSION_MISMATCH;
            break;
        }

        case VMMDevReq_ReportGuestInfo2:
        {
            pRequestHeader->rc = vmmdevReqHandler_ReportGuestInfo2(pThis, pRequestHeader);
            break;
        }

        case VMMDevReq_WriteCoreDump:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqWriteCoreDump))
            {
                AssertMsgFailed(("VMMDev WriteCoreDump structure has an invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                if (pThis->fGuestCoreDumpEnabled)
                {
                    /*
                     * User makes sure the directory exists.
                     */
                    if (!RTDirExists(pThis->szGuestCoreDumpDir))
                        return VERR_PATH_NOT_FOUND;

                    char szCorePath[RTPATH_MAX];
                    RTStrCopy(szCorePath, sizeof(szCorePath), pThis->szGuestCoreDumpDir);
                    RTPathAppend(szCorePath, sizeof(szCorePath), "VBox.core");

                    /*
                     * Rotate existing cores based on number of additional cores to keep around.
                     */
                    if (pThis->cGuestCoreDumps > 0)
                        for (int64_t i = pThis->cGuestCoreDumps - 1; i >= 0; i--)
                        {
                            char szFilePathOld[RTPATH_MAX];
                            if (i == 0)
                                RTStrCopy(szFilePathOld, sizeof(szFilePathOld), szCorePath);
                            else
                                RTStrPrintf(szFilePathOld, sizeof(szFilePathOld), "%s.%lld", szCorePath, i);

                            char szFilePathNew[RTPATH_MAX];
                            RTStrPrintf(szFilePathNew, sizeof(szFilePathNew), "%s.%lld", szCorePath, i + 1);
                            int vrc = RTFileMove(szFilePathOld, szFilePathNew, RTFILEMOVE_FLAGS_REPLACE);
                            if (vrc == VERR_FILE_NOT_FOUND)
                                RTFileDelete(szFilePathNew);
                        }

                    /*
                     * Write the core file.
                     */
                    PUVM pUVM = PDMDevHlpGetUVM(pDevIns);
                    pRequestHeader->rc = DBGFR3CoreWrite(pUVM, szCorePath, true /*fReplaceFile*/);
                }
                else
                    pRequestHeader->rc = VERR_ACCESS_DENIED;
            }
            break;
        }

        case VMMDevReq_ReportGuestStatus:
            pRequestHeader->rc = vmmdevReqHandler_ReportGuestStatus(pThis, pRequestHeader);
            break;

        /* Report guest capabilities */
        case VMMDevReq_ReportGuestCapabilities:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqGuestCapabilities))
            {
                AssertMsgFailed(("VMMDev guest caps structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqGuestCapabilities *guestCaps = (VMMDevReqGuestCapabilities*)pRequestHeader;

                /* Enable this automatically for guests using the old
                   request to report their capabilities. */
                /** @todo change this when we next bump the interface version */
                guestCaps->caps |= VMMDEV_GUEST_SUPPORTS_GRAPHICS;
                if (pThis->guestCaps != guestCaps->caps)
                {
                    /* make a copy of supplied information */
                    pThis->guestCaps = guestCaps->caps;

                    LogRel(("Guest Additions capability report: (0x%x) "
                            "seamless: %s, "
                            "hostWindowMapping: %s, "
                            "graphics: %s\n",
                            guestCaps->caps,
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_SEAMLESS ? "yes" : "no",
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING ? "yes" : "no",
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_GRAPHICS ? "yes" : "no"));

                    if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestCapabilities)
                        pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, guestCaps->caps);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /* Change guest capabilities */
        case VMMDevReq_SetGuestCapabilities:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqGuestCapabilities2))
            {
                AssertMsgFailed(("VMMDev guest caps structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqGuestCapabilities2 *guestCaps = (VMMDevReqGuestCapabilities2*)pRequestHeader;

                pThis->guestCaps |= guestCaps->u32OrMask;
                pThis->guestCaps &= ~guestCaps->u32NotMask;

                LogRel(("Guest Additions capability report: (0x%x) "
                        "seamless: %s, "
                        "hostWindowMapping: %s, "
                        "graphics: %s\n",
                        pThis->guestCaps,
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS ? "yes" : "no",
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING ? "yes" : "no",
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_GRAPHICS ? "yes" : "no"));

                if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestCapabilities)
                    pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Retrieve mouse information
         */
        case VMMDevReq_GetMouseStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMouseStatus *mouseStatus = (VMMDevReqMouseStatus*)pRequestHeader;
                mouseStatus->mouseFeatures =   pThis->mouseCapabilities
                                             & VMMDEV_MOUSE_MASK;
                mouseStatus->pointerXPos = pThis->mouseXAbs;
                mouseStatus->pointerYPos = pThis->mouseYAbs;
                LogRel2(("%s: VMMDevReq_GetMouseStatus: features = 0x%x, xAbs = %d, yAbs = %d\n",
                                __PRETTY_FUNCTION__,
                                mouseStatus->mouseFeatures,
                                mouseStatus->pointerXPos,
                                mouseStatus->pointerYPos));
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Set mouse information
         */
        case VMMDevReq_SetMouseStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size %d (%#x) version=%d!\n",
                                 pRequestHeader->size, pRequestHeader->size, pRequestHeader->size, pRequestHeader->version));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                bool fNotify = false;

                uint32_t fFeatures =
                    ((VMMDevReqMouseStatus*)pRequestHeader)->mouseFeatures;

                LogRelFlowFunc(("VMMDevReqMouseStatus: mouseFeatures = 0x%x\n",
                                fFeatures));

                if (   (fFeatures & VMMDEV_MOUSE_NOTIFY_HOST_MASK)
                    != (  pThis->mouseCapabilities
                        & VMMDEV_MOUSE_NOTIFY_HOST_MASK))
                    fNotify = true;
                pThis->mouseCapabilities &= ~VMMDEV_MOUSE_GUEST_MASK;
                pThis->mouseCapabilities |=
                    (fFeatures & VMMDEV_MOUSE_GUEST_MASK);
                LogRelFlowFunc(("VMMDevReq_SetMouseStatus: new host capabilities: 0x%x\n",
                                pThis->mouseCapabilities));

                /*
                 * Notify connector if something has changed
                 */
                if (fNotify)
                {
                    LogRelFlowFunc(("VMMDevReq_SetMouseStatus: notifying connector\n"));
                    pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }

            break;
        }

        /*
         * Set a new mouse pointer shape
         */
        case VMMDevReq_SetPointerShape:
        {
            if (pRequestHeader->size < sizeof(VMMDevReqMousePointer))
            {
                AssertMsg(pRequestHeader->size == 0x10028 && pRequestHeader->version == 10000,  /* don't complain about legacy!!! */
                          ("VMMDev mouse shape structure has invalid size %d (%#x) version=%d!\n",
                           pRequestHeader->size, pRequestHeader->size, pRequestHeader->size, pRequestHeader->version));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMousePointer *pointerShape = (VMMDevReqMousePointer*)pRequestHeader;

                bool fVisible = (pointerShape->fFlags & VBOX_MOUSE_POINTER_VISIBLE) != 0;
                bool fAlpha = (pointerShape->fFlags & VBOX_MOUSE_POINTER_ALPHA) != 0;
                bool fShape = (pointerShape->fFlags & VBOX_MOUSE_POINTER_SHAPE) != 0;

                Log(("VMMDevReq_SetPointerShape: visible: %d, alpha: %d, shape = %d, width: %d, height: %d\n",
                     fVisible, fAlpha, fShape, pointerShape->width, pointerShape->height));

                if (pRequestHeader->size == sizeof(VMMDevReqMousePointer))
                {
                    /* The guest did not provide the shape actually. */
                    fShape = false;
                }

                /* forward call to driver */
                if (fShape)
                {
                    pThis->pDrv->pfnUpdatePointerShape(pThis->pDrv,
                                                       fVisible,
                                                       fAlpha,
                                                       pointerShape->xHot, pointerShape->yHot,
                                                       pointerShape->width, pointerShape->height,
                                                       pointerShape->pointerData);
                }
                else
                {
                    pThis->pDrv->pfnUpdatePointerShape(pThis->pDrv,
                                                       fVisible,
                                                       0,
                                                       0, 0,
                                                       0, 0,
                                                       NULL);
                }
                pThis->fHostCursorRequested = fVisible;
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query the system time from the host
         */
        case VMMDevReq_GetHostTime:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHostTime))
            {
                AssertMsgFailed(("VMMDev host time structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (RT_UNLIKELY(pThis->fGetHostTimeDisabled))
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            else
            {
                VMMDevReqHostTime *hostTimeReq = (VMMDevReqHostTime*)pRequestHeader;
                RTTIMESPEC now;
                hostTimeReq->time = RTTimeSpecGetMilli(PDMDevHlpTMUtcNow(pDevIns, &now));
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query information about the hypervisor
         */
        case VMMDevReq_GetHypervisorInfo:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)pRequestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                pRequestHeader->rc = PGMR3MappingsSize(pVM, &hypervisorInfo->hypervisorSize);
            }
            break;
        }

        /*
         * Set hypervisor information
         */
        case VMMDevReq_SetHypervisorInfo:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)pRequestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                if (hypervisorInfo->hypervisorStart == 0)
                    pRequestHeader->rc = PGMR3MappingsUnfix(pVM);
                else
                {
                    /* only if the client has queried the size before! */
                    uint32_t mappingsSize;
                    pRequestHeader->rc = PGMR3MappingsSize(pVM, &mappingsSize);
                    if (RT_SUCCESS(pRequestHeader->rc) && hypervisorInfo->hypervisorSize == mappingsSize)
                    {
                        /* new reservation */
                        pRequestHeader->rc = PGMR3MappingsFix(pVM, hypervisorInfo->hypervisorStart,
                                                              hypervisorInfo->hypervisorSize);
                        LogRel(("Guest reported fixed hypervisor window at 0x%p (size = 0x%x, rc = %Rrc)\n",
                                (uintptr_t)hypervisorInfo->hypervisorStart,
                                hypervisorInfo->hypervisorSize,
                                pRequestHeader->rc));
                    }
                }
            }
            break;
        }

        case VMMDevReq_RegisterPatchMemory:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqPatchMemory))
            {
                AssertMsgFailed(("VMMDevReq_RegisterPatchMemory structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqPatchMemory *pPatchRequest = (VMMDevReqPatchMemory*)pRequestHeader;

                pRequestHeader->rc = VMMR3RegisterPatchMemory(PDMDevHlpGetVM(pDevIns), pPatchRequest->pPatchMem, pPatchRequest->cbPatchMem);
            }
            break;
        }

        case VMMDevReq_DeregisterPatchMemory:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqPatchMemory))
            {
                AssertMsgFailed(("VMMDevReq_DeregisterPatchMemory structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqPatchMemory *pPatchRequest = (VMMDevReqPatchMemory*)pRequestHeader;

                pRequestHeader->rc = VMMR3DeregisterPatchMemory(PDMDevHlpGetVM(pDevIns), pPatchRequest->pPatchMem, pPatchRequest->cbPatchMem);
            }
            break;
        }

        /*
         * Set the system power status
         */
        case VMMDevReq_SetPowerStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevPowerStateRequest))
            {
                AssertMsgFailed(("VMMDev power state request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevPowerStateRequest *powerStateRequest = (VMMDevPowerStateRequest*)pRequestHeader;
                switch(powerStateRequest->powerState)
                {
                    case VMMDevPowerState_Pause:
                    {
                        LogRel(("Guest requests the VM to be suspended (paused)\n"));
                        pRequestHeader->rc = rcRet = PDMDevHlpVMSuspend(pDevIns);
                        break;
                    }

                    case VMMDevPowerState_PowerOff:
                    {
                        LogRel(("Guest requests the VM to be turned off\n"));
                        pRequestHeader->rc = rcRet = PDMDevHlpVMPowerOff(pDevIns);
                        break;
                    }

                    case VMMDevPowerState_SaveState:
                    {
                        if (true /*pThis->fAllowGuestToSaveState*/)
                        {
                            LogRel(("Guest requests the VM to be saved and powered off\n"));
                            pRequestHeader->rc = rcRet = PDMDevHlpVMSuspendSaveAndPowerOff(pDevIns);
                        }
                        else
                        {
                            LogRel(("Guest requests the VM to be saved and powered off, declined\n"));
                            pRequestHeader->rc = VERR_ACCESS_DENIED;
                        }
                        break;
                    }

                    default:
                        AssertMsgFailed(("VMMDev invalid power state request: %d\n", powerStateRequest->powerState));
                        pRequestHeader->rc = VERR_INVALID_PARAMETER;
                        break;
                }
            }
            break;
        }

        /*
         * Retrieve a display resize request sent by the host using
         * @a IDisplay:setVideoModeHint.  Deprecated.
         * See documentation in VMMDev.h.
         */
        /**
         * @todo It looks like a multi-monitor guest which only uses
         *        @a VMMDevReq_GetDisplayChangeRequest (not the *2 version)
         *        will get into a @a VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST event
         *        loop if it tries to acknowlege host requests for additional
         *        monitors.  Should the loop which checks for those requests
         *        be removed?
         */
        case VMMDevReq_GetDisplayChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevDisplayChangeRequest))
            {
                /* Assert only if the size also not equal to a previous version size to prevent
                 * assertion with old additions.
                 */
                AssertMsg(pRequestHeader->size == sizeof(VMMDevDisplayChangeRequest) - sizeof (uint32_t),
                          ("VMMDev display change request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequest *displayChangeRequest = (VMMDevDisplayChangeRequest*)pRequestHeader;

                DISPLAYCHANGEREQUEST *pRequest = &pThis->displayChangeData.aRequests[0];

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Current request has been read at least once. */
                    pRequest->fPending = false;

                    /* Check if there are more pending requests. */
                    for (unsigned i = 1; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
                    {
                        if (pThis->displayChangeData.aRequests[i].fPending)
                        {
                            VMMDevNotifyGuest(pThis, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
                            break;
                        }
                    }

                    /* Remember which resolution the client has queried, subsequent reads
                     * will return the same values. */
                    pRequest->lastReadDisplayChangeRequest = pRequest->displayChangeRequest;
                    pThis->displayChangeData.fGuestSentChangeEventAck = true;
                }

                if (pThis->displayChangeData.fGuestSentChangeEventAck)
                {
                    displayChangeRequest->xres = pRequest->lastReadDisplayChangeRequest.xres;
                    displayChangeRequest->yres = pRequest->lastReadDisplayChangeRequest.yres;
                    displayChangeRequest->bpp  = pRequest->lastReadDisplayChangeRequest.bpp;
                }
                else
                {
                    /* This is not a response to a VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, just
                     * read the last valid video mode hint. This happens when the guest X server
                     * determines the initial mode. */
                    displayChangeRequest->xres = pRequest->displayChangeRequest.xres;
                    displayChangeRequest->yres = pRequest->displayChangeRequest.yres;
                    displayChangeRequest->bpp  = pRequest->displayChangeRequest.bpp;
                }
                Log(("VMMDev: returning display change request xres = %d, yres = %d, bpp = %d\n",
                     displayChangeRequest->xres, displayChangeRequest->yres, displayChangeRequest->bpp));

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Retrieve a display resize request sent by the host using
         * @a IDisplay:setVideoModeHint.
         * See documentation in VMMDev.h.
         */
        case VMMDevReq_GetDisplayChangeRequest2:
        {
            if (pRequestHeader->size != sizeof(VMMDevDisplayChangeRequest2))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequest2 *displayChangeRequest = (VMMDevDisplayChangeRequest2*)pRequestHeader;

                DISPLAYCHANGEREQUEST *pRequest = NULL;

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Select a pending request to report. */
                    unsigned i;
                    for (i = 0; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
                    {
                        if (pThis->displayChangeData.aRequests[i].fPending)
                        {
                            pRequest = &pThis->displayChangeData.aRequests[i];
                            /* Remember which request should be reported. */
                            pThis->displayChangeData.iCurrentMonitor = i;
                            Log3(("VMMDev: will report pending request for %d\n",
                                  i));
                            break;
                        }
                    }

                    /* Check if there are more pending requests. */
                    i++;
                    for (; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
                    {
                        if (pThis->displayChangeData.aRequests[i].fPending)
                        {
                            VMMDevNotifyGuest(pThis, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
                            Log3(("VMMDev: another pending at %d\n",
                                  i));
                            break;
                        }
                    }

                    if (pRequest)
                    {
                        /* Current request has been read at least once. */
                        pRequest->fPending = false;

                        /* Remember which resolution the client has queried, subsequent reads
                         * will return the same values. */
                        pRequest->lastReadDisplayChangeRequest = pRequest->displayChangeRequest;
                        pThis->displayChangeData.fGuestSentChangeEventAck = true;
                    }
                    else
                    {
                         Log3(("VMMDev: no pending request!!!\n"));
                    }
                }

                if (!pRequest)
                {
                    Log3(("VMMDev: default to %d\n",
                          pThis->displayChangeData.iCurrentMonitor));
                    pRequest = &pThis->displayChangeData.aRequests[pThis->displayChangeData.iCurrentMonitor];
                }

                if (pThis->displayChangeData.fGuestSentChangeEventAck)
                {
                    displayChangeRequest->xres    = pRequest->lastReadDisplayChangeRequest.xres;
                    displayChangeRequest->yres    = pRequest->lastReadDisplayChangeRequest.yres;
                    displayChangeRequest->bpp     = pRequest->lastReadDisplayChangeRequest.bpp;
                    displayChangeRequest->display = pRequest->lastReadDisplayChangeRequest.display;
                }
                else
                {
                    /* This is not a response to a VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, just
                     * read the last valid video mode hint. This happens when the guest X server
                     * determines the initial video mode. */
                    displayChangeRequest->xres    = pRequest->displayChangeRequest.xres;
                    displayChangeRequest->yres    = pRequest->displayChangeRequest.yres;
                    displayChangeRequest->bpp     = pRequest->displayChangeRequest.bpp;
                    displayChangeRequest->display = pRequest->displayChangeRequest.display;
                }
                Log(("VMMDev: returning display change request xres = %d, yres = %d, bpp = %d at %d\n",
                     displayChangeRequest->xres, displayChangeRequest->yres, displayChangeRequest->bpp, displayChangeRequest->display));

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Retrieve a display resize request sent by the host using
         * @a IDisplay:setVideoModeHint.
         * See documentation in VMMDev.h.
         */
        case VMMDevReq_GetDisplayChangeRequestEx:
        {
            LogFlowFunc(("VMMDevReq_GetDisplayChangeRequestEx\n"));
            if (pRequestHeader->size != sizeof(VMMDevDisplayChangeRequestEx))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequestEx *displayChangeRequest = (VMMDevDisplayChangeRequestEx*)pRequestHeader;

                DISPLAYCHANGEREQUEST *pRequest = NULL;

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Select a pending request to report. */
                    unsigned i;
                    for (i = 0; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
                    {
                        if (pThis->displayChangeData.aRequests[i].fPending)
                        {
                            pRequest = &pThis->displayChangeData.aRequests[i];
                            /* Remember which request should be reported. */
                            pThis->displayChangeData.iCurrentMonitor = i;
                            Log3(("VMMDev: will report pending request for %d\n",
                                  i));
                            break;
                        }
                    }

                    /* Check if there are more pending requests. */
                    i++;
                    for (; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
                    {
                        if (pThis->displayChangeData.aRequests[i].fPending)
                        {
                            VMMDevNotifyGuest(pThis, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
                            Log3(("VMMDev: another pending at %d\n",
                                  i));
                            break;
                        }
                    }

                    if (pRequest)
                    {
                        /* Current request has been read at least once. */
                        pRequest->fPending = false;

                        /* Remember which resolution the client has queried, subsequent reads
                         * will return the same values. */
                        pRequest->lastReadDisplayChangeRequest = pRequest->displayChangeRequest;
                        pThis->displayChangeData.fGuestSentChangeEventAck = true;
                    }
                    else
                    {
                         Log3(("VMMDev: no pending request!!!\n"));
                    }
                }

                if (!pRequest)
                {
                    Log3(("VMMDev: default to %d\n",
                          pThis->displayChangeData.iCurrentMonitor));
                    pRequest = &pThis->displayChangeData.aRequests[pThis->displayChangeData.iCurrentMonitor];
                }

                if (pThis->displayChangeData.fGuestSentChangeEventAck)
                {
                    displayChangeRequest->xres          = pRequest->lastReadDisplayChangeRequest.xres;
                    displayChangeRequest->yres          = pRequest->lastReadDisplayChangeRequest.yres;
                    displayChangeRequest->bpp           = pRequest->lastReadDisplayChangeRequest.bpp;
                    displayChangeRequest->display       = pRequest->lastReadDisplayChangeRequest.display;
                    displayChangeRequest->cxOrigin      = pRequest->lastReadDisplayChangeRequest.xOrigin;
                    displayChangeRequest->cyOrigin      = pRequest->lastReadDisplayChangeRequest.yOrigin;
                    displayChangeRequest->fEnabled      = pRequest->lastReadDisplayChangeRequest.fEnabled;
                    displayChangeRequest->fChangeOrigin = pRequest->lastReadDisplayChangeRequest.fChangeOrigin;
                }
                else
                {
                    /* This is not a response to a VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, just
                     * read the last valid video mode hint. This happens when the guest X server
                     * determines the initial video mode. */
                    displayChangeRequest->xres          = pRequest->displayChangeRequest.xres;
                    displayChangeRequest->yres          = pRequest->displayChangeRequest.yres;
                    displayChangeRequest->bpp           = pRequest->displayChangeRequest.bpp;
                    displayChangeRequest->display       = pRequest->displayChangeRequest.display;
                    displayChangeRequest->cxOrigin      = pRequest->displayChangeRequest.xOrigin;
                    displayChangeRequest->cyOrigin      = pRequest->displayChangeRequest.yOrigin;
                    displayChangeRequest->fEnabled      = pRequest->displayChangeRequest.fEnabled;
                    displayChangeRequest->fChangeOrigin = pRequest->displayChangeRequest.fChangeOrigin;

                }
                Log(("VMMDevEx: returning display change request xres = %d, yres = %d, bpp = %d id %d \
                      xPos = %d, yPos = %d & Enabled=%d\n",
                     displayChangeRequest->xres, displayChangeRequest->yres,
                     displayChangeRequest->bpp, displayChangeRequest->display,
                     displayChangeRequest->cxOrigin, displayChangeRequest->cyOrigin,
                     displayChangeRequest->fEnabled));

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }


        /*
         * Query whether the given video mode is supported
         */
        case VMMDevReq_VideoModeSupported:
        {
            if (pRequestHeader->size != sizeof(VMMDevVideoModeSupportedRequest))
            {
                AssertMsgFailed(("VMMDev video mode supported request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVideoModeSupportedRequest *videoModeSupportedRequest = (VMMDevVideoModeSupportedRequest*)pRequestHeader;
                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnVideoModeSupported(pThis->pDrv,
                                                                       0, /* primary screen. */
                                                                       videoModeSupportedRequest->width,
                                                                       videoModeSupportedRequest->height,
                                                                       videoModeSupportedRequest->bpp,
                                                                       &videoModeSupportedRequest->fSupported);
            }
            break;
        }

        /*
         * Query whether the given video mode is supported for a specific display
         */
        case VMMDevReq_VideoModeSupported2:
        {
            if (pRequestHeader->size != sizeof(VMMDevVideoModeSupportedRequest2))
            {
                AssertMsgFailed(("VMMDev video mode supported request 2 structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVideoModeSupportedRequest2 *videoModeSupportedRequest2 = (VMMDevVideoModeSupportedRequest2*)pRequestHeader;
                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnVideoModeSupported(pThis->pDrv,
                                                                       videoModeSupportedRequest2->display,
                                                                       videoModeSupportedRequest2->width,
                                                                       videoModeSupportedRequest2->height,
                                                                       videoModeSupportedRequest2->bpp,
                                                                       &videoModeSupportedRequest2->fSupported);
            }
            break;
        }

        /*
         * Query the height reduction in pixels
         */
        case VMMDevReq_GetHeightReduction:
        {
            if (pRequestHeader->size != sizeof(VMMDevGetHeightReductionRequest))
            {
                AssertMsgFailed(("VMMDev height reduction request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetHeightReductionRequest *heightReductionRequest = (VMMDevGetHeightReductionRequest*)pRequestHeader;
                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnGetHeightReduction(pThis->pDrv,
                                                                       &heightReductionRequest->heightReduction);
            }
            break;
        }

        /*
         * Acknowledge VMMDev events
         */
        case VMMDevReq_AcknowledgeEvents:
        {
            if (pRequestHeader->size != sizeof(VMMDevEvents))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                if (VBOX_GUEST_INTERFACE_VERSION_1_03(pThis))
                {
                    vmmdevSetIRQ_Legacy_EMT(pThis);
                }
                else
                {
                    VMMDevEvents *pAckRequest;

                    if (pThis->fNewGuestFilterMask)
                    {
                        pThis->fNewGuestFilterMask = false;
                        pThis->u32GuestFilterMask = pThis->u32NewGuestFilterMask;
                    }

                    pAckRequest = (VMMDevEvents *)pRequestHeader;
                    pAckRequest->events =
                        pThis->u32HostEventFlags & pThis->u32GuestFilterMask;

                    pThis->u32HostEventFlags &= ~pThis->u32GuestFilterMask;
                    pThis->pVMMDevRAMR3->V.V1_04.fHaveEvents = false;
                    PDMDevHlpPCISetIrqNoWait (pThis->pDevIns, 0, 0);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Change guest filter mask
         */
        case VMMDevReq_CtlGuestFilterMask:
        {
            if (pRequestHeader->size != sizeof(VMMDevCtlGuestFilterMask))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCtlGuestFilterMask *pCtlMaskRequest;

                pCtlMaskRequest = (VMMDevCtlGuestFilterMask *)pRequestHeader;
                LogRelFlowFunc(("VMMDevCtlGuestFilterMask: or mask: 0x%x, not mask: 0x%x\n",
                                pCtlMaskRequest->u32OrMask,
                                pCtlMaskRequest->u32NotMask));
                /* HGCM event notification is enabled by the VMMDev device
                 * automatically when any HGCM command is issued.  The guest
                 * cannot disable these notifications.
                 */
                VMMDevCtlSetGuestFilterMask (pThis,
                                             pCtlMaskRequest->u32OrMask,
                                             pCtlMaskRequest->u32NotMask & ~VMMDEV_EVENT_HGCM);
                pRequestHeader->rc = VINF_SUCCESS;

            }
            break;
        }

#ifdef VBOX_WITH_HGCM
        /*
         * Process HGCM request
         */
        case VMMDevReq_HGCMConnect:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMConnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMConnect structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMConnect HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMConnect *pHGCMConnect = (VMMDevHGCMConnect *)pRequestHeader;

                Log(("VMMDevReq_HGCMConnect\n"));

                pRequestHeader->rc = vmmdevHGCMConnect (pThis, pHGCMConnect, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_HGCMDisconnect:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMDisconnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMDisconnect structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMDisconnect HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMDisconnect *pHGCMDisconnect = (VMMDevHGCMDisconnect *)pRequestHeader;

                Log(("VMMDevReq_VMMDevHGCMDisconnect\n"));
                pRequestHeader->rc = vmmdevHGCMDisconnect (pThis, pHGCMDisconnect, (RTGCPHYS)u32);
            }
            break;
        }

#ifdef VBOX_WITH_64_BITS_GUESTS
        case VMMDevReq_HGCMCall32:
        case VMMDevReq_HGCMCall64:
#else
        case VMMDevReq_HGCMCall:
#endif /* VBOX_WITH_64_BITS_GUESTS */
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMCall))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCall structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCall HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pRequestHeader;

                Log2(("VMMDevReq_HGCMCall: sizeof (VMMDevHGCMRequest) = %04X\n", sizeof (VMMDevHGCMCall)));
                Log2(("%.*Rhxd\n", pRequestHeader->size, pRequestHeader));

#ifdef VBOX_WITH_64_BITS_GUESTS
                bool f64Bits = (pRequestHeader->requestType == VMMDevReq_HGCMCall64);
#else
                bool f64Bits = false;
#endif /* VBOX_WITH_64_BITS_GUESTS */

                pRequestHeader->rc = vmmdevHGCMCall (pThis, pHGCMCall, requestHeader.size, (RTGCPHYS)u32, f64Bits);
            }
            break;
        }
#endif /* VBOX_WITH_HGCM */

        case VMMDevReq_HGCMCancel:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMCancel))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCancel structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCancel HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCancel *pHGCMCancel = (VMMDevHGCMCancel *)pRequestHeader;

                Log(("VMMDevReq_VMMDevHGCMCancel\n"));
                pRequestHeader->rc = vmmdevHGCMCancel (pThis, pHGCMCancel, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_HGCMCancel2:
        {
            if (pRequestHeader->size != sizeof(VMMDevHGCMCancel2))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCancel structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCancel HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCancel2 *pHGCMCancel2 = (VMMDevHGCMCancel2 *)pRequestHeader;

                Log(("VMMDevReq_VMMDevHGCMCancel\n"));
                pRequestHeader->rc = vmmdevHGCMCancel2 (pThis, pHGCMCancel2->physReqToCancel);
            }
            break;
        }

        case VMMDevReq_VideoAccelEnable:
        {
            if (pRequestHeader->size < sizeof(VMMDevVideoAccelEnable))
            {
                Log(("VMMDevReq_VideoAccelEnable request size too small!!!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoAccelEnable Connector is NULL!!!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevVideoAccelEnable *ptr = (VMMDevVideoAccelEnable *)pRequestHeader;

                if (ptr->cbRingBuffer != VBVA_RING_BUFFER_SIZE)
                {
                    /* The guest driver seems compiled with another headers. */
                    Log(("VMMDevReq_VideoAccelEnable guest ring buffer size %d, should be %d!!!\n", ptr->cbRingBuffer, VBVA_RING_BUFFER_SIZE));
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* The request is correct. */
                    ptr->fu32Status |= VBVA_F_STATUS_ACCEPTED;

                    LogFlow(("VMMDevReq_VideoAccelEnable ptr->u32Enable = %d\n", ptr->u32Enable));

                    pRequestHeader->rc = ptr->u32Enable?
                        pThis->pDrv->pfnVideoAccelEnable (pThis->pDrv, true, &pThis->pVMMDevRAMR3->vbvaMemory):
                        pThis->pDrv->pfnVideoAccelEnable (pThis->pDrv, false, NULL);

                    if (   ptr->u32Enable
                        && RT_SUCCESS (pRequestHeader->rc))
                    {
                        ptr->fu32Status |= VBVA_F_STATUS_ENABLED;

                        /* Remember that guest successfully enabled acceleration.
                         * We need to reestablish it on restoring the VM from saved state.
                         */
                        pThis->u32VideoAccelEnabled = 1;
                    }
                    else
                    {
                        /* The acceleration was not enabled. Remember that. */
                        pThis->u32VideoAccelEnabled = 0;
                    }
                }
            }
            break;
        }

        case VMMDevReq_VideoAccelFlush:
        {
            if (pRequestHeader->size < sizeof(VMMDevVideoAccelFlush))
            {
                AssertMsgFailed(("VMMDevReq_VideoAccelFlush request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoAccelFlush Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                pThis->pDrv->pfnVideoAccelFlush (pThis->pDrv);

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_VideoSetVisibleRegion:
        {
            if (  pRequestHeader->size + sizeof(RTRECT)
                < sizeof(VMMDevVideoSetVisibleRegion))
            {
                Log(("VMMDevReq_VideoSetVisibleRegion request size too small!!!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoSetVisibleRegion Connector is NULL!!!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevVideoSetVisibleRegion *ptr = (VMMDevVideoSetVisibleRegion *)pRequestHeader;

                if (   ptr->cRect > _1M /* restrict to sane range */
                    || pRequestHeader->size != sizeof(VMMDevVideoSetVisibleRegion) + ptr->cRect * sizeof(RTRECT) - sizeof(RTRECT))
                {
                    Log(("VMMDevReq_VideoSetVisibleRegion: cRects=%#x doesn't match size=%#x or is out of bounds\n",
                         ptr->cRect, pRequestHeader->size));
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    Log(("VMMDevReq_VideoSetVisibleRegion %d rectangles\n", ptr->cRect));
                    /* forward the call */
                    pRequestHeader->rc = pThis->pDrv->pfnSetVisibleRegion(pThis->pDrv, ptr->cRect, &ptr->Rect);
                }
            }
            break;
        }

        case VMMDevReq_GetSeamlessChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevSeamlessChangeRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevSeamlessChangeRequest *seamlessChangeRequest = (VMMDevSeamlessChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning seamless change request mode=%d\n", pThis->fSeamlessEnabled));
                if (pThis->fSeamlessEnabled)
                    seamlessChangeRequest->mode = VMMDev_Seamless_Visible_Region;
                else
                    seamlessChangeRequest->mode = VMMDev_Seamless_Disabled;

                if (seamlessChangeRequest->eventAck == VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->fLastSeamlessEnabled = pThis->fSeamlessEnabled;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_GetVRDPChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevVRDPChangeRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVRDPChangeRequest *vrdpChangeRequest = (VMMDevVRDPChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning VRDP status %d level %d\n", pThis->fVRDPEnabled, pThis->uVRDPExperienceLevel));

                vrdpChangeRequest->u8VRDPActive = pThis->fVRDPEnabled;
                vrdpChangeRequest->u32VRDPExperienceLevel = pThis->uVRDPExperienceLevel;

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_GetMemBalloonChangeRequest:
        {
            Log(("VMMDevReq_GetMemBalloonChangeRequest\n"));
            if (pRequestHeader->size != sizeof(VMMDevGetMemBalloonChangeRequest))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetMemBalloonChangeRequest *memBalloonChangeRequest = (VMMDevGetMemBalloonChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning memory balloon size =%d\n", pThis->cMbMemoryBalloon));
                memBalloonChangeRequest->cBalloonChunks = pThis->cMbMemoryBalloon;
                memBalloonChangeRequest->cPhysMemChunks = pThis->cbGuestRAM / (uint64_t)_1M;

                if (memBalloonChangeRequest->eventAck == VMMDEV_EVENT_BALLOON_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->cMbMemoryBalloonLast = pThis->cMbMemoryBalloon;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ChangeMemBalloon:
        {
            VMMDevChangeMemBalloon *memBalloonChange = (VMMDevChangeMemBalloon*)pRequestHeader;

            Log(("VMMDevReq_ChangeMemBalloon\n"));
            if (    pRequestHeader->size < sizeof(VMMDevChangeMemBalloon)
                ||  memBalloonChange->cPages != VMMDEV_MEMORY_BALLOON_CHUNK_PAGES
                ||  pRequestHeader->size != (uint32_t)RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[memBalloonChange->cPages]))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                pRequestHeader->rc = PGMR3PhysChangeMemBalloon(PDMDevHlpGetVM(pDevIns), !!memBalloonChange->fInflate, memBalloonChange->cPages, memBalloonChange->aPhysPage);
                if (memBalloonChange->fInflate)
                    STAM_REL_U32_INC(&pThis->StatMemBalloonChunks);
                else
                    STAM_REL_U32_DEC(&pThis->StatMemBalloonChunks);
            }
            break;
        }

        case VMMDevReq_GetStatisticsChangeRequest:
        {
            Log(("VMMDevReq_GetStatisticsChangeRequest\n"));
            if (pRequestHeader->size != sizeof(VMMDevGetStatisticsChangeRequest))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetStatisticsChangeRequest *statIntervalChangeRequest = (VMMDevGetStatisticsChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning statistics interval %d seconds\n", pThis->u32StatIntervalSize));
                statIntervalChangeRequest->u32StatInterval = pThis->u32StatIntervalSize;

                if (statIntervalChangeRequest->eventAck == VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->u32LastStatIntervalSize= pThis->u32StatIntervalSize;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ReportGuestStats:
        {
            Log(("VMMDevReq_ReportGuestStats\n"));
            if (pRequestHeader->size != sizeof(VMMDevReportGuestStats))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReportGuestStats *stats = (VMMDevReportGuestStats*)pRequestHeader;

#ifdef DEBUG
                VBoxGuestStatistics *pGuestStats = &stats->guestStats;

                Log(("Current statistics:\n"));
                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_IDLE)
                    Log(("CPU%d: CPU Load Idle          %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_Idle));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_KERNEL)
                    Log(("CPU%d: CPU Load Kernel        %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_Kernel));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_USER)
                    Log(("CPU%d: CPU Load User          %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_User));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_THREADS)
                    Log(("CPU%d: Thread                 %d\n", pGuestStats->u32CpuId, pGuestStats->u32Threads));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PROCESSES)
                    Log(("CPU%d: Processes              %d\n", pGuestStats->u32CpuId, pGuestStats->u32Processes));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_HANDLES)
                    Log(("CPU%d: Handles                %d\n", pGuestStats->u32CpuId, pGuestStats->u32Handles));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEMORY_LOAD)
                    Log(("CPU%d: Memory Load            %d%%\n", pGuestStats->u32CpuId, pGuestStats->u32MemoryLoad));

                /* Note that reported values are in pages; upper layers expect them in megabytes */
                Log(("CPU%d: Page size              %-4d bytes\n", pGuestStats->u32CpuId, pGuestStats->u32PageSize));
                Assert(pGuestStats->u32PageSize == 4096);

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_TOTAL)
                    Log(("CPU%d: Total physical memory  %-4d MB\n", pGuestStats->u32CpuId, (pGuestStats->u32PhysMemTotal + (_1M/_4K)-1) / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_AVAIL)
                    Log(("CPU%d: Free physical memory   %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PhysMemAvail / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_BALLOON)
                    Log(("CPU%d: Memory balloon size    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PhysMemBalloon / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_COMMIT_TOTAL)
                    Log(("CPU%d: Committed memory       %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemCommitTotal / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_TOTAL)
                    Log(("CPU%d: Total kernel memory    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelTotal / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_PAGED)
                    Log(("CPU%d: Paged kernel memory    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelPaged / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_NONPAGED)
                    Log(("CPU%d: Nonpaged kernel memory %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelNonPaged / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_SYSTEM_CACHE)
                    Log(("CPU%d: System cache size      %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemSystemCache / (_1M/_4K)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PAGE_FILE_SIZE)
                    Log(("CPU%d: Page file size         %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PageFileSize / (_1M/_4K)));
                Log(("Statistics end *******************\n"));
#endif

                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnReportStatistics(pThis->pDrv, &stats->guestStats);
            }
            break;
        }

        case VMMDevReq_QueryCredentials:
        {
            if (pRequestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_QueryCredentials request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)pRequestHeader;

                /* let's start by nulling out the data */
                memset(credentials->szUserName, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                memset(credentials->szPassword, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                memset(credentials->szDomain, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);

                /* should we return whether we got credentials for a logon? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_QUERYPRESENCE)
                {
                    if (   pThis->pCredentials->Logon.szUserName[0]
                        || pThis->pCredentials->Logon.szPassword[0]
                        || pThis->pCredentials->Logon.szDomain[0])
                    {
                        credentials->u32Flags |= VMMDEV_CREDENTIALS_PRESENT;
                    }
                    else
                    {
                        credentials->u32Flags &= ~VMMDEV_CREDENTIALS_PRESENT;
                    }
                }

                /* does the guest want to read logon credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_READ)
                {
                    if (pThis->pCredentials->Logon.szUserName[0])
                        strcpy(credentials->szUserName, pThis->pCredentials->Logon.szUserName);
                    if (pThis->pCredentials->Logon.szPassword[0])
                        strcpy(credentials->szPassword, pThis->pCredentials->Logon.szPassword);
                    if (pThis->pCredentials->Logon.szDomain[0])
                        strcpy(credentials->szDomain, pThis->pCredentials->Logon.szDomain);
                    if (!pThis->pCredentials->Logon.fAllowInteractiveLogon)
                        credentials->u32Flags |= VMMDEV_CREDENTIALS_NOLOCALLOGON;
                    else
                        credentials->u32Flags &= ~VMMDEV_CREDENTIALS_NOLOCALLOGON;
                }

                if (!pThis->fKeepCredentials)
                {
                    /* does the caller want us to destroy the logon credentials? */
                    if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEAR)
                    {
                        memset(pThis->pCredentials->Logon.szUserName, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                        memset(pThis->pCredentials->Logon.szPassword, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                        memset(pThis->pCredentials->Logon.szDomain, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                    }
                }

                /* does the guest want to read credentials for verification? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_READJUDGE)
                {
                    if (pThis->pCredentials->Judge.szUserName[0])
                        strcpy(credentials->szUserName, pThis->pCredentials->Judge.szUserName);
                    if (pThis->pCredentials->Judge.szPassword[0])
                        strcpy(credentials->szPassword, pThis->pCredentials->Judge.szPassword);
                    if (pThis->pCredentials->Judge.szDomain[0])
                        strcpy(credentials->szDomain, pThis->pCredentials->Judge.szDomain);
                }

                /* does the caller want us to destroy the judgement credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEARJUDGE)
                {
                    memset(pThis->pCredentials->Judge.szUserName, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                    memset(pThis->pCredentials->Judge.szPassword, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                    memset(pThis->pCredentials->Judge.szDomain, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ReportCredentialsJudgement:
        {
            if (pRequestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_ReportCredentialsJudgement request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)pRequestHeader;

                /* what does the guest think about the credentials? (note: the order is important here!) */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_DENY)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_DENY);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_OK)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_OK);
                }
                else
                    Log(("VMMDevReq_ReportCredentialsJudgement: invalid flags: %d!!!\n", credentials->u32Flags));

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Implemented in 3.1.0.
         *
         * Note! The ring-0 VBoxGuestLib uses this to check whether
         *       VMMDevHGCMParmType_PageList is supported.
         */
        case VMMDevReq_GetHostVersion:
        {
            AssertMsgBreakStmt(pRequestHeader->size == sizeof(VMMDevReqHostVersion),
                               ("%#x < %#x\n", pRequestHeader->size, sizeof(VMMDevReqLogString)),
                               pRequestHeader->rc = VERR_INVALID_PARAMETER);
            VMMDevReqHostVersion *pReqHostVer = (VMMDevReqHostVersion*)pRequestHeader;
            pReqHostVer->major = RTBldCfgVersionMajor();
            pReqHostVer->minor = RTBldCfgVersionMinor();
            pReqHostVer->build = RTBldCfgVersionBuild();
            pReqHostVer->revision = RTBldCfgRevision();
            pReqHostVer->features = VMMDEV_HVF_HGCM_PHYS_PAGE_LIST;
            pReqHostVer->header.rc = VINF_SUCCESS;
            break;
        }

        case VMMDevReq_GetCpuHotPlugRequest:
        {
            VMMDevGetCpuHotPlugRequest *pReqCpuHotPlug = (VMMDevGetCpuHotPlugRequest *)pRequestHeader;

            if (pRequestHeader->size != sizeof(VMMDevGetCpuHotPlugRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                pReqCpuHotPlug->enmEventType = pThis->enmCpuHotPlugEvent;
                pReqCpuHotPlug->idCpuCore    = pThis->idCpuCore;
                pReqCpuHotPlug->idCpuPackage = pThis->idCpuPackage;
                pReqCpuHotPlug->header.rc    = VINF_SUCCESS;

                /* Clear the event */
                pThis->enmCpuHotPlugEvent = VMMDevCpuEventType_None;
                pThis->idCpuCore          = UINT32_MAX;
                pThis->idCpuPackage       = UINT32_MAX;
            }
            break;
        }

        case VMMDevReq_SetCpuHotPlugStatus:
        {
            VMMDevCpuHotPlugStatusRequest *pReqCpuHotPlugStatus = (VMMDevCpuHotPlugStatusRequest *)pRequestHeader;

            if (pRequestHeader->size != sizeof(VMMDevCpuHotPlugStatusRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                pRequestHeader->rc = VINF_SUCCESS;

                if (pReqCpuHotPlugStatus->enmStatusType == VMMDevCpuStatusType_Disable)
                    pThis->fCpuHotPlugEventsEnabled = false;
                else if (pReqCpuHotPlugStatus->enmStatusType == VMMDevCpuStatusType_Enable)
                    pThis->fCpuHotPlugEventsEnabled = true;
                else
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            break;
        }

#ifdef VBOX_WITH_PAGE_SHARING
        case VMMDevReq_RegisterSharedModule:
            pRequestHeader->rc = vmmdevReqHandler_RegisterSharedModule(pDevIns,
                                     (VMMDevSharedModuleRegistrationRequest *)pRequestHeader);
            break;

        case VMMDevReq_UnregisterSharedModule:
            pRequestHeader->rc = vmmdevReqHandler_UnregisterSharedModule(pDevIns,
                                     (VMMDevSharedModuleUnregistrationRequest *)pRequestHeader);
            break;

        case VMMDevReq_CheckSharedModules:
            pRequestHeader->rc = vmmdevReqHandler_CheckSharedModules(pDevIns,
                                     (VMMDevSharedModuleCheckRequest *)pRequestHeader);
            break;

        case VMMDevReq_GetPageSharingStatus:
            pRequestHeader->rc = vmmdevReqHandler_GetPageSharingStatus(pThis,
                                     (VMMDevPageSharingStatusRequest *)pRequestHeader);
            break;

        case VMMDevReq_DebugIsPageShared:
            pRequestHeader->rc = vmmdevReqHandler_DebugIsPageShared(pDevIns, (VMMDevPageIsSharedRequest *)pRequestHeader);
            break;

#endif /* VBOX_WITH_PAGE_SHARING */

#ifdef DEBUG
        case VMMDevReq_LogString:
        {
            if (pRequestHeader->size < sizeof(VMMDevReqLogString))
            {
                AssertMsgFailed(("VMMDevReq_LogString request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqLogString *pReqLogString = (VMMDevReqLogString *)pRequestHeader;
                LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_1, LOG_GROUP_DEV_VMM_BACKDOOR,
                      ("DEBUG LOG: %s", pReqLogString->szString));
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }
#endif

        /*
         * Get a unique session id for this VM; the id will be different after each start, reset or restore of the VM
         * This can be used for restore detection inside the guest.
         */
        case VMMDevReq_GetSessionId:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqSessionId))
            {
                AssertMsgFailed(("VMMDevReq_GetSessionId request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqSessionId *pReq = (VMMDevReqSessionId *)pRequestHeader;
                pReq->idSession = pThis->idSession;
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        default:
        {
            pRequestHeader->rc = VERR_NOT_IMPLEMENTED;
            Log(("VMMDev unknown request type %d\n", pRequestHeader->requestType));
            break;
        }
    }

l_end:
    /* Write the result back to guest memory */
    if (pRequestHeader)
    {
        PDMDevHlpPhysWrite(pDevIns, (RTGCPHYS)u32, pRequestHeader, pRequestHeader->size);
        RTMemFree(pRequestHeader);
    }
    else
    {
        /* early error case; write back header only */
        PDMDevHlpPhysWrite(pDevIns, (RTGCPHYS)u32, &requestHeader, sizeof(requestHeader));
    }

    PDMCritSectLeave(&pThis->CritSect);
    return rcRet;
}


/* -=-=-=-=-=- PCI Device -=-=-=-=-=- */


/**
 * @interface_method_impl{FNPCIIOREGIONMAP, MMIO/MMIO2 regions}
 */
static DECLCALLBACK(int)
vmmdevIORAMRegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    LogFlow(("vmmdevR3IORAMRegionMap: iRegion=%d GCPhysAddress=%RGp cb=%#x enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));
    PVMMDEV pThis = RT_FROM_MEMBER(pPciDev, VMMDEV, PciDev);
    int rc;

    if (iRegion == 1)
    {
        AssertReturn(enmType == PCI_ADDRESS_SPACE_MEM, VERR_INTERNAL_ERROR);
        Assert(pThis->pVMMDevRAMR3 != NULL);
        if (GCPhysAddress != NIL_RTGCPHYS)
        {
            /*
             * Map the MMIO2 memory.
             */
            pThis->GCPhysVMMDevRAM = GCPhysAddress;
            Assert(pThis->GCPhysVMMDevRAM == GCPhysAddress);
            rc = PDMDevHlpMMIO2Map(pPciDev->pDevIns, iRegion, GCPhysAddress);
        }
        else
        {
            /*
             * It is about to be unmapped, just clean up.
             */
            pThis->GCPhysVMMDevRAM = NIL_RTGCPHYS32;
            rc = VINF_SUCCESS;
        }
    }
    else if (iRegion == 2)
    {
        AssertReturn(enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH, VERR_INTERNAL_ERROR);
        Assert(pThis->pVMMDevHeapR3 != NULL);
        if (GCPhysAddress != NIL_RTGCPHYS)
        {
            /*
             * Map the MMIO2 memory.
             */
            pThis->GCPhysVMMDevHeap = GCPhysAddress;
            Assert(pThis->GCPhysVMMDevHeap == GCPhysAddress);
            rc = PDMDevHlpMMIO2Map(pPciDev->pDevIns, iRegion, GCPhysAddress);
            if (RT_SUCCESS(rc))
                rc = PDMDevHlpRegisterVMMDevHeap(pPciDev->pDevIns, GCPhysAddress, pThis->pVMMDevHeapR3, VMMDEV_HEAP_SIZE);
        }
        else
        {
            /*
             * It is about to be unmapped, just clean up.
             */
            PDMDevHlpUnregisterVMMDevHeap(pPciDev->pDevIns, pThis->GCPhysVMMDevHeap);
            pThis->GCPhysVMMDevHeap = NIL_RTGCPHYS32;
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        AssertMsgFailed(("%d\n", iRegion));
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}


/**
 * @interface_method_impl{FNPCIIOREGIONMAP, I/O Port Region}
 */
static DECLCALLBACK(int)
vmmdevIOPortRegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pPciDev, VMMDEV, PciDev);

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(iRegion == 0);
    AssertMsg(RT_ALIGN(GCPhysAddress, 8) == GCPhysAddress, ("Expected 8 byte alignment. GCPhysAddress=%#x\n", GCPhysAddress));

    /*
     * Register our port IO handlers.
     */
    int rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, (RTIOPORT)GCPhysAddress + VMMDEV_PORT_OFF_REQUEST, 1,
                                     pThis, vmmdevRequestHandler, NULL, NULL, NULL, "VMMDev Request Handler");
    AssertRC(rc);
    return rc;
}


/* -=-=-=-=-=- IBase -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) vmmdevPortQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIVMMDEVPORT, &pThis->IPort);
#ifdef VBOX_WITH_HGCM
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHGCMPORT, &pThis->IHGCMPort);
#endif
    /* Currently only for shared folders. */
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThis->SharedFolders.ILeds);
    return NULL;
}


/* -=-=-=-=-=- ILeds -=-=-=-=-=- */

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) vmmdevQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, SharedFolders.ILeds);
    if (iLUN == 0) /* LUN 0 is shared folders */
    {
        *ppLed = &pThis->SharedFolders.Led;
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/* -=-=-=-=-=- PDMIVMMDEVPORT (VMMDEV::IPort) -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnQueryAbsoluteMouse}
 */
static DECLCALLBACK(int) vmmdevIPort_QueryAbsoluteMouse(PPDMIVMMDEVPORT pInterface, int32_t *pxAbs, int32_t *pyAbs)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);

    /** @todo at the first sign of trouble in this area, just enter the critsect.
     * As indicated by the comment below, the atomic reads serves no real purpose
     * here since we can assume cache coherency protocoles and int32_t alignment
     * rules making sure we won't see a halfwritten value. */
    if (pxAbs)
        *pxAbs = ASMAtomicReadS32(&pThis->mouseXAbs); /* why the atomic read? */
    if (pyAbs)
        *pyAbs = ASMAtomicReadS32(&pThis->mouseYAbs);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnSetAbsoluteMouse}
 */
static DECLCALLBACK(int) vmmdevIPort_SetAbsoluteMouse(PPDMIVMMDEVPORT pInterface, int32_t xAbs, int32_t yAbs)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    if (   pThis->mouseXAbs != xAbs
        || pThis->mouseYAbs != yAbs)
    {
        Log2(("vmmdevIPort_SetAbsoluteMouse : settings absolute position to x = %d, y = %d\n", xAbs, yAbs));
        pThis->mouseXAbs = xAbs;
        pThis->mouseYAbs = yAbs;
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnQueryMouseCapabilities}
 */
static DECLCALLBACK(int) vmmdevIPort_QueryMouseCapabilities(PPDMIVMMDEVPORT pInterface, uint32_t *pfCapabilities)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    AssertPtrReturn(pfCapabilities, VERR_INVALID_PARAMETER);

    *pfCapabilities = pThis->mouseCapabilities;
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnUpdateMouseCapabilities}
 */
static DECLCALLBACK(int)
vmmdevIPort_UpdateMouseCapabilities(PPDMIVMMDEVPORT pInterface, uint32_t fCapsAdded, uint32_t fCapsRemoved)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    uint32_t fOldCaps = pThis->mouseCapabilities;
    pThis->mouseCapabilities &= ~(fCapsRemoved & VMMDEV_MOUSE_HOST_MASK);
    pThis->mouseCapabilities |= (fCapsAdded & VMMDEV_MOUSE_HOST_MASK)
                              | VMMDEV_MOUSE_HOST_RECHECKS_NEEDS_HOST_CURSOR;
    bool fNotify = fOldCaps != pThis->mouseCapabilities;

    LogRelFlowFunc(("fCapsAdded=0x%x, fCapsRemoved=0x%x, fNotify=%RTbool\n", fCapsAdded, fCapsRemoved, fNotify));

    if (fNotify)
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnRequestDisplayChange}
 */
static DECLCALLBACK(int)
vmmdevIPort_RequestDisplayChange(PPDMIVMMDEVPORT pInterface, uint32_t cx, uint32_t cy, uint32_t cBits, uint32_t idxDisplay,
                                 uint32_t xOrigin, uint32_t yOrigin, bool fEnabled, bool fChangeOrigin)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);

    if (idxDisplay >= RT_ELEMENTS(pThis->displayChangeData.aRequests))
        return VERR_INVALID_PARAMETER;

    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    DISPLAYCHANGEREQUEST *pRequest = &pThis->displayChangeData.aRequests[idxDisplay];

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSameResolution = (!cx    || pRequest->lastReadDisplayChangeRequest.xres == cx)
                        && (!cy    || pRequest->lastReadDisplayChangeRequest.yres == cy)
                        && (!cBits || pRequest->lastReadDisplayChangeRequest.bpp  == cBits)
                        && pRequest->lastReadDisplayChangeRequest.xOrigin  == xOrigin
                        && pRequest->lastReadDisplayChangeRequest.yOrigin  == yOrigin
                        && pRequest->lastReadDisplayChangeRequest.fEnabled == fEnabled
                        && pRequest->lastReadDisplayChangeRequest.display  == idxDisplay;

    if (!cx && !cy && !cBits)
    {
        /* Special case of reset video mode. */
        fSameResolution = false;
    }

    Log3(("vmmdevIPort_RequestDisplayChange: same=%d. new: cx=%d, cy=%d, cBits=%d, idxDisplay=%d.\
          old: cx=%d, cy=%d, cBits=%d, idxDisplay=%d. \n \
          ,OriginX = %d , OriginY=%d, Enabled=%d, ChangeOrigin=%d\n",
          fSameResolution, cx, cy, cBits, idxDisplay, pRequest->lastReadDisplayChangeRequest.xres,
          pRequest->lastReadDisplayChangeRequest.yres, pRequest->lastReadDisplayChangeRequest.bpp,
          pRequest->lastReadDisplayChangeRequest.display,
          xOrigin, yOrigin, fEnabled, fChangeOrigin));

    if (!fSameResolution)
    {
        LogRel(("VMMDev::SetVideoModeHint: got a video mode hint (%dx%dx%d) at %d\n", cx, cy, cBits, idxDisplay));

        /* we could validate the information here but hey, the guest can do that as well! */
        pRequest->displayChangeRequest.xres          = cx;
        pRequest->displayChangeRequest.yres          = cy;
        pRequest->displayChangeRequest.bpp           = cBits;
        pRequest->displayChangeRequest.display       = idxDisplay;
        pRequest->displayChangeRequest.xOrigin       = xOrigin;
        pRequest->displayChangeRequest.yOrigin       = yOrigin;
        pRequest->displayChangeRequest.fEnabled      = fEnabled;
        pRequest->displayChangeRequest.fChangeOrigin = fChangeOrigin;

        pRequest->fPending = true;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnRequestSeamlessChange}
 */
static DECLCALLBACK(int) vmmdevIPort_RequestSeamlessChange(PPDMIVMMDEVPORT pInterface, bool fEnabled)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSameMode = (pThis->fLastSeamlessEnabled == fEnabled);

    Log(("vmmdevIPort_RequestSeamlessChange: same=%d. new=%d\n", fSameMode, fEnabled));

    if (!fSameMode)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->fSeamlessEnabled = fEnabled;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnSetMemoryBalloon}
 */
static DECLCALLBACK(int) vmmdevIPort_SetMemoryBalloon(PPDMIVMMDEVPORT pInterface, uint32_t cMbBalloon)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    Log(("vmmdevIPort_SetMemoryBalloon: old=%u new=%u\n", pThis->cMbMemoryBalloonLast, cMbBalloon));
    if (pThis->cMbMemoryBalloonLast != cMbBalloon)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->cMbMemoryBalloon = cMbBalloon;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_BALLOON_CHANGE_REQUEST);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnVRDPChange}
 */
static DECLCALLBACK(int) vmmdevIPort_VRDPChange(PPDMIVMMDEVPORT pInterface, bool fVRDPEnabled, uint32_t uVRDPExperienceLevel)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    bool fSame = (pThis->fVRDPEnabled == fVRDPEnabled);

    Log(("vmmdevIPort_VRDPChange: old=%d. new=%d\n", pThis->fVRDPEnabled, fVRDPEnabled));

    if (!fSame)
    {
        pThis->fVRDPEnabled = fVRDPEnabled;
        pThis->uVRDPExperienceLevel = uVRDPExperienceLevel;

        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_VRDP);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnSetStatisticsInterval}
 */
static DECLCALLBACK(int) vmmdevIPort_SetStatisticsInterval(PPDMIVMMDEVPORT pInterface, uint32_t cSecsStatInterval)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSame = (pThis->u32LastStatIntervalSize == cSecsStatInterval);

    Log(("vmmdevIPort_SetStatisticsInterval: old=%d. new=%d\n", pThis->u32LastStatIntervalSize, cSecsStatInterval));

    if (!fSame)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->u32StatIntervalSize = cSecsStatInterval;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnSetCredentials}
 */
static DECLCALLBACK(int) vmmdevIPort_SetCredentials(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                                    const char *pszPassword, const char *pszDomain, uint32_t fFlags)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    AssertReturn(fFlags & (VMMDEV_SETCREDENTIALS_GUESTLOGON | VMMDEV_SETCREDENTIALS_JUDGE), VERR_INVALID_PARAMETER);

    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    /*
     * Logon mode
     */
    if (fFlags & VMMDEV_SETCREDENTIALS_GUESTLOGON)
    {
        /* memorize the data */
        strcpy(pThis->pCredentials->Logon.szUserName, pszUsername);
        strcpy(pThis->pCredentials->Logon.szPassword, pszPassword);
        strcpy(pThis->pCredentials->Logon.szDomain,   pszDomain);
        pThis->pCredentials->Logon.fAllowInteractiveLogon = !(fFlags & VMMDEV_SETCREDENTIALS_NOLOCALLOGON);
    }
    /*
     * Credentials verification mode?
     */
    else
    {
        /* memorize the data */
        strcpy(pThis->pCredentials->Judge.szUserName, pszUsername);
        strcpy(pThis->pCredentials->Judge.szPassword, pszPassword);
        strcpy(pThis->pCredentials->Judge.szDomain,   pszDomain);

        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_JUDGE_CREDENTIALS);
    }

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnVBVAChange}
 *
 * Notification from the Display.  Especially useful when acceleration is
 * disabled after a video mode change.
 */
static DECLCALLBACK(void) vmmdevIPort_VBVAChange(PPDMIVMMDEVPORT pInterface, bool fEnabled)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    Log(("vmmdevIPort_VBVAChange: fEnabled = %d\n", fEnabled));

    /* Only used by saved state, which I guess is why we don't bother with locking here. */
    pThis->u32VideoAccelEnabled = fEnabled;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnCpuHotUnplug}
 */
static DECLCALLBACK(int) vmmdevIPort_CpuHotUnplug(PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    int     rc    = VINF_SUCCESS;

    Log(("vmmdevIPort_CpuHotUnplug: idCpuCore=%u idCpuPackage=%u\n", idCpuCore, idCpuPackage));

    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    if (pThis->fCpuHotPlugEventsEnabled)
    {
        pThis->enmCpuHotPlugEvent = VMMDevCpuEventType_Unplug;
        pThis->idCpuCore          = idCpuCore;
        pThis->idCpuPackage       = idCpuPackage;
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_CPU_HOTPLUG);
    }
    else
        rc = VERR_CPU_HOTPLUG_NOT_MONITORED_BY_GUEST;

    PDMCritSectLeave(&pThis->CritSect);
    return rc;
}

/**
 * @interface_method_impl{PDMIVMMDEVPORT, pfnCpuHotPlug}
 */
static DECLCALLBACK(int) vmmdevIPort_CpuHotPlug(PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage)
{
    PVMMDEV pThis = RT_FROM_MEMBER(pInterface, VMMDEV, IPort);
    int     rc    = VINF_SUCCESS;

    Log(("vmmdevCpuPlug: idCpuCore=%u idCpuPackage=%u\n", idCpuCore, idCpuPackage));

    PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);

    if (pThis->fCpuHotPlugEventsEnabled)
    {
        pThis->enmCpuHotPlugEvent = VMMDevCpuEventType_Plug;
        pThis->idCpuCore          = idCpuCore;
        pThis->idCpuPackage       = idCpuPackage;
        VMMDevNotifyGuest(pThis, VMMDEV_EVENT_CPU_HOTPLUG);
    }
    else
        rc = VERR_CPU_HOTPLUG_NOT_MONITORED_BY_GUEST;

    PDMCritSectLeave(&pThis->CritSect);
    return rc;
}


/* -=-=-=-=-=- Saved State -=-=-=-=-=- */

/**
 * @callback_method_impl{NSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) vmmdevLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);

    SSMR3PutBool(pSSM, pThis->fGetHostTimeDisabled);
    SSMR3PutBool(pSSM, pThis->fBackdoorLogDisabled);
    SSMR3PutBool(pSSM, pThis->fKeepCredentials);
    SSMR3PutBool(pSSM, pThis->fHeapEnabled);

    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) vmmdevSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);

    vmmdevLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    SSMR3PutU32(pSSM, pThis->hypervisorSize);
    SSMR3PutU32(pSSM, pThis->mouseCapabilities);
    SSMR3PutS32(pSSM, pThis->mouseXAbs);
    SSMR3PutS32(pSSM, pThis->mouseYAbs);

    SSMR3PutBool(pSSM, pThis->fNewGuestFilterMask);
    SSMR3PutU32(pSSM, pThis->u32NewGuestFilterMask);
    SSMR3PutU32(pSSM, pThis->u32GuestFilterMask);
    SSMR3PutU32(pSSM, pThis->u32HostEventFlags);
    /* The following is not strictly necessary as PGM restores MMIO2, keeping it for historical reasons. */
    SSMR3PutMem(pSSM, &pThis->pVMMDevRAMR3->V, sizeof(pThis->pVMMDevRAMR3->V));

    SSMR3PutMem(pSSM, &pThis->guestInfo, sizeof(pThis->guestInfo));
    SSMR3PutU32(pSSM, pThis->fu32AdditionsOk);
    SSMR3PutU32(pSSM, pThis->u32VideoAccelEnabled);
    SSMR3PutBool(pSSM, pThis->displayChangeData.fGuestSentChangeEventAck);

    SSMR3PutU32(pSSM, pThis->guestCaps);

#ifdef VBOX_WITH_HGCM
    vmmdevHGCMSaveState(pThis, pSSM);
#endif /* VBOX_WITH_HGCM */

    SSMR3PutU32(pSSM, pThis->fHostCursorRequested);

    SSMR3PutU32(pSSM, pThis->guestInfo2.uFullVersion);
    SSMR3PutU32(pSSM, pThis->guestInfo2.uRevision);
    SSMR3PutU32(pSSM, pThis->guestInfo2.fFeatures);
    SSMR3PutStrZ(pSSM, pThis->guestInfo2.szName);
    SSMR3PutU32(pSSM, pThis->cFacilityStatuses);
    for (uint32_t i = 0; i < pThis->cFacilityStatuses; i++)
    {
        SSMR3PutU32(pSSM, pThis->aFacilityStatuses[i].uFacility);
        SSMR3PutU32(pSSM, pThis->aFacilityStatuses[i].fFlags);
        SSMR3PutU16(pSSM, pThis->aFacilityStatuses[i].uStatus);
        SSMR3PutS64(pSSM, RTTimeSpecGetNano(&pThis->aFacilityStatuses[i].TimeSpecTS));
    }

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) vmmdevLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /** @todo The code load code is assuming we're always loaded into a freshly
     *        constructed VM. */
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);
    int     rc;

    if (   uVersion > VMMDEV_SAVED_STATE_VERSION
        || uVersion < 6)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* config */
    if (uVersion > VMMDEV_SAVED_STATE_VERSION_VBOX_30)
    {
        bool f;
        rc = SSMR3GetBool(pSSM, &f); AssertRCReturn(rc, rc);
        if (pThis->fGetHostTimeDisabled != f)
            LogRel(("VMMDev: Config mismatch - fGetHostTimeDisabled: config=%RTbool saved=%RTbool\n", pThis->fGetHostTimeDisabled, f));

        rc = SSMR3GetBool(pSSM, &f); AssertRCReturn(rc, rc);
        if (pThis->fBackdoorLogDisabled != f)
            LogRel(("VMMDev: Config mismatch - fBackdoorLogDisabled: config=%RTbool saved=%RTbool\n", pThis->fBackdoorLogDisabled, f));

        rc = SSMR3GetBool(pSSM, &f); AssertRCReturn(rc, rc);
        if (pThis->fKeepCredentials != f)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - fKeepCredentials: config=%RTbool saved=%RTbool"),
                                     pThis->fKeepCredentials, f);
        rc = SSMR3GetBool(pSSM, &f); AssertRCReturn(rc, rc);
        if (pThis->fHeapEnabled != f)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - fHeapEnabled: config=%RTbool saved=%RTbool"),
                                    pThis->fHeapEnabled, f);
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* state */
    SSMR3GetU32(pSSM, &pThis->hypervisorSize);
    SSMR3GetU32(pSSM, &pThis->mouseCapabilities);
    SSMR3GetS32(pSSM, &pThis->mouseXAbs);
    SSMR3GetS32(pSSM, &pThis->mouseYAbs);

    SSMR3GetBool(pSSM, &pThis->fNewGuestFilterMask);
    SSMR3GetU32(pSSM, &pThis->u32NewGuestFilterMask);
    SSMR3GetU32(pSSM, &pThis->u32GuestFilterMask);
    SSMR3GetU32(pSSM, &pThis->u32HostEventFlags);

//    SSMR3GetBool(pSSM, &pThis->pVMMDevRAMR3->fHaveEvents);
    // here be dragons (probably)
    SSMR3GetMem(pSSM, &pThis->pVMMDevRAMR3->V, sizeof (pThis->pVMMDevRAMR3->V));

    SSMR3GetMem(pSSM, &pThis->guestInfo, sizeof (pThis->guestInfo));
    SSMR3GetU32(pSSM, &pThis->fu32AdditionsOk);
    SSMR3GetU32(pSSM, &pThis->u32VideoAccelEnabled);
    if (uVersion > 10)
        SSMR3GetBool(pSSM, &pThis->displayChangeData.fGuestSentChangeEventAck);

    rc = SSMR3GetU32(pSSM, &pThis->guestCaps);

    /* Attributes which were temporarily introduced in r30072 */
    if (uVersion == 7)
    {
        uint32_t temp;
        SSMR3GetU32(pSSM, &temp);
        rc = SSMR3GetU32(pSSM, &temp);
    }
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_HGCM
    rc = vmmdevHGCMLoadState(pThis, pSSM, uVersion);
    AssertRCReturn(rc, rc);
#endif /* VBOX_WITH_HGCM */

    if (uVersion >= 10)
        rc = SSMR3GetU32(pSSM, &pThis->fHostCursorRequested);
    AssertRCReturn(rc, rc);

    if (uVersion > VMMDEV_SAVED_STATE_VERSION_MISSING_GUEST_INFO_2)
    {
        SSMR3GetU32(pSSM, &pThis->guestInfo2.uFullVersion);
        SSMR3GetU32(pSSM, &pThis->guestInfo2.uRevision);
        SSMR3GetU32(pSSM, &pThis->guestInfo2.fFeatures);
        rc = SSMR3GetStrZ(pSSM, &pThis->guestInfo2.szName[0], sizeof(pThis->guestInfo2.szName));
        AssertRCReturn(rc, rc);
    }

    if (uVersion > VMMDEV_SAVED_STATE_VERSION_MISSING_FACILITY_STATUSES)
    {
        uint32_t cFacilityStatuses;
        rc = SSMR3GetU32(pSSM, &cFacilityStatuses);
        AssertRCReturn(rc, rc);

        for (uint32_t i = 0; i < cFacilityStatuses; i++)
        {
            uint32_t uFacility, fFlags;
            uint16_t uStatus;
            int64_t  iTimeStampNano;

            SSMR3GetU32(pSSM, &uFacility);
            SSMR3GetU32(pSSM, &fFlags);
            SSMR3GetU16(pSSM, &uStatus);
            rc = SSMR3GetS64(pSSM, &iTimeStampNano);
            AssertRCReturn(rc, rc);

            PVMMDEVFACILITYSTATUSENTRY pEntry = vmmdevGetFacilityStatusEntry(pThis, uFacility);
            AssertLogRelMsgReturn(pEntry,
                                  ("VMMDev: Ran out of entries restoring the guest facility statuses. Saved state has %u.\n", cFacilityStatuses),
                                  VERR_OUT_OF_RESOURCES);
            pEntry->uStatus = uStatus;
            pEntry->fFlags  = fFlags;
            RTTimeSpecSetNano(&pEntry->TimeSpecTS, iTimeStampNano);
        }
    }


    /*
     * On a resume, we send the capabilities changed message so
     * that listeners can sync their state again
     */
    Log(("vmmdevLoadState: capabilities changed (%x), informing connector\n", pThis->mouseCapabilities));
    if (pThis->pDrv)
    {
        pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);
        if (uVersion >= 10)
            pThis->pDrv->pfnUpdatePointerShape(pThis->pDrv,
                                               /*fVisible=*/!!pThis->fHostCursorRequested,
                                               /*fAlpha=*/false,
                                               /*xHot=*/0, /*yHot=*/0,
                                               /*cx=*/0, /*cy=*/0,
                                               /*pvShape=*/NULL);
    }

    /* Reestablish the acceleration status. */
    if (    pThis->u32VideoAccelEnabled
        &&  pThis->pDrv)
    {
        pThis->pDrv->pfnVideoAccelEnable(pThis->pDrv, !!pThis->u32VideoAccelEnabled, &pThis->pVMMDevRAMR3->vbvaMemory);
    }

    if (pThis->fu32AdditionsOk)
    {
        LogRel(("Guest Additions information report: additionsVersion = 0x%08X, osType = 0x%08X\n",
                pThis->guestInfo.interfaceVersion, pThis->guestInfo.osType));
        if (pThis->pDrv)
        {
            if (pThis->guestInfo2.uFullVersion && pThis->pDrv->pfnUpdateGuestInfo2)
                pThis->pDrv->pfnUpdateGuestInfo2(pThis->pDrv, pThis->guestInfo2.uFullVersion, pThis->guestInfo2.szName,
                                                 pThis->guestInfo2.uRevision, pThis->guestInfo2.fFeatures);
            if (pThis->pDrv->pfnUpdateGuestInfo)
                pThis->pDrv->pfnUpdateGuestInfo(pThis->pDrv, &pThis->guestInfo);

            if (pThis->pDrv->pfnUpdateGuestStatus)
            {
                for (uint32_t i = 0; i < pThis->cFacilityStatuses; i++) /* ascending order! */
                    if (   pThis->aFacilityStatuses[i].uStatus != VBoxGuestFacilityStatus_Inactive
                        || !pThis->aFacilityStatuses[i].fFixed)
                        pThis->pDrv->pfnUpdateGuestStatus(pThis->pDrv,
                                                          pThis->aFacilityStatuses[i].uFacility,
                                                          pThis->aFacilityStatuses[i].uStatus,
                                                          pThis->aFacilityStatuses[i].fFlags,
                                                          &pThis->aFacilityStatuses[i].TimeSpecTS);
            }
        }
    }
    if (pThis->pDrv && pThis->pDrv->pfnUpdateGuestCapabilities)
        pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);

    return VINF_SUCCESS;
}

/**
 * Load state done callback. Notify guest of restore event.
 *
 * @returns VBox status code.
 * @param   pDevIns    The device instance.
 * @param   pSSM The handle to the saved state.
 */
static DECLCALLBACK(int) vmmdevLoadStateDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);

#ifdef VBOX_WITH_HGCM
    int rc = vmmdevHGCMLoadStateDone(pThis, pSSM);
    AssertLogRelRCReturn(rc, rc);
#endif /* VBOX_WITH_HGCM */

    VMMDevNotifyGuest(pThis, VMMDEV_EVENT_RESTORED);

    return VINF_SUCCESS;
}


/* -=-=-=-=- PDMDEVREG -=-=-=-=- */

/**
 * (Re-)initializes the MMIO2 data.
 *
 * @param   pThis           Pointer to the VMMDev instance data.
 */
static void vmmdevInitRam(VMMDevState *pThis)
{
    memset(pThis->pVMMDevRAMR3, 0, sizeof(VMMDevMemory));
    pThis->pVMMDevRAMR3->u32Size = sizeof(VMMDevMemory);
    pThis->pVMMDevRAMR3->u32Version = VMMDEV_MEMORY_VERSION;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) vmmdevReset(PPDMDEVINS pDevIns)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);

    /*
     * Reset the mouse integration feature bits
     */
    if (pThis->mouseCapabilities & VMMDEV_MOUSE_GUEST_MASK)
    {
        pThis->mouseCapabilities &= ~VMMDEV_MOUSE_GUEST_MASK;
        /* notify the connector */
        Log(("vmmdevReset: capabilities changed (%x), informing connector\n", pThis->mouseCapabilities));
        pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);
    }
    pThis->fHostCursorRequested = false;

    pThis->hypervisorSize = 0;

    /* re-initialize the VMMDev memory */
    if (pThis->pVMMDevRAMR3)
        vmmdevInitRam(pThis);

    /* credentials have to go away (by default) */
    if (!pThis->fKeepCredentials)
    {
        memset(pThis->pCredentials->Logon.szUserName, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
        memset(pThis->pCredentials->Logon.szPassword, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
        memset(pThis->pCredentials->Logon.szDomain, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
    }
    memset(pThis->pCredentials->Judge.szUserName, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
    memset(pThis->pCredentials->Judge.szPassword, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);
    memset(pThis->pCredentials->Judge.szDomain, '\0', VMMDEV_CREDENTIALS_SZ_SIZE);

    /* Reset means that additions will report again. */
    const bool fVersionChanged = pThis->fu32AdditionsOk
                              || pThis->guestInfo.interfaceVersion
                              || pThis->guestInfo.osType != VBOXOSTYPE_Unknown;
    if (fVersionChanged)
        Log(("vmmdevReset: fu32AdditionsOk=%d additionsVersion=%x osType=%#x\n",
             pThis->fu32AdditionsOk, pThis->guestInfo.interfaceVersion, pThis->guestInfo.osType));
    pThis->fu32AdditionsOk = false;
    memset (&pThis->guestInfo, 0, sizeof (pThis->guestInfo));
    RT_ZERO(pThis->guestInfo2);

    /* Clear facilities. No need to tell Main as it will get a
       pfnUpdateGuestInfo callback. */
    RTTIMESPEC TimeStampNow;
    RTTimeNow(&TimeStampNow);
    uint32_t iFacility = pThis->cFacilityStatuses;
    while (iFacility-- > 0)
    {
        pThis->aFacilityStatuses[iFacility].uStatus    = VBoxGuestFacilityStatus_Inactive;
        pThis->aFacilityStatuses[iFacility].TimeSpecTS = TimeStampNow;
    }

    /* clear pending display change request. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->displayChangeData.aRequests); i++)
    {
        DISPLAYCHANGEREQUEST *pRequest = &pThis->displayChangeData.aRequests[i];
        memset (&pRequest->lastReadDisplayChangeRequest, 0, sizeof (pRequest->lastReadDisplayChangeRequest));
    }
    pThis->displayChangeData.iCurrentMonitor = 0;
    pThis->displayChangeData.fGuestSentChangeEventAck = false;

    /* disable seamless mode */
    pThis->fLastSeamlessEnabled = false;

    /* disabled memory ballooning */
    pThis->cMbMemoryBalloonLast = 0;

    /* disabled statistics updating */
    pThis->u32LastStatIntervalSize = 0;

    /* Clear the "HGCM event enabled" flag so the event can be automatically reenabled.  */
    pThis->u32HGCMEnabled = 0;

    /*
     * Clear the event variables.
     *
     * XXX By design we should NOT clear pThis->u32HostEventFlags because it is designed
     *     that way so host events do not depend on guest resets. However, the pending
     *     event flags actually _were_ cleared since ages so we mask out events from
     *     clearing which we really need to survive the reset. See xtracker 5767.
     */
    pThis->u32HostEventFlags    &= VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
    pThis->u32GuestFilterMask    = 0;
    pThis->u32NewGuestFilterMask = 0;
    pThis->fNewGuestFilterMask   = 0;

    /* This is the default, as Windows and OS/2 guests take this for granted. (Actually, neither does...) */
    /** @todo change this when we next bump the interface version */
    const bool fCapsChanged = pThis->guestCaps != VMMDEV_GUEST_SUPPORTS_GRAPHICS;
    if (fCapsChanged)
        Log(("vmmdevReset: fCapsChanged=%#x -> %#x\n", pThis->guestCaps, VMMDEV_GUEST_SUPPORTS_GRAPHICS));
    pThis->guestCaps = VMMDEV_GUEST_SUPPORTS_GRAPHICS; /** @todo r=bird: why? I cannot see this being done at construction?*/

    /*
     * Call the update functions as required.
     */
    if (fVersionChanged && pThis->pDrv && pThis->pDrv->pfnUpdateGuestInfo)
        pThis->pDrv->pfnUpdateGuestInfo(pThis->pDrv, &pThis->guestInfo);
    if (fCapsChanged    && pThis->pDrv && pThis->pDrv->pfnUpdateGuestCapabilities)
        pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);

    /* Generate a unique session id for this VM; it will be changed for each start, reset or restore.
     * This can be used for restore detection inside the guest.
     */
    pThis->idSession = ASMReadTSC();
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) vmmdevRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    NOREF(pDevIns);
    NOREF(offDelta);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) vmmdevDestroy(PPDMDEVINS pDevIns)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Wipe and free the credentials.
     */
    if (pThis->pCredentials)
    {
        RTMemWipeThoroughly(pThis->pCredentials, sizeof(*pThis->pCredentials), 10);
        RTMemFree(pThis->pCredentials);
        pThis->pCredentials = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) vmmdevConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PVMMDEV pThis = PDMINS_2_DATA(pDevIns, PVMMDEV);
    int rc;

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Initialize data (most of it anyway).
     */
    /* Save PDM device instance data for future reference. */
    pThis->pDevIns = pDevIns;

    /* PCI vendor, just a free bogus value */
    PCIDevSetVendorId(&pThis->PciDev, 0x80ee);
    /* device ID */
    PCIDevSetDeviceId(&pThis->PciDev, 0xcafe);
    /* class sub code (other type of system peripheral) */
    PCIDevSetClassSub(&pThis->PciDev, 0x80);
    /* class base code (base system peripheral) */
    PCIDevSetClassBase(&pThis->PciDev, 0x08);
    /* header type */
    PCIDevSetHeaderType(&pThis->PciDev, 0x00);
    /* interrupt on pin 0 */
    PCIDevSetInterruptPin(&pThis->PciDev, 0x01);

    RTTIMESPEC TimeStampNow;
    RTTimeNow(&TimeStampNow);
    vmmdevAllocFacilityStatusEntry(pThis, VBoxGuestFacilityType_VBoxGuestDriver, true /*fFixed*/, &TimeStampNow);
    vmmdevAllocFacilityStatusEntry(pThis, VBoxGuestFacilityType_VBoxService,     true /*fFixed*/, &TimeStampNow);
    vmmdevAllocFacilityStatusEntry(pThis, VBoxGuestFacilityType_VBoxTrayClient,  true /*fFixed*/, &TimeStampNow);
    vmmdevAllocFacilityStatusEntry(pThis, VBoxGuestFacilityType_Seamless,        true /*fFixed*/, &TimeStampNow);
    vmmdevAllocFacilityStatusEntry(pThis, VBoxGuestFacilityType_Graphics,        true /*fFixed*/, &TimeStampNow);
    Assert(pThis->cFacilityStatuses == 5);

    /*
     * Interfaces
     */
    /* IBase */
    pThis->IBase.pfnQueryInterface          = vmmdevPortQueryInterface;

    /* VMMDev port */
    pThis->IPort.pfnQueryAbsoluteMouse      = vmmdevIPort_QueryAbsoluteMouse;
    pThis->IPort.pfnSetAbsoluteMouse        = vmmdevIPort_SetAbsoluteMouse ;
    pThis->IPort.pfnQueryMouseCapabilities  = vmmdevIPort_QueryMouseCapabilities;
    pThis->IPort.pfnUpdateMouseCapabilities = vmmdevIPort_UpdateMouseCapabilities;
    pThis->IPort.pfnRequestDisplayChange    = vmmdevIPort_RequestDisplayChange;
    pThis->IPort.pfnSetCredentials          = vmmdevIPort_SetCredentials;
    pThis->IPort.pfnVBVAChange              = vmmdevIPort_VBVAChange;
    pThis->IPort.pfnRequestSeamlessChange   = vmmdevIPort_RequestSeamlessChange;
    pThis->IPort.pfnSetMemoryBalloon        = vmmdevIPort_SetMemoryBalloon;
    pThis->IPort.pfnSetStatisticsInterval   = vmmdevIPort_SetStatisticsInterval;
    pThis->IPort.pfnVRDPChange              = vmmdevIPort_VRDPChange;
    pThis->IPort.pfnCpuHotUnplug            = vmmdevIPort_CpuHotUnplug;
    pThis->IPort.pfnCpuHotPlug              = vmmdevIPort_CpuHotPlug;

    /* Shared folder LED */
    pThis->SharedFolders.Led.u32Magic       = PDMLED_MAGIC;
    pThis->SharedFolders.ILeds.pfnQueryStatusLed = vmmdevQueryStatusLed;

#ifdef VBOX_WITH_HGCM
    /* HGCM port */
    pThis->IHGCMPort.pfnCompleted           = hgcmCompleted;
#endif

    pThis->pCredentials = (VMMDEVCREDS *)RTMemAllocZ(sizeof(*pThis->pCredentials));
    if (!pThis->pCredentials)
        return VERR_NO_MEMORY;


    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns,
                                  "GetHostTimeDisabled|"
                                  "BackdoorLogDisabled|"
                                  "KeepCredentials|"
                                  "HeapEnabled|"
                                  "RamSize|"
                                  "RZEnabled|"
                                  "GuestCoreDumpEnabled|"
                                  "GuestCoreDumpDir|"
                                  "GuestCoreDumpCount|"
                                  "TestingEnabled"
                                  ,
                                  "");

    rc = CFGMR3QueryU64(pCfg, "RamSize", &pThis->cbGuestRAM);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"RamSize\" as a 64-bit unsigned integer"));

    rc = CFGMR3QueryBoolDef(pCfg, "GetHostTimeDisabled", &pThis->fGetHostTimeDisabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GetHostTimeDisabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "BackdoorLogDisabled", &pThis->fBackdoorLogDisabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"BackdoorLogDisabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "KeepCredentials", &pThis->fKeepCredentials, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"KeepCredentials\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "HeapEnabled", &pThis->fHeapEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"HeapEnabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "RZEnabled", &pThis->fRZEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"RZEnabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "GuestCoreDumpEnabled", &pThis->fGuestCoreDumpEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GuestCoreDumpEnabled\" as a boolean"));

    char *pszGuestCoreDumpDir = NULL;
    rc = CFGMR3QueryStringAllocDef(pCfg, "GuestCoreDumpDir", &pszGuestCoreDumpDir, "");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GuestCoreDumpDir\" as a string"));

    RTStrCopy(pThis->szGuestCoreDumpDir, sizeof(pThis->szGuestCoreDumpDir), pszGuestCoreDumpDir);
    MMR3HeapFree(pszGuestCoreDumpDir);

    rc = CFGMR3QueryU32Def(pCfg, "GuestCoreDumpCount", &pThis->cGuestCoreDumps, 3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GuestCoreDumpCount\" as a 32-bit unsigned integer"));

#ifndef VBOX_WITHOUT_TESTING_FEATURES
    rc = CFGMR3QueryBoolDef(pCfg, "TestingEnabled", &pThis->fTestingEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"TestingEnabled\" as a boolean"));
    /** @todo image-to-load-filename? */
#endif

    /*
     * Create the critical section for the device.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "VMMDev#%u", iInstance);
    AssertRCReturn(rc, rc);
    /* Later: pDevIns->pCritSectR3 = &pThis->CritSect; */

    /*
     * Register the backdoor logging port
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, RTLOG_DEBUG_PORT, 1, NULL, vmmdevBackdoorLog, NULL, NULL, NULL, "VMMDev backdoor logging");
    AssertRCReturn(rc, rc);

#ifdef TIMESYNC_BACKDOOR
    /*
     * Alternative timesync source (temporary!)
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x505, 1, NULL, vmmdevTimesyncBackdoorWrite, vmmdevTimesyncBackdoorRead, NULL, NULL, "VMMDev timesync backdoor");
    AssertRCReturn(rc, rc);
#endif

    /*
     * Allocate and initialize the MMIO2 memory.
     */
    rc = PDMDevHlpMMIO2Register(pDevIns, 1 /*iRegion*/, VMMDEV_RAM_SIZE, 0 /*fFlags*/, (void **)&pThis->pVMMDevRAMR3, "VMMDev");
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Failed to allocate %u bytes of memory for the VMM device"), VMMDEV_RAM_SIZE);
    vmmdevInitRam(pThis);

    if (pThis->fHeapEnabled)
    {
        rc = PDMDevHlpMMIO2Register(pDevIns, 2 /*iRegion*/, VMMDEV_HEAP_SIZE, 0 /*fFlags*/, (void **)&pThis->pVMMDevHeapR3, "VMMDev Heap");
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Failed to allocate %u bytes of memory for the VMM device heap"), PAGE_SIZE);
    }

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->PciDev.devfn != 32 || iInstance != 0)
        Log(("!!WARNING!!: pThis->PciDev.devfn=%d (ignore if testcase or no started by Main)\n", pThis->PciDev.devfn));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 0x20, PCI_ADDRESS_SPACE_IO, vmmdevIOPortRegionMap);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, VMMDEV_RAM_SIZE, PCI_ADDRESS_SPACE_MEM, vmmdevIORAMRegionMap);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->fHeapEnabled)
    {
        rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, VMMDEV_HEAP_SIZE, PCI_ADDRESS_SPACE_MEM_PREFETCH, vmmdevIORAMRegionMap);
        if (RT_FAILURE(rc))
            return rc;
    }

#ifndef VBOX_WITHOUT_TESTING_FEATURES
    /*
     * Initialize testing.
     */
    rc = vmmdevTestingInitialize(pDevIns);
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Get the corresponding connector interface
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "VMM Driver Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIVMMDEVCONNECTOR);
        AssertMsgReturn(pThis->pDrv, ("LUN #0 doesn't have a VMMDev connector interface!\n"), VERR_PDM_MISSING_INTERFACE);
#ifdef VBOX_WITH_HGCM
        pThis->pHGCMDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIHGCMCONNECTOR);
        if (!pThis->pHGCMDrv)
        {
            Log(("LUN #0 doesn't have a HGCM connector interface, HGCM is not supported. rc=%Rrc\n", rc));
            /* this is not actually an error, just means that there is no support for HGCM */
        }
#endif
        /* Query the initial balloon size. */
        AssertPtr(pThis->pDrv->pfnQueryBalloonSize);
        rc = pThis->pDrv->pfnQueryBalloonSize(pThis->pDrv, &pThis->cMbMemoryBalloon);
        AssertRC(rc);

        Log(("Initial balloon size %x\n", pThis->cMbMemoryBalloon));
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pReg->szName, pDevIns->iInstance));
        rc = VINF_SUCCESS;
    }
    else
        AssertMsgFailedReturn(("Failed to attach LUN #0! rc=%Rrc\n", rc), rc);

    /*
     * Attach status driver for shared folders (optional).
     */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->SharedFolders.pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Register saved state and init the HGCM CmdList critsect.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VMMDEV_SAVED_STATE_VERSION, sizeof(*pThis), NULL,
                                NULL, vmmdevLiveExec, NULL,
                                NULL, vmmdevSaveExec, NULL,
                                NULL, vmmdevLoadExec, vmmdevLoadStateDone);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_HGCM
    pThis->pHGCMCmdList = NULL;
    rc = RTCritSectInit(&pThis->critsectHGCMCmdList);
    AssertRCReturn(rc, rc);
    pThis->u32HGCMEnabled = 0;
#endif /* VBOX_WITH_HGCM */

    /*
     * In this version of VirtualBox the GUI checks whether "needs host cursor"
     * changes.
     */
    pThis->mouseCapabilities |= VMMDEV_MOUSE_HOST_RECHECKS_NEEDS_HOST_CURSOR;

    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatMemBalloonChunks, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Memory balloon size", "/Devices/VMMDev/BalloonChunks");

    /*
     * Generate a unique session id for this VM; it will be changed for each
     * start, reset or restore. This can be used for restore detection inside
     * the guest.
     */
    pThis->idSession = ASMReadTSC();
    return rc;
}

/**
 * The device registration structure.
 */
extern "C" const PDMDEVREG g_DeviceVMMDev =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "VMMDev",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "VirtualBox VMM Device\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_VMM_DEV,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(VMMDevState),
    /* pfnConstruct */
    vmmdevConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    vmmdevRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    vmmdevReset,
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
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

