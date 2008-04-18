/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VBoxBFE VM control routines
 *
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <iprt/stream.h>
#include <VBox/err.h>
#include "DisplayImpl.h"
#include "ConsoleImpl.h"
#include "VBoxBFE.h"
#include "VMControl.h"

/**
 * Fullscreen / Windowed toggle.
 */
int
VMCtrlToggleFullscreen(void)
{
    /* not allowed */
    if (!gfAllowFullscreenToggle)
        return VERR_ACCESS_DENIED;

    gFramebuffer->setFullscreen(!gFramebuffer->getFullscreen());

    /*
     * We have switched from/to fullscreen, so request a full
     * screen repaint, just to be sure.
     */
    gDisplay->InvalidateAndUpdate();

    return VINF_SUCCESS;
}

/**
 * Pause the VM.
 */
int
VMCtrlPause(void)
{
    if (machineState != VMSTATE_RUNNING)
        return VERR_VM_INVALID_VM_STATE;

    if (gConsole->inputGrabbed())
        gConsole->inputGrabEnd();

    PVMREQ pReq;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)VMR3Suspend, 1, pVM);
    AssertRC(rcVBox);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    return VINF_SUCCESS;
}

/**
 * Resume the VM.
 */
int
VMCtrlResume(void)
{
    if (machineState != VMSTATE_SUSPENDED)
        return VERR_VM_INVALID_VM_STATE;

    PVMREQ pReq;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)VMR3Resume, 1, pVM);
    AssertRC(rcVBox);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    return VINF_SUCCESS;
}

/**
 * Reset the VM
 */
int
VMCtrlReset(void)
{
    PVMREQ pReq;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)VMR3Reset, 1, pVM);
    AssertRC(rcVBox);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    return VINF_SUCCESS;
}

/**
 * Send ACPI power button press event
 */
int
VMCtrlACPIPowerButton(void)
{
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (pVM, "acpi", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnPowerButtonPress(pPort) : VERR_INVALID_POINTER;
    }
    return VINF_SUCCESS;
}

/**
 * Send ACPI sleep button press event
 */
int
VMCtrlACPISleepButton(void)
{
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (pVM, "acpi", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnSleepButtonPress(pPort) : VERR_INVALID_POINTER;
    }
    return VINF_SUCCESS;
}

/**
 * Worker thread while saving the VM
 */
DECLCALLBACK(int) VMSaveThread(RTTHREAD Thread, void *pvUser)
{
    PVMREQ pReq;
    void (*pfnQuit)(void) = (void(*)(void))pvUser;
    int rc;

    startProgressInfo("Saving");
    rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                     (PFNRT)VMR3Save, 4, pVM, g_pszStateFile, &callProgressInfo, NULL);
    endProgressInfo();
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    pfnQuit();

    return 0;
}

/*
 * Save the machine's state
 */
int
VMCtrlSave(void (*pfnQuit)(void))
{
    int rc;

    if (!g_pszStateFile || !*g_pszStateFile)
        return VERR_INVALID_PARAMETER;

    gConsole->resetKeys();
    RTThreadYield();
    if (gConsole->inputGrabbed())
        gConsole->inputGrabEnd();
    RTThreadYield();

    if (machineState == VMSTATE_RUNNING)
    {
        PVMREQ pReq;
        rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                         (PFNRT)VMR3Suspend, 1, pVM);
        AssertRC(rc);
        if (VBOX_SUCCESS(rc))
        {
            rc = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
    }

    RTTHREAD thread;
    rc = RTThreadCreate(&thread, VMSaveThread, (void*)pfnQuit, 0,
                        RTTHREADTYPE_MAIN_WORKER, 0, "Save");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Error: Thread creation failed with %d\n", rc);
        return rc;
    }

    return VINF_SUCCESS;
}
