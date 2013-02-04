/* $Id$ */
/** @file
 * Virtual USB - Device.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_VUSB
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include "VUSBInternal.h"


/** Asserts that the give device state is valid. */
#define VUSBDEV_ASSERT_VALID_STATE(enmState) \
    AssertMsg((enmState) > VUSB_DEVICE_STATE_INVALID && (enmState) < VUSB_DEVICE_STATE_DESTROYED, ("enmState=%#x\n", enmState));


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Argument package of vusbDevResetThread().
 */
typedef struct vusb_reset_args
{
    /** Pointer to the device which is being reset. */
    PVUSBDEV            pDev;
    /** Can reset on linux. */
    bool                fResetOnLinux;
    /** The reset return code. */
    int                 rc;
    /** Pointer to the completion callback. */
    PFNVUSBRESETDONE    pfnDone;
    /** User argument to pfnDone. */
    void               *pvUser;
} VUSBRESETARGS, *PVUSBRESETARGS;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Default message pipe. */
const VUSBDESCENDPOINTEX g_Endpoint0 =
{
    {
        /* .bLength = */            VUSB_DT_ENDPOINT_MIN_LEN,
        /* .bDescriptorType = */    VUSB_DT_ENDPOINT,
        /* .bEndpointAddress = */   0,
        /* .bmAttributes = */       0,
        /* .wMaxPacketSize = */     64,
        /* .bInterval = */          0
    },
    NULL
};

/** Default configuration. */
const VUSBDESCCONFIGEX g_Config0 =
{
    {
        /* .bLength = */            VUSB_DT_CONFIG_MIN_LEN,
        /* .bDescriptorType = */    VUSB_DT_CONFIG,
        /* .WTotalLength = */       0, /* (auto-calculated) */
        /* .bNumInterfaces = */     0,
        /* .bConfigurationValue =*/ 0,
        /* .iConfiguration = */     0,
        /* .bmAttributes = */       0x80,
        /* .MaxPower = */           14
    },
    NULL,
    NULL
};



static PCVUSBDESCCONFIGEX vusbDevFindCfgDesc(PVUSBDEV pDev, int iCfg)
{
    if (iCfg == 0)
        return &g_Config0;

    for (unsigned i = 0; i < pDev->pDescCache->pDevice->bNumConfigurations; i++)
        if (pDev->pDescCache->paConfigs[i].Core.bConfigurationValue == iCfg)
            return &pDev->pDescCache->paConfigs[i];
    return NULL;
}

static PVUSBINTERFACESTATE vusbDevFindIfState(PVUSBDEV pDev, int iIf)
{
    for (unsigned i = 0; i < pDev->pCurCfgDesc->Core.bNumInterfaces; i++)
        if (pDev->paIfStates[i].pIf->paSettings[0].Core.bInterfaceNumber == iIf)
            return &pDev->paIfStates[i];
    return NULL;
}

static PCVUSBDESCINTERFACEEX vusbDevFindAltIfDesc(PVUSBDEV pDev, PCVUSBINTERFACESTATE pIfState, int iAlt)
{
    for (uint32_t i = 0; i < pIfState->pIf->cSettings; i++)
        if (pIfState->pIf->paSettings[i].Core.bAlternateSetting == iAlt)
            return &pIfState->pIf->paSettings[i];
    return NULL;
}

void vusbDevMapEndpoint(PVUSBDEV pDev, PCVUSBDESCENDPOINTEX pEndPtDesc)
{
    uint8_t i8Addr = pEndPtDesc->Core.bEndpointAddress & 0xF;
    PVUSBPIPE pPipe = &pDev->aPipes[i8Addr];
    LogFlow(("vusbDevMapEndpoint: pDev=%p[%s] pEndPtDesc=%p{.bEndpointAddress=%#x, .bmAttributes=%#x} p=%p stage %s->SETUP\n",
             pDev, pDev->pUsbIns->pszName, pEndPtDesc, pEndPtDesc->Core.bEndpointAddress, pEndPtDesc->Core.bmAttributes,
             pPipe, g_apszCtlStates[pPipe->pCtrl ? pPipe->pCtrl->enmStage : 3]));

    pPipe->ReadAheadThread = NIL_RTTHREAD;
    if ((pEndPtDesc->Core.bmAttributes & 0x3) == 0)
    {
        Log(("vusb: map message pipe on address %u\n", i8Addr));
        pPipe->in  = pEndPtDesc;
        pPipe->out = pEndPtDesc;
    }
    else if (pEndPtDesc->Core.bEndpointAddress & 0x80)
    {
        Log(("vusb: map input pipe on address %u\n", i8Addr));
        pPipe->in = pEndPtDesc;

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
        /*
         * For high-speed isochronous input endpoints, spin off a read-ahead buffering thread.
         */
        if ((pEndPtDesc->Core.bmAttributes & 0x03) == 1)
            vusbReadAheadStart(pDev, pPipe);
#endif
    }
    else
    {
        Log(("vusb: map output pipe on address %u\n", i8Addr));
        pPipe->out = pEndPtDesc;
    }

    if (pPipe->pCtrl)
    {
        vusbMsgFreeExtraData(pPipe->pCtrl);
        pPipe->pCtrl = NULL;
    }
}

static void unmap_endpoint(PVUSBDEV pDev, PCVUSBDESCENDPOINTEX pEndPtDesc)
{
    uint8_t     EndPt = pEndPtDesc->Core.bEndpointAddress & 0xF;
    PVUSBPIPE   pPipe = &pDev->aPipes[EndPt];
    LogFlow(("unmap_endpoint: pDev=%p[%s] pEndPtDesc=%p{.bEndpointAddress=%#x, .bmAttributes=%#x} p=%p stage %s->SETUP\n",
             pDev, pDev->pUsbIns->pszName, pEndPtDesc, pEndPtDesc->Core.bEndpointAddress, pEndPtDesc->Core.bmAttributes,
             pPipe, g_apszCtlStates[pPipe->pCtrl ? pPipe->pCtrl->enmStage : 3]));

    if ((pEndPtDesc->Core.bmAttributes & 0x3) == 0)
    {
        Log(("vusb: unmap MSG pipe from address %u (%#x)\n", EndPt, pEndPtDesc->Core.bEndpointAddress));
        pPipe->in = NULL;
        pPipe->out = NULL;
    }
    else if (pEndPtDesc->Core.bEndpointAddress & 0x80)
    {
        Log(("vusb: unmap IN pipe from address %u (%#x)\n", EndPt, pEndPtDesc->Core.bEndpointAddress));
        pPipe->in = NULL;

        /* If there was a read-ahead thread associated with this endpoint, tell it to go away. */
        if (pPipe->pvReadAheadArgs)
        {
            Log(("vusb: and tell read-ahead thread for the endpoint to terminate\n"));
            vusbReadAheadStop(pPipe->pvReadAheadArgs);
        }
    }
    else
    {
        Log(("vusb: unmap OUT pipe from address %u (%#x)\n", EndPt, pEndPtDesc->Core.bEndpointAddress));
        pPipe->out = NULL;
    }

    if (pPipe->pCtrl)
    {
        vusbMsgFreeExtraData(pPipe->pCtrl);
        pPipe->pCtrl = NULL;
    }
}

