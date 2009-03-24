/** @file
 * VBoxGuest -- VirtualBox Win32 guest support driver
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

#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxGuest_Internal.h"
#include "Helper.h"
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

#ifndef TARGET_NT4
# include <aux_klib.h>
#endif


/**
 * Called when a bug check occurs.
 *
 * @param   pvBuffer        Pointer to our device extension, just in case it
 *                          comes in handy some day.
 * @param   cbBuffer        The size of our device extension.
 */
static VOID hlpVBoxGuestBugCheckCallback(PVOID pvBuffer, ULONG cbBuffer)
{
    /*PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pvBuffer;*/

    LogRelBackdoor(("Windows bug check (bluescreen) detected!\n"));

# ifndef TARGET_NT4
    /*
     * Try get details.
     */
    KBUGCHECK_DATA bugCheckData;
    bugCheckData.BugCheckDataSize = sizeof(KBUGCHECK_DATA);
    NTSTATUS rc = AuxKlibGetBugCheckData(&bugCheckData);
    if (NT_SUCCESS(rc))
    {
        LogRelBackdoor(("Bug check code: 0x%08x\n", bugCheckData.BugCheckCode));
        LogRelBackdoor(("Bug check parameters: 1=0x%p 2=0x%p 3=0x%p 4=0x%p\n",
                        bugCheckData.Parameter1,
                        bugCheckData.Parameter2,
                        bugCheckData.Parameter3,
                        bugCheckData.Parameter4));
        LogRelBackdoor(("For details run \"!analyze -show %x %p %p %p %p\" in WinDbg.\n",
                        bugCheckData.BugCheckCode,
                        bugCheckData.Parameter1,
                        bugCheckData.Parameter2,
                        bugCheckData.Parameter3,
                        bugCheckData.Parameter4));
    }
    else
        LogRelBackdoor(("Unable to retrieve bugcheck details! Error: %#x\n", rc));
# else  /* NT4 */
    LogRelBackdoor(("No additional information for Windows NT 4.0 bugcheck available.\n"));
# endif /* NT4 */

    /*
     * Notify the host that we've panicked.
     */
    /** @todo Notify the host somehow over DevVMM. */

    NOREF(pvBuffer); NOREF(cbBuffer);
}

void hlpRegisterBugCheckCallback(PVBOXGUESTDEVEXT pDevExt)
{
    pDevExt->bBugcheckCallbackRegistered = FALSE;

# ifndef TARGET_NT4
    /*
     * This is required for calling AuxKlibGetBugCheckData() in hlpVBoxGuestBugCheckCallback().
     */
    NTSTATUS rc = AuxKlibInitialize();
    if (NT_ERROR(rc))
    {
        LogRelBackdoor(("VBoxGuest: Failed to initialize AuxKlib! rc=%#x\n", rc));
        return;
    }
# endif

    /*
     * Setup bugcheck callback routine.
     */
    pDevExt->bBugcheckCallbackRegistered = FALSE;
    KeInitializeCallbackRecord(&pDevExt->bugcheckRecord);
    if (KeRegisterBugCheckCallback(&pDevExt->bugcheckRecord,
                                   &hlpVBoxGuestBugCheckCallback,
                                   pDevExt,
                                   sizeof(*pDevExt),
                                   (PUCHAR)"VBoxGuest"))
    {
        pDevExt->bBugcheckCallbackRegistered = TRUE;
        dprintf(("VBoxGuest::hlpRegisterBugCheckCallback: Bugcheck callback registered.\n"));
    }
    else
        LogRelBackdoor(("VBoxGuest: Could not register bugcheck callback routine!\n"));
}

void hlpDeregisterBugCheckCallback(PVBOXGUESTDEVEXT pDevExt)
{
    if (pDevExt->bBugcheckCallbackRegistered)
    {
        if (!KeDeregisterBugCheckCallback(&pDevExt->bugcheckRecord))
            dprintf(("VBoxGuest::VBoxGuestUnload: Unregistering bugcheck callback routine failed!\n"));
        pDevExt->bBugcheckCallbackRegistered = FALSE;
    }
}

#endif /* VBOX_WITH_GUEST_BUGCHECK_DETECTION */

