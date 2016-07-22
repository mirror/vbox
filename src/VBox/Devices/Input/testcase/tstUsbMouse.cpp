/* $Id$ */
/** @file
 * tstUsbMouse.cpp - testcase USB mouse and tablet devices.
 */

/*
 * Copyright (C) 2013-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxDD.h"
#include <VBox/vmm/pdmdrv.h>
#include <iprt/alloc.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/uuid.h>

/** Test mouse driver structure. */
typedef struct DRVTSTMOUSE
{
    /** The USBHID structure. */
    struct USBHID              *pUsbHid;
    /** The base interface for the mouse driver. */
    PDMIBASE                    IBase;
    /** Our mouse connector interface. */
    PDMIMOUSECONNECTOR          IConnector;
    /** The base interface of the attached mouse port. */
    PPDMIBASE                   pDrvBase;
    /** The mouse port interface of the attached mouse port. */
    PPDMIMOUSEPORT              pDrv;
    /** Is relative mode currently supported? */
    bool                        fRel;
    /** Is absolute mode currently supported? */
    bool                        fAbs;
    /** Is multi-touch mode currently supported? */
    bool                        fMT;
} DRVTSTMOUSE, *PDRVTSTMOUSE;


/** Global mouse driver variable.
 * @todo To be improved some time. */
static DRVTSTMOUSE s_drvTstMouse;


/** @interface_method_impl{PDMUSBHLPR3,pfnVMSetErrorV} */
static DECLCALLBACK(int) tstVMSetErrorV(PPDMUSBINS pUsbIns, int rc,
                                        RT_SRC_POS_DECL, const char *pszFormat,
                                        va_list va)
{
    NOREF(pUsbIns);
    RTPrintf("Error: %s:%u:%s:", RT_SRC_POS_ARGS);
    RTPrintfV(pszFormat, va);
    return rc;
}

/** @interface_method_impl{PDMUSBHLPR3,pfnDriverAttach} */
/** @todo We currently just take the driver interface from the global
 * variable.  This is sufficient for a unit test but still a bit sad. */
static DECLCALLBACK(int) tstDriverAttach(PPDMUSBINS pUsbIns, RTUINT iLun,
                                         PPDMIBASE pBaseInterface,
                                         PPDMIBASE *ppBaseInterface,
                                         const char *pszDesc)
{
    NOREF(iLun);
    NOREF(pszDesc);
    s_drvTstMouse.pDrvBase = pBaseInterface;
    s_drvTstMouse.pDrv = PDMIBASE_QUERY_INTERFACE(pBaseInterface,
                                                  PDMIMOUSEPORT);
    *ppBaseInterface = &s_drvTstMouse.IBase;
    return VINF_SUCCESS;
}


static PDMUSBHLP s_tstUsbHlp;


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) tstMouseQueryInterface(PPDMIBASE pInterface,
                                                   const char *pszIID)
{
    PDRVTSTMOUSE pThis = RT_FROM_MEMBER(pInterface, DRVTSTMOUSE, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSECONNECTOR, &pThis->IConnector);
    return NULL;
}


/**
 * @interface_method_impl{PDMIMOUSECONNECTOR,pfnReportModes}
 */
static DECLCALLBACK(void) tstMouseReportModes(PPDMIMOUSECONNECTOR pInterface,
                                              bool fRel, bool fAbs, bool fMT)
{
    PDRVTSTMOUSE pDrv = RT_FROM_MEMBER(pInterface, DRVTSTMOUSE, IConnector);
    pDrv->fRel = fRel;
    pDrv->fAbs = fAbs;
    pDrv->fMT  = fMT;
}


static int tstMouseConstruct(int iInstance, const char *pcszMode,
                             uint8_t u8CoordShift, PPDMUSBINS *ppThis)
{
    int rc = VERR_NO_MEMORY;
    PPDMUSBINS pThis = (PPDMUSBINS)RTMemAllocZ(  sizeof(*pThis)
                                               + g_UsbHidMou.cbInstance);
    PCFGMNODE pCfg = NULL;
    if (pThis)
    pCfg = CFGMR3CreateTree(NULL);
    if (pCfg)
        rc = CFGMR3InsertString(pCfg, "Mode", pcszMode);
    if (RT_SUCCESS(rc))
        rc = CFGMR3InsertInteger(pCfg, "CoordShift", u8CoordShift);
    if (RT_SUCCESS(rc))
    {
        s_drvTstMouse.pDrv = NULL;
        s_drvTstMouse.pDrvBase = NULL;
        pThis->iInstance = iInstance;
        pThis->pHlpR3 = &s_tstUsbHlp;
        rc = g_UsbHidMou.pfnConstruct(pThis, iInstance, pCfg, NULL);
        if (RT_SUCCESS(rc))
        {
           *ppThis = pThis;
           return rc;
        }
    }
    /* Failure */
    if (pCfg)
        CFGMR3DestroyTree(pCfg);
    if (pThis)
        RTMemFree(pThis);
    return rc;
}


static void testConstructAndDestruct(RTTEST hTest)
{
    PPDMUSBINS pThis;
    RTTestSub(hTest, "simple construction and destruction");
    int rc = tstMouseConstruct(0, "relative", 1, &pThis);
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (pThis)
        g_UsbHidMou.pfnDestruct(pThis);
}