static void map_interface(PVUSBDEV pDev, PCVUSBDESCINTERFACEEX pIfDesc)
{
    LogFlow(("map_interface: pDev=%p[%s] pIfDesc=%p:{.iInterface=%d, .bAlternateSetting=%d}\n",
             pDev, pDev->pUsbIns->pszName, pIfDesc, pIfDesc->Core.iInterface, pIfDesc->Core.bAlternateSetting));

    for (unsigned i = 0; i < pIfDesc->Core.bNumEndpoints; i++)
    {
        if ((pIfDesc->paEndpoints[i].Core.bEndpointAddress & 0xF) == VUSB_PIPE_DEFAULT)
            Log(("vusb: Endpoint 0x%x on interface %u.%u tried to override the default message pipe!!!\n",
                pIfDesc->paEndpoints[i].Core.bEndpointAddress, pIfDesc->Core.bInterfaceNumber, pIfDesc->Core.bAlternateSetting));
        else
            vusbDevMapEndpoint(pDev, &pIfDesc->paEndpoints[i]);
    }
}

bool vusbDevDoSelectConfig(PVUSBDEV pDev, PCVUSBDESCCONFIGEX pCfgDesc)
{
    LogFlow(("vusbDevDoSelectConfig: pDev=%p[%s] pCfgDesc=%p:{.iConfiguration=%d}\n",
             pDev, pDev->pUsbIns->pszName, pCfgDesc, pCfgDesc->Core.iConfiguration));

    /*
     * Clean up all pipes and interfaces.
     */
    unsigned i;
    for (i = 0; i < VUSB_PIPE_MAX; i++)
    {
        if (i != VUSB_PIPE_DEFAULT)
        {
            vusbMsgFreeExtraData(pDev->aPipes[i].pCtrl);
            memset(&pDev->aPipes[i], 0, sizeof(pDev->aPipes[i]));
        }
    }
    memset(pDev->paIfStates, 0, pCfgDesc->Core.bNumInterfaces * sizeof(pDev->paIfStates[0]));

    /*
     * Map in the default setting for every interface.
     */
    for (i = 0; i < pCfgDesc->Core.bNumInterfaces; i++)
    {
        PCVUSBINTERFACE pIf;
        struct vusb_interface_state *pIfState;

        pIf = &pCfgDesc->paIfs[i];
        pIfState = &pDev->paIfStates[i];
        pIfState->pIf = pIf;

        /*
         * Find the 0 setting, if it is not present we just use
         * the lowest numbered one.
         */
        for (uint32_t j = 0; j < pIf->cSettings; j++)
        {
            if (    !pIfState->pCurIfDesc
                ||  pIf->paSettings[j].Core.bAlternateSetting < pIfState->pCurIfDesc->Core.bAlternateSetting)
                pIfState->pCurIfDesc = &pIf->paSettings[j];
            if (pIfState->pCurIfDesc->Core.bAlternateSetting == 0)
                break;
        }

        if (pIfState->pCurIfDesc)
            map_interface(pDev, pIfState->pCurIfDesc);
    }

    pDev->pCurCfgDesc = pCfgDesc;

    if (pCfgDesc->Core.bmAttributes & 0x40)
        pDev->u16Status |= (1 << VUSB_DEV_SELF_POWERED);
    else
        pDev->u16Status &= ~(1 << VUSB_DEV_SELF_POWERED);

    return true;
}

/**
 * Standard device request: SET_CONFIGURATION
 * @returns success indicator.
 */
static bool vusbDevStdReqSetConfig(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    unsigned iCfg = pSetup->wValue & 0xff;

    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_DEVICE)
    {
        Log(("vusb: error: %s: SET_CONFIGURATION - invalid request (dir) !!!\n", pDev->pUsbIns->pszName, iCfg));
        return false;
    }

    /*
     * Check that the device is in a valid state.
     * (The caller has already checked that it's not being reset.)
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (enmState == VUSB_DEVICE_STATE_DEFAULT)
    {
        LogFlow(("vusbDevStdReqSetConfig: %s: default dev state !!?\n", pDev->pUsbIns->pszName));
        return false;
    }

    PCVUSBDESCCONFIGEX pNewCfgDesc = vusbDevFindCfgDesc(pDev, iCfg);
    if (!pNewCfgDesc)
    {
        Log(("vusb: error: %s: config %i not found !!!\n", pDev->pUsbIns->pszName, iCfg));
        return false;
    }

    if (iCfg == 0)
        pDev->enmState = VUSB_DEVICE_STATE_ADDRESS;
    else
        pDev->enmState = VUSB_DEVICE_STATE_CONFIGURED;
    if (pDev->pUsbIns->pReg->pfnUsbSetConfiguration)
    {
        int rc = pDev->pUsbIns->pReg->pfnUsbSetConfiguration(pDev->pUsbIns, pNewCfgDesc->Core.bConfigurationValue,
                                                                pDev->pCurCfgDesc, pDev->paIfStates, pNewCfgDesc);
        if (RT_FAILURE(rc))
        {
            Log(("vusb: error: %s: failed to set config %i (%Rrc) !!!\n", pDev->pUsbIns->pszName, iCfg, rc));
            return false;
        }
    }
    Log(("vusb: %p[%s]: SET_CONFIGURATION: Selected config %u\n", pDev, pDev->pUsbIns->pszName, iCfg));
    return vusbDevDoSelectConfig(pDev, pNewCfgDesc);
}


/**
 * Standard device request: GET_CONFIGURATION
 * @returns success indicator.
 */
static bool vusbDevStdReqGetConfig(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_DEVICE)
    {
        Log(("vusb: error: %s: GET_CONFIGURATION - invalid request (dir) !!!\n", pDev->pUsbIns->pszName));
        return false;
    }

    /*
     * Check that the device is in a valid state.
     * (The caller has already checked that it's not being reset.)
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (    enmState != VUSB_DEVICE_STATE_CONFIGURED
        &&  enmState != VUSB_DEVICE_STATE_ADDRESS)
    {
        LogFlow(("vusbDevStdReqGetConfig: error: %s: invalid device state %d!!!\n", pDev->pUsbIns->pszName, enmState));
        return false;
    }

    if (*pcbBuf < 1)
    {
        LogFlow(("vusbDevStdReqGetConfig: %s: no space for data!\n", pDev->pUsbIns->pszName));
        return true;
    }

    uint8_t iCfg;
    if (pDev->enmState == VUSB_DEVICE_STATE_ADDRESS)
        iCfg = 0;
    else
        iCfg = pDev->pCurCfgDesc->Core.bConfigurationValue;

    *pbBuf = iCfg;
    *pcbBuf = 1;
    LogFlow(("vusbDevStdReqGetConfig: %s: returns iCfg=%d\n", pDev->pUsbIns->pszName, iCfg));
    return true;
}

/**
 * Standard device request: GET_INTERFACE
 * @returns success indicator.
 */
static bool vusbDevStdReqGetInterface(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_INTERFACE)
    {
        Log(("vusb: error: %s: GET_INTERFACE - invalid request (dir) !!!\n", pDev->pUsbIns->pszName));
        return false;
    }

    /*
     * Check that the device is in a valid state.
     * (The caller has already checked that it's not being reset.)
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (enmState != VUSB_DEVICE_STATE_CONFIGURED)
    {
        LogFlow(("vusbDevStdReqGetInterface: error: %s: invalid device state %d!!!\n", pDev->pUsbIns->pszName, enmState));
        return false;
    }

    if (*pcbBuf < 1)
    {
        LogFlow(("vusbDevStdReqGetInterface: %s: no space for data!\n", pDev->pUsbIns->pszName));
        return true;
    }

    for (unsigned i = 0; i < pDev->pCurCfgDesc->Core.bNumInterfaces; i++)
    {
        PCVUSBDESCINTERFACEEX pIfDesc = pDev->paIfStates[i].pCurIfDesc;
        if (    pIfDesc
            &&  pSetup->wIndex == pIfDesc->Core.bInterfaceNumber)
        {
            *pbBuf = pIfDesc->Core.bAlternateSetting;
            *pcbBuf = 1;
            Log(("vusb: %s: GET_INTERFACE: %u.%u\n", pDev->pUsbIns->pszName, pIfDesc->Core.bInterfaceNumber, *pbBuf));
            return true;
        }
    }

    Log(("vusb: error: %s: GET_INTERFACE - unknown iface %u !!!\n", pDev->pUsbIns->pszName, pSetup->wIndex));
    return false;
}

/**
 * Standard device request: SET_INTERFACE
 * @returns success indicator.
 */
