/* $Id$ */
/** @file
 * VBoxCredProvCredential - Class for keeping and handling the passed
 *                          credentials.
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

#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif

#include "VBoxCredentialProvider.h"

#include "VBoxCredProvProvider.h"
#include "VBoxCredProvCredential.h"
#include "VBoxCredProvUtils.h"

#include <lm.h>

#include <iprt/mem.h>
#include <iprt/string.h>



VBoxCredProvCredential::VBoxCredProvCredential(VBoxCredProvProvider *pProvider) :
    m_cRefCount(1),
    m_pEvents(NULL)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Created, pProvider=%p\n",
                        pProvider);

    VBoxCredentialProviderAcquire();

    AssertPtr(pProvider);
    m_pProvider = pProvider;

    ZeroMemory(m_pwszCredentials, sizeof(m_pwszCredentials));
}


VBoxCredProvCredential::~VBoxCredProvCredential(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Destroying\n");

    VBoxCredentialProviderRelease();
}


/* IUnknown overrides. */
ULONG VBoxCredProvCredential::AddRef(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Increasing reference to %ld\n",
                        m_cRefCount + 1);

    return m_cRefCount++;
}


ULONG VBoxCredProvCredential::Release(void)
{
    Assert(m_cRefCount);

    ULONG cRefCount = --m_cRefCount;
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Decreasing reference to %ld\n",
                        cRefCount);
    if (!cRefCount)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvCredential: Calling destructor\n");
        delete this;
    }
    return cRefCount;
}


HRESULT VBoxCredProvCredential::QueryInterface(REFIID interfaceID, void **ppvInterface)
{
    HRESULT hr = S_OK;;
    if (ppvInterface)
    {
        if (   IID_IUnknown                      == interfaceID
            || IID_ICredentialProviderCredential == interfaceID)
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
 * Assigns or copies a RTUTF16 string to a UNICODE_STRING.
 *
 * When fCopy is false, this does *not* copy its contents
 * and only assigns its code points to the destination!
 * When fCopy is true, the actual string buffer gets copied.
 *
 * Does not take terminating \0 into account.
 *
 * @return  HRESULT
 * @param   pUnicodeDest            Unicode string assigning the UTF16 string to.
 * @param   pwszSource              UTF16 string to assign.
 * @param   fCopy                   Whether to just assign or copy the actual buffer
 *                                  contents from source -> dest.
 */
HRESULT VBoxCredProvCredential::RTUTF16toToUnicode(PUNICODE_STRING pUnicodeDest, PRTUTF16 pwszSource,
                                                   bool fCopy)
{
    AssertPtrReturn(pwszSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pUnicodeDest, VERR_INVALID_POINTER);

    size_t cbLen = RTUtf16Len(pwszSource) * sizeof(RTUTF16);

    pUnicodeDest->Length        = cbLen;
    pUnicodeDest->MaximumLength = pUnicodeDest->Length;

    if (fCopy)
    {
        CopyMemory(pUnicodeDest->Buffer, pwszSource, cbLen);
    }
    else /* Just assign the buffer. */
        pUnicodeDest->Buffer    = pwszSource;

    return S_OK;
}


HRESULT VBoxCredProvCredential::AllocateLogonPackage(const KERB_INTERACTIVE_UNLOCK_LOGON& UnlockLogon,
                                                     PBYTE *ppPackage, DWORD *pcbPackage)
{
    AssertPtrReturn(ppPackage, E_INVALIDARG);
    AssertPtrReturn(pcbPackage, E_INVALIDARG);

    const KERB_INTERACTIVE_LOGON *pLogonIn = &UnlockLogon.Logon;

    /*
     * First, allocate enough space for the logon structure itself and separate
     * string buffers right after it to store the actual user, password and domain
     * credentials.
     */
    DWORD cbLogon = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON)
                  + pLogonIn->LogonDomainName.Length +
                  + pLogonIn->UserName.Length +
                  + pLogonIn->Password.Length;

#ifdef DEBUG
    VBoxCredProvVerbose(3, "VBoxCredProvCredential: AllocateLogonPackage: Allocating %ld bytes (%d bytes credentials)\n",
                        cbLogon, cbLogon - sizeof(KERB_INTERACTIVE_UNLOCK_LOGON));
#endif

    KERB_INTERACTIVE_UNLOCK_LOGON *pLogon = (KERB_INTERACTIVE_UNLOCK_LOGON*)CoTaskMemAlloc(cbLogon);
    if (!pLogon)
        return E_OUTOFMEMORY;

    /* Let our byte buffer point to the end of our allocated structure so that it can
     * be used to store the credential data sequentially in a binary blob
     * (without terminating \0). */
    PBYTE pbBuffer = (PBYTE)pLogon + sizeof(KERB_INTERACTIVE_UNLOCK_LOGON);

    /* The buffer of the packed destination string does not contain the actual
     * string content but a relative offset starting at the given
     * KERB_INTERACTIVE_UNLOCK_LOGON structure. */
#define KERB_CRED_INIT_PACKED(StringDst, StringSrc, LogonOffset)         \
    StringDst.Length         = StringSrc.Length;                         \
    StringDst.MaximumLength  = StringSrc.Length;                         \
    StringDst.Buffer         = (PWSTR)pbBuffer;                          \
    CopyMemory(StringDst.Buffer, StringSrc.Buffer, StringDst.Length);    \
    StringDst.Buffer         = (PWSTR)(pbBuffer - (PBYTE)LogonOffset);   \
    pbBuffer                += StringDst.Length;

    ZeroMemory(&pLogon->LogonId, sizeof(LUID));

    KERB_INTERACTIVE_LOGON *pLogonOut = &pLogon->Logon;
    pLogonOut->MessageType = pLogonIn->MessageType;

    KERB_CRED_INIT_PACKED(pLogonOut->LogonDomainName, pLogonIn->LogonDomainName, pLogon);
    KERB_CRED_INIT_PACKED(pLogonOut->UserName       , pLogonIn->UserName,        pLogon);
    KERB_CRED_INIT_PACKED(pLogonOut->Password       , pLogonIn->Password,        pLogon);

    *ppPackage  = (PBYTE)pLogon;
    *pcbPackage = cbLogon;

#undef KERB_CRED_INIT_PACKED

    return S_OK;
}


void VBoxCredProvCredential::Reset(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Wiping credentials ...\n");

    VbglR3CredentialsDestroyUtf16(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                                  m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
                                  m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME],
                                  3 /* Passes */);
    if (m_pEvents)
    {
        m_pEvents->SetFieldString(this, VBOXCREDPROV_FIELDID_USERNAME, NULL);
        m_pEvents->SetFieldString(this, VBOXCREDPROV_FIELDID_PASSWORD, NULL);
        m_pEvents->SetFieldString(this, VBOXCREDPROV_FIELDID_DOMAINNAME, NULL);
    }
}


