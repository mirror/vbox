//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Modifications (c) 2009-2010 Oracle Corporation
//

#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif

#include "VBoxCredProv.h"
#include "VBoxCredential.h"
#include "guid.h"

#include <lm.h>

#include <iprt/mem.h>
#include <iprt/string.h>


struct REPORT_RESULT_STATUS_INFO
{
    NTSTATUS ntsStatus;
    NTSTATUS ntsSubstatus;
    PWSTR     pwzMessage;
    CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

static const REPORT_RESULT_STATUS_INFO s_rgLogonStatusInfo[] =
{
    { STATUS_LOGON_FAILURE, STATUS_SUCCESS, L"Incorrect password or username.", CPSI_ERROR, },
    { STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, L"The account is disabled.", CPSI_WARNING },
};


VBoxCredential::VBoxCredential(VBoxCredProv *pProvider) : m_cRef(1),
                                                          m_pCredProvCredentialEvents(NULL)
{
    Log(("VBoxCredential::VBoxCredential\n"));

    DllAddRef();

    AssertPtr(pProvider);
    m_pProvider = pProvider;

    ZeroMemory(m_rgCredProvFieldDescriptors, sizeof(m_rgCredProvFieldDescriptors));
    ZeroMemory(m_rgFieldStatePairs, sizeof(m_rgFieldStatePairs));
    ZeroMemory(m_rgFieldStrings, sizeof(m_rgFieldStrings));
}


VBoxCredential::~VBoxCredential()
{
    Log(("VBoxCredential::~VBoxCredential\n"));

    Reset();

    for (int i = 0; i < ARRAYSIZE(m_rgFieldStrings); i++)
    {
        CoTaskMemFree(m_rgFieldStrings[i]);
        CoTaskMemFree(m_rgCredProvFieldDescriptors[i].pszLabel);
    }

    DllRelease();
}


void VBoxCredential::WipeString(const PWSTR pwszString)
{
    if (pwszString)
        SecureZeroMemory(pwszString, wcslen(pwszString) * sizeof(WCHAR));
}


void VBoxCredential::Reset(void)
{
    WipeString(m_rgFieldStrings[SFI_USERNAME]);
    WipeString(m_rgFieldStrings[SFI_PASSWORD]);
    WipeString(m_rgFieldStrings[SFI_DOMAINNAME]);

    if (m_pCredProvCredentialEvents)
    {
        m_pCredProvCredentialEvents->SetFieldString(this, SFI_USERNAME, NULL);
        m_pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, NULL);
        m_pCredProvCredentialEvents->SetFieldString(this, SFI_DOMAINNAME, NULL);
    }
}


