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
#include <VBox/com/ptr.h>
#include "HostPower.h"
#include "Logging.h"

HostPowerService::HostPowerService(VirtualBox *aVirtualBox)
{
    mVirtualBox = aVirtualBox;
}

HostPowerService::~HostPowerService()
{
}


void HostPowerService::notify(HostPowerEvent event)
{
    switch (event)
    {
    case HostPowerEvent_Suspend:
        Log(("HostPowerService::notify SUSPEND\n"));
        break;

    case HostPowerEvent_Resume:
        Log(("HostPowerService::notify RESUME\n"));
        break;

    case HostPowerEvent_BatteryLow:
        Log(("HostPowerService::notify BATTERY LOW\n"));
        break;
    }

    VirtualBox::SessionMachineVector machines;
    mVirtualBox->getOpenedMachines (machines);

    for (size_t i = 0; i < machines.size(); i++)
        processEvent(machines[i], event);

    machines.clear();
}

HRESULT HostPowerService::processEvent(SessionMachine *machine, HostPowerEvent event)
{
    MachineState_T state;
    HRESULT        rc;

    rc = machine->COMGETTER(State)(&state);
    CheckComRCReturnRC (rc);

    if (state == MachineState_Running)
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
                    break;
                case HostPowerEvent_Resume:
                case HostPowerEvent_BatteryLow:
                    break;
                }
            }
        }
fail:
        session->Close();

    }
    return rc;
}
