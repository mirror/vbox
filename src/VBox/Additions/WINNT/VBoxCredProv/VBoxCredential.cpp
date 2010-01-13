//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Modifications (c) 2009 Sun Microsystems, Inc.
//

#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif

#include <iprt/string.h>

#include "VBoxCredential.h"
#include "guid.h"


VBoxCredential::VBoxCredential() : m_cRef(1),
                                   m_pCredProvCredentialEvents(NULL)
{
    Log(("VBoxCredential::VBoxCredential\n"));

    DllAddRef();

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


void VBoxCredential::Reset(void)
{
    if (m_rgFieldStrings[SFI_PASSWORD])
    {
        // CoTaskMemFree (below) deals with NULL, but StringCchLength does not.
        size_t lenPassword;
        HRESULT hr = StringCchLengthW(m_rgFieldStrings[SFI_PASSWORD], 128, &(lenPassword));
        if (SUCCEEDED(hr))
        {
            SecureZeroMemory(m_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*m_rgFieldStrings[SFI_PASSWORD]));
        }
        else
        {
            // TODO: Determine how to handle count error here.
        }
    }

    /** @todo securely clear other fields (user name, domain, ...) as well. */
}


int VBoxCredential::Update(const char *pszUser,
                           const char *pszPw,
                           const char *pszDomain)
{
    /* Convert credentials to unicode. */
    PWSTR *ppwszStored;

    ppwszStored = &m_rgFieldStrings[SFI_USERNAME];
    CoTaskMemFree(*ppwszStored);
    SHStrDupA(pszUser, ppwszStored);

    ppwszStored = &m_rgFieldStrings[SFI_PASSWORD];
    CoTaskMemFree(*ppwszStored);
    SHStrDupA(pszPw, ppwszStored);

    ppwszStored = &m_rgFieldStrings[SFI_DOMAINNAME];
    CoTaskMemFree(*ppwszStored);
    SHStrDupA(pszDomain, ppwszStored);

    Log(("VBoxCredential::Update: user=%ls, pw=%ls, domain=%ls\n",
         m_rgFieldStrings[SFI_USERNAME] ? m_rgFieldStrings[SFI_USERNAME] : L"<empty>",
         m_rgFieldStrings[SFI_PASSWORD] ? m_rgFieldStrings[SFI_PASSWORD] : L"<empty>",
         m_rgFieldStrings[SFI_DOMAINNAME] ? m_rgFieldStrings[SFI_DOMAINNAME] : L"<empty>"));
    return S_OK;
}


// Initializes one credential with the field information passed in.
// Set the value of the SFI_USERNAME field to pwzUsername.
// Optionally takes a password for the SetSerialization case.
HRESULT VBoxCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                   const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
                                   const FIELD_STATE_PAIR* rgfsp)
{
    Log(("VBoxCredential::Initialize: cpus=%ld, rgcpfd=%p, rgfsp=%p\n", cpus, rgcpfd, rgfsp));

    HRESULT hr = S_OK;

    m_cpUS = cpus;

    /* Copy the field descriptors for each field. This is useful if you want to vary the
     * field descriptors based on what Usage scenario the credential was created for. */
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(m_rgCredProvFieldDescriptors); i++)
    {
        m_rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &m_rgCredProvFieldDescriptors[i]);
    }

    /* Fill in the default value to make winlogon happy. */
    hr = SHStrDupW(L"Submit", &m_rgFieldStrings[SFI_SUBMIT_BUTTON]);

    return S_OK;
}


/* LogonUI calls this in order to give us a callback in case we need to notify it of anything.
 * Store this callback pointer for later use. */
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

    Reset();

    if (m_pCredProvCredentialEvents)
        m_pCredProvCredentialEvents->Release();
    m_pCredProvCredentialEvents = NULL;
    return S_OK;
}


// LogonUI calls this function when our tile is selected (zoomed).
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions.  But if you want to do something
// more complicated, like change the contents of a field when the tile is
// selected, you would do it here.
HRESULT VBoxCredential::SetSelected(BOOL* pbAutoLogon)
{
    Log(("VBoxCredential::SetSelected\n"));

    /* Don't do auto logon here because it would retry too often with
     * every credential field (user name, password, domain, ...) which makes
     * winlogon wait before new login attempts can be made.
     */
    *pbAutoLogon = FALSE;
    return S_OK;
}


// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. The most common thing to do here (which we do below)
// is to clear out the password field.
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


// Gets info for a particular field of a tile. Called by logonUI to get information to
// display the tile.
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


