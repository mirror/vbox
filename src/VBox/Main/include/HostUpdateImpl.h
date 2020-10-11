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
#include <iprt/http.h> /* RTHTTP */


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
    /** @name wrapped IHostUpdate attributes and methods
     * @{ */
    HRESULT updateCheck(UpdateCheckType_T aCheckType, ComPtr<IProgress> &aProgress) RT_OVERRIDE;
    HRESULT getUpdateResponse(BOOL *aUpdateNeeded) RT_OVERRIDE;
    HRESULT getUpdateVersion(com::Utf8Str &aUpdateVersion) RT_OVERRIDE;
    HRESULT getUpdateURL(com::Utf8Str &aUpdateURL) RT_OVERRIDE;
    HRESULT getUpdateCheckNeeded(BOOL *aUpdateCheckNeeded) RT_OVERRIDE;
    /** @} */

#ifdef VBOX_WITH_HOST_UPDATE_CHECK
    Utf8Str i_platformInfo();
    class UpdateCheckTask;
    HRESULT i_updateCheckTask(UpdateCheckTask *pTask);
    HRESULT i_checkForVBoxUpdate();
    HRESULT i_checkForVBoxUpdateInner(RTHTTP hHttp, com::Utf8Str const &strUrl, com::Utf8Str const &strUserAgent,
                                      ComPtr<ISystemProperties> const &ptrSystemProperties);
#endif

    /** @name Data members.
     * @{ */
    VirtualBox * const mVirtualBox;
    BOOL     m_updateNeeded;
    Utf8Str  m_updateVersion;
    Utf8Str  m_updateURL;
    /** @} */
};

#endif /* !MAIN_INCLUDED_HostUpdateImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
