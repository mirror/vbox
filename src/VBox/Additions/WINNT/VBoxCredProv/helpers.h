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
// Modifications (c) 2009-2010 Oracle Corporation
//

#pragma once
#include "common.h"
#include <windows.h>
#include <strsafe.h>

#pragma warning(push)
#pragma warning(disable : 4995)
#include <shlwapi.h>
#pragma warning(pop)



//makes a copy of a field descriptor using CoTaskMemAlloc
HRESULT FieldDescriptorCoAllocCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd
    );

//makes a copy of a field descriptor on the normal heap
HRESULT FieldDescriptorCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* pcpfd
    );

//creates a UNICODE_STRING from a normal string
HRESULT UnicodeStringInitWithString(
    PWSTR pwz,
    UNICODE_STRING* pus
    );

//packages the credentials into the buffer that the system expects
HRESULT KerbInteractiveLogonPack(
    const KERB_INTERACTIVE_LOGON& rkil,
    BYTE** prgb,
    DWORD* pcb
    );

//unpackages the "packed" version of the creds in-place into the "unpacked" version
void KerbInteractiveLogonUnpackInPlace(
    KERB_INTERACTIVE_UNLOCK_LOGON* pkiul
    );


//get the authentication package that will be used for our logon attempt
HRESULT RetrieveNegotiateAuthPackage(
    ULONG * pulAuthPackage
    );
