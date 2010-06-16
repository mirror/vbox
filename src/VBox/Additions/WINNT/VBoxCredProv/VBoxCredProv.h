//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Modifications (c) 2009-2010 Oracle Corporation
//

#ifndef ___VBoxCredProv_h
#define ___VBoxCredProv_h

#include <credentialprovider.h>
#include <windows.h>
#include <strsafe.h>

#include "VBoxCredential.h"
#include "VBoxCredPoller.h"
#include "helpers.h"

class VBoxCredProv : public ICredentialProvider
{
    public:

        /** IUnknown methods. */
        STDMETHOD_(ULONG, AddRef)()
        {
            return m_cRef++;
        }

        STDMETHOD_(ULONG, Release)()
        {
            LONG cRef = m_cRef--;
            if (!cRef)
            {
                delete this;
            }
            return cRef;
        }

        STDMETHOD (QueryInterface)(REFIID riid, void **ppv)
        {
            HRESULT hr;
            if (IID_IUnknown == riid ||
                IID_ICredentialProvider == riid)
            {
                *ppv = this;
                reinterpret_cast<IUnknown*>(*ppv)->AddRef();
                hr = S_OK;
            }
            else
            {
                *ppv = NULL;
                hr = E_NOINTERFACE;
            }
            return hr;
        }

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

        friend HRESULT VBoxCredProv_CreateInstance(REFIID riid, __deref_out void **ppv);

    protected:

        VBoxCredProv(void);
        virtual ~VBoxCredProv(void);

    public:

        /** Events. */
        void OnCredentialsProvided();
    private:

        /** Interface reference count. */
        LONG                                     m_cRef;                              
        /** Our one and only credential. */
        VBoxCredential                          *m_pCred;  
        /** Poller thread for credential lookup. */                           
        VBoxCredPoller                          *m_pPoller;
        /** Used to tell our owner to re-enumerate credentials. */
        ICredentialProviderEvents               *m_pCredProvEvents;
        /** Used to tell our owner who we are when asking to re-enumerate credentials. */           
        UINT_PTR                                 m_upAdviseContext;
        /** Saved usage scenario. */
        CREDENTIAL_PROVIDER_USAGE_SCENARIO       m_cpUsageScenario;
        /** Flag indicating we got some credentials to work with. */
        bool                                     m_fGotCredentials;
};

#endif /* ___VBoxCredProv_h */

