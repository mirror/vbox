/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_HostUpdateImpl_h
#define MAIN_INCLUDED_HostUpdateImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostUpdateWrap.h"


class ATL_NO_VTABLE HostUpdate
    : public HostUpdateWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(HostUpdate)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(VirtualBox *aVirtualBox);
    void uninit();

private:
    // wrapped IHostUpdate attributes and methods
    HRESULT getUpdate(ComPtr<IHostUpdate> &aUpdate);
    HRESULT updateCheck(UpdateCheckType_T aCheckType, ComPtr<IProgress> &aProgress);
    HRESULT getUpdateResponse(BOOL *aUpdateNeeded);
    HRESULT getUpdateVersion(com::Utf8Str &aUpdateVersion);
    HRESULT getUpdateURL(com::Utf8Str &aUpdateURL);
    HRESULT getUpdateCheckNeeded(BOOL *aUpdateCheckNeeded);

    Bstr platformInfo();

    VirtualBox * const mVirtualBox;
    ComPtr<IHostUpdate> m_pHostUpdate;
    BOOL m_updateNeeded;
    Utf8Str  m_updateVersion;
    Utf8Str  m_updateURL;

    class UpdateCheckTask;

    HRESULT i_updateCheckTask(UpdateCheckTask *pTask);
    HRESULT i_checkForVBoxUpdate();
};

#endif /* !MAIN_INCLUDED_HostUpdateImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
