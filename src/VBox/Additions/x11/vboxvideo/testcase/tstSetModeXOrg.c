/* $Id$ */
/** @file
 * vboxvideo unit test - modesetting.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include "../vboxvideo.h"

#include <VBox/VBoxVideoGuest.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/test.h>

#include <xf86.h>

void xf86Msg(MessageType type, const char *format, ...) {}
void xf86DrvMsg(int scrnIndex, MessageType type, const char *format, ...) {}

static ScrnInfoRec scrnInfo[1];
static ScrnInfoPtr pScrns[1] = { &scrnInfo[0] };
ScrnInfoPtr *xf86Screens = pScrns;

Bool vbox_device_available(VBOXPtr pVBox)
{
    return TRUE;
}

void VBOXDRIUpdateStride(ScrnInfoPtr pScrn, VBOXPtr pVBox) {}

static struct
{
    uint16_t cWidth;
    uint16_t cHeight;
    uint16_t cVirtWidth;
    uint16_t cBPP;
    uint16_t fFlags;
    uint16_t cx;
    uint16_t cy;
} s_ModeRegs;

RTDECL(void) VBoxVideoSetModeRegisters(uint16_t cWidth, uint16_t cHeight,
                                       uint16_t cVirtWidth, uint16_t cBPP,
                                       uint16_t fFlags,
                                       uint16_t cx, uint16_t cy)
{
    s_ModeRegs.cWidth     = cWidth;
    s_ModeRegs.cHeight    = cHeight;
    s_ModeRegs.cVirtWidth = cVirtWidth;
    s_ModeRegs.cBPP       = cBPP;
    s_ModeRegs.fFlags     = fFlags;
    s_ModeRegs.cx         = cx;
    s_ModeRegs.cy         = cy;
}

static struct
{
    PHGSMIGUESTCOMMANDCONTEXT pCtx;
    uint32_t cDisplay;
    int32_t  cOriginX;
    int32_t  cOriginY;
    uint32_t offStart;
    uint32_t cbPitch;
    uint32_t cWidth;
    uint32_t cHeight;
    uint16_t cBPP;
    uint16_t fFlags;
} s_DisplayInfo;

void VBoxHGSMIProcessDisplayInfo(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                 uint32_t cDisplay, int32_t  cOriginX,
                                 int32_t  cOriginY, uint32_t offStart,
                                 uint32_t cbPitch, uint32_t cWidth,
                                 uint32_t cHeight, uint16_t cBPP,
                                 uint16_t fFlags)
{
    s_DisplayInfo.pCtx     = pCtx;
    s_DisplayInfo.cDisplay = cDisplay;
    s_DisplayInfo.cOriginX = cOriginX;
    s_DisplayInfo.cOriginY = cOriginY;
    s_DisplayInfo.offStart = offStart;
    s_DisplayInfo.cbPitch  = cbPitch;
    s_DisplayInfo.cWidth   = cWidth;
    s_DisplayInfo.cHeight  = cHeight;
    s_DisplayInfo.cBPP     = cBPP;
    s_DisplayInfo.fFlags   = fFlags;
}


static int setup(void)
{
    return VINF_SUCCESS;
}

static void teardown(void)
{
}

int main(void)
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVBoxVideoXOrg", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Run the tests.
     */
    AssertRC(setup());
    teardown();

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