int VBoxCredential::Update(const char *pszUser,
                           const char *pszPw,
                           const char *pszDomain)
{
    Log(("VBoxCredential::Update: User=%s, Password=%s, Domain=%s\n",
         pszUser ? pszUser : "NULL",
         pszPw ? pszPw : "NULL",
         pszDomain ? pszDomain : "NULL"));

    PWSTR *ppwszStored;

    /*
     * Update domain name (can be NULL) and will
     * be later replaced by the local computer name in the
     * Kerberos authentication package or by the first part
     * of the principcal name.
     */
    if (pszDomain && strlen(pszDomain))
    {
        ppwszStored = &m_rgFieldStrings[SFI_DOMAINNAME];
        CoTaskMemFree(*ppwszStored);
        SHStrDupA(pszDomain, ppwszStored);
    }

    /* Update user name. */
    if (pszUser && strlen(pszUser))
    {
        ppwszStored = &m_rgFieldStrings[SFI_USERNAME];
        CoTaskMemFree(*ppwszStored);
        SHStrDupA(pszUser, ppwszStored);

        /*
         * In case we got a "display name" (e.g. "John Doe")
         * instead of the real user name (e.g. "jdoe") we have
         * to translate the data first ...
         */
        PWSTR pwszAcount;
        if (TranslateAccountName(*ppwszStored, &pwszAcount))
        {
            CoTaskMemFree(*ppwszStored);
            m_rgFieldStrings[SFI_USERNAME] = pwszAcount;
        }
        else
        {
            /*
             * Oky, no display name, but mabye it's a
             * principal name from which we have to extract the
             * domain from? (jdoe@my-domain.sub.net.com -> jdoe in
             * domain my-domain.sub.net.com.)
             */
            PWSTR pwszDomain;
            if (ExtractAccoutData(*ppwszStored, &pwszAcount, &pwszDomain))
            {
                /* Update user name. */
                CoTaskMemFree(*ppwszStored);
                m_rgFieldStrings[SFI_USERNAME] = pwszAcount;

                /* Update domain. */
                ppwszStored = &m_rgFieldStrings[SFI_DOMAINNAME];
                CoTaskMemFree(*ppwszStored);
                m_rgFieldStrings[SFI_DOMAINNAME] = pwszDomain;
            }
        }
    }

    /* Update password. */
    if (pszPw && strlen(pszPw))
    {
        ppwszStored = &m_rgFieldStrings[SFI_PASSWORD];
        CoTaskMemFree(*ppwszStored);
        SHStrDupA(pszPw, ppwszStored);
    }

    Log(("VBoxCredential::Update: Finished - User=%ls, Password=%ls, Domain=%ls\n",
         m_rgFieldStrings[SFI_USERNAME] ? m_rgFieldStrings[SFI_USERNAME] : L"NULL",
         m_rgFieldStrings[SFI_PASSWORD] ? m_rgFieldStrings[SFI_PASSWORD] : L"NULL",
         m_rgFieldStrings[SFI_DOMAINNAME] ? m_rgFieldStrings[SFI_DOMAINNAME] : L"NULL"));
    return S_OK;
}


/*
 * Initializes one credential with the field information passed in.
 * Set the value of the SFI_USERNAME field to pwzUsername.
 * Optionally takes a password for the SetSerialization case.
 */
HRESULT VBoxCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                   const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
                                   const FIELD_STATE_PAIR* rgfsp)
{
    Log(("VBoxCredential::Initialize: cpus=%ld, rgcpfd=%p, rgfsp=%p\n", cpus, rgcpfd, rgfsp));

    HRESULT hr = S_OK;

    m_cpUS = cpus;

    /*
     * Copy the field descriptors for each field. This is useful if you want to vary the
     * field descriptors based on what Usage scenario the credential was created for.
     */
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(m_rgCredProvFieldDescriptors); i++)
    {
        m_rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &m_rgCredProvFieldDescriptors[i]);
    }

    /* Fill in the default value to make winlogon happy. */
    hr = SHStrDupW(L"Submit", &m_rgFieldStrings[SFI_SUBMIT_BUTTON]);

    return S_OK;
}


/*
 * LogonUI calls this in order to give us a callback in case we need to notify it of anything.
 * Store this callback pointer for later use.
 */
HRESULT VBoxCredential::Advise(ICredentialProviderCredentialEvents* pcpce)
{
    Log(("VBoxCredential::Advise\n"));

    if (m_pCredProvCredentialEvents != NULL)
        m_pCredProvCredentialEvents->Release();
    m_pCredProvCredentialEvents = pcpce;
    m_pCredProvCredentialEvents->AddRef();
    return S_OK;
}


