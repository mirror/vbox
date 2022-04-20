/* $Id$ */
/** @file
 * Update agent COM class implementation - Header
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_UpdateAgentImpl_h
#define MAIN_INCLUDED_UpdateAgentImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/http.h>

#include <VBox/settings.h>

#include "UpdateAgentWrap.h"
#include "HostUpdateAgentWrap.h"

class  UpdateAgentTask;
struct UpdateAgentTaskParms;

struct UpdateAgentTaskResult
{
    Utf8Str          strVer;
    Utf8Str          strWebUrl;
    Utf8Str          strDownloadUrl;
    UpdateSeverity_T enmSeverity;
    Utf8Str          strReleaseNotes;
};

class UpdateAgentBase
{
protected: /* Not directly instancable. */

    UpdateAgentBase()
        : m_VirtualBox(NULL)
        , m(new settings::UpdateAgent) { }

    virtual ~UpdateAgentBase() { }

public:

    /** @name Pure virtual public methods for internal purposes only
     *        (ensure there is a caller and a read lock before calling them!)
     * @{ */
    virtual HRESULT i_loadSettings(const settings::UpdateAgent &data) = 0;
    virtual HRESULT i_saveSettings(settings::UpdateAgent &data) = 0;

    virtual HRESULT i_setCheckCount(ULONG aCount) = 0;
    virtual HRESULT i_setLastCheckDate(const com::Utf8Str &aDate) = 0;
    /** @} */

protected:

    /** @name Pure virtual internal task callbacks.
     * @{ */
    friend UpdateAgentTask;
    virtual DECLCALLBACK(HRESULT) i_updateTask(UpdateAgentTask *pTask) = 0;
    /** @} */

    /** @name Static helper methods.
     * @{ */
    static Utf8Str i_getPlatformInfo(void);
    /** @} */

protected:
    VirtualBox * const m_VirtualBox;

    /** @name Data members.
     * @{ */
    settings::UpdateAgent *m;

    struct Data
    {
        UpdateAgentTaskResult m_lastResult;

        Utf8Str               m_strName;
        bool                  m_fHidden;
        UpdateState_T         m_enmState;
        uint32_t              m_uOrder;

        Data(void)
        {
            m_fHidden  = true;
            m_enmState = UpdateState_Invalid;
            m_uOrder   = UINT32_MAX;
        }
    } mData;
    /** @} */
};

class ATL_NO_VTABLE UpdateAgent :
    public UpdateAgentWrap,
    public UpdateAgentBase
{
public:
    DECLARE_COMMON_CLASS_METHODS(UpdateAgent)

    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox);
    void uninit(void);
    /** @}  */

    /** @name Public methods for internal purposes only
     *        (ensure there is a caller and a read lock before calling them!) */
    HRESULT i_loadSettings(const settings::UpdateAgent &data);
    HRESULT i_saveSettings(settings::UpdateAgent &data);

    virtual HRESULT i_setCheckCount(ULONG aCount);
    virtual HRESULT i_setLastCheckDate(const com::Utf8Str &aDate);
    /** @}  */

protected:
    /** @name Wrapped IUpdateAgent attributes and methods
     * @{ */
    virtual HRESULT check(ComPtr<IProgress> &aProgress);
    virtual HRESULT download(ComPtr<IProgress> &aProgress);
    virtual HRESULT install(ComPtr<IProgress> &aProgress);
    virtual HRESULT rollback(void);

    virtual HRESULT getName(com::Utf8Str &aName);
    virtual HRESULT getOrder(ULONG *aOrder);
    virtual HRESULT getDependsOn(std::vector<com::Utf8Str> &aDeps);
    virtual HRESULT getVersion(com::Utf8Str &aVer);
    virtual HRESULT getDownloadUrl(com::Utf8Str &aUrl);
    virtual HRESULT getWebUrl(com::Utf8Str &aUrl);
    virtual HRESULT getReleaseNotes(com::Utf8Str &aRelNotes);
    virtual HRESULT getEnabled(BOOL *aEnabled);
    virtual HRESULT setEnabled(BOOL aEnabled);
    virtual HRESULT getHidden(BOOL *aHidden);
    virtual HRESULT getState(UpdateState_T *aState);
    virtual HRESULT getCheckCount(ULONG *aCount);
    virtual HRESULT getCheckFrequency(ULONG  *aFreqSeconds);
    virtual HRESULT setCheckFrequency(ULONG aFreqSeconds);
    virtual HRESULT getChannel(UpdateChannel_T *aChannel);
    virtual HRESULT setChannel(UpdateChannel_T aChannel);
    virtual HRESULT getRepositoryURL(com::Utf8Str &aRepo);
    virtual HRESULT setRepositoryURL(const com::Utf8Str &aRepo);
    virtual HRESULT getProxyMode(ProxyMode_T *aMode);
    virtual HRESULT setProxyMode(ProxyMode_T aMode);
    virtual HRESULT getProxyURL(com::Utf8Str &aAddress);
    virtual HRESULT setProxyURL(const com::Utf8Str &aAddress);
    virtual HRESULT getLastCheckDate(com::Utf8Str &aData);
    /** @} */
};

/** @todo Put this into an own module, e.g. HostUpdateAgentImpl.[cpp|h]. */

class ATL_NO_VTABLE HostUpdateAgent :
    public UpdateAgent
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_COMMON_CLASS_METHODS(HostUpdateAgent)

    HRESULT init(VirtualBox *aVirtualBox);
    void    uninit(void);

    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

private:
    /** @name Implemented (pure) virtual methods from UpdateAgent.
     * @{ */
    HRESULT check(ComPtr<IProgress> &aProgress);

    DECLCALLBACK(HRESULT) i_updateTask(UpdateAgentTask *pTask);
    /** @}  */

    HRESULT i_checkForUpdate(void);
    HRESULT i_checkForUpdateInner(RTHTTP hHttp, com::Utf8Str const &strUrl, com::Utf8Str const &strUserAgent);
};

#endif /* !MAIN_INCLUDED_UpdateAgentImpl_h */

