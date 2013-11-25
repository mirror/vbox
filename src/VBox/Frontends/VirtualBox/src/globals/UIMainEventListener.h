/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMainEventListener class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMainEventListener_h__
#define __UIMainEventListener_h__

/* COM includes: */
#include "COMEnums.h"
#include "CVirtualBoxErrorInfo.h"
#include "CMediumAttachment.h"
#include "CNetworkAdapter.h"
#include "CUSBDevice.h"

/* Other VBox includes: */
#include <VBox/com/listeners.h>

/* Note: On a first look this may seems a little bit complicated.
 * There are two reasons to use a separate class here which handles the events
 * and forward them to the public class as signals. The first one is that on
 * some platforms (e.g. Win32) this events not arrive in the main GUI thread.
 * So there we have to make sure they are first delivered to the main GUI
 * thread and later executed there. The second reason is, that the initiator
 * method may hold a lock on a object which has to be manipulated in the event
 * consumer. Doing this without being asynchronous would lead to a dead lock. To
 * avoid both problems we send signals as a queued connection to the event
 * consumer. Qt will create a event for us, place it in the main GUI event
 * queue and deliver it later on. */

class UIMainEventListener: public QObject
{
    Q_OBJECT;

public:
    UIMainEventListener();


    HRESULT init(QObject *pParent);
    void    uninit();

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *pEvent);

signals:
    /* All VirtualBox Signals */
    void sigMachineStateChange(QString strId, KMachineState state);
    void sigMachineDataChange(QString strId);
    void sigExtraDataCanChange(QString strId, QString strKey, QString strValue, bool &fVeto, QString &strVetoReason); /* use Qt::DirectConnection */
    void sigExtraDataChange(QString strId, QString strKey, QString strValue);
    void sigMachineRegistered(QString strId, bool fRegistered);
    void sigSessionStateChange(QString strId, KSessionState state);
    void sigSnapshotTake(QString strId, QString strSnapshotId);
    void sigSnapshotDelete(QString strId, QString strSnapshotId);
    void sigSnapshotChange(QString strId, QString strSnapshotId);
    /* All Console Signals */
    void sigMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape);
    void sigMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fSupportsMultiTouch, bool fNeedsHostCursor);
    void sigKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock);
    void sigStateChange(KMachineState state);
    void sigAdditionsChange();
    void sigNetworkAdapterChange(CNetworkAdapter adapter);
    void sigMediumChange(CMediumAttachment attachment);
    void sigVRDEChange();
    void sigVideoCaptureChange();
    void sigUSBControllerChange();
    void sigUSBDeviceStateChange(CUSBDevice device, bool fAttached, CVirtualBoxErrorInfo error);
    void sigSharedFolderChange();
    void sigRuntimeError(bool fFatal, QString strId, QString strMessage);
    void sigCanShowWindow(bool &fVeto, QString &strReason); /* use Qt::DirectConnection */
    void sigShowWindow(LONG64 &winId); /* use Qt::DirectConnection */
    void sigCPUExecutionCapChange();
    void sigGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);
};

/* Wrap the IListener interface around our implementation class. */
typedef ListenerImpl<UIMainEventListener, QObject*> UIMainEventListenerImpl;

#endif /* !__UIMainEventListener_h__ */

