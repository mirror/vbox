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
#include <iprt/mem.h>
#include <VBox/com/ptr.h>
#include "HostPower.h"
#include "Logging.h"

HostPowerService::HostPowerService(VirtualBox *aVirtualBox) : aMachineSuspended(NULL), cbMachineSuspended(0)
{
    mVirtualBox = aVirtualBox;
}

HostPowerService::~HostPowerService()
{
}


void HostPowerService::notify(HostPowerEvent event)
{
    VirtualBox::SessionMachineVector machines;
    mVirtualBox->getOpenedMachines (machines);

    switch (event)
    {
    case HostPowerEvent_Suspend:
        if (machines.size())
            aMachineSuspended = (BOOL *)RTMemAllocZ(sizeof(BOOL) * machines.size());

        cbMachineSuspended = machines.size();

        for (size_t i = 0; i < machines.size(); i++)
            processEvent(machines[i], HostPowerEvent_Suspend, (aMachineSuspended) ? &aMachineSuspended[i] : NULL);

        Log(("HostPowerService::notify SUSPEND\n"));
        break;

    case HostPowerEvent_Resume:
        Log(("HostPowerService::notify RESUME\n"));

        if (aMachineSuspended)
        {
            /* It's possible (in theory) that machines are created or destroyed between the suspend notification and the actual host suspend.
             * Ignore this edge case and just make sure not to access invalid data.
             */
            cbMachineSuspended = RT_MIN(machines.size(), cbMachineSuspended);

            for (size_t i = 0; i < cbMachineSuspended; i++)
                processEvent(machines[i], HostPowerEvent_Resume, &aMachineSuspended[i]);

            RTMemFree(aMachineSuspended);
            cbMachineSuspended = 0;
            aMachineSuspended  = NULL;
        }
        break;

    case HostPowerEvent_BatteryLow:
        Log(("HostPowerService::notify BATTERY LOW\n"));
        for (size_t i = 0; i < machines.size(); i++)
            processEvent(machines[i], HostPowerEvent_BatteryLow, NULL);
        break;
    }

    machines.clear();
}

HRESULT HostPowerService::processEvent(SessionMachine *machine, HostPowerEvent event, BOOL *pMachineSuspended)
{
    MachineState_T state;
    HRESULT        rc;

    rc = machine->COMGETTER(State)(&state);
    CheckComRCReturnRC (rc);

    /* Valid combinations:
     * running & suspend or battery low notification events
     * pause   & resume or battery low notification events
     */
    if (    (state == MachineState_Running && event != HostPowerEvent_Resume)
        ||  (state == MachineState_Paused  && event != HostPowerEvent_Suspend))
    {
        ComPtr <ISession> session;

        rc = session.createInprocObject (CLSID_Session);
        if (FAILED (rc))
            return rc;

        /* get the IInternalSessionControl interface */
        ComPtr <IInternalSessionControl> control = session;
        if (!control)
        {
            rc = E_INVALIDARG;
            goto fail;
        }

        rc = machine->openExistingSession (control);
        if (SUCCEEDED (rc))
        {
            /* get the associated console */
            ComPtr<IConsole> console;
            rc = session->COMGETTER(Console)(console.asOutParam());
            if (SUCCEEDED (rc))
            {
                switch (event)
                {
                case HostPowerEvent_Suspend:
                    rc = console->Pause();
                    if (    SUCCEEDED(rc)
                        &&  pMachineSuspended)
                        *pMachineSuspended = TRUE;
                    break;

                case HostPowerEvent_Resume:
                    Assert(pMachineSuspended);
                    if (*pMachineSuspended == TRUE)
                        rc = console->Resume();
                    break;

                case HostPowerEvent_BatteryLow:
                {
                    ComPtr<IProgress> progress;

                    rc = console->SaveState(progress.asOutParam());
                    if (SUCCEEDED(rc))
                    {
                        /* Wait until the operation has been completed. */
                        progress->WaitForCompletion(-1); 

                        progress->COMGETTER(ResultCode)(&rc);
                        AssertMsg(SUCCEEDED(rc), ("SaveState WaitForCompletion failed with %x\n", rc));
                    }

                    break;
                }

                } /* switch (event) */
            }
        }
fail:
        session->Close();
    }
    return rc;
}
