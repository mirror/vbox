/* $Id$ */
/** @file
 * VBoxCredProvProvider - The actual credential provider class.
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

#ifndef __VBOX_CREDPROV_PROVIDER_H__
#define __VBOX_CREDPROV_PROVIDER_H__

#include <credentialprovider.h>
#include <windows.h>
#include <strsafe.h>

#include <VBox/VboxGuestLib.h>

#include "VBoxCredProvCredential.h"
#include "VBoxCredProvPoller.h"

class VBoxCredProvProvider : public ICredentialProvider
{
    public:

        /** IUnknown methods. */
        IFACEMETHODIMP_(ULONG) AddRef(void);
        IFACEMETHODIMP_(ULONG) Release(void);
        IFACEMETHODIMP         QueryInterface(REFIID interfaceID, void **ppvInterface);

    public:

        /** ICredentialProvider interface. */
        IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpUsageScenario, DWORD dwFlags);
        IFACEMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpCredentialSerialization);

        IFACEMETHODIMP Advise(__in ICredentialProviderEvents *pcpEvents, UINT_PTR upAdviseContext);
        IFACEMETHODIMP UnAdvise();

        IFACEMETHODIMP GetFieldDescriptorCount(__out DWORD* pdwCount);
        IFACEMETHODIMP GetFieldDescriptorAt(DWORD dwIndex,  __deref_out CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpFieldDescriptor);

        IFACEMETHODIMP GetCredentialCount(__out DWORD *pdwCount,
                                          __out DWORD *pdwDefault,
                                          __out BOOL *pbAutoLogonWithDefault);
        IFACEMETHODIMP GetCredentialAt(DWORD dwIndex,
                                       __out ICredentialProviderCredential **ppcpc);
    public:

        friend HRESULT VBoxCredProvProviderCreate(REFIID riid, __deref_out void **ppv);

    protected:

        VBoxCredProvProvider(void);

        virtual ~VBoxCredProvProvider(void);

    public:

        DWORD LoadConfiguration(void);
        bool HandleCurrentSession(void);

        /** Events. */
        void OnCredentialsProvided(void);

    private:

        /** Interface reference count. */
        ULONG                                    m_cRefCount;
        /** Our one and only credential. */
        VBoxCredProvCredential                  *m_pCred;
        /** Poller thread for credential lookup. */
        VBoxCredProvPoller                      *m_pPoller;
        /** Used to tell our owner to re-enumerate credentials. */
        ICredentialProviderEvents               *m_pCredProvEvents;
        /** Used to tell our owner who we are when asking to re-enumerate credentials. */
        UINT_PTR                                 m_upAdviseContext;
        /** Saved usage scenario. */
        CREDENTIAL_PROVIDER_USAGE_SCENARIO       m_cpUsageScenario;
        /** Flag indicating we got some credentials to work with. */
        bool                                     m_fGotCredentials;
        /** Flag whether we need to handle remote session over Windows Remote
         *  Desktop Service. */
        bool                                     m_fHandleRemoteSessions;
};

#endif /* !__VBOX_CREDPROV_PROVIDER_H__ */