int VBoxCredProvCredential::RetrieveCredentials(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Checking for credentials ...\n");

    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_SUCCESS(rc))
    {
        /*
         * Set status to "terminating" to let the host know this module now
         * tries to receive and use passed credentials so that credentials from
         * the host won't be sent twice.
         */
        VBoxCredProvReportStatus(VBoxGuestFacilityStatus_Terminating);

        rc = VbglR3CredentialsRetrieveUtf16(&m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                                            &m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
                                            &m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]);

        VBoxCredProvVerbose(0, "VBoxCredProvCredential: Retrieving credentials returned rc=%Rrc\n", rc);
    }

    if (RT_SUCCESS(rc))
    {
        VBoxCredProvVerbose(0, "VBoxCredProvCredential: Got credentials: User=%ls, Password=%ls, Domain=%ls\n",
                            m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
#ifdef DEBUG
                            m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
#else
                            L"XXX" /* Don't show any passwords in release mode. */,
#endif
                            m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]);

        /*
         * In case we got a "display name" (e.g. "John Doe")
         * instead of the real user name (e.g. "jdoe") we have
         * to translate the data first ...
         */
        PWSTR pwszAcount;
        if (TranslateAccountName(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME], &pwszAcount))
        {
            VBoxCredProvVerbose(0, "VBoxCredProvCredential: Translated account name %ls -> %ls\n",
                                m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME], pwszAcount);

            RTUtf16Free(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME]);
            m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME] = pwszAcount;
        }
        else
        {
            /*
             * Oky, no display name, but maybe it's a
             * principal name from which we have to extract the
             * domain from? (jdoe@my-domain.sub.net.com -> jdoe in
             * domain my-domain.sub.net.com.)
             */
            PWSTR pwszDomain;
            if (ExtractAccoutData(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                                  &pwszAcount, &pwszDomain))
            {
                /* Update user name. */
                RTUtf16Free(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME]);
                m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME] = pwszAcount;

                /* Update domain. */
                RTUtf16Free(m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]);
                m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME] = pwszDomain;

                VBoxCredProvVerbose(0, "VBoxCredProvCredential: Extracted account data is pwszAccount=%ls, pwszDomain=%ls\n",
                                    pwszAcount, pwszDomain);
            }
        }
    }

    return rc;
}


