//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Helper functions for copying parameters and packaging the buffer
// for GetSerialization.
//
// Modifications (c) 2009 Sun Microsystems, Inc.
//

#include "helpers.h"
#include <intsafe.h>

#include <VBox/log.h>

//
// Copies the field descriptor pointed to by rcpfd into a buffer allocated
// using CoTaskMemAlloc. Returns that buffer in ppcpfd.
//
HRESULT FieldDescriptorCoAllocCopy(
                                   const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
                                   CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd
                                   )
{
    HRESULT hr;
    DWORD cbStruct = sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR);

    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* pcpfd =
        (CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*)CoTaskMemAlloc(cbStruct);

    if (pcpfd)
    {
        pcpfd->dwFieldID = rcpfd.dwFieldID;
        pcpfd->cpft = rcpfd.cpft;

        if (rcpfd.pszLabel)
        {
            hr = SHStrDupW(rcpfd.pszLabel, &pcpfd->pszLabel);
        }
        else
        {
            pcpfd->pszLabel = NULL;
            hr = S_OK;
        }
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(hr))
    {
        *ppcpfd = pcpfd;
    }
    else
    {
        CoTaskMemFree(pcpfd);
        *ppcpfd = NULL;
    }


    return hr;
}

//
// Coppies rcpfd into the buffer pointed to by pcpfd. The caller is responsible for
// allocating pcpfd. This function uses CoTaskMemAlloc to allocate memory for
// pcpfd->pszLabel.
//
HRESULT FieldDescriptorCopy(
                            const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
                            CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* pcpfd
                            )
{
    HRESULT hr;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR cpfd;

    cpfd.dwFieldID = rcpfd.dwFieldID;
    cpfd.cpft = rcpfd.cpft;

    if (rcpfd.pszLabel)
    {
        hr = SHStrDupW(rcpfd.pszLabel, &cpfd.pszLabel);
    }
    else
    {
        cpfd.pszLabel = NULL;
        hr = S_OK;
    }

    if (SUCCEEDED(hr))
    {
        *pcpfd = cpfd;
    }

    return hr;
}

