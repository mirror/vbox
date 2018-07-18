/* $Id$ */
/** @file
 * ICloudUserProfileManager  COM class implementations.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/com/array.h>
#include <map>

#include "CloudUserProfileManagerImpl.h"
#include "CloudUserProfileListImpl.h"
#include "VirtualBoxImpl.h"
#include "Global.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// CloudUserProfileManager constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////
CloudUserProfileManager::CloudUserProfileManager()
    : mParent(NULL)
{
}

CloudUserProfileManager::~CloudUserProfileManager()
{
}


HRESULT CloudUserProfileManager::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CloudUserProfileManager::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

HRESULT CloudUserProfileManager::init(VirtualBox *aParent)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    mSupportedProviders.clear();
    mSupportedProviders.push_back(CloudProviderId_OCI);

    autoInitSpan.setSucceeded();
    return S_OK;
}

void CloudUserProfileManager::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent) = NULL;
}

HRESULT CloudUserProfileManager::getSupportedProviders(std::vector<CloudProviderId_T> &aSupportedProviders)
{
    aSupportedProviders = mSupportedProviders;
    return S_OK;
}

HRESULT CloudUserProfileManager::getAllProfiles(std::vector<ComPtr<ICloudUserProfileList> > &aProfilesList)
{
    HRESULT hrc = S_OK;
    std::vector<ComPtr<ICloudUserProfileList> > lProfilesList;
    for (size_t i=0;i<mSupportedProviders.size();++i)
    {
        ComPtr<ICloudUserProfileList> lProfiles;
        hrc = getProfilesByProvider(mSupportedProviders.at(i), lProfiles);
        if (FAILED(hrc))
            break;

        lProfilesList.push_back(lProfiles);
    }

    if (SUCCEEDED(hrc))
        aProfilesList = lProfilesList;

    return hrc;
}

HRESULT CloudUserProfileManager::getProfilesByProvider(CloudProviderId_T aProviderType,
                                                       ComPtr<ICloudUserProfileList> &aProfiles)
{
    ComObjPtr<CloudUserProfileList> ptrCloudUserProfileList;
    HRESULT hrc = ptrCloudUserProfileList.createObject();
    switch(aProviderType)
    {
        case CloudProviderId_OCI:
        default:
            ComObjPtr<OCIUserProfileList> ptrOCIUserProfileList;
            hrc = ptrOCIUserProfileList.createObject();
            if (SUCCEEDED(hrc))
            {
                AutoReadLock wlock(this COMMA_LOCKVAL_SRC_POS);

                hrc = ptrOCIUserProfileList->init(mParent);
                if (SUCCEEDED(hrc))
                {
                    char szOciConfigPath[RTPATH_MAX];
                    int vrc = RTPathUserHome(szOciConfigPath, sizeof(szOciConfigPath));
                    if (RT_SUCCESS(vrc))
                        vrc = RTPathAppend(szOciConfigPath, sizeof(szOciConfigPath), ".oci" RTPATH_SLASH_STR "config");
                    if (RT_SUCCESS(vrc))
                    {
                        LogRel(("config = %s\n", szOciConfigPath));
                        if (RTFileExists(szOciConfigPath))
                        {
                            hrc = ptrOCIUserProfileList->readProfiles(szOciConfigPath);
                            if (SUCCEEDED(hrc))
                                LogRel(("Reading profiles from %s has been done\n", szOciConfigPath));
                            else
                                LogRel(("Reading profiles from %s hasn't been done\n", szOciConfigPath));

                            ptrCloudUserProfileList = ptrOCIUserProfileList;
                            hrc = ptrCloudUserProfileList.queryInterfaceTo(aProfiles.asOutParam());
                        }
                        else
                            /** @todo r=bird: You are constantly mixing up status codes.
                             * VERR_FILE_NOT_FOUND, VERR_INVALID_PARAMETER and VERR_NOT_FOUND are all VBox
                             * status codes.  They don't work for HRESULT types, because that's for
                             * COM/XPCOM status codes.   Here you should use setErrorBoth or setErrorVrc. */
                            hrc = setError(VERR_FILE_NOT_FOUND, tr("Could not locate the config file '%s'"), szOciConfigPath);
                    }
                    else
                        hrc = setErrorVrc(vrc);
                }
            }
            break;
    }

    return hrc;
}

