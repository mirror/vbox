/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class declaration.
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

#ifndef ___UIVirtualBoxEventHandler_h___
#define ___UIVirtualBoxEventHandler_h___

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"

/** Active event handler singleton for the CVirtualBox event-source. */
class UIVirtualBoxEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about the VBoxSVC become @a fAvailable. */
    void sigVBoxSVCAvailabilityChange(bool fAvailable);
    /** Notifies listeners about @a state change event for the machine with @a strId. */
    void sigMachineStateChange(QString strId, KMachineState state);
    /** Notifies listeners about data change event for the machine with @a strId. */
    void sigMachineDataChange(QString strId);
    /** Notifies listeners about machine with @a strId was @a fRegistered. */
    void sigMachineRegistered(QString strId, bool fRegistered);
    /** Notifies listeners about @a state change event for the session of the machine with @a strId. */
    void sigSessionStateChange(QString strId, KSessionState state);
    /** Notifies listeners about snapshot with @a strSnapshotId was taken for the machine with @a strId. */
    void sigSnapshotTake(QString strId, QString strSnapshotId);
    /** Notifies listeners about snapshot with @a strSnapshotId was deleted for the machine with @a strId. */
    void sigSnapshotDelete(QString strId, QString strSnapshotId);
    /** Notifies listeners about snapshot with @a strSnapshotId was changed for the machine with @a strId. */
    void sigSnapshotChange(QString strId, QString strSnapshotId);

public:

    /** Returns singleton instance created by the factory. */
    static UIVirtualBoxEventHandler* instance();
    /** Destroys singleton instance created by the factory. */
    static void destroy();

private:

    /** Constructor. */
    UIVirtualBoxEventHandler();
    /** Destructor. */
    ~UIVirtualBoxEventHandler();

    /** Holds the static singleton instance. */
    static UIVirtualBoxEventHandler *m_pInstance;
    /** Holds the COM event-listener instance. */
    CEventListener m_mainEventListener;
};

#define gVBoxEvents UIVirtualBoxEventHandler::instance()

#endif /* !___UIVirtualBoxEventHandler_h___ */
