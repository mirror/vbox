/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ____H_CLOUDUSERPROFILEMANAGERIMPL
#define ____H_CLOUDUSERPROFILEMANAGERIMPL

/* VBox includes */
#include "CloudUserProfileManagerWrap.h"
#include "CloudUserProfilesImpl.h"

/* VBox forward declarations */

class ATL_NO_VTABLE CloudUserProfileManager :
    public CloudUserProfileManagerWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(CloudUserProfileManager)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox);
    void uninit();

private:
    ComPtr<VirtualBox> const mParent;       /**< Strong reference to the parent object (VirtualBox/IMachine). */
#ifdef CLOUD_PROVIDERS_IN_EXTPACK
    std::vector<ComPtr<ICloudUserProfileManager>> mUserProfileManagers;
#else
    std::vector<CloudProviderId_T> mSupportedProviders;
#endif

    HRESULT getSupportedProviders(std::vector<CloudProviderId_T> &aProviderTypes);
    HRESULT getAllProfiles(std::vector< ComPtr<ICloudUserProfiles> > &aProfilesList);
    HRESULT getProfilesByProvider(CloudProviderId_T aProviderType, ComPtr<ICloudUserProfiles> &aProfiles);
};

#endif // !____H_CLOUDUSERPROFILEMANAGERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

