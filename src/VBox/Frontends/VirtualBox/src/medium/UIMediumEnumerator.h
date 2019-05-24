/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumEnumerator class declaration.
 */

/*
 * Copyright (C) 2013-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_UIMediumEnumerator_h
#define FEQT_INCLUDED_SRC_medium_UIMediumEnumerator_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

//#define VBOX_GUI_WITH_NEW_MEDIA_EVENTS

/* Qt includes: */
#include <QObject>
#include <QSet>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"
#include "UIMedium.h"

/* COM includes: */
#ifdef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
# include "CMedium.h"
# include "CMediumAttachment.h"
#endif

/* Forward declarations: */
class UITask;
class UIThreadPool;

/** A map of CMedium objects ordered by their IDs. */
typedef QMap<QUuid, CMedium> CMediumMap;

/** QObject extension operating as medium-enumeration object.
  * Manages access to cached UIMedium information via public API.
  * Updates cache on corresponding Main events using thread-pool interface. */
class SHARED_LIBRARY_STUFF UIMediumEnumerator : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about UIMedium with @a uMediumID created. */
    void sigMediumCreated(const QUuid &uMediumID);
    /** Notifies listeners about UIMedium with @a uMediumID deleted. */
    void sigMediumDeleted(const QUuid &uMediumID);

    /** Notifies listeners about consolidated medium-enumeration process has started. */
    void sigMediumEnumerationStarted();
    /** Notifies listeners about UIMedium with @a uMediumID updated. */
    void sigMediumEnumerated(const QUuid &uMediumID);
    /** Notifies listeners about consolidated medium-enumeration process has finished. */
    void sigMediumEnumerationFinished();

public:

    /** Constructs medium-enumerator object. */
    UIMediumEnumerator();

    /** Returns cached UIMedium ID list. */
    QList<QUuid> mediumIDs() const;
    /** Returns a wrapper of cached UIMedium with specified @a uMediumID. */
    UIMedium medium(const QUuid &uMediumID) const;

    /** Creates UIMedium thus caching it internally on the basis of passed @a guiMedium information. */
    void createMedium(const UIMedium &guiMedium);
    /** Deletes UIMedium with specified @a uMediumID thus removing it from internal cache. */
    void deleteMedium(const QUuid &uMediumID);

    /** Returns whether consolidated medium-enumeration process is in progress. */
    bool isMediumEnumerationInProgress() const { return m_fMediumEnumerationInProgress; }
    /** Makes a request to enumerate specified @a comMedia.
      * @note  Previous map will be replaced with the new one, values present in both
      *        maps will be merged from the previous to new one, keep that all in mind.
      * @note  Empty passed map means that full/overall medium-enumeration is requested. */
    void startMediumEnumeration(const CMediumVector &comMedia = CMediumVector());
    /** Refresh all the lightweight UIMedium information for all the cached UIMedium(s).
      * @note  Please note that this is a lightweight version, which doesn't perform
      *        heavy state/accessibility checks thus doesn't require to be performed
      *        by a worker COM-aware thread. */
    void refreshMedia();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    /** Handles machine-data-change and snapshot-change events for a machine with specified @a uMachineID. */
    void sltHandleMachineUpdate(const QUuid &uMachineID);
    /** Handles machine-[un]registration events for a machine with specified @a uMachineID.
      * @param  fRegistered  Specifies whether the machine was registered or unregistered otherwise. */
    void sltHandleMachineRegistration(const QUuid &uMachineID, const bool fRegistered);
    /** Handles snapshot-deleted events for a machine with specified @a uMachineID and a snapshot with specified @a uSnapshotID. */
    void sltHandleSnapshotDeleted(const QUuid &uMachineID, const QUuid &uSnapshotID);