//
// This function copies the length of pwz and the pointer pwz into the UNICODE_STRING structure
// This function is intended for serializing a credential in GetSerialization only.
// Note that this function just makes a copy of the string pointer. It DOES NOT ALLOCATE storage!
// Be very, very sure that this is what you want, because it probably isn't outside of the
// exact GetSerialization call where the sample uses it.
//
HRESULT UnicodeStringInitWithString(
                                    PWSTR pwz,
                                    UNICODE_STRING* pus
                                    )
{
    HRESULT hr;
    if (pwz)
    {
        size_t lenString;
        hr = StringCchLengthW(pwz, USHORT_MAX, &(lenString));
        if (SUCCEEDED(hr))
        {
            USHORT usCharCount;
            hr = SizeTToUShort(lenString, &usCharCount);
            if (SUCCEEDED(hr))
            {
                USHORT usSize;
                hr = SizeTToUShort(sizeof(WCHAR), &usSize);
                if (SUCCEEDED(hr))
                {
                    hr = UShortMult(usCharCount, usSize, &(pus->Length)); // Explicitly NOT including NULL terminator
                    if (SUCCEEDED(hr))
                    {
                        pus->MaximumLength = pus->Length;
                        pus->Buffer = pwz;
                        hr = S_OK;
                    }
                    else
                    {
                        hr = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
                    }
                }
            }
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

//
// The following function is intended to be used ONLY with the Kerb*Pack functions.  It does
// no bounds-checking because its callers have precise requirements and are written to respect
// its limitations.
// You can read more about the UNICODE_STRING type at:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/secauthn/security/unicode_string.asp
//
static void _UnicodeStringPackedUnicodeStringCopy(
    const UNICODE_STRING& rus,
    PWSTR pwzBuffer,
    UNICODE_STRING* pus
    )
{
    pus->Length = rus.Length;
    pus->MaximumLength = rus.Length;
    pus->Buffer = pwzBuffer;

    CopyMemory(pus->Buffer, rus.Buffer, pus->Length);
}

//
// WinLogon and LSA consume "packed" KERB_INTERACTIVE_LOGONs.  In these, the PWSTR members of each
// UNICODE_STRING are not actually pointers but byte offsets into the overall buffer represented
// by the packed KERB_INTERACTIVE_LOGON.  For example:
//
// kil.LogonDomainName.Length = 14                             -> Length is in bytes, not characters
// kil.LogonDomainName.Buffer = sizeof(KERB_INTERACTIVE_LOGON) -> LogonDomainName begins immediately
//                                                                after the KERB_... struct in the buffer
// kil.UserName.Length = 10
// kil.UserName.Buffer = sizeof(KERB_INTERACTIVE_LOGON) + 14   -> UNICODE_STRINGS are NOT null-terminated
//
// kil.Password.Length = 16
// kil.Password.Buffer = sizeof(KERB_INTERACTIVE_LOGON) + 14 + 10
//
// THere's more information on this at:
// http://msdn.microsoft.com/msdnmag/issues/05/06/SecurityBriefs/#void
//

HRESULT KerbInteractiveLogonPack(
                                 const KERB_INTERACTIVE_LOGON& rkil,
                                 BYTE** prgb,
                                 DWORD* pcb
                                 )
{
    HRESULT hr;

    // alloc space for struct plus extra for the three strings
    DWORD cb = sizeof(rkil) +
        rkil.LogonDomainName.Length +
        rkil.UserName.Length +
        rkil.Password.Length;

    KERB_INTERACTIVE_LOGON* pkil = (KERB_INTERACTIVE_LOGON*)CoTaskMemAlloc(cb);

    if (pkil)
    {
        pkil->MessageType = rkil.MessageType;

        //
        // point pbBuffer at the beginning of the extra space
        //
        BYTE* pbBuffer = (BYTE*)pkil + sizeof(KERB_INTERACTIVE_LOGON);

        //
        // copy each string,
        // fix up appropriate buffer pointer to be offset,
        // advance buffer pointer over copied characters in extra space
        //
        _UnicodeStringPackedUnicodeStringCopy(rkil.LogonDomainName, (PWSTR)pbBuffer, &pkil->LogonDomainName);
        pkil->LogonDomainName.Buffer = (PWSTR)(pbBuffer - (BYTE*)pkil);
        pbBuffer += pkil->LogonDomainName.Length;

        _UnicodeStringPackedUnicodeStringCopy(rkil.UserName, (PWSTR)pbBuffer, &pkil->UserName);
        pkil->UserName.Buffer = (PWSTR)(pbBuffer - (BYTE*)pkil);
        pbBuffer += pkil->UserName.Length;

        _UnicodeStringPackedUnicodeStringCopy(rkil.Password, (PWSTR)pbBuffer, &pkil->Password);
        pkil->Password.Buffer = (PWSTR)(pbBuffer - (BYTE*)pkil);

        *prgb = (BYTE*)pkil;
        *pcb = cb;

        hr = S_OK;
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

//
// Unpack a KERB_INTERACTIVE_UNLOCK_LOGON *in place*.  That is, reset the Buffers from being offsets to
// being real pointers.  This means, of course, that passing the resultant struct across any sort of
// memory space boundary is not going to work -- repack it if necessary!
//
//
// Unpack a KERB_INTERACTIVE_UNLOCK_LOGON *in place*.  That is, reset the Buffers from being offsets to
// being real pointers.  This means, of course, that passing the resultant struct across any sort of
// memory space boundary is not going to work -- repack it if necessary!
//
void KerbInteractiveLogonUnpackInPlace(
    __inout_bcount(cb) KERB_INTERACTIVE_UNLOCK_LOGON* pkiul
    )
{
    KERB_INTERACTIVE_LOGON* pkil = &pkiul->Logon;

    pkil->LogonDomainName.Buffer = pkil->LogonDomainName.Buffer
        ? (PWSTR)((BYTE*)pkiul + (ULONG_PTR)pkil->LogonDomainName.Buffer)
        : NULL;

    pkil->UserName.Buffer = pkil->UserName.Buffer
        ? (PWSTR)((BYTE*)pkiul + (ULONG_PTR)pkil->UserName.Buffer)
        : NULL;

    pkil->Password.Buffer = pkil->Password.Buffer
        ? (PWSTR)((BYTE*)pkiul + (ULONG_PTR)pkil->Password.Buffer)
        : NULL;
}

//
// This function packs the string pszSourceString in pszDestinationString
// for use with LSA functions including LsaLookupAuthenticationPackage.
//
HRESULT LsaInitString(PSTRING pszDestinationString, PCSTR pszSourceString)
{
    size_t cchLength;
    HRESULT hr = StringCchLengthA(pszSourceString, USHORT_MAX, &cchLength);
    if (SUCCEEDED(hr))
    {
        USHORT usLength;
        hr = SizeTToUShort(cchLength, &usLength);

        if (SUCCEEDED(hr))
        {
            pszDestinationString->Buffer = (PCHAR)pszSourceString;
            pszDestinationString->Length = usLength;
            pszDestinationString->MaximumLength = pszDestinationString->Length+1;
            hr = S_OK;
        }
    }
    return hr;
}

//
// Retrieves the 'negotiate' AuthPackage from the LSA. In this case, Kerberos
// For more information on auth packages see this msdn page:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/secauthn/security/msv1_0_lm20_logon.asp
//
HRESULT RetrieveNegotiateAuthPackage(ULONG * pulAuthPackage)
{
    HRESULT hr;
    HANDLE hLsa;

    NTSTATUS status = LsaConnectUntrusted(&hLsa);
    if (SUCCEEDED(HRESULT_FROM_NT(status)))
    {

        ULONG ulAuthPackage;
        LSA_STRING lsaszKerberosName;
        LsaInitString(&lsaszKerberosName, NEGOSSP_NAME_A);

        status = LsaLookupAuthenticationPackage(hLsa, &lsaszKerberosName, &ulAuthPackage);
        if (SUCCEEDED(HRESULT_FROM_NT(status)))
        {
            *pulAuthPackage = ulAuthPackage;
            hr = S_OK;
        }
        else
        {
            hr = HRESULT_FROM_NT(status);
        }
        LsaDeregisterLogonProcess(hLsa);
    }
    else
    {
        hr= HRESULT_FROM_NT(status);
    }

    return hr;
}

/* Re-defined function for *not* having a log handle (= log file created).
   For some reason this does not work in this early stage (yet). */
PRTLOGGER RTLogDefaultInit(void)
{
    return NULL;
}