static void testSendPositionRel(RTTEST hTest)
{
    PPDMUSBINS pThis = NULL;
    VUSBURB Urb;
    RTTestSub(hTest, "sending a relative position event");
    int rc = tstMouseConstruct(0, "relative", 1, &pThis);
    RT_ZERO(Urb);
    if (RT_SUCCESS(rc))
        rc = g_UsbHidMou.pfnUsbReset(pThis, false);
    if (RT_SUCCESS(rc) && !s_drvTstMouse.pDrv)
        rc = VERR_PDM_MISSING_INTERFACE;
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (RT_SUCCESS(rc))
    {
        s_drvTstMouse.pDrv->pfnPutEvent(s_drvTstMouse.pDrv, 123, -16, 1, -1, 3);
        Urb.EndPt = 0x01;
        rc = g_UsbHidMou.pfnUrbQueue(pThis, &Urb);
    }
    if (RT_SUCCESS(rc))
    {
        PVUSBURB pUrb = g_UsbHidMou.pfnUrbReap(pThis, 0);
        if (pUrb)
        {
            if (pUrb == &Urb)
            {
                if (   Urb.abData[0] != 3    /* Buttons */
                    || Urb.abData[1] != 123  /* x */
                    || Urb.abData[2] != 240  /* 256 - y */
                    || Urb.abData[3] != 255  /* z */)
                    rc = VERR_GENERAL_FAILURE;
            }
            else
                rc = VERR_GENERAL_FAILURE;
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (pThis)
        g_UsbHidMou.pfnDestruct(pThis);
}


static void testSendPositionAbs(RTTEST hTest)
{
    PPDMUSBINS pThis = NULL;
    VUSBURB Urb;
    RTTestSub(hTest, "sending an absolute position event");
    int rc = tstMouseConstruct(0, "absolute", 1, &pThis);
    RT_ZERO(Urb);
    if (RT_SUCCESS(rc))
    {
        rc = g_UsbHidMou.pfnUsbReset(pThis, false);
    }
    if (RT_SUCCESS(rc))
    {
        if (s_drvTstMouse.pDrv)
            s_drvTstMouse.pDrv->pfnPutEventAbs(s_drvTstMouse.pDrv, 300, 200, 1,
                                               3, 3);
        else
            rc = VERR_PDM_MISSING_INTERFACE;
    }
    if (RT_SUCCESS(rc))
    {
        Urb.EndPt = 0x01;
        rc = g_UsbHidMou.pfnUrbQueue(pThis, &Urb);
    }
    if (RT_SUCCESS(rc))
    {
        PVUSBURB pUrb = g_UsbHidMou.pfnUrbReap(pThis, 0);
        if (pUrb)
        {
            if (pUrb == &Urb)
            {
                if (   Urb.abData[0] != 3                  /* Buttons */
                    || (int8_t)Urb.abData[1] != -1         /* dz */
                    || (int8_t)Urb.abData[2] != -3         /* dw */
                    || *(uint16_t *)&Urb.abData[4] != 150  /* x >> 1 */
                    || *(uint16_t *)&Urb.abData[6] != 100  /* y >> 1 */)
                    rc = VERR_GENERAL_FAILURE;
            }
            else
                rc = VERR_GENERAL_FAILURE;
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (pThis)
        g_UsbHidMou.pfnDestruct(pThis);
}

#if 0
/** @todo PDM interface was updated. This is not working anymore. */
static void testSendPositionMT(RTTEST hTest)
{
    PPDMUSBINS pThis = NULL;
    VUSBURB Urb;
    RTTestSub(hTest, "sending a multi-touch position event");
    int rc = tstMouseConstruct(0, "multitouch", 1, &pThis);
    RT_ZERO(Urb);
    if (RT_SUCCESS(rc))
    {
        rc = g_UsbHidMou.pfnUsbReset(pThis, false);
    }
    if (RT_SUCCESS(rc))
    {
        if (s_drvTstMouse.pDrv)
            s_drvTstMouse.pDrv->pfnPutEventMT(s_drvTstMouse.pDrv, 300, 200, 2,
                                              3);
        else
            rc = VERR_PDM_MISSING_INTERFACE;
    }
    if (RT_SUCCESS(rc))
    {
        Urb.EndPt = 0x01;
        rc = g_UsbHidMou.pfnUrbQueue(pThis, &Urb);
    }
    if (RT_SUCCESS(rc))
    {
        PVUSBURB pUrb = g_UsbHidMou.pfnUrbReap(pThis, 0);
        if (pUrb)
        {
            if (pUrb == &Urb)
            {
                if (   Urb.abData[0] != 1                  /* Report ID */
                    || Urb.abData[1] != 3                  /* Contact flags */
                    || *(uint16_t *)&Urb.abData[2] != 150  /* x >> 1 */
                    || *(uint16_t *)&Urb.abData[4] != 100  /* y >> 1 */
                    || Urb.abData[6] != 2                  /* Contact number */)
                    rc = VERR_GENERAL_FAILURE;
            }
            else
                rc = VERR_GENERAL_FAILURE;
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (pThis)
        g_UsbHidMou.pfnDestruct(pThis);
}
#endif

int main()
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstUsbMouse", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);
    /* Set up our faked PDMUSBHLP interface. */
    s_tstUsbHlp.pfnVMSetErrorV  = tstVMSetErrorV;
    s_tstUsbHlp.pfnDriverAttach = tstDriverAttach;
    /* Set up our global mouse driver */
    s_drvTstMouse.IBase.pfnQueryInterface = tstMouseQueryInterface;
    s_drvTstMouse.IConnector.pfnReportModes = tstMouseReportModes;

    /*
     * Run the tests.
     */
    testConstructAndDestruct(hTest);
    testSendPositionRel(hTest);
    testSendPositionAbs(hTest);
    /* testSendPositionMT(hTest); */
    return RTTestSummaryAndDestroy(hTest);
}
