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

#include <credentialprovider.h>

#include <iprt/err.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxCredentialProvider.h"
#include "VBoxCredProvProvider.h"
#include "VBoxCredProvCredential.h"

VBoxCredProvProvider::VBoxCredProvProvider(void) :
    m_cRefCount(1),
    m_pPoller(NULL),
    m_pCred(NULL),
    m_pEvents(NULL),
    m_fHandleRemoteSessions(false)
{
    VBoxCredentialProviderAcquire();

    VBoxCredProvReportStatus(VBoxGuestFacilityStatus_Init);
}


VBoxCredProvProvider::~VBoxCredProvProvider(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProv: Destroying\n");

    if (m_pCred)
    {
        m_pCred->Release();
        m_pCred = NULL;
    }

    if (m_pPoller)
    {
        m_pPoller->Shutdown();
        delete m_pPoller;
        m_pPoller = NULL;
    }

    VBoxCredProvReportStatus(VBoxGuestFacilityStatus_Terminated);

    VBoxCredentialProviderRelease();
}


/* IUnknown overrides. */
ULONG VBoxCredProvProvider::AddRef(void)
{
    LONG cRefCount = InterlockedIncrement(&m_cRefCount);
    VBoxCredProvVerbose(0, "VBoxCredProv: AddRef: Returning refcount=%ld\n",
                        cRefCount);
    return cRefCount;
}


ULONG VBoxCredProvProvider::Release(void)
{
    LONG cRefCount = InterlockedDecrement(&m_cRefCount);
    VBoxCredProvVerbose(0, "VBoxCredProv: Release: Returning refcount=%ld\n",
                        cRefCount);
    if (!cRefCount)
    {
        VBoxCredProvVerbose(0, "VBoxCredProv: Calling destructor\n");
        delete this;
    }
    return cRefCount;
}


HRESULT VBoxCredProvProvider::QueryInterface(REFIID interfaceID, void **ppvInterface)
{
    HRESULT hr = S_OK;;
    if (ppvInterface)
    {
        if (   IID_IUnknown            == interfaceID
            || IID_ICredentialProvider == interfaceID)
        {
            *ppvInterface = static_cast<IUnknown*>(this);
            reinterpret_cast<IUnknown*>(*ppvInterface)->AddRef();
        }
        else
        {
            *ppvInterface = NULL;
            hr = E_NOINTERFACE;
        }
    }
    else
        hr = E_INVALIDARG;

    return hr;
}


/**
 * Loads the global configuration from registry.
 *
 * @return  DWORD       Windows error code.
 */
