/* $Id$ */
/** @file
 * VBoxCredPoller - Thread for retrieving user credentials.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxCredPoller_h
#define ___VBoxCredPoller_h

#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

class VBoxCredProv;
class VBoxCredential;

class VBoxCredPoller
{
    public:

        VBoxCredPoller();
        virtual ~VBoxCredPoller();

    public:

        bool Initialize(VBoxCredProv *pProvider);
        bool Shutdown();
        VBoxCredProv* GetProvider() { return m_pProv; }
        bool QueryCredentials(VBoxCredential *pCred);

    protected:

        void credentialsReset(void);
        int credentialsRetrieve(void);
        static int threadPoller(RTTHREAD ThreadSelf, void *pvUser);

    protected:

        RTTHREAD      m_hThreadPoller;  /* Thread handle */
        RTCRITSECT    m_csCredUpate;
        VBoxCredProv *m_pProv;          /* Pointer to parent */

        char         *m_pszUser;        /* Our lovely credentials */
        char         *m_pszPw;
        char         *m_pszDomain;

};

#endif /* ___VBoxCredPoller_h */