#else /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */
    /** Handles signal about storage controller change.
      * @param  uMachineId         Brings the ID of machine corresponding controller belongs to.
      * @param  strControllerName  Brings the name of controller this event is related to. */
    void sltHandleStorageControllerChange(const QUuid &uMachineId, const QString &strControllerName);
    /** Handles signal about storage device change.
      * @param  comAttachment  Brings corresponding attachment.
      * @param  fRemoved       Brings whether medium is removed or added.
      * @param  fSilent        Brings whether this change has gone silent for guest. */
    void sltHandleStorageDeviceChange(CMediumAttachment comAttachment, bool fRemoved, bool fSilent);
    /** Handles signal about storage medium @a comAttachment state change. */
    void sltHandleMediumChange(CMediumAttachment comAttachment);
    /** Handles signal about storage @a comMedium config change. */
    void sltHandleMediumConfigChange(CMedium comMedium);
    /** Handles signal about storage medium is (un)registered.
      * @param  uMediumId      Brings corresponding medium ID.
      * @param  enmMediumType  Brings corresponding medium type.
      * @param  fRegistered    Brings whether medium is registered or unregistered. */
    void sltHandleMediumRegistered(const QUuid &uMediumId, KDeviceType enmMediumType, bool fRegistered);
#endif /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

    /** Handles medium-enumeration @a pTask complete signal. */
    void sltHandleMediumEnumerationTaskComplete(UITask *pTask);

private:

    /** Creates medium-enumeration task for certain @a guiMedium. */
    void createMediumEnumerationTask(const UIMedium &guiMedium);
    /** Adds NULL UIMedium to specified @a outputMedia map. */
    void addNullMediumToMap(UIMediumMap &outputMedia);
    /** Adds @a inputMedia to specified @a outputMedia map. */
    void addMediaToMap(const CMediumVector &inputMedia, UIMediumMap &outputMedia);

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    /** Updates usage for machine with specified @a uMachineID on the basis of cached data.
      * @param  previousUIMediumIDs               Brings UIMedium IDs used in cached data.
      * @param  fTakeIntoAccountCurrentStateOnly  Brings whether we should take into accound current VM state only. */
    void calculateCachedUsage(const QUuid &uMachineID,
                              QList<QUuid> &previousUIMediumIDs,
                              const bool fTakeIntoAccountCurrentStateOnly) const;
    /** Updates usage for machine with specified @a uMachineID on the basis of actual data.
      * @param  currentCMediums                   Brings CMedium used in actual data.
      * @param  currentCMediumIDs                 Brings CMedium IDs used in actual data.
      * @param  fTakeIntoAccountCurrentStateOnly  Brings whether we should take into accound current VM state only. */
    void calculateActualUsage(const QUuid &uMachineID,
                              CMediumMap &currentCMediums,
                              QList<QUuid> &currentCMediumIDs,
                              const bool fTakeIntoAccountCurrentStateOnly) const;
    /** Updates usage for machine specified by its @a comSnapshot reference on the basis of actual data.
      * @param  currentCMediums                   Brings CMedium used in actual data.
      * @param  currentCMediumIDs                 Brings CMedium IDs used in actual data. */
    void calculateActualUsage(const CSnapshot &comSnapshot,
                              CMediumMap &currentCMediums,
                              QList<QUuid> &currentCMediumIDs) const;
    /** Updates usage for machine specified by own @a comMachine reference on the basis of actual data.
      * @param  currentCMediums                   Brings CMedium used in actual data.
      * @param  currentCMediumIDs                 Brings CMedium IDs used in actual data. */
    void calculateActualUsage(const CMachine &comMachine,
                              CMediumMap &currentCMediums,
                              QList<QUuid> &currentCMediumIDs) const;

    /** Updates cache using known changes in cached data.
      * @param  previousUIMediumIDs               Brings UIMedium IDs used in cached data. */
    void recacheFromCachedUsage(const QList<QUuid> &previousUIMediumIDs);
    /** Updates cache using known changes in actual data.
      * @param  currentCMediums                   Brings CMedium used in actual data.
      * @param  currentCMediumIDs                 Brings CMedium IDs used in actual data. */
    void recacheFromActualUsage(const CMediumMap &currentCMediums,
                                const QList<QUuid> &currentCMediumIDs);
#else /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */
    /** Parses incoming @a comAttachment. */
    void parseAttachment(CMediumAttachment &comAttachment);
    /** Parses incoming @a comMedium. */
    void parseMedium(CMedium &comMedium);
#endif /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

    /** Holds whether consolidated medium-enumeration process is in progress. */
    bool  m_fMediumEnumerationInProgress;

    /** Holds a set of current medium-enumeration tasks. */
    QSet<UITask*>  m_tasks;

    /** Holds a map of current cached (enumerated) media. */
    UIMediumMap  m_media;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIMediumEnumerator_h */
