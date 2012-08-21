/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

#include "VirtualBoxBase.h"
#include <iprt/list.h>
#include <iprt/time.h>
#include <VBox/ostypes.h>

#include "AdditionsFacilityImpl.h"
#include "GuestCtrlImplPrivate.h"
#include "GuestSessionImpl.h"
#include "HGCM.h"

#ifdef VBOX_WITH_DRAG_AND_DROP
class GuestDnD;
#endif

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
#ifdef VBOX_WITH_GUEST_CONTROL
class Progress;
#endif

class ATL_NO_VTABLE Guest :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuest)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Guest, IGuest)

    DECLARE_NOT_AGGREGATABLE(Guest)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Guest)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuest)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Guest)

    HRESULT FinalConstruct(void);
    void FinalRelease(void);

    // Public initializer/uninitializer for internal purposes only.
    HRESULT init (Console *aParent);
    void uninit();

    // IGuest properties.
    STDMETHOD(COMGETTER(OSTypeId)) (BSTR *aOSTypeId);
    STDMETHOD(COMGETTER(AdditionsRunLevel)) (AdditionsRunLevelType_T *aRunLevel);
    STDMETHOD(COMGETTER(AdditionsVersion))(BSTR *a_pbstrAdditionsVersion);
    STDMETHOD(COMGETTER(AdditionsRevision))(ULONG *a_puAdditionsRevision);
    STDMETHOD(COMGETTER(Facilities)) (ComSafeArrayOut(IAdditionsFacility *, aFacilities));
    STDMETHOD(COMGETTER(Sessions)) (ComSafeArrayOut(IGuestSession *, aSessions));
    STDMETHOD(COMGETTER(MemoryBalloonSize)) (ULONG *aMemoryBalloonSize);
    STDMETHOD(COMSETTER(MemoryBalloonSize)) (ULONG aMemoryBalloonSize);
    STDMETHOD(COMGETTER(StatisticsUpdateInterval)) (ULONG *aUpdateInterval);
    STDMETHOD(COMSETTER(StatisticsUpdateInterval)) (ULONG aUpdateInterval);
    // IGuest methods.
    STDMETHOD(GetFacilityStatus)(AdditionsFacilityType_T aType, LONG64 *aTimestamp, AdditionsFacilityStatus_T *aStatus);
    STDMETHOD(GetAdditionsStatus)(AdditionsRunLevelType_T aLevel, BOOL *aActive);
    STDMETHOD(SetCredentials)(IN_BSTR aUsername, IN_BSTR aPassword,
                              IN_BSTR aDomain, BOOL aAllowInteractiveLogon);
    // Drag'n drop support.
    STDMETHOD(DragHGEnter)(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction);
    STDMETHOD(DragHGMove)(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction);
    STDMETHOD(DragHGLeave)(ULONG uScreenId);
    STDMETHOD(DragHGDrop)(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), BSTR *pstrFormat, DragAndDropAction_T *pResultAction);
    STDMETHOD(DragHGPutData)(ULONG uScreenId, IN_BSTR strFormat, ComSafeArrayIn(BYTE, data), IProgress **ppProgress);
    STDMETHOD(DragGHPending)(ULONG uScreenId, ComSafeArrayOut(BSTR, formats), ComSafeArrayOut(DragAndDropAction_T, allowedActions), DragAndDropAction_T *pDefaultAction);
    STDMETHOD(DragGHDropped)(IN_BSTR strFormat, DragAndDropAction_T action, IProgress **ppProgress);
    STDMETHOD(DragGHGetData)(ComSafeArrayOut(BYTE, data));
    // Misc stuff
    STDMETHOD(InternalGetStatistics)(ULONG *aCpuUser, ULONG *aCpuKernel, ULONG *aCpuIdle,
                                     ULONG *aMemTotal, ULONG *aMemFree, ULONG *aMemBalloon, ULONG *aMemShared, ULONG *aMemCache,
                                     ULONG *aPageTotal, ULONG *aMemAllocTotal, ULONG *aMemFreeTotal, ULONG *aMemBalloonTotal, ULONG *aMemSharedTotal);
    STDMETHOD(UpdateGuestAdditions)(IN_BSTR aSource, ComSafeArrayIn(AdditionsUpdateFlag_T, aFlags), IProgress **aProgress);
    STDMETHOD(CreateSession)(IN_BSTR aUser, IN_BSTR aPassword, IN_BSTR aDomain, IN_BSTR aSessionName, IGuestSession **aGuestSession);
    STDMETHOD(FindSession)(IN_BSTR aSessionName, ComSafeArrayOut(IGuestSession *, aSessions));

