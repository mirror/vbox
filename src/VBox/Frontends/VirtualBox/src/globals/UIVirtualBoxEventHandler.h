/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVirtualBoxEventHandler_h___
#define ___UIVirtualBoxEventHandler_h___

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class UIVirtualBoxEventHandlerProxy;

/** Singleton QObject extension
  * providing GUI with the CVirtualBoxClient and CVirtualBox event-sources. */
class SHARED_LIBRARY_STUFF UIVirtualBoxEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about the VBoxSVC become @a fAvailable. */
    void sigVBoxSVCAvailabilityChange(bool fAvailable);

    /** Notifies about @a state change event for the machine with @a uId. */
    void sigMachineStateChange(const QUuid &uId, const KMachineState state);
    /** Notifies about data change event for the machine with @a uId. */
    void sigMachineDataChange(const QUuid &uId);
    /** Notifies about machine with @a uId was @a fRegistered. */
    void sigMachineRegistered(const QUuid &uId, const bool fRegistered);
    /** Notifies about @a state change event for the session of the machine with @a uId. */
    void sigSessionStateChange(const QUuid &uId, const KSessionState state);
    /** Notifies about snapshot with @a uSnapshotId was taken for the machine with @a uId. */
    void sigSnapshotTake(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was deleted for the machine with @a uId. */
    void sigSnapshotDelete(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was changed for the machine with @a uId. */
    void sigSnapshotChange(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was restored for the machine with @a uId. */
    void sigSnapshotRestore(const QUuid &uId, const QUuid &uSnapshotId);

public:

    /** Returns singleton instance. */
    static UIVirtualBoxEventHandler *instance();
    /** Destroys singleton instance. */
    static void destroy();

protected:

    /** Constructs VirtualBox event handler. */
    UIVirtualBoxEventHandler();

    /** Prepares all. */
    void prepare();
    /** Prepares connections. */
    void prepareConnections();

private:

    /** Holds the singleton instance. */
    static UIVirtualBoxEventHandler *s_pInstance;

    /** Holds the VirtualBox event proxy instance. */
    UIVirtualBoxEventHandlerProxy *m_pProxy;
};

/** Singleton VirtualBox Event Handler 'official' name. */
#define gVBoxEvents UIVirtualBoxEventHandler::instance()

#endif /* !___UIVirtualBoxEventHandler_h___ */

