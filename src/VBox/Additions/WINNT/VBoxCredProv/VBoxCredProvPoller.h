/* $Id$ */
/** @file
 * VBoxCredPoller - Thread for retrieving user credentials.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvPoller_h
#define GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvPoller_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

class VBoxCredProvProvider;

class VBoxCredProvPoller
{
public:
    VBoxCredProvPoller(void);
    virtual ~VBoxCredProvPoller(void);

    int Initialize(VBoxCredProvProvider *pProvider);
    int Shutdown(void);

protected:
    /** Static function for our poller routine, used in an own thread to
     * check for credentials on the host. */
    static DECLCALLBACK(int) threadPoller(RTTHREAD ThreadSelf, void *pvUser);

    /** Thread handle. */
    RTTHREAD              m_hThreadPoller;
    /** Pointer to parent. Needed for notifying if credentials
     *  are available. */
    VBoxCredProvProvider *m_pProv;
};

#endif /* !GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvPoller_h */

