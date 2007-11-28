/** @file
 *
 * Guest client: seamless mode
 * Proxy between the guest and host components
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __Additions_client_seamless_glue_h
# define __Additions_client_seamless_glue_h

#if 0

#include "seamless-host.h"
#include "seamless-guest.h"

#endif

class VBoxGuestSeamlessObserver
{
public:
    virtual void notify(void) = 0;
    virtual ~VBoxGuestSeamlessObserver() {}
};

#if 0
/**
 * Observer to connect the guest and the host parts of the seamless guest client.
 * It starts the host part when the guest reports that it supports seamless mode and
 * stops it when the guest no longer supports seamless.  It also proxies enter and
 * exit seamless mode requests from the host part to the guest and informs the host
 * part when the guest's visible area changes.
 */
class VBoxGuestSeamlessGlue
{
private:
    VBoxGuestSeamlessHost *mHost;
    VBoxGuestSeamlessGuestImpl *mGuest;

public:
    VBoxGuestSeamlessGlue(VBoxGuestSeamlessHost *pHost, VBoxGuestSeamlessGuestImpl *pGuest)
            : mHost(pHost), mGuest(pGuest) {}
    ~VBoxGuestSeamlessGlue();

    /** State change notification callback for the guest. */
    void guestNotify(VBoxGuestSeamlessGuest::meEvent event)
    {
        switch(event)
        {
            case VBoxGuestSeamlessGuest::CAPABLE:
                mHost->start();
                break;
            case VBoxGuestSeamlessGuest::INCAPABLE:
                mHost->stop();
                break;
            case VBoxGuestSeamlessGuest::UPDATE:
                mHost->updateRects(mGuest->getRects());
                break;
            default:
                break;
        }
    }

    /** State change notification callback for the host. */
    void hostNotify(VBoxGuestSeamlessHost::meEvent event)
    {
        switch(event)
        {
            case VBoxGuestSeamlessHost::ENABLE:
                mGuest->start();
                break;
            case VBoxGuestSeamlessHost::DISABLE:
                mGuest->stop();
                break;
            default:
                break;
        }
    }
};
#endif /* 0 */

#endif /* __Additions_client_seamless_glue_h not defined */