// Sets ppwsz to the string value of the field at the index dwFieldID.
HRESULT VBoxCredential::GetStringValue(DWORD dwFieldID,
                                       PWSTR* ppwsz)
{
    Log(("VBoxCredential::GetStringValue: dwFieldID=%ld, pwz=%p\n", dwFieldID, ppwsz));

    HRESULT hr;

    // Check to make sure dwFieldID is a legitimate index.
    if (dwFieldID < ARRAYSIZE(m_rgCredProvFieldDescriptors) && ppwsz)
    {
        // Make a copy of the string and return that. The caller
        // is responsible for freeing it.
        hr = SHStrDupW(m_rgFieldStrings[dwFieldID], ppwsz);
        if (SUCCEEDED(hr))
            Log(("VBoxCredential::GetStringValue: dwFieldID=%ld, pwz=%ls\n", dwFieldID, *ppwsz));
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}


// Sets pdwAdjacentTo to the index of the field the submit button should be
// adjacent to. We recommend that the submit button is placed next to the last
// field which the user is required to enter information in. Optional fields
// should be below the submit button.
HRESULT VBoxCredential::GetSubmitButtonValue(DWORD dwFieldID,
                                             DWORD* pdwAdjacentTo)
{
    Log(("VBoxCredential::GetSubmitButtonValue: dwFieldID=%ld\n", dwFieldID));

    HRESULT hr;

    // Validate parameters.
    if ((SFI_SUBMIT_BUTTON == dwFieldID) && pdwAdjacentTo)
    {
        // pdwAdjacentTo is a pointer to the fieldID you want the submit button to appear next to.
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


// Sets the value of a field which can accept a string as a value.
// This is called on each keystroke when a user types into an edit field.
HRESULT VBoxCredential::SetStringValue(DWORD dwFieldID,
                                       PCWSTR pwz)
{
    Log(("VBoxCredential::SetStringValue: dwFieldID=%ld, pwz=%ls\n", dwFieldID, pwz));

    HRESULT hr;

    // Validate parameters.
    if (   dwFieldID < ARRAYSIZE(m_rgCredProvFieldDescriptors)
        && (CPFT_EDIT_TEXT     == m_rgCredProvFieldDescriptors[dwFieldID].cpft ||
            CPFT_PASSWORD_TEXT == m_rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        PWSTR* ppwszStored = &m_rgFieldStrings[dwFieldID];
        CoTaskMemFree(*ppwszStored);
        hr = SHStrDupW(pwz, ppwszStored);
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}


// The following methods are for logonUI to get the values of various UI elements and then communicate
// to the credential about what the user did in that field.  However, these methods are not implemented
// because our tile doesn't contain these types of UI elements

/* Gets the image to show in the user tile */
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


// Collect the username and password into a serialized credential for the correct usage scenario
// (logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials
// back to the system to log on.
HRESULT VBoxCredential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
                                         CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
                                         PWSTR* ppwszOptionalStatusText,
                                         CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    Log(("VBoxCredential::GetSerialization: pcpgsr=%p, pcpcs=%p, ppwszOptionalStatusText=%p, pcpsiOptionalStatusIcon=%p\n",
         pcpgsr, pcpcs, ppwszOptionalStatusText, pcpsiOptionalStatusIcon));

    UNREFERENCED_PARAMETER(ppwszOptionalStatusText);
    UNREFERENCED_PARAMETER(pcpsiOptionalStatusIcon);

    KERB_INTERACTIVE_LOGON kil;
    ZeroMemory(&kil, sizeof(kil));

    HRESULT hr;

    WCHAR wsz[MAX_COMPUTERNAME_LENGTH+1];
    DWORD cch = ARRAYSIZE(wsz);
    if (GetComputerNameW(wsz, &cch))
    {
        /* Is a domain name missing? Then use the name of the local computer. */
        if (NULL == m_rgFieldStrings [SFI_DOMAINNAME])
            hr = UnicodeStringInitWithString(wsz, &kil.LogonDomainName);
        else
            hr = UnicodeStringInitWithString(
                m_rgFieldStrings [SFI_DOMAINNAME], &kil.LogonDomainName);

        /* Fill in the username and password. */
        if (SUCCEEDED(hr))
        {
            hr = UnicodeStringInitWithString(m_rgFieldStrings[SFI_USERNAME], &kil.UserName);

            if (SUCCEEDED(hr))
            {
                hr = UnicodeStringInitWithString(m_rgFieldStrings[SFI_PASSWORD], &kil.Password);

                if (SUCCEEDED(hr))
                {
                    // Allocate copies of, and package, the strings in a binary blob
                    kil.MessageType = KerbInteractiveLogon;
                    hr = KerbInteractiveLogonPack(kil, &pcpcs->rgbSerialization, &pcpcs->cbSerialization);

                    if (SUCCEEDED(hr))
                    {
                        ULONG ulAuthPackage;
                        hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                        if (SUCCEEDED(hr))
                        {
                            pcpcs->ulAuthenticationPackage = ulAuthPackage;
                            pcpcs->clsidCredentialProvider = CLSID_VBoxCredProvider;

                            // At this point the credential has created the serialized credential used for logon
                            // By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
                            // that we have all the information we need and it should attempt to submit the
                            // serialized credential.
                            *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
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


// ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
// and the icon displayed in the case of a logon failure.  For example, we have chosen to
// customize the error shown in the case of bad username/password and in the case of the account
// being disabled.
HRESULT VBoxCredential::ReportResult(NTSTATUS ntsStatus,
                                     NTSTATUS ntsSubstatus,
                                     PWSTR* ppwszOptionalStatusText,
                                     CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = NULL;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    DWORD dwStatusInfo = (DWORD)-1;

    // Look for a match on status and substatus.
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
        {
            *pcpsiOptionalStatusIcon = s_rgLogonStatusInfo[dwStatusInfo].cpsi;
        }
    }

    /* Try to lookup a text message for error code */
    LPVOID lpMessageBuffer = NULL;
    HMODULE hMod = LoadLibrary(L"NTDLL.DLL");
    if (hMod)
    {
        FormatMessage(  FORMAT_MESSAGE_ALLOCATE_BUFFER
                      | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_FROM_HMODULE,
                      hMod,
                      ntsStatus,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMessageBuffer,
                      0,
                      NULL);
    }

    Log(("VBoxCredential::ReportResult: ntsStatus=%ld, ntsSubstatus=%ld, ppwszOptionalStatusText=%p, pcpsiOptionalStatusIcon=%p, dwStatusInfo=%ld, Message=%ls\n",
         ntsStatus, ntsSubstatus, ppwszOptionalStatusText, pcpsiOptionalStatusIcon, dwStatusInfo, lpMessageBuffer ? lpMessageBuffer : L"none"));

    if (lpMessageBuffer)
        LocalFree(lpMessageBuffer);
    FreeLibrary(hMod);

    // Since NULL is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
    // this function can't fail.
    return S_OK;
}
