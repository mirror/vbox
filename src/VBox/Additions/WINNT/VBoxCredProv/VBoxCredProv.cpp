//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Modifications (c) 2009-2011 Oracle Corporation
//

#include <credentialprovider.h>

#include <iprt/err.h>
#include <VBox/VBoxGuestLib.h>

#include "VBoxCredProv.h"
#include "VBoxCredential.h"
#include "guid.h"


VBoxCredProv::VBoxCredProv(void) :
    m_cRef(1),
    m_pPoller(NULL),
    m_pCred(NULL),
    m_pCredProvEvents(NULL),
    m_fGotCredentials(false),
    m_fHandleRemoteSessions(false)
{
    LONG l = DllAddRef();

    int rc = RTR3InitDll(0); /* Never terminate the runtime! */
    if (RT_FAILURE(rc))
        LogRel(("VBoxCredProv: Could not init runtime! rc = %Rrc\n", rc));
    rc = VbglR3Init();
    if (RT_FAILURE(rc))
        LogRel(("VBoxCredProv: Could not init guest library! rc = %Rrc\n", rc));

    if (l == 1) /* First instance? */
        LogRel(("VBoxCredProv: Loaded.\n"));
    Log(("VBoxCredProv: DLL refcount (load) = %ld\n", l));
}


VBoxCredProv::~VBoxCredProv(void)
{
    Log(("VBoxCredProv::~VBoxCredProv\n"));

    if (m_pCred != NULL)
    {
        m_pCred->Release();
        m_pCred = NULL;
    }

    if (m_pPoller != NULL)
    {
        m_pPoller->Shutdown();
        delete m_pPoller;
        m_pPoller = NULL;
    }

    LONG lRefCount = DllGetRefCount();
    if (lRefCount == 1) /* First (=last) instance unloaded? */
        LogRel(("VBoxCredProv: Unloaded.\n"));
    Log(("VBoxCredProv: DLL refcount (unload) = %ld\n", lRefCount));

    VbglR3Term();
    DllRelease();
}


/**
 * Loads the global configuration from registry.
 *
 * @return  DWORD       Windows error code.
 */
DWORD VBoxCredProv::LoadConfiguration(void)
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
bool VBoxCredProv::HandleCurrentSession(void)
{
    bool fHandle = false;
    if (isRemoteSession())
    {
        if (m_fHandleRemoteSessions) /* Force remote session handling. */
            fHandle = true;
    }
    else /* No remote session. */
        fHandle = true;
    return fHandle;
}


/*
 * SetUsageScenario is the provider's cue that it's going to be asked for tiles
 * in a subsequent call. This call happens after the user pressed CTRL+ALT+DEL
 * and we need to handle the CPUS_LOGON event.
 */
