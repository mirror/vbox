/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVirtualBoxEventHandler class declaration
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

#ifndef __UIVirtualBoxEventHandler_h__
#define __UIVirtualBoxEventHandler_h__

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"

class UIVirtualBoxEventHandler: public QObject
{
    Q_OBJECT;

public:
    static UIVirtualBoxEventHandler* instance();
    static void destroy();

signals:
    /* VirtualBox main signals */
    void sigMachineStateChange(QString strId, KMachineState state);
    void sigMachineDataChange(QString strId);
    void sigMachineRegistered(QString strId, bool fRegistered);
    void sigSessionStateChange(QString strId, KSessionState state);
    void sigSnapshotTake(QString strId, QString strSnapshotId);
    void sigSnapshotDelete(QString strId, QString strSnapshotId);
    void sigSnapshotChange(QString strId, QString strSnapshotId);

private:
    UIVirtualBoxEventHandler();
    ~UIVirtualBoxEventHandler();

    /* Private member vars */
    static UIVirtualBoxEventHandler *m_pInstance;
    CEventListener m_mainEventListener;
};

#define gVBoxEvents UIVirtualBoxEventHandler::instance()

#endif /* !__UIVirtualBoxEventHandler_h__ */

