/* $Id: VBoxCredPoller.h 60692 2010-04-27 08:22:32Z umoeller $ */
/** @file
 * VBoxCredPoller - Thread for retrieving user credentials.
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

#ifndef __VBOX_CREDPROV_POLLER_H__
#define __VBOX_CREDPROV_POLLER_H__

#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

class VBoxCredProvProvider;
//class VBoxCredProvCredential;

class VBoxCredProvPoller
{
    public:

        VBoxCredProvPoller(void);

        virtual ~VBoxCredProvPoller(void);

    public:

        int Initialize(VBoxCredProvProvider *pProvider);
        int Shutdown(void);
        //VBoxCredProvProvider* GetProvider(void) { return m_pProv; }
        //bool QueryCredentials(VBoxCredProvCredential *pCred);

    protected:

       /* void credentialsReset(void);
        int credentialsRetrieve(void);*/
        static int threadPoller(RTTHREAD ThreadSelf, void *pvUser);

    protected:

        RTTHREAD              m_hThreadPoller;  /* Thread handle */
        //RTCRITSECT            m_csCredUpate;
        VBoxCredProvProvider *m_pProv;          /* Pointer to parent */

    #if 0
        char                 *m_pszUser;        /* Our lovely credentials */
        char                 *m_pszPw;
        char                 *m_pszDomain;
    #endif
};

#endif /* !__VBOX_CREDPROV_POLLER_H__ */