DWORD VBoxCredProvProvider::LoadConfiguration(void)
{
    HKEY hKey;
    /** @todo Add some registry wrapper function(s) as soon as we got more values to retrieve. */
    DWORD dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Oracle\\VirtualBox Guest Additions\\AutoLogon",
                               0L, KEY_QUERY_VALUE, &hKey);
    if (dwRet == ERROR_SUCCESS)
    {
        DWORD dwValue;
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(DWORD);

        dwRet = RegQueryValueEx(hKey, L"HandleRemoteSessions", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
        if (   dwRet  == ERROR_SUCCESS
            && dwType == REG_DWORD
            && dwSize == sizeof(DWORD))
        {
            m_fHandleRemoteSessions = true;
        }

        dwRet = RegQueryValueEx(hKey, L"LoggingEnabled", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
        if (   dwRet  == ERROR_SUCCESS
            && dwType == REG_DWORD
            && dwSize == sizeof(DWORD))
        {
            g_dwVerbosity = 1; /* Default logging level. */
        }

        if (g_dwVerbosity) /* Do we want logging at all? */
        {
            dwRet = RegQueryValueEx(hKey, L"LoggingLevel", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
            if (   dwRet  == ERROR_SUCCESS
                && dwType == REG_DWORD
                && dwSize == sizeof(DWORD))
            {
                g_dwVerbosity = dwValue;
            }
        }

        RegCloseKey(hKey);
    }
    /* Do not report back an error here yet. */
    return ERROR_SUCCESS;
}


/**
 * Determines whether we should handle the current session or not.
 *
 * @return  bool        true if we should handle this session, false if not.
 */
bool VBoxCredProvProvider::HandleCurrentSession(void)
{
    /* Load global configuration from registry. */
    int rc = LoadConfiguration();
    if (RT_FAILURE(rc))
        VBoxCredProvVerbose(0, "VBoxCredProv: Error loading global configuration, rc=%Rrc\n",
                            rc);

    bool fHandle = false;
    if (VbglR3AutoLogonIsRemoteSession())
    {
        if (m_fHandleRemoteSessions) /* Force remote session handling. */
            fHandle = true;
    }
    else /* No remote session. */
        fHandle = true;

    VBoxCredProvVerbose(3, "VBoxCredProv: Handling current session=%RTbool\n", fHandle);
    return fHandle;
}


/*
 * SetUsageScenario is the provider's cue that it's going to be asked for tiles
 * in a subsequent call. This call happens after the user pressed CTRL+ALT+DEL
 * and we need to handle the CPUS_LOGON event.
 */
HRESULT VBoxCredProvProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpUsageScenario,
                                               DWORD                              dwFlags)
{
    HRESULT hr = S_OK;
    DWORD dwErr;

    VBoxCredProvVerbose(0, "VBoxCredProv::SetUsageScenario: cpUS=%d, dwFlags=%ld\n",
                        cpUsageScenario, dwFlags);

    m_cpUsageScenario = cpUsageScenario;

    /* Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
     * that we're not designed for that scenario. */
    switch (m_cpUsageScenario)
    {
        case CPUS_LOGON:
        case CPUS_UNLOCK_WORKSTATION:

            VBoxCredProvReportStatus(VBoxGuestFacilityStatus_Active);

            dwErr = LoadConfiguration();
            if (dwErr != ERROR_SUCCESS)
                VBoxCredProvVerbose(0, "VBoxCredProv: Error while loading configuration, error=%ld\n", dwErr);
            /* Do not stop running on a misconfigured system. */

            /*
             * If we're told to not handle the current session just bail out and let the
             * user know.
             */
            if (!HandleCurrentSession())
                break;

            if (!m_pPoller)
            {
                m_pPoller = new VBoxCredProvPoller();
                AssertPtr(m_pPoller);
                int rc = m_pPoller->Initialize(this);
                if (RT_FAILURE(rc))
                    VBoxCredProvVerbose(0, "VBoxCredProv::SetUsageScenario: Error initializing poller thread, rc=%Rrc\n", rc);
            }

            if (!m_pCred)
            {
                m_pCred = new VBoxCredProvCredential();
                if (m_pCred)
                {
                    hr = m_pCred->Initialize(m_cpUsageScenario);
                }
                else
                    hr = E_OUTOFMEMORY;
            }
            else
            {
                /* All set up already! Nothing to do here right now. */
            }

            /* If we failed, do some cleanup. */
            if (FAILED(hr))
            {
                if (m_pCred != NULL)
                {
                    m_pCred->Release();
                    m_pCred = NULL;
                }
            }
            break;

        case CPUS_CHANGE_PASSWORD:
        case CPUS_CREDUI:
        case CPUS_PLAP:

            hr = E_NOTIMPL;
            break;

        default:

            hr = E_INVALIDARG;
            break;
    }

    VBoxCredProvVerbose(0, "VBoxCredProv::SetUsageScenario returned hr=0x%08x\n", hr);
    return hr;
}


/*
 * SetSerialization takes the kind of buffer that you would normally return to LogonUI for
 * an authentication attempt.  It's the opposite of ICredentialProviderCredential::GetSerialization.
 * GetSerialization is implement by a credential and serializes that credential.  Instead,
 * SetSerialization takes the serialization and uses it to create a credential.
 *
 * SetSerialization is called for two main scenarios.  The first scenario is in the credUI case
 * where it is prepopulating a tile with credentials that the user chose to store in the OS.
 * The second situation is in a remote logon case where the remote client may wish to
 * prepopulate a tile with a username, or in some cases, completely populate the tile and
 * use it to logon without showing any UI.
 */
STDMETHODIMP VBoxCredProvProvider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpCredentialSerialization)
{
    NOREF(pcpCredentialSerialization);
    return E_NOTIMPL;
}


