/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_GUESTIMPL
#define ____H_GUESTIMPL

#include "VirtualBoxBase.h"
#include <VBox/ostypes.h>

#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/HostServices/GuestControlSvc.h>
# include <hgcm/HGCM.h>
using namespace guestControl;
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
    public VirtualBoxSupportErrorInfoImpl<Guest, IGuest>,
    public VirtualBoxSupportTranslation<Guest>,
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuest)
{
public:

    DECLARE_NOT_AGGREGATABLE(Guest)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Guest)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IGuest)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Guest)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();

    // IGuest properties
    STDMETHOD(COMGETTER(OSTypeId)) (BSTR *aOSTypeId);
    STDMETHOD(COMGETTER(AdditionsActive)) (BOOL *aAdditionsActive);
    STDMETHOD(COMGETTER(AdditionsVersion)) (BSTR *aAdditionsVersion);
    STDMETHOD(COMGETTER(SupportsSeamless)) (BOOL *aSupportsSeamless);
    STDMETHOD(COMGETTER(SupportsGraphics)) (BOOL *aSupportsGraphics);
    STDMETHOD(COMGETTER(MemoryBalloonSize)) (ULONG *aMemoryBalloonSize);
    STDMETHOD(COMSETTER(MemoryBalloonSize)) (ULONG aMemoryBalloonSize);
    STDMETHOD(COMGETTER(StatisticsUpdateInterval)) (ULONG *aUpdateInterval);
    STDMETHOD(COMSETTER(StatisticsUpdateInterval)) (ULONG aUpdateInterval);

    // IGuest methods
    STDMETHOD(SetCredentials)(IN_BSTR aUserName, IN_BSTR aPassword,
                              IN_BSTR aDomain, BOOL aAllowInteractiveLogon);
    STDMETHOD(ExecuteProcess)(IN_BSTR aCommand, ULONG aFlags,
                              ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                              IN_BSTR aStdIn, IN_BSTR aStdOut, IN_BSTR aStdErr,
                              IN_BSTR aUserName, IN_BSTR aPassword,
                              ULONG aTimeoutMS, ULONG* aPID, IProgress **aProgress);
    STDMETHOD(GetProcessOutput)(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, ULONG64 aSize, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(InternalGetStatistics)(ULONG *aCpuUser, ULONG *aCpuKernel, ULONG *aCpuIdle,
                                     ULONG *aMemTotal, ULONG *aMemFree, ULONG *aMemBalloon, ULONG *aMemCache,
                                     ULONG *aPageTotal, ULONG *aMemAllocTotal, ULONG *aMemFreeTotal, ULONG *aMemBalloonTotal);

    // public methods that are not in IDL
    void setAdditionsVersion (Bstr aVersion, VBOXOSTYPE aOsType);

    void setSupportsSeamless (BOOL aSupportsSeamless);

    void setSupportsGraphics (BOOL aSupportsGraphics);

    HRESULT SetStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Guest"; }

# ifdef VBOX_WITH_GUEST_CONTROL
    /** Static callback for handling guest notifications. */
    static DECLCALLBACK(int) doGuestCtrlNotification(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
# endif

private:

# ifdef VBOX_WITH_GUEST_CONTROL
    struct CallbackContext
    {
        uint32_t            mContextID;
        void               *pvData;
        uint32_t            cbData;
        /** Atomic flag whether callback was called. */
        volatile bool       bCalled;
        /** Pointer to user-supplied IProgress. */
        ComObjPtr<Progress> pProgress;
    };
    typedef std::list< CallbackContext > CallbackList;
    typedef std::list< CallbackContext >::iterator CallbackListIter;
    typedef std::list< CallbackContext >::const_iterator CallbackListIterConst;

    int prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnv);
    /** Handler for guest execution control notifications. */
    int notifyCtrlExec(uint32_t u32Function, PHOSTEXECCALLBACKDATA pData);
    int notifyCtrlExecOut(uint32_t u32Function, PHOSTEXECOUTCALLBACKDATA pData);
    CallbackListIter getCtrlCallbackContextByID(uint32_t u32ContextID);
    CallbackListIter getCtrlCallbackContextByPID(uint32_t u32PID);
    void removeCtrlCallbackContext(CallbackListIter it);
    uint32_t addCtrlCallbackContext(void *pvData, uint32_t cbData, Progress* pProgress);
# endif

    struct Data
    {
        Data() : mAdditionsActive (FALSE), mSupportsSeamless (FALSE),
                 mSupportsGraphics (FALSE) {}

        Bstr  mOSTypeId;
        BOOL  mAdditionsActive;
        Bstr  mAdditionsVersion;
        BOOL  mSupportsSeamless;
        BOOL  mSupportsGraphics;
    };

    ULONG mMemoryBalloonSize;
    ULONG mStatUpdateInterval;
    ULONG mCurrentGuestStat[GUESTSTATTYPE_MAX];

    Console *mParent;
    Data mData;

# ifdef VBOX_WITH_GUEST_CONTROL
    /** General extension callback for guest control. */
    HGCMSVCEXTHANDLE  mhExtCtrl;

    volatile uint32_t mNextContextID;
    CallbackList mCallbackList;
# endif
};

#endif // ____H_GUESTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
