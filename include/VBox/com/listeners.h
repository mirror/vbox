/* $Id$ */
/** @file
 *
 * Listeners helpers.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_com_listeners_h
#define ___VBox_com_listeners_h
#include <VBox/com/com.h>
#include <VBox/com/defs.h>
#include <iprt/asm.h>

#ifdef VBOX_WITH_XPCOM
#define NS_IMPL_QUERY_HEAD_INLINE()                                   \
NS_IMETHODIMP QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                     \
  NS_ASSERTION(aInstancePtr,                                          \
               "QueryInterface requires a non-NULL destination!");    \
  nsISupports* foundInterface;
#define NS_INTERFACE_MAP_BEGIN_INLINE()      NS_IMPL_QUERY_HEAD_INLINE()
#define NS_IMPL_QUERY_INTERFACE1_INLINE(_i1)                                 \
  NS_INTERFACE_MAP_BEGIN_INLINE()                                            \
    NS_INTERFACE_MAP_ENTRY(_i1)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                       \
  NS_INTERFACE_MAP_END
#endif

template <class T, class TParam = void* >
class ListenerImpl : VBOX_SCRIPTABLE_IMPL(IEventListener)
{
    T                 mListener;
    volatile uint32_t mRefCnt;

    virtual ~ListenerImpl()
    {}

public:
    ListenerImpl(TParam param)
    : mListener(param), mRefCnt(1)
    {
    }

    ListenerImpl()
    : mRefCnt(1)
    {
    }


    /* On Windows QI implemented by VBOX_SCRIPTABLE_DISPATCH_IMPL */
#ifndef RT_OS_WINDOWS
    NS_IMPL_QUERY_INTERFACE1_INLINE(IEventListener)
#endif

#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)()
#else
    NS_IMETHOD_(nsrefcnt) AddRef(void)
#endif
    {
        return ASMAtomicIncU32(&mRefCnt);
    }

#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, Release)()
#else
    NS_IMETHOD_(nsrefcnt) Release(void)
#endif
    {
        uint32_t cnt = ::ASMAtomicDecU32(&mRefCnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }

    VBOX_SCRIPTABLE_DISPATCH_IMPL(IEventListener)

    STDMETHOD(HandleEvent)(IEvent * aEvent)
    {
        VBoxEventType_T aType = VBoxEventType_Invalid;
        aEvent->COMGETTER(Type)(&aType);
        return mListener.HandleEvent(aType, aEvent);
    }
};

#ifdef VBOX_WITH_XPCOM
#define VBOX_LISTENER_DECLARE(klazz) NS_DECL_CLASSINFO(klazz)
#else
#define VBOX_LISTENER_DECLARE(klazz)
#endif

#endif