static bool vusbDevStdReqSetInterface(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_INTERFACE)
    {
        Log(("vusb: error: %s: SET_INTERFACE - invalid request (dir) !!!\n", pDev->pUsbIns->pszName));
        return false;
    }

    /*
     * Check that the device is in a valid state.
     * (The caller has already checked that it's not being reset.)
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (enmState != VUSB_DEVICE_STATE_CONFIGURED)
    {
        LogFlow(("vusbDevStdReqSetInterface: error: %s: invalid device state %d !!!\n", pDev->pUsbIns->pszName, enmState));
        return false;
    }

    /*
     * Find the interface.
     */
    uint8_t iIf = pSetup->wIndex;
    PVUSBINTERFACESTATE pIfState = vusbDevFindIfState(pDev, iIf);
    if (!pIfState)
    {
        LogFlow(("vusbDevStdReqSetInterface: error: %s: couldn't find interface %u !!!\n", pDev->pUsbIns->pszName, iIf));
        return false;
    }
    uint8_t iAlt = pSetup->wValue;
    PCVUSBDESCINTERFACEEX pIfDesc = vusbDevFindAltIfDesc(pDev, pIfState, iAlt);
    if (!pIfDesc)
    {
        LogFlow(("vusbDevStdReqSetInterface: error: %s: couldn't find alt interface %u.%u !!!\n", pDev->pUsbIns->pszName, iIf, iAlt));
        return false;
    }

    if (pDev->pUsbIns->pReg->pfnUsbSetInterface)
    {
        int rc = pDev->pUsbIns->pReg->pfnUsbSetInterface(pDev->pUsbIns, iIf, iAlt);
        if (RT_FAILURE(rc))
        {
            LogFlow(("vusbDevStdReqSetInterface: error: %s: couldn't find alt interface %u.%u (%Rrc)\n", pDev->pUsbIns->pszName, iIf, iAlt, rc));
            return false;
        }
    }

    for (unsigned i = 0; i < pIfState->pCurIfDesc->Core.bNumEndpoints; i++)
        unmap_endpoint(pDev, &pIfState->pCurIfDesc->paEndpoints[i]);

    Log(("vusb: SET_INTERFACE: Selected %u.%u\n", iIf, iAlt));

    map_interface(pDev, pIfDesc);
    pIfState->pCurIfDesc = pIfDesc;

    return true;
}

/**
 * Standard device request: SET_ADDRESS
 * @returns success indicator.
 */
static bool vusbDevStdReqSetAddress(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_DEVICE)
    {
        Log(("vusb: error: %s: SET_ADDRESS - invalid request (dir) !!!\n", pDev->pUsbIns->pszName));
        return false;
    }

    /*
     * Check that the device is in a valid state.
     * (The caller has already checked that it's not being reset.)
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (    enmState != VUSB_DEVICE_STATE_DEFAULT
        &&  enmState != VUSB_DEVICE_STATE_ADDRESS)
    {
        LogFlow(("vusbDevStdReqSetAddress: error: %s: invalid device state %d !!!\n", pDev->pUsbIns->pszName, enmState));
        return false;
    }

    pDev->u8NewAddress = pSetup->wValue;
    return true;
}

/**
 * Standard device request: CLEAR_FEATURE
 * @returns success indicator.
 *
 * @remark This is only called for VUSB_TO_ENDPOINT && ep == 0 && wValue == ENDPOINT_HALT.
 *         All other cases of CLEAR_FEATURE is handled in the normal async/sync manner.
 */
static bool vusbDevStdReqClearFeature(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
    {
        case VUSB_TO_DEVICE:
            Log(("vusb: ClearFeature: dev(%u): selector=%u\n", pSetup->wIndex, pSetup->wValue));
            break;
        case VUSB_TO_INTERFACE:
            Log(("vusb: ClearFeature: iface(%u): selector=%u\n", pSetup->wIndex, pSetup->wValue));
            break;
        case VUSB_TO_ENDPOINT:
            Log(("vusb: ClearFeature: ep(%u): selector=%u\n", pSetup->wIndex, pSetup->wValue));
            if (    !EndPt /* Default control pipe only */
                &&  pSetup->wValue == 0 /* ENDPOINT_HALT */
                &&  pDev->pUsbIns->pReg->pfnUsbClearHaltedEndpoint)
            {
                int rc = pDev->pUsbIns->pReg->pfnUsbClearHaltedEndpoint(pDev->pUsbIns, pSetup->wIndex);
                return RT_SUCCESS(rc);
            }
            break;
        default:
            AssertMsgFailed(("VUSB_TO_OTHER!\n"));
            break;
    }

    AssertMsgFailed(("Invalid safe check !!!\n"));
    return false;
}

/**
 * Standard device request: SET_FEATURE
 * @returns success indicator.
 */
static bool vusbDevStdReqSetFeature(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
    {
        case VUSB_TO_DEVICE:
            Log(("vusb: SetFeature: dev(%u): selector=%u\n",
                pSetup->wIndex, pSetup->wValue));
            break;
        case VUSB_TO_INTERFACE:
            Log(("vusb: SetFeature: if(%u): selector=%u\n",
                pSetup->wIndex, pSetup->wValue));
            break;
        case VUSB_TO_ENDPOINT:
            Log(("vusb: SetFeature: ep(%u): selector=%u\n",
                pSetup->wIndex, pSetup->wValue));
            break;
        default:
            AssertMsgFailed(("VUSB_TO_OTHER!\n"));
            return false;
    }
    AssertMsgFailed(("This stuff is bogus\n"));
    return false;
}

static bool vusbDevStdReqGetStatus(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if (*pcbBuf != 2)
    {
        LogFlow(("vusbDevStdReqGetStatus: %s: buffer is too small! (%d)\n", pDev->pUsbIns->pszName, *pcbBuf));
        return false;
    }

    uint16_t u16Status;
    switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
    {
        case VUSB_TO_DEVICE:
            u16Status = pDev->u16Status;
            LogFlow(("vusbDevStdReqGetStatus: %s: device status %#x (%d)\n", pDev->pUsbIns->pszName, u16Status, u16Status));
            break;
        case VUSB_TO_INTERFACE:
            u16Status = 0;
            LogFlow(("vusbDevStdReqGetStatus: %s: bogus interface status request!!\n", pDev->pUsbIns->pszName));
            break;
        case VUSB_TO_ENDPOINT:
            u16Status = 0;
            LogFlow(("vusbDevStdReqGetStatus: %s: bogus endpoint status request!!\n", pDev->pUsbIns->pszName));
            break;
        default:
            AssertMsgFailed(("VUSB_TO_OTHER!\n"));
            return false;
    }

    *(uint16_t *)pbBuf = u16Status;
    return true;
}


/**
 * Finds a cached string.
 *
 * @returns Pointer to the cached string if found.  NULL if not.
 * @param   paLanguages         The languages to search.
 * @param   cLanguages          The number of languages in the table.
 * @param   idLang              The language ID.
 * @param   iString             The string index.
 */