/* LogonUI calls this to tell us to release the callback. */
HRESULT VBoxCredential::UnAdvise()
{
    Log(("VBoxCredential::UnAdvise\n"));

    /*
     * We're done with the current iteration, trigger a refresh of ourselves
     * to reset credentials and to keep the logon UI clean (no stale entries anymore).
     */
    Reset();

    /*
     * Force a re-iteration of the provider (which will give zero credentials
     * to try out because we just resetted our one and only a line above.
     */
    if (m_pProvider)
        m_pProvider->OnCredentialsProvided();

    if (m_pCredProvCredentialEvents)
        m_pCredProvCredentialEvents->Release();
    m_pCredProvCredentialEvents = NULL;
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
HRESULT VBoxCredential::SetSelected(BOOL* pbAutoLogon)
{
    Log(("VBoxCredential::SetSelected\n"));

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
HRESULT VBoxCredential::SetDeselected()
{
    Log(("VBoxCredential::SetDeselected\n"));

    HRESULT hr = S_OK;
    if (m_rgFieldStrings[SFI_PASSWORD])
    {
        // CoTaskMemFree (below) deals with NULL, but StringCchLength does not.
        size_t lenPassword;
        hr = StringCchLengthW(m_rgFieldStrings[SFI_PASSWORD], 128, &(lenPassword));
        if (SUCCEEDED(hr))
        {
            SecureZeroMemory(m_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*m_rgFieldStrings[SFI_PASSWORD]));

            CoTaskMemFree(m_rgFieldStrings[SFI_PASSWORD]);
            hr = SHStrDupW(L"", &m_rgFieldStrings[SFI_PASSWORD]);
        }

        if (SUCCEEDED(hr) && m_pCredProvCredentialEvents)
        {
            m_pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, m_rgFieldStrings[SFI_PASSWORD]);
        }
    }

    return hr;
}


/*
 * Gets info for a particular field of a tile. Called by logonUI to get information to
 * display the tile.
 */
