/** @file
 *
 * Guest client: seamless mode.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <unistd.h>

#include <X11/Xlib.h>

#include "VBoxClient.h"
#include "seamless.h"

class SeamlessService : public VBoxClient::Service
{
private:
    VBoxGuestSeamless mSeamless;
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-seamless.pid";
    }
    virtual int run(bool fDaemonised /* = false */)
    {
        /* Initialise threading in X11 and in Xt. */
        if (!XInitThreads())
        {
            LogRel(("VBoxClient: error initialising threads in X11, exiting.\n"));
            return VERR_NOT_SUPPORTED;
        }
        int rc = mSeamless.init();
        if (RT_FAILURE(rc))
            return rc;
        /* Stay running as long as X does... */
        Display *pDisplay = XOpenDisplay(NULL);
        XEvent ev;
        while (true)
            XNextEvent(pDisplay, &ev);
        return VERR_INTERRUPTED;
    }
    virtual void cleanup()
    {
        VbglR3SeamlessSetCap(false);
    }
};

VBoxClient::Service *VBoxClient::GetSeamlessService()
{
    return new SeamlessService;
}