/*
 * Called by LogonUI to give you a callback.  Providers often use the callback if they
 * some event would cause them to need to change the set of tiles (visible UI elements)
 * that they enumerated.
 */
HRESULT VBoxCredProvProvider::Advise(ICredentialProviderEvents *pcpEvents,
                                     UINT_PTR                   upAdviseContext)
{
    VBoxCredProvVerbose(0, "VBoxCredProv::Advise, pcpEvents=0x%p, upAdviseContext=%u\n",
                        pcpEvents, upAdviseContext);
    if (m_pEvents)
    {
        m_pEvents->Release();
        m_pEvents = NULL;
    }

    m_pEvents = pcpEvents;
    if (m_pEvents)
        m_pEvents->AddRef();

    /*
     * Save advice context for later use when binding to
     * certain ICredentialProviderEvents events.
     */
    m_upAdviseContext = upAdviseContext;
    return S_OK;
}


/* Called by LogonUI when the ICredentialProviderEvents callback is no longer valid. */
HRESULT VBoxCredProvProvider::UnAdvise(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProv::UnAdvise: pEvents=0x%p\n",
                        m_pEvents);
    if (m_pEvents)
    {
        m_pEvents->Release();
        m_pEvents = NULL;
    }

    return S_OK;
}


/*
 * Called by LogonUI to determine the number of fields in your tiles. This
 * does mean that all your tiles must have the same number of fields.
 * This number must include both visible and invisible fields. If you want a tile
 * to have different fields from the other tiles you enumerate for a given usage
 * scenario you must include them all in this count and then hide/show them as desired
 * using the field descriptors.
 */
HRESULT VBoxCredProvProvider::GetFieldDescriptorCount(DWORD *pdwCount)
{
    if (pdwCount)
    {
        *pdwCount = VBOXCREDPROV_NUM_FIELDS;
        VBoxCredProvVerbose(0, "VBoxCredProv::GetFieldDescriptorCount: %ld\n", *pdwCount);
    }
    return S_OK;
}


/* Returns the field descriptor for a particular field. */
HRESULT VBoxCredProvProvider::GetFieldDescriptorAt(DWORD                                  dwIndex,
                                                   CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpFieldDescriptor)
{
    /* Verify dwIndex is a valid field. */
    HRESULT hr = S_OK;
    if (   dwIndex < VBOXCREDPROV_NUM_FIELDS
        && ppcpFieldDescriptor)
    {
        PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR pcpFieldDesc =
            (PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR)CoTaskMemAlloc(sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR));

        if (pcpFieldDesc)
        {
            const VBOXCREDPROV_FIELD &field = s_VBoxCredProvFields[dwIndex];

            ZeroMemory(pcpFieldDesc, sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR));

            pcpFieldDesc->dwFieldID = field.desc.dwFieldID;
            pcpFieldDesc->cpft      = field.desc.cpft;
            if (field.desc.pszLabel)
                hr = SHStrDupW(field.desc.pszLabel, &pcpFieldDesc->pszLabel);
        }
        else
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
        {
            *ppcpFieldDescriptor = pcpFieldDesc;
        }
        else
            CoTaskMemFree(pcpFieldDesc);
    }
    else
        hr = E_INVALIDARG;

    VBoxCredProvVerbose(0, "VBoxCredProv::GetFieldDescriptorAt: dwIndex=%ld, ppDesc=0x%p, hr=0x%08x\n",
                        dwIndex, ppcpFieldDescriptor, hr);
    return hr;
}


