/** @file
 *
 * VirtualBox interface to host's power notification service
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include "HostPower.h"
#include "Logging.h"

#include <VBox/com/ptr.h>

#include <iprt/mem.h>

HostPowerService::HostPowerService (VirtualBox *aVirtualBox)
{
    Assert (aVirtualBox != NULL);
    mVirtualBox = aVirtualBox;
}

HostPowerService::~HostPowerService()
{
}

void HostPowerService::notify (HostPowerEvent aEvent)
{
    VirtualBox::SessionMachineVector machines;
    VirtualBox::InternalControlVector controls;

    HRESULT rc = S_OK;

    switch (aEvent)
    {
        case HostPowerEvent_Suspend:
        {
            LogFunc (("SUSPEND\n"));

            mVirtualBox->getOpenedMachinesAndControls (machines, controls);

            /* pause running VMs */
            for (size_t i = 0; i < controls.size(); ++ i)
            {
                /* get the remote console */
                ComPtr <IConsole> console;
                rc = controls [i]->GetRemoteConsole (console.asOutParam());
                /* the VM could have been powered down and closed or whatever */
                if (FAILED (rc))
                    continue;

                /* note that Pause() will simply return a failure if the VM is
                 * in an inappropriate state */
                rc = console->Pause();
                if (FAILED (rc))
                    continue;

                /* save the control to un-pause the VM later */
                mConsoles.push_back (console);
            }

            LogFunc (("Suspended %d VMs\n", mConsoles.size()));

            break;
        }

        case HostPowerEvent_Resume:
        {
            LogFunc (("RESUME\n"));

            size_t resumed = 0;

            /* go through VMs we paused on Suspend */
            for (size_t i = 0; i < mConsoles.size(); ++ i)
            {
                /* note that Resume() will simply return a failure if the VM is
                 * in an inappropriate state (it will also fail if the VM has
                 * been somehow closed by this time already so that the
                 * console reference we have is dead) */
                rc = mConsoles [i]->Resume();
                if (FAILED (rc))
                    continue;

                ++ resumed;
            }

            LogFunc (("Resumed %d VMs\n", resumed));

            mConsoles.clear();

            break;
        }

        case HostPowerEvent_BatteryLow:
        {
            LogFunc (("BATTERY LOW\n"));

            mVirtualBox->getOpenedMachinesAndControls (machines, controls);

            size_t saved = 0;

            /* save running VMs */
            for (size_t i = 0; i < controls.size(); ++ i)
            {
                /* get the remote console */
                ComPtr <IConsole> console;
                rc = controls [i]->GetRemoteConsole (console.asOutParam());
                /* the VM could have been powered down and closed or whatever */
                if (FAILED (rc))
                    continue;

                ComPtr<IProgress> progress;

                /* note that SaveState() will simply return a failure if the VM
                 * is in an inappropriate state */
                rc = console->SaveState (progress.asOutParam());
                if (FAILED (rc))
                    continue;

                /* Wait until the operation has been completed. */
                LONG iRc;
                rc = progress->WaitForCompletion(-1);
                if (SUCCEEDED (rc))
                    progress->COMGETTER(ResultCode) (&iRc);
                rc = iRc;

                AssertMsg (SUCCEEDED (rc), ("SaveState WaitForCompletion "
                                            "failed with %Rhrc (%#08X)\n", rc, rc));

                if (SUCCEEDED (rc))
                    ++ saved;
            }

            LogFunc (("Saved %d VMs\n", saved));

            break;
        }
    }
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