/*
 * Initializes one credential with the field information passed in.
 * Set the value of the VBOXCREDPROV_FIELDID_USERNAME field to pwzUsername.
 * Optionally takes a password for the SetSerialization case.
 */
HRESULT VBoxCredProvCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpUS)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Initialize: cpUS=%ld\n", cpUS);

    m_cpUS = cpUS;

    return S_OK;
}


/*
 * LogonUI calls this in order to give us a callback in case we need to notify it of anything.
 * Store this callback pointer for later use.
 */
HRESULT VBoxCredProvCredential::Advise(ICredentialProviderCredentialEvents* pcpce)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: Advise\n");

    if (m_pEvents != NULL)
        m_pEvents->Release();
    m_pEvents = pcpce;
    m_pEvents->AddRef();
    return S_OK;
}


/* LogonUI calls this to tell us to release the callback. */
HRESULT VBoxCredProvCredential::UnAdvise(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: UnAdvise\n");

    /*
     * We're done with the current iteration, trigger a refresh of ourselves
     * to reset credentials and to keep the logon UI clean (no stale entries anymore).
     */
    Reset();

    /*
     * Force a re-iteration of the provider (which will give zero credentials
     * to try out because we just reset our one and only a line above.
     */
    if (m_pProvider)
        m_pProvider->OnCredentialsProvided();

    if (m_pEvents)
        m_pEvents->Release();
    m_pEvents = NULL;
    return S_OK;
}


/*
 * LogonUI calls this function when our tile is selected (zoomed).
 * If you simply want fields to show/hide based on the selected state,
 * there's no need to do anything here - you can set that up in the
 * field definitions.  But if you want to do something
 * more complicated, like change the contents of a field when the tile is
 * selected, you would do it here.
 */
HRESULT VBoxCredProvCredential::SetSelected(BOOL* pbAutoLogon)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: SetSelected\n");

    /*
     * Don't do auto logon here because it would retry too often with
     * every credential field (user name, password, domain, ...) which makes
     * winlogon wait before new login attempts can be made.
     */
    *pbAutoLogon = FALSE;
    return S_OK;
}


/*
 * Similarly to SetSelected, LogonUI calls this when your tile was selected
 * and now no longer is. The most common thing to do here (which we do below)
 * is to clear out the password field.
 */
HRESULT VBoxCredProvCredential::SetDeselected()
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: SetDeselected\n");

    VbglR3CredentialsDestroyUtf16(m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                                  m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
                                  m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME],
                                  3 /* Passes */);

    if (m_pEvents)
        m_pEvents->SetFieldString(this, VBOXCREDPROV_FIELDID_PASSWORD, L"");

    return S_OK;
}


/*
 * Gets info for a particular field of a tile. Called by logonUI to get information to
 * display the tile.
 */
HRESULT VBoxCredProvCredential::GetFieldState(DWORD                                        dwFieldID,
                                              CREDENTIAL_PROVIDER_FIELD_STATE*             pFieldState,
                                              CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pFieldstateInteractive)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetFieldState: dwFieldID=%ld\n", dwFieldID);

    HRESULT hr = S_OK;

    if (   (dwFieldID < VBOXCREDPROV_NUM_FIELDS)
        && pFieldState
        && pFieldstateInteractive)
    {
        *pFieldState            = s_VBoxCredProvFields[dwFieldID].state;
        *pFieldstateInteractive = s_VBoxCredProvFields[dwFieldID].stateInteractive;
    }
    else
        hr = E_INVALIDARG;

    return hr;
}


/*
 * Searches the account name based on a display (real) name (e.g. "John Doe" -> "jdoe").
 * Result "ppwszAccoutName" needs to be freed with CoTaskMemFree!
 */
