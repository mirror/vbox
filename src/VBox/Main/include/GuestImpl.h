/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTIMPL
#define ____H_GUESTIMPL

#include "GuestWrap.h"
#include "VirtualBoxBase.h"
#include <iprt/list.h>
#include <iprt/time.h>
#include <VBox/ostypes.h>
#include <VBox/vmm/stam.h>

#include "AdditionsFacilityImpl.h"
#include "GuestCtrlImplPrivate.h"
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "GuestDnDSourceImpl.h"
# include "GuestDnDTargetImpl.h"
#endif
#include "GuestSessionImpl.h"
#include "HGCM.h"

typedef enum
{
    GUESTSTATTYPE_CPUUSER     = 0,
    GUESTSTATTYPE_CPUKERNEL   = 1,
    GUESTSTATTYPE_CPUIDLE     = 2,
    GUESTSTATTYPE_MEMTOTAL    = 3,
    GUESTSTATTYPE_MEMFREE     = 4,
    GUESTSTATTYPE_MEMBALLOON  = 5,
    GUESTSTATTYPE_MEMCACHE    = 6,
    GUESTSTATTYPE_PAGETOTAL   = 7,
    GUESTSTATTYPE_PAGEFREE    = 8,
    GUESTSTATTYPE_MAX         = 9
} GUESTSTATTYPE;

class Console;

class ATL_NO_VTABLE Guest :
    public GuestWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR (Guest)

    HRESULT FinalConstruct();
    void FinalRelease();

    // Public initializer/uninitializer for internal purposes only.
    HRESULT init(Console *aParent);
    void uninit();


public:
    /** @name Static internal methods.
     * @{ */
#ifdef VBOX_WITH_GUEST_CONTROL
    /** Static callback for handling guest control notifications. */
    static DECLCALLBACK(int) i_notifyCtrlDispatcher(void *pvExtension, uint32_t u32Function, void *pvData, uint32_t cbData);
    static DECLCALLBACK(void) i_staticUpdateStats(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick);