HRESULT VBoxCredProv::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpUsageScenario,
                                       DWORD                              dwFlags)
{
    UNREFERENCED_PARAMETER(dwFlags);
    HRESULT hr;
    DWORD dwErr;

    m_cpUsageScenario = cpUsageScenario;

    /* Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
     * that we're not designed for that scenario. */
    switch (m_cpUsageScenario)
    {
        case CPUS_LOGON:
        case CPUS_UNLOCK_WORKSTATION:

            dwErr = LoadConfiguration();
            if (dwErr != ERROR_SUCCESS)
                LogRel(("VBoxCredProv: Error while loading configuration, error=%ld\n", dwErr));
            /* Do not stop running on a misconfigured system. */

            /*
             * If we're told to not handle the current session just bail out and let the
             * user know.
             */
            if (!HandleCurrentSession())
            {
                LogRel(("VBoxCredProv: Handling of remote desktop sessions is disabled.\n"));
                hr = S_OK; /* Just report back that everything is fine to prevent winlogon from stopping. */
                break;
            }

            if (m_pPoller == NULL)
            {
                m_pPoller = new VBoxCredPoller();
                Assert(m_pPoller);
                if (false ==  m_pPoller->Initialize(this))
                    Log(("VBoxCredProv::SetUsageScenario: Could not initialize credentials poller thread!\n"));
            }

            if (m_pCred == NULL)
            {
                m_pCred = new VBoxCredential(this);

                /* All stuff allocated? */
                if (m_pCred != NULL)
                {
                    hr = m_pCred->Initialize(m_cpUsageScenario,
                                             s_rgCredProvFieldDescriptors,
                                             s_rgFieldStatePairs);
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    Log(("VBoxCredProv::SetUsageScenario: Out of memory!\n"));
                }
            }
            else
            {
                /* All set up already! */
                hr = S_OK;
            }

            /* If we did fail -> cleanup */
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

    Log(("VBoxCredProv::SetUsageScenario returned 0x%08x (cpUS: %d, Flags: %ld)\n", hr, cpUsageScenario, dwFlags));
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
STDMETHODIMP VBoxCredProv::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpCredentialSerialization)
{
    UNREFERENCED_PARAMETER(pcpCredentialSerialization);
    return E_NOTIMPL;
}


/*
 * Called by LogonUI to give you a callback.  Providers often use the callback if they
 * some event would cause them to need to change the set of tiles (visible UI elements)
 * that they enumerated.
 */
HRESULT VBoxCredProv::Advise(ICredentialProviderEvents *pcpEvents,
                             UINT_PTR                   upAdviseContext)
{
    Log(("VBoxCredProv::Advise\n"));

    if (m_pCredProvEvents != NULL)
        m_pCredProvEvents->Release();

    m_pCredProvEvents = pcpEvents;
    Assert(m_pCredProvEvents);
    m_pCredProvEvents->AddRef();

    /*
     * Save advice context for later use when binding to
     * certain ICredentialProviderEvents events.
     */
    m_upAdviseContext = upAdviseContext;
    return S_OK;
}


/* Called by LogonUI when the ICredentialProviderEvents callback is no longer valid. */
HRESULT VBoxCredProv::UnAdvise()
{
    Log(("VBoxCredProv::UnAdvise\n"));
    if (m_pCredProvEvents != NULL)
    {
        m_pCredProvEvents->Release();
        m_pCredProvEvents = NULL;
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
HRESULT VBoxCredProv::GetFieldDescriptorCount(DWORD *pdwCount)
{
    Assert(pdwCount);
    *pdwCount = SFI_NUM_FIELDS;

    Log(("VBoxCredProv::GetFieldDescriptorCount: %ld\n", *pdwCount));
    return S_OK;
}


/* Gets the field descriptor for a particular field. */
HRESULT VBoxCredProv::GetFieldDescriptorAt(DWORD dwIndex,
                                           CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpFieldDescriptor)
{
    /* Verify dwIndex is a valid field */
    HRESULT hr;
    if (   dwIndex < SFI_NUM_FIELDS
        && ppcpFieldDescriptor)
    {
        hr = FieldDescriptorCoAllocCopy(s_rgCredProvFieldDescriptors[dwIndex], ppcpFieldDescriptor);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    Log(("VBoxCredProv::GetFieldDescriptorAt: hr=0x%08x, index=%ld, ppcpfd=%p\n",
         hr, dwIndex, ppcpFieldDescriptor));
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
HRESULT VBoxCredProv::GetCredentialCount(DWORD *pdwCount,
                                         DWORD *pdwDefault,
                                         BOOL  *pbAutoLogonWithDefault)
{
    Assert(pdwCount);
    Assert(pdwDefault);
    Assert(pbAutoLogonWithDefault);

    bool fGotCredentials = false;

    /* Poller thread create/active? */
    if (   m_pPoller
        && m_pCred)
    {
        fGotCredentials = m_pPoller->QueryCredentials(m_pCred);
    }

    if (fGotCredentials)
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

    Log(("VBoxCredProv::GetCredentialCount: *pdwCount=%ld, *pdwDefault=%ld, *pbAutoLogonWithDefault=%s\n",
         *pdwCount, *pdwDefault, *pbAutoLogonWithDefault ? "true" : "false"));
    return S_OK;
}


/*
 * Returns the credential at the index specified by dwIndex. This function is called by logonUI to enumerate
 * the tiles.
 */
HRESULT VBoxCredProv::GetCredentialAt(DWORD                           dwIndex,
                                      ICredentialProviderCredential **ppCredProvCredential)
{
    HRESULT hr;

    Log(("VBoxCredProv::GetCredentialAt: Index=%ld, ppCredProvCredential=%p\n", dwIndex, ppCredProvCredential));

    if (m_pCred == NULL)
    {
        Log(("VBoxCredProv::GetCredentialAt: No credentials available.\n"));
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
        Log(("VBoxCredProv::GetCredentialAt: More than one credential not supported!\n"));
        hr = E_INVALIDARG;
    }
    return hr;
}


/* Do a credential re-enumeration if we got the event to do so. */
void VBoxCredProv::OnCredentialsProvided()
{
    Log(("VBoxCredProv::OnCredentialsProvided\n"));

    if (m_pCredProvEvents != NULL)
        m_pCredProvEvents->CredentialsChanged(m_upAdviseContext);
}


/* Creates our provider. This happens *before* CTRL-ALT-DEL was pressed! */
HRESULT VBoxCredProv_CreateInstance(REFIID riid, void** ppv)
{
    HRESULT hr;

    VBoxCredProv* pProvider = new VBoxCredProv();
    if (pProvider)
    {
        hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}

