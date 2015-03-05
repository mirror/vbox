/* $Id$ */
/** @file
 * VBox Qt GUI - UIMainEventListener class implementation.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIMainEventListener.h"

/* COM includes: */
# include "COMEnums.h"
# include "CEvent.h"
# include "CVBoxSVCAvailabilityChangedEvent.h"
# include "CVirtualBoxErrorInfo.h"
# include "CMachineStateChangedEvent.h"
# include "CMachineDataChangedEvent.h"
# include "CMachineRegisteredEvent.h"
# include "CSessionStateChangedEvent.h"
# include "CSnapshotTakenEvent.h"
# include "CSnapshotDeletedEvent.h"
# include "CSnapshotChangedEvent.h"
# include "CExtraDataCanChangeEvent.h"
# include "CExtraDataChangedEvent.h"
# include "CMousePointerShapeChangedEvent.h"
# include "CMouseCapabilityChangedEvent.h"
# include "CKeyboardLedsChangedEvent.h"
# include "CStateChangedEvent.h"
# include "CNetworkAdapterChangedEvent.h"
# include "CMediumChangedEvent.h"
# include "CUSBDevice.h"
# include "CUSBDeviceStateChangedEvent.h"
# include "CGuestMonitorChangedEvent.h"
# include "CRuntimeErrorEvent.h"
# include "CCanShowWindowEvent.h"
# include "CShowWindowEvent.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIMainEventListener::UIMainEventListener()
{
    /* Register meta-types for required enums. */
    qRegisterMetaType<KMachineState>("KMachineState");
    qRegisterMetaType<KSessionState>("KSessionState");
    qRegisterMetaType< QVector<uint8_t> >("QVector<uint8_t>");
    qRegisterMetaType<CNetworkAdapter>("CNetworkAdapter");
    qRegisterMetaType<CMediumAttachment>("CMediumAttachment");
    qRegisterMetaType<CUSBDevice>("CUSBDevice");
    qRegisterMetaType<CVirtualBoxErrorInfo>("CVirtualBoxErrorInfo");
    qRegisterMetaType<KGuestMonitorChangedEventType>("KGuestMonitorChangedEventType");
}

