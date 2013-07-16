/* $Id$ */
/** @file
 * Main unit test - Mouse class.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
*   Header Files                                                              *
******************************************************************************/
#include "MouseImpl.h"
#include "VMMDev.h"
#include "DisplayImpl.h"

#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/VMMDev.h>
#include <iprt/assert.h>
#include <iprt/test.h>

NS_DECL_CLASSINFO(Mouse)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Mouse, IMouse)

PDMIVMMDEVPORT VMMDevPort;

class TestVMMDev : public VMMDevMouseInterface
{
    PPDMIVMMDEVPORT getVMMDevPort(void) { return &VMMDevPort; }
};

class TestDisplay : public DisplayMouseInterface
{
    void getFramebufferDimensions(int32_t *px1, int32_t *py1,
                                  int32_t *px2, int32_t *py2);
    int getScreenResolution(uint32_t cScreen, uint32_t *pcx, uint32_t *pcy,
                            uint32_t *pcBPP);
};

class TestConsole : public ConsoleMouseInterface
{
public:
    VMMDevMouseInterface *getVMMDevMouseInterface() { return &mVMMDev; }
    DisplayMouseInterface *getDisplayMouseInterface() { return &mDisplay; }
    /** @todo why on earth is this not implemented? */
    void onMouseCapabilityChange(BOOL supportsAbsolute, BOOL supportsRelative,
                                 BOOL supportsMT, BOOL needsHostCursor) {}

private:
    TestVMMDev mVMMDev;
    TestDisplay mDisplay;
};

static int pdmdrvhlpAttach(PPDMDRVINS pDrvIns, uint32_t fFlags,
                           PPDMIBASE *ppBaseInterface)
{
    return VERR_PDM_NO_ATTACHED_DRIVER;
}

static struct PDMDRVHLPR3 pdmHlpR3 =
{
    PDM_DRVHLPR3_VERSION,
    pdmdrvhlpAttach
};

static struct
{
    int32_t cx;
    int32_t cy;
} mouseEvent;

static int mousePutEvent(PPDMIMOUSEPORT pInterface, int32_t iDeltaX,
                         int32_t iDeltaY, int32_t iDeltaZ, int32_t iDeltaW,
                         uint32_t fButtonStates)
{
    mouseEvent.cx = iDeltaX;
    mouseEvent.cy = iDeltaY;
    return VINF_SUCCESS;
}

static struct
{
    int32_t x;
    int32_t y;
} mouseEventAbs;

static int mousePutEventAbs(PPDMIMOUSEPORT pInterface, uint32_t uX,
                            uint32_t uY, int32_t iDeltaZ, int32_t iDeltaW,
                            uint32_t fButtonStates)
{
    mouseEventAbs.x = uX;
    mouseEventAbs.y = uY;
    return VINF_SUCCESS;
}

static struct PDMIMOUSEPORT pdmiMousePort =
{
    mousePutEvent,
    mousePutEventAbs
};

static void *pdmiBaseQuery(struct PDMIBASE *pInterface, const char *pszIID)
{
    return &pdmiMousePort;
}

static struct PDMIBASE pdmiBase =
{
    pdmiBaseQuery
};

static struct PDMDRVINS pdmdrvInsCore =
{
    PDM_DRVINS_VERSION,
    0,
    NIL_RTRCPTR,
    NIL_RTRCPTR,
    NIL_RTR0PTR,
    NIL_RTR0PTR,
    &pdmHlpR3,
    NULL,
    NULL,
    NULL,
    &pdmiBase
};

static struct PDMDRVINS *ppdmdrvIns = NULL;

Mouse *pMouse;
ConsoleMouseInterface *pConsole;

static struct
{
    int32_t x;
    int32_t y;
} absoluteMouse;

static int setAbsoluteMouse(PPDMIVMMDEVPORT, int32_t x, int32_t y)
{
    absoluteMouse.x = x;
    absoluteMouse.y = y;
    return VINF_SUCCESS;
}

static int updateMouseCapabilities(PPDMIVMMDEVPORT, uint32_t, uint32_t)
{
    return VINF_SUCCESS;
}

void TestDisplay::getFramebufferDimensions(int32_t *px1, int32_t *py1,
                                           int32_t *px2, int32_t *py2)
{
    if (px1)
        *px1 = -320;
    if (py1)
        *py1 = -240;
    if (px2)
        *px2 = 320;
    if (py2)
        *py2 = 240;
}

int TestDisplay::getScreenResolution(uint32_t cScreen, uint32_t *pcx,
                                     uint32_t *pcy, uint32_t *pcBPP)
{
    NOREF(cScreen);
    if (pcx)
        *pcx = 640;
    if (pcy)
        *pcy = 480;
    if (pcBPP)
        *pcBPP = 32;
    return S_OK;
}

DECLEXPORT(bool) CFGMR3AreValuesValid(PCFGMNODE, const char *)
{
    return true;
}

DECLEXPORT(int) CFGMR3QueryPtr(PCFGMNODE, const char *, void **pv)
{
    *pv = pMouse;
    return VINF_SUCCESS;
}

/******************************************************************************
*   Main test code                                                            *
******************************************************************************/

static int setup(void)
{
    VMMDevPort.pfnSetAbsoluteMouse = setAbsoluteMouse;
    VMMDevPort.pfnUpdateMouseCapabilities = updateMouseCapabilities;
    pMouse = new Mouse;
    Assert(SUCCEEDED(pMouse->FinalConstruct()));
    pConsole = new TestConsole;
    pMouse->init(pConsole);
    ppdmdrvIns = (struct PDMDRVINS *) RTMemAllocZ(  sizeof(struct PDMDRVINS)
                                                  + Mouse::DrvReg.cbInstance);
    *ppdmdrvIns = pdmdrvInsCore;
    Mouse::DrvReg.pfnConstruct(ppdmdrvIns, NULL, 0);
    return VINF_SUCCESS;
}