static PCPDMUSBDESCCACHESTRING FindCachedString(PCPDMUSBDESCCACHELANG paLanguages, unsigned cLanguages,
                                                uint16_t idLang, uint8_t iString)
{
    /** @todo binary lookups! */
    unsigned iCurLang = cLanguages;
    while (iCurLang-- > 0)
        if (paLanguages[iCurLang].idLang == idLang)
        {
            PCPDMUSBDESCCACHESTRING paStrings = paLanguages[iCurLang].paStrings;
            unsigned                iCurStr   = paLanguages[iCurLang].cStrings;
            while (iCurStr-- > 0)
                if (paStrings[iCurStr].idx == iString)
                    return &paStrings[iCurStr];
            break;
        }
    return NULL;
}


/** Macro for copying descriptor data. */
#define COPY_DATA(pbDst, cbLeft, pvSrc, cbSrc) \
    do { \
        uint32_t cbSrc_ = cbSrc; \
        uint32_t cbCopy = RT_MIN(cbLeft, cbSrc_); \
        memcpy(pbBuf, pvSrc, cbCopy); \
        cbLeft -= cbCopy; \
        if (!cbLeft) \
            return; \
        pbBuf += cbCopy; \
    } while (0)

/**
 * Internal function for reading the language IDs.
 */
static void ReadCachedStringDesc(PCPDMUSBDESCCACHESTRING pString, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    uint32_t        cbLeft = *pcbBuf;

    RTUTF16         wsz[128];           /* 128-1 => bLength=0xff */
    PRTUTF16        pwsz = wsz;
    size_t          cwc;
    int rc = RTStrToUtf16Ex(pString->psz, RT_ELEMENTS(wsz) - 1, &pwsz, RT_ELEMENTS(wsz), &cwc);
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        wsz[0] = 'e';
        wsz[1] = 'r';
        wsz[2] = 'r';
        cwc = 3;
    }

    VUSBDESCSTRING  StringDesc;
    StringDesc.bLength          = sizeof(StringDesc) + cwc * sizeof(RTUTF16);
    StringDesc.bDescriptorType  = VUSB_DT_STRING;
    COPY_DATA(pbBuf, cbLeft, &StringDesc, sizeof(StringDesc));
    COPY_DATA(pbBuf, cbLeft, wsz, cwc * sizeof(RTUTF16));

    /* updated the size of the output buffer. */
    *pcbBuf -= cbLeft;
}


/**
 * Internal function for reading the language IDs.
 */
static void ReadCachedLangIdDesc(PCPDMUSBDESCCACHELANG paLanguages, unsigned cLanguages,
                                 uint8_t *pbBuf, uint32_t *pcbBuf)
{
    uint32_t        cbLeft      = *pcbBuf;

    VUSBDESCLANGID  LangIdDesc;
    size_t          cbDesc      = sizeof(LangIdDesc) + cLanguages * sizeof(paLanguages[0].idLang);
    LangIdDesc.bLength          = RT_MIN(0xff, cbDesc);
    LangIdDesc.bDescriptorType  = VUSB_DT_STRING;
    COPY_DATA(pbBuf, cbLeft, &LangIdDesc, sizeof(LangIdDesc));

    unsigned iLanguage = cLanguages;
    while (iLanguage-- > 0)
        COPY_DATA(pbBuf, cbLeft, &paLanguages[iLanguage].idLang, sizeof(paLanguages[iLanguage].idLang));

    /* updated the size of the output buffer. */
    *pcbBuf -= cbLeft;
}


/**
 * Internal function which performs a descriptor read on the cached descriptors.
 */
static void ReadCachedConfigDesc(PCVUSBDESCCONFIGEX pCfgDesc, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    uint32_t cbLeft = *pcbBuf;

/** @todo See @bugref{2693} */
    /*
     * Make a copy of the config descriptor and calculate the wTotalLength field.
     */
    VUSBDESCCONFIG CfgDesc;
    memcpy(&CfgDesc, pCfgDesc, VUSB_DT_CONFIG_MIN_LEN);
    uint32_t cbTotal = pCfgDesc->Core.bLength;
    for (unsigned i = 0; i < pCfgDesc->Core.bNumInterfaces; i++)
    {
        PCVUSBINTERFACE pIf = &pCfgDesc->paIfs[i];
        for (uint32_t j = 0; j < pIf->cSettings; j++)
        {
            cbTotal += pIf->paSettings[j].cbIAD;
            cbTotal += pIf->paSettings[j].Core.bLength;
            cbTotal += pIf->paSettings[j].cbClass;
            for (unsigned k = 0; k < pIf->paSettings[j].Core.bNumEndpoints; k++)
            {
                cbTotal += pIf->paSettings[j].paEndpoints[k].Core.bLength;
                cbTotal += pIf->paSettings[j].paEndpoints[k].cbClass;
            }
        }
    }
    CfgDesc.wTotalLength = RT_H2LE_U16(cbTotal);

    /*
     * Copy the config descriptor
     */
    COPY_DATA(pbBuf, cbLeft, &CfgDesc, VUSB_DT_CONFIG_MIN_LEN);
    COPY_DATA(pbBuf, cbLeft, pCfgDesc->pvMore, pCfgDesc->Core.bLength - VUSB_DT_CONFIG_MIN_LEN);

    /*
     * Copy out all the interfaces for this configuration
     */
    for (unsigned i = 0; i < pCfgDesc->Core.bNumInterfaces; i++)
    {
        PCVUSBINTERFACE pIf = &pCfgDesc->paIfs[i];
        for (uint32_t j = 0; j < pIf->cSettings; j++)
        {
            PCVUSBDESCINTERFACEEX pIfDesc = &pIf->paSettings[j];

            COPY_DATA(pbBuf, cbLeft, pIfDesc->pIAD, pIfDesc->cbIAD);
            COPY_DATA(pbBuf, cbLeft, pIfDesc, VUSB_DT_INTERFACE_MIN_LEN);
            COPY_DATA(pbBuf, cbLeft, pIfDesc->pvMore, pIfDesc->Core.bLength - VUSB_DT_INTERFACE_MIN_LEN);
            COPY_DATA(pbBuf, cbLeft, pIfDesc->pvClass, pIfDesc->cbClass);

            /*
             * Copy out all the endpoints for this interface
             */
            for (unsigned k = 0; k < pIfDesc->Core.bNumEndpoints; k++)
            {
                VUSBDESCENDPOINT EndPtDesc;
                memcpy(&EndPtDesc, &pIfDesc->paEndpoints[k], VUSB_DT_ENDPOINT_MIN_LEN);
                EndPtDesc.wMaxPacketSize = RT_H2LE_U16(EndPtDesc.wMaxPacketSize);

                COPY_DATA(pbBuf, cbLeft, &EndPtDesc, VUSB_DT_ENDPOINT_MIN_LEN);
                COPY_DATA(pbBuf, cbLeft, pIfDesc->paEndpoints[k].pvMore, EndPtDesc.bLength - VUSB_DT_ENDPOINT_MIN_LEN);
                COPY_DATA(pbBuf, cbLeft, pIfDesc->paEndpoints[k].pvClass, pIfDesc->paEndpoints[k].cbClass);
            }
        }
    }

    /* updated the size of the output buffer. */
    *pcbBuf -= cbLeft;
}

/**
 * Internal function which performs a descriptor read on the cached descriptors.
 */
