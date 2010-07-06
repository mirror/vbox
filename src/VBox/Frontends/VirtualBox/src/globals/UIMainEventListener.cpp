/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMainEventListener class implementation
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
 */

/* Local includes */
#include "UIMainEventListener.h"

/* Global includes */
//#include <iprt/thread.h>
//#include <iprt/stream.h>

#ifndef Q_WS_WIN
NS_DECL_CLASSINFO(UIMainEventListener)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(UIMainEventListener, IEventListener)
#endif

UIMainEventListener::UIMainEventListener(QObject * /* pParent */)
//  : QObject(pParent) /* Todo: Not sure if pParent should delete this. Especially on Win there is ref counting implemented. */
  : QObject()
#ifdef Q_WS_WIN
  , m_refcnt
#endif /* Q_WS_WIN */
{
    /* For queued events we have to extra register our enums/interface classes
     * (Q_DECLARE_METATYPE isn't sufficient).
     * Todo: Try to move this to a global function, which is auto generated
     * from xslt. */
    qRegisterMetaType<KMachineState>("KMachineState");
    qRegisterMetaType<KSessionState>("KSessionState");
    qRegisterMetaType< QVector<uint8_t> >("QVector<uint8_t>");
    qRegisterMetaType<CNetworkAdapter>("CNetworkAdapter");
    qRegisterMetaType<CMediumAttachment>("CMediumAttachment");
    qRegisterMetaType<CUSBDevice>("CUSBDevice");
    qRegisterMetaType<CVirtualBoxErrorInfo>("CVirtualBoxErrorInfo");
}

STDMETHODIMP UIMainEventListener::HandleEvent(IEvent *pEvent)
{
    CEvent event(pEvent);
//    RTPrintf("Event received: %d (%RTthrd)\n", event.GetType(), RTThreadSelf());
    switch(event.GetType())
    {
        /*
         * All VirtualBox Events
         */
        case KVBoxEventType_OnMachineStateChange:
        {
            CMachineStateChangeEvent es(event);
            emit sigMachineStateChange(es.GetMachineId(), es.GetState());
            break;
        }
        case KVBoxEventType_OnMachineDataChange:
        {
            CMachineDataChangeEvent es(event);
            emit sigMachineDataChange(es.GetMachineId());
            break;
        }
        case KVBoxEventType_OnExtraDataCanChange:
        {
            CExtraDataCanChangeEvent es(event);
            /* Has to be done in place to give an answer */
            bool fVeto = false;
            QString strReason;
            emit sigExtraDataCanChange(es.GetMachineId(), es.GetKey(), es.GetValue(), fVeto, strReason);
            if (fVeto)
                es.AddVeto(strReason);
            break;
        }
        case KVBoxEventType_OnExtraDataChange:
        {
            CExtraDataChangeEvent es(event);
            emit sigExtraDataChange(es.GetMachineId(), es.GetKey(), es.GetValue());
            break;
        }
        /* Not used:
        case KVBoxEventType_OnMediumRegistered:
         */
        case KVBoxEventType_OnMachineRegistered:
        {
            CMachineRegisteredEvent es(event);
            emit sigMachineRegistered(es.GetMachineId(), es.GetRegistered());
            break;
        }
        case KVBoxEventType_OnSessionStateChange:
        {
            CSessionStateEvent es(event);
            emit sigSessionStateChange(es.GetMachineId(), es.GetState());
            break;
        }
        /* Not used:
        case KVBoxEventType_OnSnapshotTaken:
        case KVBoxEventType_OnSnapshotDeleted:
         */
        case KVBoxEventType_OnSnapshotChange:
        {
            CSnapshotChangeEvent es(event);
            emit sigSnapshotChange(es.GetMachineId(), es.GetSnapshotId());
            break;
        }
        /* Not used:
        case KVBoxEventType_OnGuestPropertyChange:
         */
        /*
         * All Console Events
         */
        case KVBoxEventType_OnMousePointerShapeChange:
        {
            CMousePointerShapeChangeEvent es(event);
            emit sigMousePointerShapeChange(es.GetVisible(), es.GetAlpha(), QPoint(es.GetXhot(), es.GetYhot()), QSize(es.GetWidth(), es.GetHeight()), es.GetShape());
            break;
        }
        case KVBoxEventType_OnMouseCapabilityChange:
        {
            CMouseCapabilityChangeEvent es(event);
            emit sigMouseCapabilityChange(es.GetSupportsAbsolute(), es.GetSupportsRelative(), es.GetNeedsHostCursor());
            break;
        }
        case KVBoxEventType_OnKeyboardLedsChange:
        {
            CKeyboardLedsChangeEvent es(event);
            emit sigKeyboardLedsChangeEvent(es.GetNumLock(), es.GetCapsLock(), es.GetScrollLock());
            break;
        }
        case KVBoxEventType_OnStateChange:
        {
            CStateChangeEvent es(event);
            emit sigStateChange(es.GetState());
            break;
        }
        case KVBoxEventType_OnAdditionsStateChange:
        {
            emit sigAdditionsChange();
            break;
        }
        case KVBoxEventType_OnNetworkAdapterChange:
        {
            CNetworkAdapterChangeEvent es(event);
            emit sigNetworkAdapterChange(es.GetNetworkAdapter());
            break;
        }
        /* Not used *
        case KVBoxEventType_OnSerialPortChange:
        case KVBoxEventType_OnParallelPortChange:
        case KVBoxEventType_OnStorageControllerChange:
         */
        case KVBoxEventType_OnMediumChange:
        {
            CMediumChangeEvent es(event);
            emit sigMediumChange(es.GetMediumAttachment());
            break;
        }
        /* Not used *
        case KVBoxEventType_OnCPUChange:
        case KVBoxEventType_OnVRDPServerChange:
        case KVBoxEventType_OnRemoteDisplayInfoChange:
         */
        case KVBoxEventType_OnUSBControllerChange:
        {
            emit sigUSBControllerChange();
            break;
        }
        case KVBoxEventType_OnUSBDeviceStateChange:
        {
            CUSBDeviceStateChangeEvent es(event);
            emit sigUSBDeviceStateChange(es.GetDevice(), es.GetAttached(), es.GetError());
            break;
        }
        case KVBoxEventType_OnSharedFolderChange:
        {
            CSharedFolderChangeEvent es(event);
            emit sigSharedFolderChange();
            break;
        }
        case KVBoxEventType_OnRuntimeError:
        {
            CRuntimeErrorEvent es(event);
            emit sigRuntimeError(es.GetFatal(), es.GetId(), es.GetMessage());
            break;
        }
        case KVBoxEventType_OnCanShowWindow:
        {
            CCanShowWindowEvent es(event);
            /* Has to be done in place to give an answer */
            bool fVeto = false;
            QString strReason;
            emit sigCanShowWindow(fVeto, strReason);
            if (fVeto)
                es.AddVeto(strReason);
            break;
        }
        case KVBoxEventType_OnShowWindow:
        {
            CShowWindowEvent es(event);
            /* Has to be done in place to give an answer */
            ULONG64 winId;
            emit sigShowWindow(winId);
            es.SetWinId(winId);
            break;
        }
        default: break;
    }
    return S_OK;
}

