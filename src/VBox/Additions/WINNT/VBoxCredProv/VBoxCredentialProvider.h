/* $Id$ */
/** @file
 * VBoxCredentialProvider - Main file of the VirtualBox Credential Provider.
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

#ifndef ___VBOX_CREDENTIALPROVIDER_H___
#define ___VBOX_CREDENTIALPROVIDER_H___

#include <windows.h>
#include <credentialprovider.h>

#include "VBoxCredProvUtils.h"

/** The VirtualBox credential provider class ID -- must not be changed. */
DEFINE_GUID(CLSID_VBoxCredProvider, 0x275d3bcc, 0x22bb, 0x4948, 0xa7, 0xf6, 0x3a, 0x30, 0x54, 0xeb, 0xa9, 0x2b);

/**
 * The credential provider's UI field IDs, used for
 * handling / identifying them.
 */
enum VBOXCREDPROV_FIELDID
{
    VBOXCREDPROV_FIELDID_TILEIMAGE       = 0,
    VBOXCREDPROV_FIELDID_USERNAME        = 1,
    VBOXCREDPROV_FIELDID_PASSWORD        = 2,
    VBOXCREDPROV_FIELDID_DOMAINNAME      = 3,
    VBOXCREDPROV_FIELDID_SUBMIT_BUTTON   = 4
};

/* Note: If new fields are added to VBOXCREDPROV_FIELDID and s_VBoxCredProvFields,
         don't forget to increase this define! */
#define VBOXCREDPROV_NUM_FIELDS            5

struct VBOXCREDPROV_FIELD
{
    /** The actual description of this field: It's label,
     *  official field type ID, ... */
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR desc;
    /** The field's display state within the. */
    CREDENTIAL_PROVIDER_FIELD_STATE state;
    /** The interactive state: Used when this field gets shown to determine
     *  its state -- currently, only focussing is implemented. */
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE stateInteractive;
};

#ifndef PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR
 #define PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*
#endif

static const VBOXCREDPROV_FIELD s_VBoxCredProvFields[] =
{
    /** The user's profile image. */
    { { VBOXCREDPROV_FIELDID_TILEIMAGE,     CPFT_TILE_IMAGE,    L"Image" },       CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE    },
    { { VBOXCREDPROV_FIELDID_USERNAME,      CPFT_LARGE_TEXT,    L"Username" },    CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE    },
    { { VBOXCREDPROV_FIELDID_PASSWORD,      CPFT_PASSWORD_TEXT, L"Password" },    CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_FOCUSED },
    { { VBOXCREDPROV_FIELDID_SUBMIT_BUTTON, CPFT_SUBMIT_BUTTON, L"Submit" },      CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE    },
    { { VBOXCREDPROV_FIELDID_DOMAINNAME,    CPFT_LARGE_TEXT,    L"Domain Name" }, CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_FOCUSED }
};

/** Prototypes. */
void VBoxCredentialProviderAcquire(void);
void VBoxCredentialProviderRelease(void);
LONG VBoxCredentialProviderRefCount(void);

HRESULT VBoxCredentialProviderCreate(REFCLSID classID,
                                     REFIID interfaceID, void **ppvInterface);

#endif /* !___VBOX_CREDENTIALPROVIDER_H___ */