static void teardown(void)
{
    delete pMouse;
    delete pConsole;
    RTMemFree(ppdmdrvIns);
}

static bool approxEq(int a, int b, int prec)
{
    return a - b < prec && b - a < prec;
}

/** @test testAbsToVMMDevNewProtocol */
static void testAbsToVMMDevNewProtocol(RTTEST hTest)
{
    PPDMIBASE pBase;
    PPDMIMOUSECONNECTOR pConnector;

    RTTestSub(hTest, "Absolute event to VMMDev, new protocol");
    pBase = &ppdmdrvIns->IBase;
    pConnector = (PPDMIMOUSECONNECTOR)pBase->pfnQueryInterface(pBase,
                                                 PDMIMOUSECONNECTOR_IID);
    pConnector->pfnReportModes(pConnector, true, false, false);
    pMouse->onVMMDevGuestCapsChange(  VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                    | VMMDEV_MOUSE_NEW_PROTOCOL);
    pMouse->PutMouseEventAbsolute(0, 0, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, 0x8000, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, 0x8000, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    pMouse->PutMouseEventAbsolute(-319, -239, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, 0, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, 0, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    pMouse->PutMouseEventAbsolute(320, 240, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, 0xffff, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, 0xffff, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    RTTestSubDone(hTest);
}

/** @test testAbsToVMMDevOldProtocol */
static void testAbsToVMMDevOldProtocol(RTTEST hTest)
{
    PPDMIBASE pBase;
    PPDMIMOUSECONNECTOR pConnector;

    RTTestSub(hTest, "Absolute event to VMMDev, old protocol");
    pBase = &ppdmdrvIns->IBase;
    pConnector = (PPDMIMOUSECONNECTOR)pBase->pfnQueryInterface(pBase,
                                                 PDMIMOUSECONNECTOR_IID);
    pConnector->pfnReportModes(pConnector, true, false, false);
    pMouse->onVMMDevGuestCapsChange(VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE);
    pMouse->PutMouseEventAbsolute(320, 240, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, 0x8000, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, 0x8000, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    pMouse->PutMouseEventAbsolute(0, 0, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, 0, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, 0, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    pMouse->PutMouseEventAbsolute(-319, -239, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.x, -0x8000, 200),
                      ("absoluteMouse.x=%d\n", absoluteMouse.x));
    RTTESTI_CHECK_MSG(approxEq(absoluteMouse.y, -0x8000, 200),
                      ("absoluteMouse.y=%d\n", absoluteMouse.y));
    RTTestSubDone(hTest);
}

/** @test testAbsToAbsDev */
static void testAbsToAbsDev(RTTEST hTest)
{
    PPDMIBASE pBase;
    PPDMIMOUSECONNECTOR pConnector;

    RTTestSub(hTest, "Absolute event to absolute device");
    pBase = &ppdmdrvIns->IBase;
    pConnector = (PPDMIMOUSECONNECTOR)pBase->pfnQueryInterface(pBase,
                                                 PDMIMOUSECONNECTOR_IID);
    pConnector->pfnReportModes(pConnector, false, true, false);
    pMouse->onVMMDevGuestCapsChange(  VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                    | VMMDEV_MOUSE_NEW_PROTOCOL);
    pMouse->PutMouseEventAbsolute(0, 0, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.x, 0x8000, 200),
                      ("mouseEventAbs.x=%d\n", mouseEventAbs.x));
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.y, 0x8000, 200),
                      ("mouseEventAbs.y=%d\n", mouseEventAbs.y));
    pMouse->PutMouseEventAbsolute(-319, -239, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.x, 0, 200),
                      ("mouseEventAbs.x=%d\n", mouseEventAbs.x));
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.y, 0, 200),
                      ("mouseEventAbs.y=%d\n", mouseEventAbs.y));
    pMouse->PutMouseEventAbsolute(320, 240, 0, 0, 0);
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.x, 0xffff, 200),
                      ("mouseEventAbs.x=%d\n", mouseEventAbs.x));
    RTTESTI_CHECK_MSG(approxEq(mouseEventAbs.y, 0xffff, 200),
                      ("mouseEventAbs.y=%d\n", mouseEventAbs.y));
    mouseEventAbs.x = mouseEventAbs.y = 0xffff;
    pMouse->PutMouseEventAbsolute(-640, -480, 0, 0, 0);
    RTTESTI_CHECK_MSG(mouseEventAbs.x = 0xffff,
                      ("mouseEventAbs.x=%d\n", mouseEventAbs.x));
    RTTESTI_CHECK_MSG(mouseEventAbs.y == 0xffff,
                      ("mouseEventAbs.y=%d\n", mouseEventAbs.y));
    RTTestSubDone(hTest);
}

/** @todo generate this using the @test blocks above */
typedef void (*PFNTEST)(RTTEST);
static PFNTEST g_tests[] =
{
    testAbsToVMMDevNewProtocol,
    testAbsToVMMDevOldProtocol,
    testAbsToAbsDev,
    NULL
};

int main(void)
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstMouseImpl", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Run the tests.
     */
    for (unsigned i = 0; g_tests[i]; ++i)
    {
        AssertRC(setup());
        g_tests[i](hTest);
        teardown();
    }

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

