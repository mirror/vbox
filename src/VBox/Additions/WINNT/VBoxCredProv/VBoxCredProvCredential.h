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

#ifndef __VBOX_CREDPROV_CREDENTIAL_H__
#define __VBOX_CREDPROV_CREDENTIAL_H__

#include <windows.h>
#include <intsafe.h>
#include <ntsecapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <shlguid.h>
#include <strsafe.h>

#pragma warning(push)
#pragma warning(disable : 4995)
#include <shlwapi.h>
#pragma warning(pop)

#include <iprt/string.h>

#include "VBoxCredentialProvider.h"

class VBoxCredProvProvider;

class VBoxCredProvCredential : public ICredentialProviderCredential
{
    public:

        VBoxCredProvCredential(VBoxCredProvProvider *pProvider);

        virtual ~VBoxCredProvCredential(void);

    public: /* IUknown overrides. */

        IFACEMETHODIMP_(ULONG) AddRef(void);
        IFACEMETHODIMP_(ULONG) Release(void);
        IFACEMETHODIMP         QueryInterface(REFIID interfaceID, void **ppvInterface);

    public: /* ICredentialProviderCredential overrides. */

        IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* pcpce);
        IFACEMETHODIMP UnAdvise(void);

        IFACEMETHODIMP SetSelected(BOOL* pbAutoLogon);
        IFACEMETHODIMP SetDeselected(void);

        IFACEMETHODIMP GetFieldState(DWORD dwFieldID,
                                     CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                                     CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis);

        IFACEMETHODIMP GetStringValue(DWORD dwFieldID, PWSTR* ppwsz);
        IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp);
        IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, BOOL* pbChecked, PWSTR* ppwszLabel);
        IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem);
        IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, PWSTR* ppwszItem);
        IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo);

        IFACEMETHODIMP SetStringValue(DWORD dwFieldID, PCWSTR pcwzString);
        IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked);
        IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem);
        IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID);

        IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpGetSerializationResponse,
                                         CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION  *pcpCredentialSerialization,
                                         PWSTR                                         *ppwszOptionalStatusText,
                                         CREDENTIAL_PROVIDER_STATUS_ICON               *pcpsiOptionalStatusIcon);
        IFACEMETHODIMP ReportResult(NTSTATUS                         ntsStatus,
                                    NTSTATUS                         ntsSubstatus,
                                    PWSTR*                           ppwszOptionalStatusText,
                                    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon);

    public:

        void Reset(void);
        HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);
        int RetrieveCredentials(void);
        BOOL TranslateAccountName(PWSTR pwszDisplayName, PWSTR *ppwszAccoutName);
        BOOL ExtractAccoutData(PWSTR pwszAccountData, PWSTR *ppwszAccoutName, PWSTR *ppwszDomain);

    protected:

        HRESULT RTUTF16toToUnicode(PUNICODE_STRING pUnicodeDest, PRTUTF16 pwszSource, bool fCopy);
        HRESULT AllocateLogonPackage(const KERB_INTERACTIVE_UNLOCK_LOGON& rkiulIn,
                                     BYTE** prgb, DWORD* pcb);

    private:

        /** Pointer to parent. */
        VBoxCredProvProvider                 *m_pProvider;
        /** Internal reference count. */
        ULONG                                 m_cRefCount;
        /** The usage scenario for which we were enumerated. */
        CREDENTIAL_PROVIDER_USAGE_SCENARIO    m_cpUS;
        /** The actual credential strings. */
        PRTUTF16                              m_pwszCredentials[VBOXCREDPROV_NUM_FIELDS];
        /** Pointer to event handler. */
        ICredentialProviderCredentialEvents  *m_pEvents;
};
#endif /* !__VBOX_CREDPROV_CREDENTIAL_H__ */

