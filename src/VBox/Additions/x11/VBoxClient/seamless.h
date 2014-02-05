/** @file
 *
 * Guest client: seamless mode.
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

#ifndef __Additions_xclient_seamless_h
# define __Additions_xclient_seamless_h

#include <VBox/log.h>

#include "seamless-host.h"
#include "seamless-x11.h"

class VBoxGuestSeamless
{
private:
    VBoxGuestSeamlessHost mHost;
    VBoxGuestSeamlessX11 mGuest;

    bool isInitialised;
public:
    int init(void)
    {
        int rc = VINF_SUCCESS;

        LogRelFlowFunc(("\n"));
        if (isInitialised)  /* Assertion */
        {
            LogRelFunc(("error: called a second time! (VBoxClient)\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        if (RT_SUCCESS(rc))
        {
            rc = mHost.init(&mGuest);
        }
        if (RT_SUCCESS(rc))
        {
            rc = mGuest.init(&mHost);
        }
        if (RT_SUCCESS(rc))
        {
            rc = mHost.start();
        }
        if (RT_SUCCESS(rc))
        {
            isInitialised = true;
        }
        if (RT_FAILURE(rc))
        {
            LogRelFunc(("returning %Rrc (VBoxClient)\n", rc));
        }
        LogRelFlowFunc(("returning %Rrc\n", rc));
        return rc;
    }

    void uninit(RTMSINTERVAL cMillies = RT_INDEFINITE_WAIT)
    {
        LogRelFlowFunc(("\n"));
        if (isInitialised)
        {
            mHost.stop(cMillies);
            mGuest.uninit();
            isInitialised = false;
        }
        LogRelFlowFunc(("returning\n"));
    }

    VBoxGuestSeamless() { isInitialised = false; }
    ~VBoxGuestSeamless() { uninit(); }
};

#endif /* __Additions_xclient_seamless_h not defined */
