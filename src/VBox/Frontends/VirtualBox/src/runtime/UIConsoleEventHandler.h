/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIConsoleEventHandler class declaration
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIConsoleEventHandler_h__
#define __UIConsoleEventHandler_h__

/* COM includes: */
#include "COMEnums.h"
#include "CVirtualBoxErrorInfo.h"
#include "CEventListener.h"
#include "CMediumAttachment.h"
#include "CNetworkAdapter.h"
#include "CUSBDevice.h"

/* Forward declarations: */
class UISession;

class UIConsoleEventHandler: public QObject
{
    Q_OBJECT;

public:
    static UIConsoleEventHandler* instance(UISession *pSession = 0);
    static void destroy();

signals:
    void sigMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape);
    void sigMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fNeedsHostCursor);
    void sigKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock);
    void sigStateChange(KMachineState state);
    void sigAdditionsChange();
    void sigNetworkAdapterChange(CNetworkAdapter adapter);
    void sigMediumChange(CMediumAttachment attachment);
    void sigVRDEChange();
    void sigUSBControllerChange();
    void sigUSBDeviceStateChange(CUSBDevice device, bool fAttached, CVirtualBoxErrorInfo error);
    void sigSharedFolderChange();
    void sigRuntimeError(bool fFatal, QString strId, QString strMessage);
#ifdef RT_OS_DARWIN
    void sigShowWindow();
#endif /* RT_OS_DARWIN */
    void sigCPUExecutionCapChange();
    void sigGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);

private slots:
    void sltCanShowWindow(bool &fVeto, QString &strReason);
    void sltShowWindow(LONG64 &winId);

private:
    UIConsoleEventHandler(UISession *pSession);
    ~UIConsoleEventHandler();

    static UIConsoleEventHandler *m_pInstance;
    UISession *m_pSession;
    CEventListener m_mainEventListener;
};

#define gConsoleEvents UIConsoleEventHandler::instance()

#endif /* !__UIConsoleEventHandler_h__ */

