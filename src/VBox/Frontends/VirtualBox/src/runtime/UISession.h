/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISession class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef ___UIConsole_h___
#define ___UIConsole_h___

/* Global includes */
#include <QObject>

/* Local includes */
#include "COMDefs.h"

/* Local forwards */
class UIMachine;
class UIConsoleCallback;

/* CConsole callback event types: */
enum UIConsoleEventType
{
    UIConsoleEventType_MousePointerShapeChange = QEvent::User + 1,
    UIConsoleEventType_MouseCapabilityChange,
    UIConsoleEventType_KeyboardLedsChange,
    UIConsoleEventType_StateChange,
    UIConsoleEventType_AdditionsStateChange,
    UIConsoleEventType_NetworkAdapterChange,
    UIConsoleEventType_SerialPortChange,
    UIConsoleEventType_ParallelPortChange,
    UIConsoleEventType_StorageControllerChange,
    UIConsoleEventType_MediumChange,
    UIConsoleEventType_CPUChange,
    UIConsoleEventType_VRDPServerChange,
    UIConsoleEventType_RemoteDisplayInfoChange,
    UIConsoleEventType_USBControllerChange,
    UIConsoleEventType_USBDeviceStateChange,
    UIConsoleEventType_SharedFolderChange,
    UIConsoleEventType_RuntimeError,
    UIConsoleEventType_CanShowWindow,
    UIConsoleEventType_ShowWindow,
    UIConsoleEventType_MAX
};

class UISession : public QObject
{
    Q_OBJECT;

public:

    /* Machine session constructor/destructor: */
    UISession(UIMachine *pMachine, const CSession &session);
    virtual ~UISession();

    /* Public getters: */
    CSession& session() { return m_session; }

signals:

    /* Console signals: */
    void sigMousePointerShapeChange(bool bIsVisible, bool bHasAlpha, bool uXHot, bool uYHot, bool uWidth, bool uHeight, const uchar *pShapeData);
    void sigMouseCapabilityChange(bool bIsSupportsAbsolute, bool bNeedsHostCursor);
    void sigKeyboardLedsChange(bool bNumLock, bool bCapsLock, bool bScrollLock);
    void sigStateChange(KMachineState machineState);
    void sigAdditionsStateChange();
    void sigNetworkAdapterChange(const CNetworkAdapter &networkAdapter);
    void sigSerialPortChange(const CSerialPort &serialPort);
    void sigParallelPortChange(const CParallelPort &parallelPort);
    void sigStorageControllerChange();
    void sigMediumChange(const CMediumAttachment &mediumAttachment);
    void sigCPUChange(ulong uCPU, bool bRemove);
    void sigVRDPServerChange();
    void sigRemoteDisplayInfoChange();
    void sigUSBControllerChange();
    void sigUSBDeviceStateChange(const CUSBDevice &device, bool bIsAttached, const CVirtualBoxErrorInfo &error);
    void sigSharedFolderChange();
    void sigRuntimeError(bool bIsFatal, QString strErrorId, QString strMessage);

private:

    /* Private getters: */
    UIMachine* machine() const { return m_pMachine; }

    /* Event handlers: */
    bool event(QEvent *pEvent);

    /* Helper routines: */
    qulonglong winId() const;

    /* Private variables: */
    UIMachine *m_pMachine;
    CSession m_session;
    UIConsoleCallback *m_pCallback;
    const CConsoleCallback &m_callback;

    /* Friend classes: */
    friend class UIConsoleCallback;
};

#endif // !___UIConsole_h___

