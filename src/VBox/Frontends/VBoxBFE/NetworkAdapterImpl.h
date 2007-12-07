/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of NetworkAdapter class
 *
 * This is adapted from frontends/VirtualBox/NetworkAdapter.cpp.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ____H_NETWORKADAPTER
#define ____H_NETWORKADAPTER


#include <VBox/types.h>
#include <COMDefs.h>
#include <COMStr.h>
#include <iprt/critsect.h>
#include <iprt/assert.h>

class Console;


#define BSTR PRUnichar*

#define INPTR const

class NetworkAdapter
{
public:
    RTCRITSECT mCritSec;
    struct Data
    {
        Data()
            : mSlot (0), mEnabled (FALSE)
            ,  mCableConnected (TRUE), mTraceEnabled (FALSE)
#ifdef RT_OS_LINUX
            , mTAPFD (NIL_RTFILE)
#endif
            , mInternalNetwork ("") // cannot be null
        {}

        ULONG mSlot;
        BOOL mEnabled;
        Bstr mMACAddress;
        BOOL mCableConnected;
        BOOL mTraceEnabled;
        Bstr mHostInterface;
#ifdef RT_OS_LINUX
        Bstr mTAPSetupApplication;
        Bstr mTAPTerminateApplication;
        RTFILE mTAPFD;
#endif
        Bstr mInternalNetwork;
    };

    NetworkAdapter();
    virtual ~NetworkAdapter();

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    int init (Console *parent, ULONG slot);
    //    int init (Console *parent, NetworkAdapter *that);
    void uninit();

    // INetworkAdapter properties
    STDMETHOD(COMGETTER(Slot)) (ULONG *slot);
    STDMETHOD(COMGETTER(Enabled)) (BOOL *enabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL enabled);
    STDMETHOD(COMGETTER(MACAddress))(BSTR *macAddress);
    STDMETHOD(COMSETTER(MACAddress))(INPTR BSTR macAddress);
    //    STDMETHOD(COMGETTER(AttachmentType))(NetworkAttachmentType_T *attachmentType);
    STDMETHOD(COMGETTER(HostInterface))(BSTR *hostInterface);
    STDMETHOD(COMSETTER(HostInterface))(INPTR BSTR hostInterface);
#ifdef RT_OS_LINUX
    STDMETHOD(COMGETTER(TAPFileDescriptor))(LONG *tapFileDescriptor);
    STDMETHOD(COMSETTER(TAPFileDescriptor))(LONG tapFileDescriptor);
    STDMETHOD(COMGETTER(TAPSetupApplication))(BSTR *tapSetupApplication);
    STDMETHOD(COMSETTER(TAPSetupApplication))(INPTR BSTR tapSetupApplication);
    STDMETHOD(COMGETTER(TAPTerminateApplication))(BSTR *tapTerminateApplication);
    STDMETHOD(COMSETTER(TAPTerminateApplication))(INPTR BSTR tapTerminateApplication);
#endif
    STDMETHOD(COMGETTER(InternalNetwork))(BSTR *internalNetwork);
    STDMETHOD(COMSETTER(InternalNetwork))(INPTR BSTR internalNetwork);
    STDMETHOD(COMGETTER(CableConnected))(BOOL *connected);
    STDMETHOD(COMSETTER(CableConnected))(BOOL connected);
    STDMETHOD(COMGETTER(TraceEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(TraceEnabled))(BOOL enabled);

    // INetworkAdapter methods
    STDMETHOD(AttachToNAT)();
    STDMETHOD(AttachToHostInterface)();
    STDMETHOD(AttachToInternalNetwork)();
    STDMETHOD(Detach)();

    static const wchar_t *getComponentName() { return L"NetworkAdapter"; }

private:

    void detach();
    void generateMACAddress();

    Console *mParent;
    Data mData;
};

class AutoLock
    {
    public:
        AutoLock (NetworkAdapter *that) : outer (that), mLevel (0) { lock(); }
        ~AutoLock() {
           AssertMsg (mLevel <= 1, ("Lock level > 1: %d\n", mLevel));
            while (mLevel --)
                RTCritSectLeave (&outer->mCritSec);
        }
        void lock() {
            ++ mLevel;
            RTCritSectEnter (&outer->mCritSec);
        }
        void unlock() {
           AssertMsg (mLevel > 0, ("Lock level is zero\n"));
            if (mLevel > 0) {
                RTCritSectLeave (&outer->mCritSec);
                -- mLevel;
            }
        }
    private:
        AutoLock (const AutoLock &that);
        AutoLock &operator = (const AutoLock &that);
        NetworkAdapter *outer;
        unsigned int mLevel;
    };

#endif // ____H_NETWORKADAPTER