static void ReadCachedDeviceDesc(PCVUSBDESCDEVICE pDevDesc, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    uint32_t cbLeft = *pcbBuf;

    /*
     * Duplicate the device description and update some fields we keep in cpu type.
     */
    Assert(sizeof(VUSBDESCDEVICE) == 18);
    VUSBDESCDEVICE DevDesc = *pDevDesc;
    DevDesc.bcdUSB    = RT_H2LE_U16(DevDesc.bcdUSB);
    DevDesc.idVendor  = RT_H2LE_U16(DevDesc.idVendor);
    DevDesc.idProduct = RT_H2LE_U16(DevDesc.idProduct);
    DevDesc.bcdDevice = RT_H2LE_U16(DevDesc.bcdDevice);

    COPY_DATA(pbBuf, cbLeft, &DevDesc, sizeof(DevDesc));
    COPY_DATA(pbBuf, cbLeft, pDevDesc + 1, pDevDesc->bLength - sizeof(DevDesc));

    /* updated the size of the output buffer. */
    *pcbBuf -= cbLeft;
}

#undef COPY_DATA

/**
 * Standard device request: GET_DESCRIPTOR
 * @returns success indicator.
 * @remark not really used yet as we consider GET_DESCRIPTOR 'safe'.
 */
static bool vusbDevStdReqGetDescriptor(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, uint8_t *pbBuf, uint32_t *pcbBuf)
{
    if ((pSetup->bmRequestType & VUSB_RECIP_MASK) == VUSB_TO_DEVICE)
    {
        switch (pSetup->wValue >> 8)
        {
            case VUSB_DT_DEVICE:
                ReadCachedDeviceDesc(pDev->pDescCache->pDevice, pbBuf, pcbBuf);
                LogFlow(("vusbDevStdReqGetDescriptor: %s: %u bytes of device descriptors\n", pDev->pUsbIns->pszName, *pcbBuf));
                return true;

            case VUSB_DT_CONFIG:
            {
                unsigned int iIndex = (pSetup->wValue & 0xff);
                if (iIndex >= pDev->pDescCache->pDevice->bNumConfigurations)
                {
                    LogFlow(("vusbDevStdReqGetDescriptor: %s: iIndex=%p >= bNumConfigurations=%d !!!\n",
                             pDev->pUsbIns->pszName, iIndex, pDev->pDescCache->pDevice->bNumConfigurations));
                    return false;
                }
                ReadCachedConfigDesc(&pDev->pDescCache->paConfigs[iIndex], pbBuf, pcbBuf);
                LogFlow(("vusbDevStdReqGetDescriptor: %s: %u bytes of config descriptors\n", pDev->pUsbIns->pszName, *pcbBuf));
                return true;
            }

            case VUSB_DT_STRING:
            {
                if (pSetup->wIndex == 0)
                {
                    ReadCachedLangIdDesc(pDev->pDescCache->paLanguages, pDev->pDescCache->cLanguages, pbBuf, pcbBuf);
                    LogFlow(("vusbDevStdReqGetDescriptor: %s: %u bytes of language ID (string) descriptors\n", pDev->pUsbIns->pszName, *pcbBuf));
                    return true;
                }
                PCPDMUSBDESCCACHESTRING pString;
                pString = FindCachedString(pDev->pDescCache->paLanguages, pDev->pDescCache->cLanguages,
                                           pSetup->wIndex, pSetup->wValue & 0xff);
                if (pString)
                {
                    ReadCachedStringDesc(pString, pbBuf, pcbBuf);
                    LogFlow(("vusbDevStdReqGetDescriptor: %s: %u bytes of string descriptors \"%s\"\n",
                             pDev->pUsbIns->pszName, *pcbBuf, pString->psz));
                    return true;
                }
                break;
            }

            default:
                break;
        }
    }
    Log(("vusb: %s: warning: unknown descriptor: type=%u descidx=%u lang=%u len=%u!!!\n",
         pDev->pUsbIns->pszName, pSetup->wValue >> 8, pSetup->wValue & 0xff, pSetup->wIndex, pSetup->wLength));
    return false;
}


/**
 * Service the standard USB requests.
 *
 * Devices may call this from controlmsg() if you want vusb core to handle your standard
 * request, it's not necessary - you could handle them manually
 *
 * @param   pDev        The device.
 * @param   EndPoint    The endpoint.
 * @param   pSetup      Pointer to the setup request structure.
 * @param   pvBuf       Buffer?
 * @param   pcbBuf      ?
 */
bool vusbDevStandardRequest(PVUSBDEV pDev, int EndPoint, PVUSBSETUP pSetup, void *pvBuf, uint32_t *pcbBuf)
{
    static bool (* const s_apfnStdReq[VUSB_REQ_MAX])(PVUSBDEV, int, PVUSBSETUP, uint8_t *, uint32_t *) =
    {
        vusbDevStdReqGetStatus,
        vusbDevStdReqClearFeature,
        NULL,
        vusbDevStdReqSetFeature,
        NULL,
        vusbDevStdReqSetAddress,
        vusbDevStdReqGetDescriptor,
        NULL,
        vusbDevStdReqGetConfig,
        vusbDevStdReqSetConfig,
        vusbDevStdReqGetInterface,
        vusbDevStdReqSetInterface,
        NULL /* for iso */
    };

    /*
     * Check that the device is in a valid state.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: standard control message ignored, the device is resetting\n", pDev->pUsbIns->pszName));
        return false;
    }

    /*
     * Do the request if it's one we want to deal with.
     */
    if (    pSetup->bRequest >= VUSB_REQ_MAX
        ||  !s_apfnStdReq[pSetup->bRequest])
    {
        Log(("vusb: warning: standard req not implemented: message %u: val=%u idx=%u len=%u !!!\n",
             pSetup->bRequest, pSetup->wValue, pSetup->wIndex, pSetup->wLength));
        return false;
    }

    return s_apfnStdReq[pSetup->bRequest](pDev, EndPoint, pSetup, (uint8_t *)pvBuf, pcbBuf);
}


/**
 * Add a device to the address hash
 */
static void vusbDevAddressHash(PVUSBDEV pDev)
{
    if (pDev->u8Address == VUSB_INVALID_ADDRESS)
        return;
    uint8_t u8Hash = vusbHashAddress(pDev->u8Address);
    pDev->pNextHash = pDev->pHub->pRootHub->apAddrHash[u8Hash];
    pDev->pHub->pRootHub->apAddrHash[u8Hash] = pDev;
}

/**
 * Remove a device from the address hash
 */
static void vusbDevAddressUnHash(PVUSBDEV pDev)
{
    if (pDev->u8Address == VUSB_INVALID_ADDRESS)
        return;

    uint8_t u8Hash = vusbHashAddress(pDev->u8Address);
    pDev->u8Address = VUSB_INVALID_ADDRESS;
    pDev->u8NewAddress = VUSB_INVALID_ADDRESS;

    PVUSBDEV pCur = pDev->pHub->pRootHub->apAddrHash[u8Hash];
    if (pCur == pDev)
    {
        /* special case, we're at the head */
        pDev->pHub->pRootHub->apAddrHash[u8Hash] = pDev->pNextHash;
        pDev->pNextHash = NULL;
    }
    else
    {
        /* search the list */
        PVUSBDEV pPrev;
        for (pPrev = pCur, pCur = pCur->pNextHash;
             pCur;
             pPrev = pCur, pCur = pCur->pNextHash)
        {
            if (pCur == pDev)
            {
                pPrev->pNextHash = pCur->pNextHash;
                pDev->pNextHash = NULL;
                break;
            }
        }
    }
}

/**
 * Sets the address of a device.
 *
 * Called by status_completion() and vusbDevResetWorker().
 */