/*
 * Sets pdwCount to the number of tiles that we wish to show at this time.
 * Sets pdwDefault to the index of the tile which should be used as the default.
 *
 * The default tile is the tile which will be shown in the zoomed view by default. If
 * more than one provider specifies a default tile the behavior is the last used cred
 * prov gets to specify the default tile to be displayed
 *
 * If *pbAutoLogonWithDefault is TRUE, LogonUI will immediately call GetSerialization
 * on the credential you've specified as the default and will submit that credential
 * for authentication without showing any further UI.
 */
HRESULT VBoxCredProvProvider::GetCredentialCount(DWORD *pdwCount,
                                                 DWORD *pdwDefault,
                                                 BOOL  *pbAutoLogonWithDefault)
{
    AssertPtr(pdwCount);
    AssertPtr(pdwDefault);
    AssertPtr(pbAutoLogonWithDefault);

    bool fHasCredentials = false;

    /* Do we have credentials? */
    if (m_pCred)
    {
        int rc = m_pCred->RetrieveCredentials();
        fHasCredentials = rc == VINF_SUCCESS;
    }

    if (fHasCredentials)
    {
        *pdwCount = 1;                   /* This provider always has the same number of credentials (1). */
        *pdwDefault = 0;                 /* The credential we provide is *always* at index 0! */
        *pbAutoLogonWithDefault = TRUE;  /* We always at least try to auto-login (if password is correct). */
    }
    else
    {
        *pdwCount = 0;
        *pdwDefault = CREDENTIAL_PROVIDER_NO_DEFAULT;
        *pbAutoLogonWithDefault = FALSE;
    }

    VBoxCredProvVerbose(0, "VBoxCredProv::GetCredentialCount: *pdwCount=%ld, *pdwDefault=%ld, *pbAutoLogonWithDefault=%s\n",
                        *pdwCount, *pdwDefault, *pbAutoLogonWithDefault ? "true" : "false");
    return S_OK;
}


/*
 * Returns the credential at the index specified by dwIndex. This function is called by logonUI to enumerate
 * the tiles.
 */
HRESULT VBoxCredProvProvider::GetCredentialAt(DWORD                           dwIndex,
                                              ICredentialProviderCredential **ppCredProvCredential)
{
    HRESULT hr;

    VBoxCredProvVerbose(0, "VBoxCredProv::GetCredentialAt: Index=%ld, ppCredProvCredential=0x%p\n",
                        dwIndex, ppCredProvCredential);

    if (!m_pCred)
    {
        VBoxCredProvVerbose(0, "VBoxCredProv::GetCredentialAt: No credentials available\n");
        return E_INVALIDARG;
    }

    /* Validate parameters (we only have one credential). */
    if(   dwIndex == 0
       && ppCredProvCredential)
    {
        hr = m_pCred->QueryInterface(IID_ICredentialProviderCredential,
                                     reinterpret_cast<void**>(ppCredProvCredential));
    }
    else
    {
        VBoxCredProvVerbose(0, "VBoxCredProv::GetCredentialAt: More than one credential not supported!\n");
        hr = E_INVALIDARG;
    }
    return hr;
}


/* Do a credential re-enumeration if we got the event to do so. This then invokes
 * GetCredentialCount() and GetCredentialAt() called by Winlogon. */
void VBoxCredProvProvider::OnCredentialsProvided(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProv::OnCredentialsProvided\n");

    if (m_pEvents)
        m_pEvents->CredentialsChanged(m_upAdviseContext);
}


/* Creates our provider. This happens *before* CTRL-ALT-DEL was pressed! */
HRESULT VBoxCredProvProviderCreate(REFIID interfaceID, void **ppvInterface)
{
    HRESULT hr;

    VBoxCredProvProvider* pProvider = new VBoxCredProvProvider();
    if (pProvider)
    {
        hr = pProvider->QueryInterface(interfaceID, ppvInterface);
        pProvider->Release();
    }
    else
        hr = E_OUTOFMEMORY;

    return hr;
}