#endif
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    void i_enableVMMStatistics(BOOL aEnable) { mCollectVMMStats = aEnable; };
    void i_setAdditionsInfo(com::Utf8Str aInterfaceVersion, VBOXOSTYPE aOsType);
    void i_setAdditionsInfo2(uint32_t a_uFullVersion, const char *a_pszName, uint32_t a_uRevision, uint32_t a_fFeatures);
    bool i_facilityIsActive(VBoxGuestFacilityType enmFacility);
    void i_facilityUpdate(VBoxGuestFacilityType a_enmFacility, VBoxGuestFacilityStatus a_enmStatus,
                          uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    ComObjPtr<Console> i_getConsole(void) { return mParent; }
    void i_setAdditionsStatus(VBoxGuestFacilityType a_enmFacility, VBoxGuestFacilityStatus a_enmStatus,
                              uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    void i_onUserStateChange(Bstr aUser, Bstr aDomain, VBoxGuestUserState enmState, const uint8_t *puDetails, uint32_t cbDetails);
    void i_setSupportedFeatures(uint32_t aCaps);
    HRESULT i_setStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal);
    BOOL i_isPageFusionEnabled();
    static HRESULT i_setErrorStatic(HRESULT aResultCode, const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }
#ifdef VBOX_WITH_GUEST_CONTROL
    int         i_dispatchToSession(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    uint32_t    i_getAdditionsVersion(void) { return mData.mAdditionsVersionFull; }
    int         i_sessionRemove(GuestSession *pSession);
    int         i_sessionCreate(const GuestSessionStartupInfo &ssInfo, const GuestCredentials &guestCreds,
                                ComObjPtr<GuestSession> &pGuestSession);
    inline bool i_sessionExists(uint32_t uSessionID);

#endif
    /** @}  */

private:

     // wrapped IGuest properties
     HRESULT getOSTypeId(com::Utf8Str &aOSTypeId);
     HRESULT getAdditionsRunLevel(AdditionsRunLevelType_T *aAdditionsRunLevel);
     HRESULT getAdditionsVersion(com::Utf8Str &aAdditionsVersion);
     HRESULT getAdditionsRevision(ULONG *aAdditionsRevision);
     HRESULT getDnDSource(ComPtr<IGuestDnDSource> &aDnDSource);
     HRESULT getDnDTarget(ComPtr<IGuestDnDTarget> &aDnDTarget);
     HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);
     HRESULT getFacilities(std::vector<ComPtr<IAdditionsFacility> > &aFacilities);
     HRESULT getSessions(std::vector<ComPtr<IGuestSession> > &aSessions);
     HRESULT getMemoryBalloonSize(ULONG *aMemoryBalloonSize);
     HRESULT setMemoryBalloonSize(ULONG aMemoryBalloonSize);
     HRESULT getStatisticsUpdateInterval(ULONG *aStatisticsUpdateInterval);
     HRESULT setStatisticsUpdateInterval(ULONG aStatisticsUpdateInterval);
     HRESULT internalGetStatistics(ULONG *aCpuUser,
                                   ULONG *aCpuKernel,
                                   ULONG *aCpuIdle,
                                   ULONG *aMemTotal,
                                   ULONG *aMemFree,
                                   ULONG *aMemBalloon,
                                   ULONG *aMemShared,
                                   ULONG *aMemCache,
                                   ULONG *aPagedTotal,
                                   ULONG *aMemAllocTotal,
                                   ULONG *aMemFreeTotal,
                                   ULONG *aMemBalloonTotal,
                                   ULONG *aMemSharedTotal);
     HRESULT getFacilityStatus(AdditionsFacilityType_T aFacility,
                               LONG64 *aTimestamp,
                               AdditionsFacilityStatus_T *aStatus);
     HRESULT getAdditionsStatus(AdditionsRunLevelType_T aLevel,
                                BOOL *aActive);
     HRESULT setCredentials(const com::Utf8Str &aUserName,
                            const com::Utf8Str &aPassword,
                            const com::Utf8Str &aDomain,
                            BOOL aAllowInteractiveLogon);

     // wrapped IGuest methods
     HRESULT createSession(const com::Utf8Str &aUser,
                           const com::Utf8Str &aPassword,
                           const com::Utf8Str &aDomain,
                           const com::Utf8Str &aSessionName,
                           ComPtr<IGuestSession> &aGuestSession);

     HRESULT findSession(const com::Utf8Str &aSessionName,
                         std::vector<ComPtr<IGuestSession> > &aSessions);
     HRESULT updateGuestAdditions(const com::Utf8Str &aSource,
                                  const std::vector<com::Utf8Str> &aArguments,
                                  const std::vector<AdditionsUpdateFlag_T> &aFlags,
                                  ComPtr<IProgress> &aProgress);


    /** @name Private internal methods.
     * @{ */
    void i_updateStats(uint64_t iTick);
    static int i_staticEnumStatsCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                         STAMVISIBILITY enmVisiblity, const char *pszDesc, void *pvUser);

    /** @}  */

    typedef std::map< AdditionsFacilityType_T, ComObjPtr<AdditionsFacility> > FacilityMap;
    typedef std::map< AdditionsFacilityType_T, ComObjPtr<AdditionsFacility> >::iterator FacilityMapIter;
    typedef std::map< AdditionsFacilityType_T, ComObjPtr<AdditionsFacility> >::const_iterator FacilityMapIterConst;

    /** Map for keeping the guest sessions. The primary key marks the guest session ID. */
    typedef std::map <uint32_t, ComObjPtr<GuestSession> > GuestSessions;

    struct Data
    {
        Data() : mAdditionsRunLevel(AdditionsRunLevelType_None)
            , mAdditionsVersionFull(0), mAdditionsRevision(0), mAdditionsFeatures(0)
        { }

        Utf8Str                     mOSTypeId;
        FacilityMap                 mFacilityMap;
        AdditionsRunLevelType_T     mAdditionsRunLevel;
        uint32_t                    mAdditionsVersionFull;
        Utf8Str                     mAdditionsVersionNew;
        uint32_t                    mAdditionsRevision;
        uint32_t                    mAdditionsFeatures;
        Utf8Str                     mInterfaceVersion;
        GuestSessions               mGuestSessions;
        uint32_t                    mNextSessionID;
    } mData;

    ULONG                           mMemoryBalloonSize;
    ULONG                           mStatUpdateInterval;
    uint64_t                        mNetStatRx;
    uint64_t                        mNetStatTx;
    uint64_t                        mNetStatLastTs;
    ULONG                           mCurrentGuestStat[GUESTSTATTYPE_MAX];
    ULONG                           mVmValidStats;
    BOOL                            mCollectVMMStats;
    BOOL                            mfPageFusionEnabled;

    const ComObjPtr<Console>        mParent;

#ifdef VBOX_WITH_GUEST_CONTROL
    /**
     * This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization.
     */
    const ComObjPtr<EventSource>    mEventSource;
    /** General extension callback for guest control. */
    HGCMSVCEXTHANDLE                mhExtCtrl;
#endif

#ifdef VBOX_WITH_DRAG_AND_DROP
    /** The guest's DnD source. */
    const ComObjPtr<GuestDnDSource> mDnDSource;
    /** The guest's DnD target. */
    const ComObjPtr<GuestDnDTarget> mDnDTarget;
#endif

    RTTIMERLR                       mStatTimer;
    uint32_t                        mMagic; /** @todo r=andy Rename this to something more meaningful. */
};
#define GUEST_MAGIC 0xCEED2006u /** @todo r=andy Not very well defined!? */

#endif // ____H_GUESTIMPL

