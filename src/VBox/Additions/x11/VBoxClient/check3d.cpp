/* $Id$ */
/** @file
 * X11 guest client - 3D pass-through check.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxClient.h"

#include <VBox/VBoxGuestLib.h>

#include <stdlib.h>

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    int rc;
    HGCMCLIENTID cClientID;
    LogFlowFunc(("\n"));

    NOREF(ppInterface);
    NOREF(fDaemonised);
    /* Initialise the guest library. */
    rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        exit(1);
    rc = VbglR3HGCMConnect("VBoxSharedCrOpenGL", &cClientID);
    if (RT_FAILURE(rc))
        exit(1);
    VbglR3HGCMDisconnect(cClientID);
    VbglR3Term();
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

static struct VBCLSERVICE g_vbclCheck3DInterface =
{
    NULL,
    VBClServiceDefaultHandler, /* init */
    run,
    VBClServiceDefaultCleanup
};
static struct VBCLSERVICE *g_pvbclCheck3DInterface = &g_vbclCheck3DInterface;

/* Static factory */
struct VBCLSERVICE **VBClCheck3DService()
{
    return &g_pvbclCheck3DInterface;
}