void vusbDevSetAddress(PVUSBDEV pDev, uint8_t u8Address)
{
    LogFlow(("vusbDevSetAddress: pDev=%p[%s]/%i u8Address=%#x\n",
             pDev, pDev->pUsbIns->pszName, pDev->i16Port, u8Address));

    /*
     * Check that the device is in a valid state.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    if (    enmState == VUSB_DEVICE_STATE_ATTACHED
        ||  enmState == VUSB_DEVICE_STATE_DETACHED)
    {
        LogFlow(("vusbDevSetAddress: %s: fails because %d < POWERED\n", pDev->pUsbIns->pszName, pDev->enmState));
        return;
    }
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: set address ignored, the device is resetting\n", pDev->pUsbIns->pszName));
        return;
    }

    /*
     * Ok, get on with it.
     */
    if (pDev->u8Address == u8Address)
        return;

    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
    if (pDev->u8Address == VUSB_DEFAULT_ADDRESS)
        pRh->pDefaultAddress = NULL;

    vusbDevAddressUnHash(pDev);

    if (u8Address == VUSB_DEFAULT_ADDRESS)
    {
        if (pRh->pDefaultAddress != NULL)
        {
            vusbDevAddressUnHash(pRh->pDefaultAddress);
            pRh->pDefaultAddress->enmState = VUSB_DEVICE_STATE_POWERED;
            Log(("2 DEFAULT ADDRS\n"));
        }

        pRh->pDefaultAddress = pDev;
        pDev->enmState = VUSB_DEVICE_STATE_DEFAULT;
    }
    else
        pDev->enmState = VUSB_DEVICE_STATE_ADDRESS;

    pDev->u8Address = u8Address;
    vusbDevAddressHash(pDev);

    Log(("vusb: %p[%s]/%i: Assigned address %u\n",
         pDev, pDev->pUsbIns->pszName, pDev->i16Port, u8Address));
}


/**
 * Cancels and completes (with CRC failure) all async URBs pending
 * on a device. This is typically done as part of a reset and
 * before detaching a device.
 *
 * @param   fDetaching  If set, we will unconditionally unlink (and leak)
 *                      any URBs which isn't reaped.
 */
static void vusbDevCancelAllUrbs(PVUSBDEV pDev, bool fDetaching)
{
    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);

    /*
     * Iterate the URBs and cancel them.
     */
    PVUSBURB pUrb = pRh->pAsyncUrbHead;
    while (pUrb)
    {
        PVUSBURB pNext = pUrb->VUsb.pNext;
        if (pUrb->VUsb.pDev == pDev)
        {
            LogFlow(("%s: vusbDevCancelAllUrbs: CANCELING URB\n", pUrb->pszDesc));
            vusbUrbCancel(pUrb, CANCELMODE_FAIL);
        }
        pUrb = pNext;
    }

    /*
     * Reap any URBs which became ripe during cancel now.
     */
    unsigned cReaped;
    do
    {
        cReaped = 0;
        pUrb = pRh->pAsyncUrbHead;
        while (pUrb)
        {
            PVUSBURB pNext = pUrb->VUsb.pNext;
            if (pUrb->VUsb.pDev == pDev)
            {
                PVUSBURB pRipe = NULL;
                if (pUrb->enmState == VUSBURBSTATE_REAPED)
                    pRipe = pUrb;
                else if (pUrb->enmState == VUSBURBSTATE_CANCELLED)
#ifdef RT_OS_WINDOWS   /** @todo Windows doesn't do cancelling, thus this kludge to prevent really bad
                        * things from happening if we leave a pending URB behinds. */
                    pRipe = pDev->pUsbIns->pReg->pfnUrbReap(pDev->pUsbIns, fDetaching ? 1500 : 0 /*ms*/);
#else
                    pRipe = pDev->pUsbIns->pReg->pfnUrbReap(pDev->pUsbIns, fDetaching ? 10 : 0 /*ms*/);
#endif
                else
                    AssertMsgFailed(("pUrb=%p enmState=%d\n", pUrb, pUrb->enmState));
                if (pRipe)
                {
                    if (pRipe == pNext)
                        pNext = pNext->VUsb.pNext;
                    vusbUrbRipe(pRipe);
                    cReaped++;
                }
            }
            pUrb = pNext;
        }
    } while (cReaped > 0);

    /*
     * If we're detaching, we'll have to orphan any leftover URBs.
     */
    if (fDetaching)
    {
        pUrb = pRh->pAsyncUrbHead;
        while (pUrb)
        {
            PVUSBURB pNext = pUrb->VUsb.pNext;
            if (pUrb->VUsb.pDev == pDev)
            {
                AssertMsgFailed(("%s: Leaking left over URB! state=%d pDev=%p[%s]\n",
                                 pUrb->pszDesc, pUrb->enmState, pDev, pDev->pUsbIns->pszName));
                vusbUrbUnlink(pUrb);
            }
            pUrb = pNext;
        }
    }
}


/**
 * Detaches a device from the hub it's attached to.
 *
 * @returns VBox status code.
 * @param   pDev        The device to detach.
 *
 * @remark  This can be called in any state but reset.
 */
int vusbDevDetach(PVUSBDEV pDev)
{
    LogFlow(("vusbDevDetach: pDev=%p[%s] enmState=%#x\n", pDev, pDev->pUsbIns->pszName, pDev->enmState));
    VUSBDEV_ASSERT_VALID_STATE(pDev->enmState);
    Assert(pDev->enmState != VUSB_DEVICE_STATE_RESET);

    vusbDevCancelAllUrbs(pDev, true);
    vusbDevAddressUnHash(pDev);

    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
    if (!pRh)
        AssertMsgFailedReturn(("Not attached!\n"), VERR_VUSB_DEVICE_NOT_ATTACHED);
    if (pRh->pDefaultAddress == pDev)
        pRh->pDefaultAddress = NULL;

    pDev->pHub->pOps->pfnDetach(pDev->pHub, pDev);
    pDev->i16Port = -1;
    pDev->enmState = VUSB_DEVICE_STATE_DETACHED;
    pDev->pHub = NULL;

    /* Remove the configuration */
    pDev->pCurCfgDesc = NULL;
    for (unsigned i = 0; i < VUSB_PIPE_MAX; i++)
        vusbMsgFreeExtraData(pDev->aPipes[i].pCtrl);
    memset(pDev->aPipes, 0, sizeof(pDev->aPipes));
    return VINF_SUCCESS;
}


/**
 * Destroys a device, detaching it from the hub if necessary.
 *
 * @param   pDev    The device.
 * @thread  EMT
 */
void vusbDevDestroy(PVUSBDEV pDev)
{
    LogFlow(("vusbDevDestroy: pDev=%p[%s] enmState=%d\n", pDev, pDev->pUsbIns->pszName, pDev->enmState));

    /*
     * Deal with pending async reset.
     */
    if (pDev->enmState == VUSB_DEVICE_STATE_RESET)
    {
        Assert(pDev->pvResetArgs && pDev->hResetThread != NIL_RTTHREAD);
        int rc = RTThreadWait(pDev->hResetThread, 5000, NULL);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            PVUSBRESETARGS pArgs = (PVUSBRESETARGS)pDev->pvResetArgs;
            Assert(pArgs->pDev == pDev);
            RTMemTmpFree(pArgs);

            pDev->hResetThread = NIL_RTTHREAD;
            pDev->pvResetArgs = NULL;
            pDev->enmState = VUSB_DEVICE_STATE_DEFAULT; /* anything but reset */
        }
    }

    /*
     * Detach and free resources.
     */
    if (pDev->pHub)
        vusbDevDetach(pDev);
    RTMemFree(pDev->paIfStates);
    pDev->enmState = VUSB_DEVICE_STATE_DESTROYED;
    TMR3TimerDestroy(pDev->pResetTimer);
}


/* -=-=-=-=-=- VUSBIDEVICE methods -=-=-=-=-=- */