BOOL VBoxCredProvCredential::TranslateAccountName(PWSTR pwszDisplayName, PWSTR *ppwszAccoutName)
{
    AssertPtrReturn(pwszDisplayName, FALSE);
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: TranslateAccountName: Getting account name for \"%ls\" ...\n",
                        pwszDisplayName);

    /** @todo Do we need ADS support (e.g. TranslateNameW) here? */
    BOOL fFound = FALSE;                        /* Did we find the desired user? */
    NET_API_STATUS nStatus;
    DWORD dwLevel = 2;                          /* Detailed information about user accounts. */
    DWORD dwPrefMaxLen = MAX_PREFERRED_LENGTH;
    DWORD dwEntriesRead = 0;
    DWORD dwTotalEntries = 0;
    DWORD dwResumeHandle = 0;
    LPUSER_INFO_2 pBuf = NULL;
    LPUSER_INFO_2 pCurBuf = NULL;
    do
    {
        nStatus = NetUserEnum(NULL,             /* Server name, NULL for localhost. */
                              dwLevel,
                              FILTER_NORMAL_ACCOUNT,
                              (LPBYTE*)&pBuf,
                              dwPrefMaxLen,
                              &dwEntriesRead,
                              &dwTotalEntries,
                              &dwResumeHandle);
        if (   (nStatus == NERR_Success)
            || (nStatus == ERROR_MORE_DATA))
        {
            if ((pCurBuf = pBuf) != NULL)
            {
                for (DWORD i = 0; i < dwEntriesRead; i++)
                {
                    /*
                     * Search for the "display name" - that might be
                     * "John Doe" or something similar the user recognizes easier
                     * and may not the same as the "account" name (e.g. "jdoe").
                     */
                    if (   pCurBuf
                        && pCurBuf->usri2_full_name
                        && StrCmpI(pwszDisplayName, pCurBuf->usri2_full_name) == 0)
                    {
                        /*
                         * Copy the real user name (e.g. "jdoe") to our
                         * output buffer.
                         */
                        LPWSTR pwszTemp;
                        HRESULT hr = SHStrDupW(pCurBuf->usri2_name, &pwszTemp);
                        if (hr == S_OK)
                        {
                            *ppwszAccoutName = pwszTemp;
                            fFound = TRUE;
                        }
                        else
                            VBoxCredProvVerbose(0, "VBoxCredProvCredential: TranslateAccountName: Error copying data, hr=%08x\n", hr);
                        break;
                    }
                    pCurBuf++;
                }
            }
            if (pBuf != NULL)
            {
                NetApiBufferFree(pBuf);
                pBuf = NULL;
            }
        }
    } while (nStatus == ERROR_MORE_DATA && !fFound);

    if (pBuf != NULL)
    {
        NetApiBufferFree(pBuf);
        pBuf = NULL;
    }

    VBoxCredProvVerbose(0, "VBoxCredProvCredential: TranslateAccountName: Returned nStatus=%ld, fFound=%s\n",
                        nStatus, fFound ? "Yes" : "No");
    return fFound;

#if 0
    DWORD dwErr = NO_ERROR;
    ULONG cbLen = 0;
    if (   TranslateNameW(pwszName, NameUnknown, NameUserPrincipal, NULL, &cbLen)
        && cbLen > 0)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetAccountName: Translated ADS name has %u characters\n", cbLen));

        ppwszAccoutName = (PWSTR)RTMemAlloc(cbLen * sizeof(WCHAR));
        AssertPtrReturn(pwszName, FALSE);
        if (TranslateNameW(pwszName, NameUnknown, NameUserPrincipal, ppwszAccoutName, &cbLen))
        {
            VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetAccountName: Real ADS account name of '%ls' is '%ls'\n",
                 pwszName, ppwszAccoutName));
        }
        else
        {
            RTMemFree(ppwszAccoutName);
            dwErr = GetLastError();
        }
    }
    else
        dwErr = GetLastError();
    /* The above method for looking up in ADS failed, try another one. */
    if (dwErr != NO_ERROR)
    {
        dwErr = NO_ERROR;

    }
#endif
}


/*
 * Extracts the actual account name & domain from a (raw) account data string. This might
 * be a principal or FQDN string.
 */