public:
    /** @name Static internal methods.
     * @{ */
#ifdef VBOX_WITH_GUEST_CONTROL
    /** Static callback for handling guest control notifications. */
    static DECLCALLBACK(int) notifyCtrlDispatcher(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
    static void staticUpdateStats(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick);
#endif
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    void enableVMMStatistics(BOOL aEnable) { mCollectVMMStats = aEnable; };
    void setAdditionsInfo(Bstr aInterfaceVersion, VBOXOSTYPE aOsType);
    void setAdditionsInfo2(uint32_t a_uFullVersion, const char *a_pszName, uint32_t a_uRevision, uint32_t a_fFeatures);
    bool facilityIsActive(VBoxGuestFacilityType enmFacility);
    void facilityUpdate(VBoxGuestFacilityType a_enmFacility, VBoxGuestFacilityStatus a_enmStatus, uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    void setAdditionsStatus(VBoxGuestFacilityType a_enmFacility, VBoxGuestFacilityStatus a_enmStatus, uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    void setSupportedFeatures(uint32_t aCaps);
    HRESULT setStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal);
    BOOL isPageFusionEnabled();
    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }
#ifdef VBOX_WITH_GUEST_CONTROL
    int         dispatchToSession(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData);
    uint32_t    getAdditionsVersion(void) { return mData.mAdditionsVersionFull; }
    Console    *getConsole(void) { return mParent; }
    int         sessionRemove(GuestSession *pSession);
    int         sessionCreate(const Utf8Str &strUser, const Utf8Str &strPassword, const Utf8Str &strDomain,
                              const Utf8Str &strSessionName, ComObjPtr<GuestSession> &pGuestSession);
    inline bool sessionExists(uint32_t uSessionID);
#endif
    /** @}  */

private:
    /** @name Private internal methods.
     * @{ */
    void updateStats(uint64_t iTick);
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

        Bstr                    mOSTypeId;
        FacilityMap             mFacilityMap;
        AdditionsRunLevelType_T mAdditionsRunLevel;
        uint32_t                mAdditionsVersionFull;
        Bstr                    mAdditionsVersionNew;
        uint32_t                mAdditionsRevision;
        uint32_t                mAdditionsFeatures;
        Bstr                    mInterfaceVersion;
        GuestSessions           mGuestSessions;
        uint32_t                mNextSessionID;
    };

    ULONG             mMemoryBalloonSize;
    ULONG             mStatUpdateInterval;
    ULONG             mCurrentGuestStat[GUESTSTATTYPE_MAX];
    ULONG             mGuestValidStats;
    BOOL              mCollectVMMStats;
    BOOL              mfPageFusionEnabled;

    Console *mParent;
    Data mData;

#ifdef VBOX_WITH_GUEST_CONTROL
    /** General extension callback for guest control. */
    HGCMSVCEXTHANDLE  mhExtCtrl;
#endif

#ifdef VBOX_WITH_DRAG_AND_DROP
    GuestDnD         *m_pGuestDnD;
    friend class GuestDnD;
    friend class GuestDnDPrivate;
#endif

    RTTIMERLR         mStatTimer;
    uint32_t          mMagic;
};
#define GUEST_MAGIC 0xCEED2006u

#endif // ____H_GUESTIMPL