/**
 * Perform the actual reset.
 *
 * @thread EMT or a VUSB reset thread.
 */
static int vusbDevResetWorker(PVUSBDEV pDev, bool fResetOnLinux)
{
    int rc = VINF_SUCCESS;

    if (pDev->pUsbIns->pReg->pfnUsbReset)
        rc = pDev->pUsbIns->pReg->pfnUsbReset(pDev->pUsbIns, fResetOnLinux);

    LogFlow(("vusbDevResetWorker: %s: returns %Rrc\n", pDev->pUsbIns->pszName, rc));
    return rc;
}


/**
 * The actual reset has been done, do completion on EMT.
 *
 * There are several things we have to do now, like set default
 * config and address, and cleanup the state of control pipes.
 *
 * It's possible that the device has a delayed destroy request
 * pending when we get here. This can happen for async resetting.
 * We deal with it here, since we're now executing on the EMT
 * thread and the destruction will be properly serialized now.
 *
 * @param   pDev    The device that is being reset.
 * @param   rc      The vusbDevResetWorker return code.
 * @param   pfnDone The done callback specified by the caller of vusbDevReset().
 * @param   pvUser  The user argument for the callback.
 */
static void vusbDevResetDone(PVUSBDEV pDev, int rc, PFNVUSBRESETDONE pfnDone, void *pvUser)
{
    VUSBDEV_ASSERT_VALID_STATE(pDev->enmState);
    Assert(pDev->enmState == VUSB_DEVICE_STATE_RESET);

    /*
     * Do control pipe cleanup regardless of state and result.
     */
    for (unsigned i = 0; i < VUSB_PIPE_MAX; i++)
        if (pDev->aPipes[i].pCtrl)
            vusbMsgResetExtraData(pDev->aPipes[i].pCtrl);

    /*
     * Switch to the default state.
     */
    pDev->enmState = VUSB_DEVICE_STATE_DEFAULT;
    pDev->u16Status = 0;
    vusbDevDoSelectConfig(pDev, &g_Config0);
    if (!vusbDevIsRh(pDev))
        vusbDevSetAddress(pDev, VUSB_DEFAULT_ADDRESS);
    if (pfnDone)
        pfnDone(&pDev->IDevice, rc, pvUser);
}


/**
 * Timer callback for doing reset completion.
 *
 * @param   pUsbIns     The USB device instance.
 * @param   pTimer      The timer instance.
 * @param   pvUser      The VUSB device data.
 * @thread EMT
 */
static DECLCALLBACK(void) vusbDevResetDoneTimer(PPDMUSBINS pUsbIns, PTMTIMER pTimer, void *pvUser)
{
    PVUSBDEV        pDev  = (PVUSBDEV)pvUser;
    PVUSBRESETARGS  pArgs = (PVUSBRESETARGS)pDev->pvResetArgs;
    AssertPtr(pArgs); Assert(pArgs->pDev == pDev); Assert(pDev->pUsbIns == pUsbIns);

    /*
     * Release the thread and update the device structure.
     */
    int rc = RTThreadWait(pDev->hResetThread, 2, NULL);
    AssertRC(rc);
    pDev->hResetThread = NIL_RTTHREAD;
    pDev->pvResetArgs  = NULL;

    /*
     * Reset-done processing and cleanup.
     */
    vusbDevResetDone(pArgs->pDev, pArgs->rc, pArgs->pfnDone, pArgs->pvUser);

    RTMemTmpFree(pArgs);
}


/**
 * Thread function for performing an async reset.
 *
 * This will pass the argument packet back to EMT upon completion
 * by means of a one shot timer.
 *
 * @returns whatever vusbDevResetWorker() returns.
 * @param   Thread      This thread.
 * @param   pvUser      Pointer to a VUSBRESETARGS structure.
 */
static DECLCALLBACK(int) vusbDevResetThread(RTTHREAD Thread, void *pvUser)
{
    PVUSBRESETARGS  pArgs = (PVUSBRESETARGS)pvUser;
    PVUSBDEV        pDev  = pArgs->pDev;
    LogFlow(("vusb: reset thread started\n"));

    /*
     * Tell EMT that we're in flow and then perform the reset.
     */
    uint64_t u64EndTS = TMTimerGet(pDev->pResetTimer) + TMTimerFromMilli(pDev->pResetTimer, 10);
    RTThreadUserSignal(Thread);

    int rc = pArgs->rc = vusbDevResetWorker(pDev, pArgs->fResetOnLinux);

    /*
     * We use a timer to communicate the result back to EMT.
     * This avoids suspend + poweroff issues, and it should give
     * us more accurate scheduling than making this thread sleep.
     */
    int rc2 = TMTimerSet(pDev->pResetTimer, u64EndTS);
    AssertReleaseRC(rc2);

    LogFlow(("vusb: reset thread exiting, rc=%Rrc\n", rc));
    return rc;
}


/**
 * Resets a device.
 *
 * Since a device reset shall take at least 10ms from the guest point of view,
 * it must be performed asynchronously.  We create a thread which performs this
 * operation and ensures it will take at least 10ms.
 *
 * At times - like init - a synchronous reset is required, this can be done
 * by passing NULL for pfnDone.
 *
 * While the device is being reset it is in the VUSB_DEVICE_STATE_RESET state.
 * On completion it will be in the VUSB_DEVICE_STATE_DEFAULT state if successful,
 * or in the VUSB_DEVICE_STATE_DETACHED state if the rest failed.
 *
 * @returns VBox status code.
 *
 * @param   pDev            Pointer to the VUSB device interface.
 * @param   fResetOnLinux   Whether it's safe to reset the device(s) on a linux
 *                          host system. See discussion of logical reconnects elsewhere.
 * @param   pfnDone         Pointer to the completion routine. If NULL a synchronous
 *                          reset is preformed not respecting the 10ms.
 * @param   pVM             Pointer to the VM handle for performing the done function
 *                          on the EMT thread.
 * @thread  EMT
 */
DECLCALLBACK(int) vusbDevReset(PVUSBIDEVICE pDevice, bool fResetOnLinux, PFNVUSBRESETDONE pfnDone, void *pvUser, PVM pVM)
{
    PVUSBDEV pDev = (PVUSBDEV)pDevice;
    Assert(!pfnDone || pVM);
    LogFlow(("vusb: reset: [%s]/%i\n", pDev->pUsbIns->pszName, pDev->i16Port));

    /*
     * Only one reset operation at a time.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: reset request is ignored, the device is already resetting!\n", pDev->pUsbIns->pszName));
        return VERR_VUSB_DEVICE_IS_RESETTING;
    }
    pDev->enmState = VUSB_DEVICE_STATE_RESET;

    /*
     * First, cancel all async URBs.
     */
    vusbDevCancelAllUrbs(pDev, false);

    /* Async or sync? */
    if (pfnDone)
    {
        /*
         * Async fashion.
         */
        PVUSBRESETARGS pArgs = (PVUSBRESETARGS)RTMemTmpAlloc(sizeof(*pArgs));
        if (pArgs)
        {
            pDev->pvResetArgs = pArgs;
            pArgs->pDev = pDev;
            pArgs->fResetOnLinux = fResetOnLinux;
            pArgs->rc = VERR_INTERNAL_ERROR;
            pArgs->pfnDone = pfnDone;
            pArgs->pvUser = pvUser;
            int rc = RTThreadCreate(&pDev->hResetThread, vusbDevResetThread, pArgs, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "USBRESET");
            if (RT_SUCCESS(rc))
            {
                /* give the thread a chance to get started. */
                RTThreadUserWait(pDev->hResetThread, 2);
                return rc;
            }

            pDev->pvResetArgs  = NULL;
            pDev->hResetThread = NIL_RTTHREAD;
            RTMemTmpFree(pArgs);
        }
        /* fall back to sync on failure */
    }

    /*
     * Sync fashion.
     */
    int rc = vusbDevResetWorker(pDev, fResetOnLinux);
    vusbDevResetDone(pDev, rc, pfnDone, pvUser);
    return rc;
}


