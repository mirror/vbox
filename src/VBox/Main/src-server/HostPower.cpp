/** @file
 *
 * VirtualBox interface to host's power notification service
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#include "HostPower.h"
#include "Logging.h"

#include <VBox/com/ptr.h>

#include "VirtualBoxImpl.h"

#include <iprt/mem.h>

HostPowerService::HostPowerService (VirtualBox *aVirtualBox)
{
    Assert(aVirtualBox != NULL);
    mVirtualBox = aVirtualBox;
}

HostPowerService::~HostPowerService()
{
}

void HostPowerService::notify(HostPowerEvent aEvent)
{
    SessionMachinesList machines;
    VirtualBox::InternalControlList controls;

    HRESULT rc = S_OK;

    switch (aEvent)
    {
        case HostPowerEvent_Suspend:
        {
            LogFunc (("SUSPEND\n"));

#ifdef VBOX_WITH_RESOURCE_USAGE_API
            /* Suspend performance sampling to avoid unnecessary callbacks due to jumps in time. */
            PerformanceCollector *perfcollector = mVirtualBox->performanceCollector();

            if (perfcollector)
                perfcollector->suspendSampling();
#endif
            mVirtualBox->getOpenedMachines(machines, &controls);

            /* pause running VMs */
            for (VirtualBox::InternalControlList::const_iterator it = controls.begin();
                 it != controls.end();
                 ++it)
            {
                ComPtr<IInternalSessionControl> pControl = *it;

                /* get the remote console */
                ComPtr<IConsole> console;
                rc = pControl->GetRemoteConsole(console.asOutParam());
                /* the VM could have been powered down and closed or whatever */
                if (FAILED(rc))
                    continue;

                /* note that Pause() will simply return a failure if the VM is
                 * in an inappropriate state */
                rc = console->Pause();
                if (FAILED(rc))
                    continue;

                /* save the control to un-pause the VM later */
                mConsoles.push_back(console);
            }

            LogFunc (("Suspended %d VMs\n", mConsoles.size()));

            break;
        }

        case HostPowerEvent_Resume:
        {
            LogFunc (("RESUME\n"));

            size_t resumed = 0;

            /* go through VMs we paused on Suspend */
            for (size_t i = 0; i < mConsoles.size(); ++i)
            {
                /* note that Resume() will simply return a failure if the VM is
                 * in an inappropriate state (it will also fail if the VM has
                 * been somehow closed by this time already so that the
                 * console reference we have is dead) */
                rc = mConsoles[i]->Resume();
                if (FAILED(rc))
                    continue;

                ++ resumed;
            }

            LogFunc (("Resumed %d VMs\n", resumed));

#ifdef VBOX_WITH_RESOURCE_USAGE_API
            /* Resume the performance sampling. */
            PerformanceCollector *perfcollector = mVirtualBox->performanceCollector();

            if (perfcollector)
                perfcollector->resumeSampling();
#endif

            mConsoles.clear();

            break;
        }

        case HostPowerEvent_BatteryLow:
        {
            LogFunc (("BATTERY LOW\n"));

            mVirtualBox->getOpenedMachines(machines, &controls);

            size_t saved = 0;

            /* save running VMs */
            for (VirtualBox::InternalControlList::const_iterator it = controls.begin();
                 it != controls.end();
                 ++it)
            {
                ComPtr<IInternalSessionControl> pControl = *it;
                /* get the remote console */
                ComPtr<IConsole> console;
                rc = pControl->GetRemoteConsole (console.asOutParam());
                /* the VM could have been powered down and closed or whatever */
                if (FAILED(rc))
                    continue;

                ComPtr<IProgress> progress;

                /* note that SaveState() will simply return a failure if the VM
                 * is in an inappropriate state */
                rc = console->SaveState (progress.asOutParam());
                if (FAILED(rc))
                    continue;

                /* Wait until the operation has been completed. */
                rc = progress->WaitForCompletion(-1);
                if (SUCCEEDED(rc))
                {
                    LONG iRc;
                    progress->COMGETTER(ResultCode)(&iRc);
                    rc = iRc;
                }

                AssertMsg (SUCCEEDED(rc), ("SaveState WaitForCompletion "
                                            "failed with %Rhrc (%#08X)\n", rc, rc));

                if (SUCCEEDED(rc))
                    ++ saved;
            }

            LogFunc (("Saved %d VMs\n", saved));

            break;
        }
    }
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