BOOL VBoxCredProvCredential::ExtractAccoutData(PWSTR pwszAccountData, PWSTR *ppwszAccoutName, PWSTR *ppwszDomain)
{
    AssertPtrReturn(pwszAccountData, FALSE);
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: ExtractAccoutData: Getting account name for \"%ls\" ...\n",
                        pwszAccountData);
    HRESULT hr = E_FAIL;

    /* Try to figure out whether this is a principal name (user@domain). */
    LPWSTR pPos = NULL;
    if (   (pPos  = StrChrW(pwszAccountData, L'@')) != NULL
        &&  pPos != pwszAccountData)
    {
        DWORD cbSize = (pPos - pwszAccountData) * sizeof(WCHAR);
        LPWSTR pwszName = (LPWSTR)CoTaskMemAlloc(cbSize + sizeof(WCHAR)); /* Space for terminating zero. */
        LPWSTR pwszDomain = NULL;
        AssertPtr(pwszName);
        hr = StringCbCopyN(pwszName, cbSize + sizeof(WCHAR), pwszAccountData, cbSize);
        if (SUCCEEDED(hr))
        {
            *ppwszAccoutName = pwszName;
            *pPos++; /* Skip @, point to domain name (if any). */
            if (    pPos != NULL
                && *pPos != L'\0')
            {
                hr = SHStrDupW(pPos, &pwszDomain);
                if (SUCCEEDED(hr))
                {
                    *ppwszDomain = pwszDomain;
                }
                else
                    VBoxCredProvVerbose(0, "VBoxCredProvCredential: ExtractAccoutData: Error copying domain data, hr=%08x\n", hr);
            }
            else
            {
                hr = E_FAIL;
                VBoxCredProvVerbose(0, "VBoxCredProvCredential: ExtractAccoutData: No domain name found!\n");
            }
        }
        else
            VBoxCredProvVerbose(0, "VBoxCredProvCredential: ExtractAccoutData: Error copying account data, hr=%08x\n", hr);

        if (hr != S_OK)
        {
            CoTaskMemFree(pwszName);
            if (pwszDomain)
                CoTaskMemFree(pwszDomain);
        }
    }
    else
        VBoxCredProvVerbose(0, "VBoxCredProvCredential: ExtractAccoutData: No valid principal account name found!\n");

    return (hr == S_OK);
}


/* Sets ppwsz to the string value of the field at the index dwFieldID. */
HRESULT VBoxCredProvCredential::GetStringValue(DWORD  dwFieldID,
                                               PWSTR *ppwszString)
{
    /* Check to make sure dwFieldID is a legitimate index. */
    HRESULT hr;
    if (   dwFieldID < VBOXCREDPROV_NUM_FIELDS
        && ppwszString)
    {
        switch (dwFieldID)
        {
            case VBOXCREDPROV_FIELDID_SUBMIT_BUTTON:
                /* Fill in standard value to make Winlogon happy. */
                hr = SHStrDupW(L"Submit", ppwszString);
                break;

            default:

                /*
                 * Make a copy of the string and return that, the caller is responsible for freeing it.
                 * Note that there can be empty fields (like a missing domain name); handle them
                 * by writing an empty string.
                 */
                if (   m_pwszCredentials[dwFieldID]
                    && RTUtf16Len(m_pwszCredentials[dwFieldID]))
                {
                    hr = SHStrDupW(m_pwszCredentials[dwFieldID], ppwszString);
                }
                else /* Fill in an empty value. */
                    hr = SHStrDupW(L"", ppwszString);
                break;
        }
#ifdef DEBUG
        if (SUCCEEDED(hr))
            VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetStringValue: dwFieldID=%ld, ppwszString=%ls\n",
                                dwFieldID, *ppwszString);
#endif
    }
    else
        hr = E_INVALIDARG;
    return hr;
}


/*
 * Sets pdwAdjacentTo to the index of the field the submit button should be
 * adjacent to. We recommend that the submit button is placed next to the last
 * field which the user is required to enter information in. Optional fields
 * should be below the submit button.
 */
HRESULT VBoxCredProvCredential::GetSubmitButtonValue(DWORD  dwFieldID,
                                                     DWORD* pdwAdjacentTo)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetSubmitButtonValue: dwFieldID=%ld\n", dwFieldID);

    HRESULT hr = S_OK;

    /* Validate parameters. */
    if (   dwFieldID == VBOXCREDPROV_FIELDID_SUBMIT_BUTTON
        && pdwAdjacentTo)
    {
        /* pdwAdjacentTo is a pointer to the fieldID you want the submit button to appear next to. */
        *pdwAdjacentTo = VBOXCREDPROV_FIELDID_PASSWORD;
        VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetSubmitButtonValue: dwFieldID=%ld, *pdwAdjacentTo=%ld\n",
                            dwFieldID, *pdwAdjacentTo);
    }
    else
        hr = E_INVALIDARG;

    return hr;
}