/**
 * Powers on the device.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLCALLBACK(int) vusbDevPowerOn(PVUSBIDEVICE pInterface)
{
    PVUSBDEV pDev = (PVUSBDEV)pInterface;
    LogFlow(("vusbDevPowerOn: pDev=%p[%s]\n", pDev, pDev->pUsbIns->pszName));

    /*
     * Check that the device is in a valid state.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    if (enmState == VUSB_DEVICE_STATE_DETACHED)
    {
        Log(("vusb: warning: attempt to power on detached device %p[%s]\n", pDev, pDev->pUsbIns->pszName));
        return VERR_VUSB_DEVICE_NOT_ATTACHED;
    }
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: power on ignored, the device is resetting!\n", pDev->pUsbIns->pszName));
        return VERR_VUSB_DEVICE_IS_RESETTING;
    }

    /*
     * Do the job.
     */
    if (enmState == VUSB_DEVICE_STATE_ATTACHED)
        pDev->enmState = VUSB_DEVICE_STATE_POWERED;

    return VINF_SUCCESS;
}


/**
 * Powers off the device.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLCALLBACK(int) vusbDevPowerOff(PVUSBIDEVICE pInterface)
{
    PVUSBDEV pDev = (PVUSBDEV)pInterface;
    LogFlow(("vusbDevPowerOff: pDev=%p[%s]\n", pDev, pDev->pUsbIns->pszName));

    /*
     * Check that the device is in a valid state.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    if (enmState == VUSB_DEVICE_STATE_DETACHED)
    {
        Log(("vusb: warning: attempt to power off detached device %p[%s]\n", pDev, pDev->pUsbIns->pszName));
        return VERR_VUSB_DEVICE_NOT_ATTACHED;
    }
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: power off ignored, the device is resetting!\n", pDev->pUsbIns->pszName));
        return VERR_VUSB_DEVICE_IS_RESETTING;
    }

    /*
     * If it's a root hub, we will have to cancel all URBs and reap them.
     */
    if (vusbDevIsRh(pDev))
    {
        PVUSBROOTHUB pRh = (PVUSBROOTHUB)pDev;
        VUSBIRhCancelAllUrbs(&pRh->IRhConnector);
        VUSBIRhReapAsyncUrbs(&pRh->IRhConnector, 0);
    }

    pDev->enmState = VUSB_DEVICE_STATE_ATTACHED;

    return VINF_SUCCESS;
}


/**
 * Get the state of the device.
 *
 * @returns Device state.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLCALLBACK(VUSBDEVICESTATE) vusbDevGetState(PVUSBIDEVICE pInterface)
{
    return ((PVUSBDEV)pInterface)->enmState;
}


/**
 * The maximum number of interfaces the device can have in all of it's configuration.
 *
 * @returns Number of interfaces.
 * @param   pDev        The device.
 */
size_t vusbDevMaxInterfaces(PVUSBDEV pDev)
{
    uint8_t cMax = 0;
    unsigned i = pDev->pDescCache->pDevice->bNumConfigurations;
    while (i-- > 0)
    {
        if (pDev->pDescCache->paConfigs[i].Core.bNumInterfaces > cMax)
            cMax = pDev->pDescCache->paConfigs[i].Core.bNumInterfaces;
    }

    return cMax;
}


/**
 * Initialize a new VUSB device.
 *
 * @returns VBox status code.
 * @param   pDev    The VUSB device to initialize.
 * @param   pUsbIns Pointer to the PDM USB Device instance.
 */
int vusbDevInit(PVUSBDEV pDev, PPDMUSBINS pUsbIns)
{
    /*
     * Initialize the device data members.
     * (All that are Non-Zero at least.)
     */
    Assert(!pDev->IDevice.pfnReset);
    pDev->IDevice.pfnReset = vusbDevReset;
    Assert(!pDev->IDevice.pfnPowerOn);
    pDev->IDevice.pfnPowerOn = vusbDevPowerOn;
    Assert(!pDev->IDevice.pfnPowerOff);
    pDev->IDevice.pfnPowerOff = vusbDevPowerOff;
    Assert(!pDev->IDevice.pfnGetState);
    pDev->IDevice.pfnGetState = vusbDevGetState;
    pDev->pUsbIns = pUsbIns;
    pDev->pNext = NULL;
    pDev->pNextHash = NULL;
    pDev->pHub = NULL;
    pDev->enmState = VUSB_DEVICE_STATE_DETACHED;
    pDev->u8Address = VUSB_INVALID_ADDRESS;
    pDev->u8NewAddress = VUSB_INVALID_ADDRESS;
    pDev->i16Port = -1;
    pDev->u16Status = 0;
    pDev->pDescCache = NULL;
    pDev->pCurCfgDesc = NULL;
    pDev->paIfStates = NULL;
    memset(&pDev->aPipes[0], 0, sizeof(pDev->aPipes));
    pDev->hResetThread = NIL_RTTHREAD;
    pDev->pvResetArgs = NULL;
    pDev->pResetTimer = NULL;

    /*
     * Create the reset timer.
     */
    int rc = PDMUsbHlpTMTimerCreate(pUsbIns, TMCLOCK_VIRTUAL, vusbDevResetDoneTimer, pDev, 0 /*fFlags*/,
                                    "USB Device Reset Timer",  &pDev->pResetTimer);
    AssertRCReturn(rc, rc);

    /*
     * Get the descriptor cache from the device. (shall cannot fail)
     */
    pDev->pDescCache = pUsbIns->pReg->pfnUsbGetDescriptorCache(pUsbIns);
    Assert(pDev->pDescCache);
#ifdef VBOX_STRICT
    if (pDev->pDescCache->fUseCachedStringsDescriptors)
    {
        int32_t iPrevId = -1;
        for (unsigned iLang = 0; iLang < pDev->pDescCache->cLanguages; iLang++)
        {
            Assert((int32_t)pDev->pDescCache->paLanguages[iLang].idLang > iPrevId);
            iPrevId = pDev->pDescCache->paLanguages[iLang].idLang;

            int32_t                 idxPrevStr = -1;
            PCPDMUSBDESCCACHESTRING paStrings  = pDev->pDescCache->paLanguages[iLang].paStrings;
            unsigned                cStrings   = pDev->pDescCache->paLanguages[iLang].cStrings;
            for (unsigned iStr = 0; iStr < cStrings; iStr++)
            {
                Assert((int32_t)paStrings[iStr].idx > idxPrevStr);
                idxPrevStr = paStrings[iStr].idx;
                size_t cch = strlen(paStrings[iStr].psz);
                Assert(cch <= 127);
            }
        }
    }
#endif

    /*
     * Allocate memory for the interface states.
     */
    size_t cbIface = vusbDevMaxInterfaces(pDev) * sizeof(*pDev->paIfStates);
    pDev->paIfStates = (PVUSBINTERFACESTATE)RTMemAllocZ(cbIface);
    AssertMsgReturn(pDev->paIfStates, ("RTMemAllocZ(%d) failed\n", cbIface), VERR_NO_MEMORY);

    return VINF_SUCCESS;
}

/*
 * Local Variables:
 *  mode: c
 *  c-file-style: "bsd"
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: s
 * End:
 */