STDMETHODIMP UIMainEventListener::HandleEvent(VBoxEventType_T /* type */, IEvent *pEvent)
{
    CEvent event(pEvent);
    // printf("Event received: %d\n", event.GetType());
    switch (event.GetType())
    {
        case KVBoxEventType_OnVBoxSVCAvailabilityChanged:
        {
            CVBoxSVCAvailabilityChangedEvent es(pEvent);
            emit sigVBoxSVCAvailabilityChange(es.GetAvailable());
            break;
        }

        case KVBoxEventType_OnMachineStateChanged:
        {
            CMachineStateChangedEvent es(pEvent);
            emit sigMachineStateChange(es.GetMachineId(), es.GetState());
            break;
        }
        case KVBoxEventType_OnMachineDataChanged:
        {
            CMachineDataChangedEvent es(pEvent);
            emit sigMachineDataChange(es.GetMachineId());
            break;
        }
        case KVBoxEventType_OnMachineRegistered:
        {
            CMachineRegisteredEvent es(pEvent);
            emit sigMachineRegistered(es.GetMachineId(), es.GetRegistered());
            break;
        }
        case KVBoxEventType_OnSessionStateChanged:
        {
            CSessionStateChangedEvent es(pEvent);
            emit sigSessionStateChange(es.GetMachineId(), es.GetState());
            break;
        }
        case KVBoxEventType_OnSnapshotTaken:
        {
            CSnapshotTakenEvent es(pEvent);
            emit sigSnapshotTake(es.GetMachineId(), es.GetSnapshotId());
            break;
        }
        case KVBoxEventType_OnSnapshotDeleted:
        {
            CSnapshotDeletedEvent es(pEvent);
            emit sigSnapshotDelete(es.GetMachineId(), es.GetSnapshotId());
            break;
        }
        case KVBoxEventType_OnSnapshotChanged:
        {
            CSnapshotChangedEvent es(pEvent);
            emit sigSnapshotChange(es.GetMachineId(), es.GetSnapshotId());
            break;
        }
//        case KVBoxEventType_OnMediumRegistered:
//        case KVBoxEventType_OnGuestPropertyChange:

        case KVBoxEventType_OnExtraDataCanChange:
        {
            CExtraDataCanChangeEvent es(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigExtraDataCanChange(es.GetMachineId(), es.GetKey(), es.GetValue(), fVeto, strReason);
            if (fVeto)
                es.AddVeto(strReason);
            break;
        }
        case KVBoxEventType_OnExtraDataChanged:
        {
            CExtraDataChangedEvent es(pEvent);
            emit sigExtraDataChange(es.GetMachineId(), es.GetKey(), es.GetValue());
            break;
        }

        case KVBoxEventType_OnMousePointerShapeChanged:
        {
            CMousePointerShapeChangedEvent es(pEvent);
            emit sigMousePointerShapeChange(es.GetVisible(), es.GetAlpha(), QPoint(es.GetXhot(), es.GetYhot()), QSize(es.GetWidth(), es.GetHeight()), es.GetShape());
            break;
        }
        case KVBoxEventType_OnMouseCapabilityChanged:
        {
            CMouseCapabilityChangedEvent es(pEvent);
            emit sigMouseCapabilityChange(es.GetSupportsAbsolute(), es.GetSupportsRelative(), es.GetSupportsMultiTouch(), es.GetNeedsHostCursor());
            break;
        }
        case KVBoxEventType_OnKeyboardLedsChanged:
        {
            CKeyboardLedsChangedEvent es(pEvent);
            emit sigKeyboardLedsChangeEvent(es.GetNumLock(), es.GetCapsLock(), es.GetScrollLock());
            break;
        }
        case KVBoxEventType_OnStateChanged:
        {
            CStateChangedEvent es(pEvent);
            emit sigStateChange(es.GetState());
            break;
        }
        case KVBoxEventType_OnAdditionsStateChanged:
        {
            emit sigAdditionsChange();
            break;
        }
        case KVBoxEventType_OnNetworkAdapterChanged:
        {
            CNetworkAdapterChangedEvent es(pEvent);
            emit sigNetworkAdapterChange(es.GetNetworkAdapter());
            break;
        }
        case KVBoxEventType_OnMediumChanged:
        {
            CMediumChangedEvent es(pEvent);
            emit sigMediumChange(es.GetMediumAttachment());
            break;
        }
        case KVBoxEventType_OnVRDEServerChanged:
        case KVBoxEventType_OnVRDEServerInfoChanged:
        {
            emit sigVRDEChange();
            break;
        }
        case KVBoxEventType_OnVideoCaptureChanged:
        {
            emit sigVideoCaptureChange();
            break;
        }
        case KVBoxEventType_OnUSBControllerChanged:
        {
            emit sigUSBControllerChange();
            break;
        }
        case KVBoxEventType_OnUSBDeviceStateChanged:
        {
            CUSBDeviceStateChangedEvent es(pEvent);
            emit sigUSBDeviceStateChange(es.GetDevice(), es.GetAttached(), es.GetError());
            break;
        }
        case KVBoxEventType_OnSharedFolderChanged:
        {
            emit sigSharedFolderChange();
            break;
        }
        case KVBoxEventType_OnCPUExecutionCapChanged:
        {
            emit sigCPUExecutionCapChange();
            break;
        }
        case KVBoxEventType_OnGuestMonitorChanged:
        {
            CGuestMonitorChangedEvent es(pEvent);
            emit sigGuestMonitorChange(es.GetChangeType(), es.GetScreenId(),
                                       QRect(es.GetOriginX(), es.GetOriginY(), es.GetWidth(), es.GetHeight()));
            break;
        }
        case KVBoxEventType_OnRuntimeError:
        {
            CRuntimeErrorEvent es(pEvent);
            emit sigRuntimeError(es.GetFatal(), es.GetId(), es.GetMessage());
            break;
        }
        case KVBoxEventType_OnCanShowWindow:
        {
            CCanShowWindowEvent es(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigCanShowWindow(fVeto, strReason);
            if (fVeto)
                es.AddVeto(strReason);
            break;
        }
        case KVBoxEventType_OnShowWindow:
        {
            CShowWindowEvent es(pEvent);
            /* Has to be done in place to give an answer: */
            LONG64 winId;
            emit sigShowWindow(winId);
            es.SetWinId(winId);
            break;
        }
//        case KVBoxEventType_OnSerialPortChanged:
//        case KVBoxEventType_OnParallelPortChanged:
//        case KVBoxEventType_OnStorageControllerChanged:
//        case KVBoxEventType_OnCPUChange:

        default: break;
    }
    return S_OK;
}

