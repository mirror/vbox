/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIVirtualBoxEventHandler_h
#define FEQT_INCLUDED_SRC_globals_UIVirtualBoxEventHandler_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"
#include "CMediumAttachment.h"

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

    /** Notifies about storage controller change.
      * @param  uMachineId         Brings the ID of machine corresponding controller belongs to.
      * @param  strControllerName  Brings the name of controller this event is related to. */
    void sigStorageControllerChange(const QUuid &uMachineId, const QString &strControllerName);
    /** Notifies about storage device change.
      * @param  comAttachment  Brings corresponding attachment.
      * @param  fRemoved       Brings whether medium is removed or added.
      * @param  fSilent        Brings whether this change has gone silent for guest. */
    void sigStorageDeviceChange(CMediumAttachment comAttachment, bool fRemoved, bool fSilent);
    /** Notifies about storage medium @a comAttachment state change. */
    void sigMediumChange(CMediumAttachment comAttachment);
    /** Notifies about storage @a comMedium config change. */
    void sigMediumConfigChange(CMedium comMedium);
    /** Notifies about storage medium is (un)registered.
      * @param  uMediumId      Brings corresponding medium ID.
      * @param  enmMediumType  Brings corresponding medium type.
      * @param  fRegistered    Brings whether medium is registered or unregistered. */
    void sigMediumRegistered(const QUuid &uMediumId, KDeviceType enmMediumType, bool fRegistered);

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

#endif /* !FEQT_INCLUDED_SRC_globals_UIVirtualBoxEventHandler_h */