HRESULT VBoxCredential::GetFieldState(DWORD dwFieldID,
                                      CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                                      CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis)
{
    Log(("VBoxCredential::GetFieldState: dwFieldID=%ld, pcpfs=%p, pcpfis=%p\n", dwFieldID, pcpfs, pcpfis));

    HRESULT hr;

    if (   (dwFieldID < ARRAYSIZE(m_rgFieldStatePairs))
        && pcpfs
        && pcpfis)
    {
        *pcpfs = m_rgFieldStatePairs[dwFieldID].cpfs;
        *pcpfis = m_rgFieldStatePairs[dwFieldID].cpfis;

        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}


/*
 * Searches the account name based on a display (real) name (e.g. "John Doe" -> "jdoe").
 * Result "ppwszAccoutName" needs to be freed with CoTaskMemFree!
 */
BOOL VBoxCredential::TranslateAccountName(PWSTR pwszDisplayName, PWSTR *ppwszAccoutName)
{
    Log(("VBoxCredential::TranslateAccountName\n"));

    AssertPtr(pwszDisplayName);
    Log(("VBoxCredential::TranslateAccountName: Getting account name for '%ls' ...\n", pwszDisplayName));

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
                            Log(("VBoxCredential::TranslateAccountName: Error copying data, hr=%08x\n", hr));
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

    Log(("VBoxCredential::TranslateAccountName: Returned nStatus=%ld, fFound=%s\n",
         nStatus, fFound ? "Yes" : "No"));
    return fFound;

#if 0
    DWORD dwErr = NO_ERROR;
    ULONG cbLen = 0;
    if (   TranslateNameW(pwszName, NameUnknown, NameUserPrincipal, NULL, &cbLen)
        && cbLen > 0)
    {
        Log(("VBoxCredential::GetAccountName: Translated ADS name has %u characters\n", cbLen));

        ppwszAccoutName = (PWSTR)RTMemAlloc(cbLen * sizeof(WCHAR));
        AssertPtrReturn(pwszName, FALSE);
        if (TranslateNameW(pwszName, NameUnknown, NameUserPrincipal, ppwszAccoutName, &cbLen))
        {
            Log(("VBoxCredential::GetAccountName: Real ADS account name of '%ls' is '%ls'\n",
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
 * be a prncipal or FQDN string.
 */
BOOL VBoxCredential::ExtractAccoutData(PWSTR pwszAccountData, PWSTR *ppwszAccoutName, PWSTR *ppwszDomain)
{
    Log(("VBoxCredential::ExtractAccoutData\n"));

    AssertPtr(pwszAccountData);
    Log(("VBoxCredential::ExtractAccoutData: Getting account name for '%ls' ...\n", pwszAccountData));
    HRESULT hr = E_FAIL;

    /* Try to figure out whether this is a principal name (user@domain). */
    LPWSTR pPos = NULL;
    if (    (pPos                       = StrChrW(pwszAccountData, L'@')) != NULL
        &&   pPos                      != pwszAccountData)
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
                    Log(("VBoxCredential::ExtractAccoutData/Principal: Error copying domain data, hr=%08x\n", hr));
            }
            else
            {
                hr = E_FAIL;
                Log(("VBoxCredential::ExtractAccoutData/Principal: No domain name found!\n"));
            }
        }
        else
            Log(("VBoxCredential::ExtractAccoutData/Principal: Error copying account data, hr=%08x\n", hr));

        if (hr != S_OK)
        {
            CoTaskMemFree(pwszName);
            if (pwszDomain)
                CoTaskMemFree(pwszDomain);
        }
    }
    else
        Log(("VBoxCredential::ExtractAccoutData/Principal: No valid prinicipal account name found!\n"));

    return (hr == S_OK);
}


/* Sets ppwsz to the string value of the field at the index dwFieldID. */
HRESULT VBoxCredential::GetStringValue(DWORD dwFieldID,
                                       PWSTR *ppwszString)
{
    /* Check to make sure dwFieldID is a legitimate index. */
    HRESULT hr;
    if (   dwFieldID < ARRAYSIZE(m_rgCredProvFieldDescriptors)
        && ppwszString)
    {
        switch (dwFieldID)
        {
            /** @todo Add more specific field IDs here if needed. */

            default:

                /*
                 * Make a copy of the string and return that, the caller is responsible for freeing it.
                 * Note that there can be empty fields (like a missing domain name); handle them
                 * by writing an empty string.
                 */
                hr = SHStrDupW(m_rgFieldStrings[dwFieldID] ? m_rgFieldStrings[dwFieldID] : L"",
                               ppwszString);
                break;
        }
        if (SUCCEEDED(hr))
            Log(("VBoxCredential::GetStringValue: dwFieldID=%ld, ppwszString=%ls\n", dwFieldID, *ppwszString));
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
HRESULT VBoxCredential::GetSubmitButtonValue(DWORD dwFieldID,
                                             DWORD* pdwAdjacentTo)
{
    Log(("VBoxCredential::GetSubmitButtonValue: dwFieldID=%ld\n", dwFieldID));

    HRESULT hr;

    /* Validate parameters. */
    if ((SFI_SUBMIT_BUTTON == dwFieldID) && pdwAdjacentTo)
    {
        /* pdwAdjacentTo is a pointer to the fieldID you want the submit button to appear next to. */
        *pdwAdjacentTo = SFI_PASSWORD;
        Log(("VBoxCredential::GetSubmitButtonValue: dwFieldID=%ld, *pdwAdjacentTo=%ld\n", dwFieldID, *pdwAdjacentTo));
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}


/*
 * Sets the value of a field which can accept a string as a value.
 * This is called on each keystroke when a user types into an edit field.
 */
HRESULT VBoxCredential::SetStringValue(DWORD dwFieldID,
                                       PCWSTR pcwzString)
{
    Log(("VBoxCredential::SetStringValue: dwFieldID=%ld, pcwzString=%ls\n",
         dwFieldID, pcwzString));

    HRESULT hr;

    /*
     * We don't set any values into fields (e.g. the password, hidden
     * by dots), instead keep it secret by resetting all credentials.
     */
#if 0
    /* Validate parameters. */
    if (   dwFieldID < ARRAYSIZE(m_rgCredProvFieldDescriptors)
        && (   CPFT_EDIT_TEXT     == m_rgCredProvFieldDescriptors[dwFieldID].cpft
            || CPFT_PASSWORD_TEXT == m_rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        PWSTR* ppwszStored = &m_rgFieldStrings[dwFieldID];
        CoTaskMemFree(*ppwszStored);
        hr = SHStrDupW(pwz, ppwszStored);
    }
    else
    {
        hr = E_INVALIDARG;
    }
#else
    hr = E_NOTIMPL;
#endif
    return hr;
}


/*
 * The following methods are for logonUI to get the values of various UI elements and then communicate
 * to the credential about what the user did in that field. However, these methods are not implemented
 * because our tile doesn't contain these types of UI elements.
 */
HRESULT VBoxCredential::GetBitmapValue(DWORD dwFieldID,
                                       HBITMAP* phbmp)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(phbmp);

    /* We don't do own bitmaps */
    return E_NOTIMPL;
}


HRESULT VBoxCredential::GetCheckboxValue(DWORD dwFieldID,
                                         BOOL* pbChecked,
                                         PWSTR* ppwszLabel)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pbChecked);
    UNREFERENCED_PARAMETER(ppwszLabel);
    return E_NOTIMPL;
}


HRESULT VBoxCredential::GetComboBoxValueCount(DWORD dwFieldID,
                                              DWORD* pcItems,
                                              DWORD* pdwSelectedItem)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pcItems);
    UNREFERENCED_PARAMETER(pdwSelectedItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredential::GetComboBoxValueAt(DWORD dwFieldID,
                                           DWORD dwItem,
                                           PWSTR* ppwszItem)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(dwItem);
    UNREFERENCED_PARAMETER(ppwszItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredential::SetCheckboxValue(DWORD dwFieldID,
                                         BOOL bChecked)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(bChecked);
    return E_NOTIMPL;
}


HRESULT VBoxCredential::SetComboBoxSelectedValue(DWORD dwFieldId,
                                                 DWORD dwSelectedItem)
{
    UNREFERENCED_PARAMETER(dwFieldId);
    UNREFERENCED_PARAMETER(dwSelectedItem);
    return E_NOTIMPL;
}


HRESULT VBoxCredential::CommandLinkClicked(DWORD dwFieldID)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    return E_NOTIMPL;
}


/*
 * Collect the username and password into a serialized credential for the correct usage scenario
 * LogonUI then passes these credentials back to the system to log on.
 */
HRESULT VBoxCredential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpGetSerializationResponse,
                                         CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION   *pcpCredentialSerialization,
                                         PWSTR                                          *ppwszOptionalStatusText,
                                         CREDENTIAL_PROVIDER_STATUS_ICON                *pcpsiOptionalStatusIcon)
{
    Log(("VBoxCredential::GetSerialization: pcpGetSerializationResponse=%p, pcpCredentialSerialization=%p, ppwszOptionalStatusText=%p, pcpsiOptionalStatusIcon=%p\n",
         pcpGetSerializationResponse, pcpCredentialSerialization, ppwszOptionalStatusText, pcpsiOptionalStatusIcon));

    UNREFERENCED_PARAMETER(ppwszOptionalStatusText);
    UNREFERENCED_PARAMETER(pcpsiOptionalStatusIcon);

    KERB_INTERACTIVE_LOGON kil;
    ZeroMemory(&kil, sizeof(kil));

    HRESULT hr;

    WCHAR wszComputerName[MAX_COMPUTERNAME_LENGTH+1];
    DWORD cch = ARRAYSIZE(wszComputerName);
    if (GetComputerNameW(wszComputerName, &cch))
    {
        /* Is a domain name missing? Then use the name of the local computer. */
        if (NULL == m_rgFieldStrings [SFI_DOMAINNAME])
            hr = UnicodeStringInitWithString(wszComputerName, &kil.LogonDomainName);
        else
            hr = UnicodeStringInitWithString(m_rgFieldStrings [SFI_DOMAINNAME],
                                             &kil.LogonDomainName);

        /* Fill in the username and password. */
        if (SUCCEEDED(hr))
        {
            hr = UnicodeStringInitWithString(m_rgFieldStrings[SFI_USERNAME], &kil.UserName);
            if (SUCCEEDED(hr))
            {
                hr = UnicodeStringInitWithString(m_rgFieldStrings[SFI_PASSWORD], &kil.Password);
                if (SUCCEEDED(hr))
                {
                    /* Allocate copies of, and package, the strings in a binary blob. */
                    kil.MessageType = KerbInteractiveLogon;
                    hr = KerbInteractiveLogonPack(kil,
                                                  &pcpCredentialSerialization->rgbSerialization,
                                                  &pcpCredentialSerialization->cbSerialization);
                    if (SUCCEEDED(hr))
                    {
                        ULONG ulAuthPackage;
                        hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                        if (SUCCEEDED(hr))
                        {
                            pcpCredentialSerialization->ulAuthenticationPackage = ulAuthPackage;
                            pcpCredentialSerialization->clsidCredentialProvider = CLSID_VBoxCredProvider;

                            /*
                             * At this point the credential has created the serialized credential used for logon
                             * By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
                             * that we have all the information we need and it should attempt to submit the
                             * serialized credential.
                             */
                            *pcpGetSerializationResponse = CPGSR_RETURN_CREDENTIAL_FINISHED;
                        }
                    }
                }
            }
        }
    }
    else
    {
        DWORD dwErr = GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
    }

    Log(("VBoxCredential::GetSerialization: Returned 0x%08x\n", hr));
    return hr;
}


/*
 * ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
 * and the icon displayed in the case of a logon failure.  For example, we have chosen to
 * customize the error shown in the case of bad username/password and in the case of the account
 * being disabled.
 */
HRESULT VBoxCredential::ReportResult(NTSTATUS ntsStatus,
                                     NTSTATUS ntsSubstatus,
                                     PWSTR* ppwszOptionalStatusText,
                                     CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = NULL;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    DWORD dwStatusInfo = (DWORD)-1;

    /* Look for a match on status and substatus. */
    for (DWORD i = 0; i < ARRAYSIZE(s_rgLogonStatusInfo); i++)
    {
        if (s_rgLogonStatusInfo[i].ntsStatus == ntsStatus && s_rgLogonStatusInfo[i].ntsSubstatus == ntsSubstatus)
        {
            dwStatusInfo = i;
            break;
        }
    }

    if ((DWORD)-1 != dwStatusInfo)
    {
        if (SUCCEEDED(SHStrDupW(s_rgLogonStatusInfo[dwStatusInfo].pwzMessage, ppwszOptionalStatusText)))
            *pcpsiOptionalStatusIcon = s_rgLogonStatusInfo[dwStatusInfo].cpsi;
    }

    /* Try to lookup a text message for error code. */
    LPVOID lpMessageBuffer = NULL;
    HMODULE hNtDLL = LoadLibrary(L"NTDLL.DLL");
    if (hNtDLL)
    {
        FormatMessage(  FORMAT_MESSAGE_ALLOCATE_BUFFER
                      | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_FROM_HMODULE,
                      hNtDLL,
                      ntsStatus,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMessageBuffer,
                      0,
                      NULL);
        FreeLibrary(hNtDLL);
    }

    Log(("VBoxCredential::ReportResult: ntsStatus=%ld, ntsSubstatus=%ld, ppwszOptionalStatusText=%p, pcpsiOptionalStatusIcon=%p, dwStatusInfo=%ld, Message=%ls\n",
         ntsStatus, ntsSubstatus, ppwszOptionalStatusText, pcpsiOptionalStatusIcon, dwStatusInfo, lpMessageBuffer ? lpMessageBuffer : L"<None>"));

    /* Print to user-friendly message to release log. */
    if (FAILED(ntsStatus))
    {
        if (lpMessageBuffer)
            LogRel(("VBoxCredProv: %ls\n", lpMessageBuffer));
        /* Login attempt failed; reset and clear all data. */
        Reset();
    }

    if (lpMessageBuffer)
        LocalFree(lpMessageBuffer);

    /*
     * Since NULL is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
     * this function can't fail
     */
    return S_OK;
}