/*
 * Sets the value of a field which can accept a string as a value.
 * This is called on each keystroke when a user types into an edit field.
 */
HRESULT VBoxCredProvCredential::SetStringValue(DWORD  dwFieldID,
                                               PCWSTR pcwzString)
{
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: SetStringValue: dwFieldID=%ld, pcwzString=%ls\n",
                        dwFieldID, pcwzString);

    /*
     * We don't set any values into fields (e.g. the password, hidden
     * by dots), instead keep it secret by resetting all credentials.
     */
    Reset();

    return S_OK;
}


/*
 * The following methods are for logonUI to get the values of various UI elements and then communicate
 * to the credential about what the user did in that field. However, these methods are not implemented
 * because our tile doesn't contain these types of UI elements.
 */
HRESULT VBoxCredProvCredential::GetBitmapValue(DWORD    dwFieldID,
                                               HBITMAP* phBitmap)
{
    NOREF(dwFieldID);
    NOREF(phBitmap);

    /* We don't do own bitmaps. */
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::GetCheckboxValue(DWORD  dwFieldID,
                                                 BOOL*  pbChecked,
                                                 PWSTR* ppwszLabel)
{
    NOREF(dwFieldID);
    NOREF(pbChecked);
    NOREF(ppwszLabel);
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::GetComboBoxValueCount(DWORD dwFieldID,
                                                      DWORD* pcItems,
                                                      DWORD* pdwSelectedItem)
{
    NOREF(dwFieldID);
    NOREF(pcItems);
    NOREF(pdwSelectedItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::GetComboBoxValueAt(DWORD dwFieldID,
                                                   DWORD dwItem,
                                                   PWSTR* ppwszItem)
{
    NOREF(dwFieldID);
    NOREF(dwItem);
    NOREF(ppwszItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::SetCheckboxValue(DWORD dwFieldID,
                                                 BOOL bChecked)
{
    NOREF(dwFieldID);
    NOREF(bChecked);
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::SetComboBoxSelectedValue(DWORD dwFieldId,
                                                         DWORD dwSelectedItem)
{
    NOREF(dwFieldId);
    NOREF(dwSelectedItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredProvCredential::CommandLinkClicked(DWORD dwFieldID)
{
    NOREF(dwFieldID);
    return E_NOTIMPL;
}


/*
 * Collect the username and password into a serialized credential for the correct usage scenario
 * LogonUI then passes these credentials back to the system to log on.
 */
HRESULT VBoxCredProvCredential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpGetSerializationResponse,
                                                 CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION   *pcpCredentialSerialization,
                                                 PWSTR                                          *ppwszOptionalStatusText,
                                                 CREDENTIAL_PROVIDER_STATUS_ICON                *pcpsiOptionalStatusIcon)
{
    NOREF(ppwszOptionalStatusText);
    NOREF(pcpsiOptionalStatusIcon);

    KERB_INTERACTIVE_UNLOCK_LOGON KerberosUnlockLogon;
    ZeroMemory(&KerberosUnlockLogon, sizeof(KerberosUnlockLogon));

    /* Save a pointer to the interactive logon struct. */
    KERB_INTERACTIVE_LOGON* pKerberosLogon = &KerberosUnlockLogon.Logon;
    AssertPtr(pKerberosLogon);

    HRESULT hr;

#ifdef DEBUG
    VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetSerialization: Username=%ls, Password=%ls, Domain=%ls\n",
                        m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                        m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
                        m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]);
#endif

    WCHAR wszComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD cch = ARRAYSIZE(wszComputerName);
    if (GetComputerNameW(wszComputerName, &cch))
    {
        /* Is a domain name missing? Then use the name of the local computer. */
        if (   m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]
            && RTUtf16Len(m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME]))
        {
            hr = RTUTF16toToUnicode(&pKerberosLogon->LogonDomainName,
                                    m_pwszCredentials[VBOXCREDPROV_FIELDID_DOMAINNAME],
                                    false /* Just assign, no copy */);
        }
        else
            hr = RTUTF16toToUnicode(&pKerberosLogon->LogonDomainName,
                                    wszComputerName,
                                    false /* Just assign, no copy */);

        /* Fill in the username and password. */
        if (SUCCEEDED(hr))
        {
            hr = RTUTF16toToUnicode(&pKerberosLogon->UserName,
                                    m_pwszCredentials[VBOXCREDPROV_FIELDID_USERNAME],
                                    false /* Just assign, no copy */);
            if (SUCCEEDED(hr))
            {
                hr = RTUTF16toToUnicode(&pKerberosLogon->Password,
                                        m_pwszCredentials[VBOXCREDPROV_FIELDID_PASSWORD],
                                        false /* Just assign, no copy */);
                if (SUCCEEDED(hr))
                {
                    /* Set credential type according to current usage scenario. */
                    AssertPtr(pKerberosLogon);
                    switch (m_cpUS)
                    {
                        case CPUS_UNLOCK_WORKSTATION:
                            pKerberosLogon->MessageType = KerbWorkstationUnlockLogon;
                            break;

                        case CPUS_LOGON:
                            pKerberosLogon->MessageType = KerbInteractiveLogon;
                            break;

                        case CPUS_CREDUI:
                            pKerberosLogon->MessageType = (KERB_LOGON_SUBMIT_TYPE)0; /* No message type required here. */
                            break;

                        default:
                            hr = E_FAIL;
                            break;
                    }

                    if (SUCCEEDED(hr))
                    {
                        /* Allocate copies of, and package, the strings in a binary blob. */
                        hr = AllocateLogonPackage(KerberosUnlockLogon,
                                                  &pcpCredentialSerialization->rgbSerialization,
                                                  &pcpCredentialSerialization->cbSerialization);
                    }

                    if (SUCCEEDED(hr))
                    {
                        ULONG ulAuthPackage;

                        HANDLE hLsa;
                        NTSTATUS s = LsaConnectUntrusted(&hLsa);
                        if (SUCCEEDED(HRESULT_FROM_NT(s)))
                        {
                            LSA_STRING lsaszKerberosName;
                            size_t cchKerberosName;
                            hr = StringCchLengthA(NEGOSSP_NAME_A, USHORT_MAX, &cchKerberosName);
                            if (SUCCEEDED(hr))
                            {
                                USHORT usLength;
                                hr = SizeTToUShort(cchKerberosName, &usLength);
                                if (SUCCEEDED(hr))
                                {
                                    lsaszKerberosName.Buffer        = (PCHAR)NEGOSSP_NAME_A;
                                    lsaszKerberosName.Length        = usLength;
                                    lsaszKerberosName.MaximumLength = lsaszKerberosName.Length + 1;

                                }
                            }

                            if (SUCCEEDED(hr))
                            {
                                s = LsaLookupAuthenticationPackage(hLsa, &lsaszKerberosName,
                                                                   &ulAuthPackage);
                                if (FAILED(HRESULT_FROM_NT(s)))
                                    hr = HRESULT_FROM_NT(s);
                            }

                            LsaDeregisterLogonProcess(hLsa);
                        }

                        if (SUCCEEDED(hr))
                        {
                            pcpCredentialSerialization->ulAuthenticationPackage = ulAuthPackage;
                            pcpCredentialSerialization->clsidCredentialProvider = CLSID_VBoxCredProvider;

                            /* We're done -- let the logon UI know. */
                            *pcpGetSerializationResponse = CPGSR_RETURN_CREDENTIAL_FINISHED;
                        }
                    }
                }
            }
        }
    }
    else
        hr = HRESULT_FROM_WIN32(GetLastError());

    VBoxCredProvVerbose(0, "VBoxCredProvCredential: GetSerialization: Returned 0x%08x\n", hr);
    return hr;
}


/*
 * ReportResult is completely optional. Its purpose is to allow a credential to customize the string
 * and the icon displayed in the case of a logon failure.  For example, we have chosen to
 * customize the error shown in the case of bad username/password and in the case of the account
 * being disabled.
 */
HRESULT VBoxCredProvCredential::ReportResult(NTSTATUS                         ntsStatus,
                                             NTSTATUS                         ntsSubstatus,
                                             PWSTR                           *ppwszOptionalStatusText,
                                             CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
    /*
     * Since NULL is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
     * this function can't fail
     */
    return S_OK;
}

