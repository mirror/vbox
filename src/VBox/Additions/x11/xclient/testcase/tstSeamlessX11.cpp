/** @file
 * Linux seamless guest additions simulator in host.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iostream>

#include <iprt/semaphore.h>
#include <iprt/runtime.h>
#include <VBox/VBoxGuest.h>

#include "../seamless.h"

static RTSEMEVENT eventSem;

int VbglR3SeamlessSendRects(uint32_t cRects, PRTRECT pRects)
{
    std::cout << "Received rectangle update (" << cRects << " rectangles):" << std::endl;
    for (unsigned i = 0; i < cRects; ++i)
    {
        std::cout << "  xLeft: " << pRects[i].xLeft << "  yTop: " << pRects[i].yTop
                  << "  xRight: " << pRects[i].xRight << "  yBottom: " << pRects[i].yBottom
                  << std::endl;
    }
    return true;
}

int VbglR3SeamlessSetCap(bool bState)
{
    std::cout << (bState ? "Seamless capability set" : "Seamless capability unset")
              << std::endl;
    return true;
}

int VbglR3CtlFilterMask(uint32_t u32OrMask, uint32_t u32NotMask)
{
    std::cout << "IRQ filter mask changed.  Or mask: 0x" << std::hex << u32OrMask
              << ".  Not mask: 0x" << u32NotMask << std::dec
              << std::endl;
    return true;
}

int VbglR3SeamlessWaitEvent(VMMDevSeamlessMode *pMode)
{
    static bool active = false;

    int rc = VINF_SUCCESS;
    if (!active)
    {
        active = true;
        *pMode = VMMDev_Seamless_Visible_Region;
    }
    else
    {
        rc = RTSemEventWait(eventSem, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            rc = VERR_INTERRUPTED;
        }
    }
    return true;
}

int VbglR3InterruptEventWaits(void)
{
    return RTSemEventSignal(eventSem);
}

/**
 * Xlib error handler for certain errors that we can't avoid.
 */
int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    if (pError->error_code == BadWindow)
    {
        /* This can be triggered if a guest application destroys a window before we notice. */
        std::cout << "ignoring BadAtom error and returning" << std::endl;
        return 0;
    }
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    std::cout << "An X Window protocol error occurred: " << errorText << std::endl
              << "  Request code: " << int(pError->request_code) << std::endl
              << "  Minor code: " << int(pError->minor_code) << std::endl
              << "  Serial number of the failed request: " << int(pError->serial)
              << std::endl;
    std::cout << std::endl << "exiting." << std::endl;
    exit(1);
}

int main( int argc, char **argv)
{
    int rc = VINF_SUCCESS;
    std::string sTmp;

    RTR3Init(false);
    std::cout << "VirtualBox guest additions X11 seamless mode testcase" << std::endl;
    if (0 == XInitThreads())
    {
        std::cout << "Failed to initialise X11 threading, exiting." << std::endl;
        exit(1);
    }
    /* Set an X11 error handler, so that we don't die when we get unavoidable errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    std::cout << std::endl << "Press <Enter> to exit..." << std::endl;
    RTSemEventCreate(&eventSem);
    /** Our instance of the seamless class. */
    VBoxGuestSeamless seamless;
    try
    {
        LogRel(("Starting seamless Guest Additions...\n"));
        rc = seamless.init();
        if (rc != VINF_SUCCESS)
        {
            std::cout << "Failed to initialise seamless Additions, rc = " << rc << std::endl;
        }
    }
    catch (std::exception e)
    {
        std::cout << "Failed to initialise seamless Additions - caught exception: " << e.what()
                  << std::endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        std::cout << "Failed to initialise seamless Additions - caught unknown exception.\n"
                  << std::endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
    std::getline(std::cin, sTmp);
    try
    {
        seamless.uninit();
    }
    catch (std::exception e)
    {
        std::cout << "Error shutting down seamless Additions - caught exception: " << e.what()
                  << std::endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        std::cout << "Error shutting down seamless Additions - caught unknown exception.\n"
                  << std::endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
    return rc;
}
