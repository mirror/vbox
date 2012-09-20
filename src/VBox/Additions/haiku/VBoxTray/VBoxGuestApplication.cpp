/* $Id$ */
/** @file
 * VBoxGuestApplication, Haiku Guest Additions, implementation.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *                    François Revol <revol@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <errno.h>
#define DEBUG 1
#include <Alert.h>
#include <Debug.h>
#include <Invoker.h>
#include <String.h>

#include "VBoxClipboard.h"
#include "VBoxGuestApplication.h"
#include "VBoxGuestDeskbarView.h"

VBoxGuestApplication::VBoxGuestApplication()
     : BApplication(VBOX_GUEST_APP_SIG)
{
    PRINT(("%s()\n", __FUNCTION__));
}


VBoxGuestApplication::~VBoxGuestApplication()
{
    PRINT(("%s()\n", __FUNCTION__));
}


void VBoxGuestApplication::ReadyToRun()
{
    status_t err;

    err = VBoxGuestDeskbarView::AddToDeskbar();
    PRINT(("%s: VBoxGuestDeskbarView::AddToDeskbar: 0x%08lx\n", __FUNCTION__, err));

    exit(0);
}


int main(int argc, const char **argv)
{
    new VBoxGuestApplication();
    be_app->Run();
    delete be_app;

/*    int rc = RTR3Init();
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3Init();
        if (RT_SUCCESS(rc))
            rc = vboxOpenBaseDriver();
    }

    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestApp: Init successful\n"));

        new VBoxGuestApplication(gVBoxDriverFD);
        be_app->Run();
        delete be_app;

    }

    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestApp: Error while starting, rc=%Rrc\n", rc));
    }
    LogRel(("VBoxGuestApp: Ended\n"));

    vboxCloseBaseDriver();

    VbglR3Term();*/

    return 0;
}

