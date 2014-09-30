/** @file
 * VBox Qt GUI - UIConsoleEventHandler class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIConsoleEventHandler_h___
#define ___UIConsoleEventHandler_h___

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"
#include "CVirtualBoxErrorInfo.h"
#include "CMediumAttachment.h"
#include "CNetworkAdapter.h"
#include "CUSBDevice.h"

/* Forward declarations: */
class UISession;

/** Console event handler. */
class UIConsoleEventHandler: public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about mouse pointer shape change. */
    void sigMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape);
    /** Notifies about mouse capability change. */
    void sigMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fSupportsMultiTouch, bool fNeedsHostCursor);
    /** Notifies about keyboard LEDs change. */
    void sigKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock);
    /** Notifies about machine state change. */
    void sigStateChange(KMachineState state);
    /** Notifies about guest additions state change. */
    void sigAdditionsChange();
    /** Notifies about network adapter state change. */
    void sigNetworkAdapterChange(CNetworkAdapter adapter);
    /** Notifies about storage medium state change. */
    void sigMediumChange(CMediumAttachment attachment);
    /** Notifies about VRDE device state change. */
    void sigVRDEChange();
    /** Notifies about Video Capture device state change. */
    void sigVideoCaptureChange();
    /** Notifies about USB controller state change. */
    void sigUSBControllerChange();
    /** Notifies about USB device state change. */
    void sigUSBDeviceStateChange(CUSBDevice device, bool fAttached, CVirtualBoxErrorInfo error);
    /** Notifies about shared folder state change. */
    void sigSharedFolderChange();
    /** Notifies about CPU execution-cap change. */
    void sigCPUExecutionCapChange();
    /** Notifies about guest-screen configuration change. */
    void sigGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);
    /** Notifies about Runtime error. */
    void sigRuntimeError(bool fFatal, QString strId, QString strMessage);
#ifdef RT_OS_DARWIN
    /** Notifies about VM window should be shown. */
    void sigShowWindow();
#endif /* RT_OS_DARWIN */

public:

    /** Static instance factory. */
    static UIConsoleEventHandler* instance(UISession *pSession = 0);
    /** Static instance destructor. */
    static void destroy();

private slots:

    /** Returns whether VM window can be shown. */
    void sltCanShowWindow(bool &fVeto, QString &strReason);
    /** Shows VM window if possible. */
    void sltShowWindow(LONG64 &winId);

private:

    /** Constructor: */
    UIConsoleEventHandler(UISession *pSession);

    /** Prepare routine. */
    void prepare();
    /** Cleanup routine. */
    void cleanup();

    /** Holds the static instance. */
    static UIConsoleEventHandler *m_pInstance;

    /** Holds the UI session reference. */
    UISession *m_pSession;

    /** Holds the main event listener instance. */
    CEventListener m_mainEventListener;
};

#define gConsoleEvents UIConsoleEventHandler::instance()

#endif /* !___UIConsoleEventHandler_h___ */
