/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumEnumerator class implementation.
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

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMediumEnumerator.h"
#include "UIMessageCenter.h"
#include "UIThreadPool.h"
#include "UIVirtualBoxEventHandler.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CMediumAttachment.h"
#include "CSnapshot.h"


/** Template function to convert a list of
  * abstract objects to a human readable string list.
  * @note T should have .toString() member implemented. */
template<class T> static QStringList toStringList(const QList<T> &list)
{
    QStringList l;
    foreach(const T &t, list)
        l << t.toString();
    return l;
}


/** UITask extension used for medium-enumeration purposes.
  * @note Indeed property/setProperty API isn't thread-safe stuff.
  *       This isn't dangerous for us since setter/getter API calls
  *       are splitted in time by enumerator's logic, but latest Qt
  *       versions prohibits property/setProperty API usage from
  *       other than the GUI thread anyway so we will have to rework
  *       that stuff to be thread-safe one day at least for Qt >= 5.11. */
class UITaskMediumEnumeration : public UITask
{
    Q_OBJECT;

public:

    /** Constructs @a medium-enumeration task. */
    UITaskMediumEnumeration(const UIMedium &medium)
        : UITask(UITask::Type_MediumEnumeration)
    {
        /* Store medium as property: */
        /// @todo rework to thread-safe stuff one day
        setProperty("medium", QVariant::fromValue(medium));
    }

private:

    /** Contains medium-enumeration task body. */
    virtual void run() /* override */
    {
        /* Get medium: */
        /// @todo rework to thread-safe stuff one day
        UIMedium guiMedium = property("medium").value<UIMedium>();
        /* Enumerate it: */
        guiMedium.blockAndQueryState();
        /* Put it back: */
        setProperty("medium", QVariant::fromValue(guiMedium));
    }
};


/*********************************************************************************************************************************
*   Class UIMediumEnumerator implementation.                                                                                     *
*********************************************************************************************************************************/

UIMediumEnumerator::UIMediumEnumerator()
    : m_fMediumEnumerationInProgress(false)
{
    /* Allow UIMedium to be used in inter-thread signals: */
    qRegisterMetaType<UIMedium>();

    /* Prepare Main event handlers: */
#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineDataChange,
            this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotTake,
            this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotDelete,
            this, &UIMediumEnumerator::sltHandleSnapshotDeleted);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotChange,
            this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotRestore,
            this, &UIMediumEnumerator::sltHandleSnapshotDeleted);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
            this, &UIMediumEnumerator::sltHandleMachineRegistration);
#else /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigStorageControllerChange,
            this, &UIMediumEnumerator::sltHandleStorageControllerChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigStorageDeviceChange,
            this, &UIMediumEnumerator::sltHandleStorageDeviceChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMediumChange,
            this, &UIMediumEnumerator::sltHandleMediumChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMediumConfigChange,
            this, &UIMediumEnumerator::sltHandleMediumConfigChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMediumRegistered,
            this, &UIMediumEnumerator::sltHandleMediumRegistered);
#endif /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

    /* Prepare global thread-pool listener: */
    connect(vboxGlobal().threadPool(), &UIThreadPool::sigTaskComplete,
            this, &UIMediumEnumerator::sltHandleMediumEnumerationTaskComplete);
}

QList<QUuid> UIMediumEnumerator::mediumIDs() const
{
    /* Return keys of current media map: */
    return m_media.keys();
}

UIMedium UIMediumEnumerator::medium(const QUuid &uMediumID) const
{
    /* Search through current media map
     * for the UIMedium with passed ID: */
    if (m_media.contains(uMediumID))
        return m_media.value(uMediumID);
    /* Return NULL UIMedium otherwise: */
    return UIMedium();
}

void UIMediumEnumerator::createMedium(const UIMedium &guiMedium)
{
    /* Get UIMedium ID: */
    const QUuid uMediumID = guiMedium.id();

    /* Do not create UIMedium(s) with incorrect ID: */
    AssertReturnVoid(!uMediumID.isNull());
    /* Make sure UIMedium doesn't exist already: */
    AssertReturnVoid(!m_media.contains(uMediumID));

    /* Insert UIMedium: */
    m_media[uMediumID] = guiMedium;
    LogRel(("GUI: UIMediumEnumerator: Medium with key={%s} created\n", uMediumID.toString().toUtf8().constData()));

    /* Notify listener: */
    emit sigMediumCreated(uMediumID);
}

void UIMediumEnumerator::deleteMedium(const QUuid &uMediumID)
{
    /* Do not delete UIMedium(s) with incorrect ID: */
    AssertReturnVoid(!uMediumID.isNull());
    /* Make sure UIMedium still exists: */
    AssertReturnVoid(m_media.contains(uMediumID));

    /* Remove UIMedium: */
    m_media.remove(uMediumID);
    LogRel(("GUI: UIMediumEnumerator: Medium with key={%s} deleted\n", uMediumID.toString().toUtf8().constData()));

    /* Notify listener: */
    emit sigMediumDeleted(uMediumID);
}

void UIMediumEnumerator::startMediumEnumeration(const CMediumVector &comMedia /* = CMediumVector() */)
{
    /* Make sure we are not already in progress: */
    AssertReturnVoid(!m_fMediumEnumerationInProgress);

    /* Compose new map of currently cached media & their children.
     * While composing we are using data from already cached media. */
    UIMediumMap media;
    addNullMediumToMap(media);
    /* If @p comMedia is empty we start the medium-enumeration with all
     * known hard disk media. Otherwise only passed comMedia will be
     * enumerated. But be aware we will enumerate all the optical media
     * in any case to make them listed within the first run wizard for
     * now. But I think later we can add proper enumeration call to the
     * wizard instead and enumerate only comMedia in 'else' case. */
    if (comMedia.isEmpty())
    {
        addMediaToMap(vboxGlobal().virtualBox().GetHardDisks(), media);
        addMediaToMap(vboxGlobal().host().GetDVDDrives(), media);
        addMediaToMap(vboxGlobal().virtualBox().GetDVDImages(), media);
        addMediaToMap(vboxGlobal().host().GetFloppyDrives(), media);
        addMediaToMap(vboxGlobal().virtualBox().GetFloppyImages(), media);
    }
    else
    {
        addMediaToMap(vboxGlobal().host().GetDVDDrives(), media);
        addMediaToMap(vboxGlobal().virtualBox().GetDVDImages(), media);
        addMediaToMap(comMedia, media);
    }

    /* VBoxGlobal is cleaning up, abort immediately: */
    if (VBoxGlobal::isCleaningUp())
        return;

    /* Replace existing media map: */
    m_media = media;

    /* Notify listener about enumeration started: */
    LogRel(("GUI: UIMediumEnumerator: Medium-enumeration started...\n"));
    m_fMediumEnumerationInProgress = true;
    emit sigMediumEnumerationStarted();

    /* Make sure we really have more than one UIMedium (which is NULL): */
    if (m_media.size() == 1)
    {
        /* Notify listener about enumeration finished instantly: */
        LogRel(("GUI: UIMediumEnumerator: Medium-enumeration finished!\n"));
        m_fMediumEnumerationInProgress = false;
        emit sigMediumEnumerationFinished();
    }

    /* Start enumeration for media with non-NULL ID: */
    foreach (const QUuid &uMediumID, m_media.keys())
        if (!uMediumID.isNull())
            createMediumEnumerationTask(m_media[uMediumID]);
}

void UIMediumEnumerator::enumerateAdditionalMedium(const CMedium &comMedium)
{
    /* Put the medium to vector for convenience: */
    CMediumVector inputMedia;
    inputMedia << comMedium;

    /* Compose new map of passed medium & it's children.
     * While composing we are using data from already cached media. */
    UIMediumMap media;
    addMediaToMap(inputMedia, media);

    /* VBoxGlobal is cleaning up, abort immediately: */
    if (VBoxGlobal::isCleaningUp())
        return;

    /* Throw the media to existing map: */
    foreach (const QUuid &uMediumId, media.keys())
        m_media[uMediumId] = media.value(uMediumId);

    /* Start enumeration for media with non-NULL ID: */
    foreach (const QUuid &uMediumID, media.keys())
        if (!uMediumID.isNull())
            createMediumEnumerationTask(media[uMediumID]);
}

void UIMediumEnumerator::refreshMedia()
{
    /* Make sure we are not already in progress: */
    AssertReturnVoid(!m_fMediumEnumerationInProgress);

    /* Refresh all cached media we have: */
    foreach (const QUuid &uMediumID, m_media.keys())
        m_media[uMediumID].refresh();
}

void UIMediumEnumerator::retranslateUi()
{
    /* Translating NULL UIMedium by recreating it: */
    if (m_media.contains(UIMedium::nullID()))
        m_media[UIMedium::nullID()] = UIMedium();
}

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS

void UIMediumEnumerator::sltHandleMachineUpdate(const QUuid &uMachineID)
{
    LogRel2(("GUI: UIMediumEnumerator: Machine (or snapshot) event received, ID = %s\n",
             uMachineID.toString().toUtf8().constData()));

    /* Gather previously used UIMedium IDs: */
    QList<QUuid> previousUIMediumIDs;
    calculateCachedUsage(uMachineID, previousUIMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
             previousUIMediumIDs.isEmpty() ? "<empty>" : toStringList(previousUIMediumIDs).join(", ").toUtf8().constData()));

    /* Gather currently used CMediums and their IDs: */
    CMediumMap currentCMediums;
    QList<QUuid> currentCMediumIDs;
    calculateActualUsage(uMachineID, currentCMediums, currentCMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
             currentCMediumIDs.isEmpty() ? "<empty>" : toStringList(currentCMediumIDs).join(", ").toUtf8().constData()));

    /* Determine excluded media: */
    const QSet<QUuid> previousSet = previousUIMediumIDs.toSet();
    const QSet<QUuid> currentSet = currentCMediumIDs.toSet();
    const QSet<QUuid> excludedSet = previousSet - currentSet;
    const QList<QUuid> excludedUIMediumIDs = excludedSet.toList();
    if (!excludedUIMediumIDs.isEmpty())
        LogRel2(("GUI: UIMediumEnumerator:  Items excluded from usage: %s\n", toStringList(excludedUIMediumIDs).join(", ").toUtf8().constData()));
    if (!currentCMediumIDs.isEmpty())
        LogRel2(("GUI: UIMediumEnumerator:  Items currently in usage: %s\n", toStringList(currentCMediumIDs).join(", ").toUtf8().constData()));

    /* Update cache for excluded UIMediums: */
    recacheFromCachedUsage(excludedUIMediumIDs);

    /* Update cache for current CMediums: */
    recacheFromActualUsage(currentCMediums, currentCMediumIDs);

    LogRel2(("GUI: UIMediumEnumerator: Machine (or snapshot) event processed, ID = %s\n",
             uMachineID.toString().toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleMachineRegistration(const QUuid &uMachineID, const bool fRegistered)
{
    LogRel2(("GUI: UIMediumEnumerator: Machine %s event received, ID = %s\n",
             fRegistered ? "registration" : "unregistration",
             uMachineID.toString().toUtf8().constData()));

    /* Machine was registered: */
    if (fRegistered)
    {
        /* Gather currently used CMediums and their IDs: */
        CMediumMap currentCMediums;
        QList<QUuid> currentCMediumIDs;
        calculateActualUsage(uMachineID, currentCMediums, currentCMediumIDs, false /* take into account current state only */);
        LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
                 currentCMediumIDs.isEmpty() ? "<empty>" : toStringList(currentCMediumIDs).join(", ").toUtf8().constData()));
        /* Update cache with currently used CMediums: */
        recacheFromActualUsage(currentCMediums, currentCMediumIDs);
    }
    /* Machine was unregistered: */
    else
    {
        /* Gather previously used UIMedium IDs: */
        QList<QUuid> previousUIMediumIDs;
        calculateCachedUsage(uMachineID, previousUIMediumIDs, false /* take into account current state only */);
        LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
                 previousUIMediumIDs.isEmpty() ? "<empty>" : toStringList(previousUIMediumIDs).join(", ").toUtf8().constData()));
        /* Update cache for previously used UIMediums: */
        recacheFromCachedUsage(previousUIMediumIDs);
    }

    LogRel2(("GUI: UIMediumEnumerator: Machine %s event processed, ID = %s\n",
             fRegistered ? "registration" : "unregistration",
             uMachineID.toString().toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleSnapshotDeleted(const QUuid &uMachineID, const QUuid &uSnapshotID)
{
    LogRel2(("GUI: UIMediumEnumerator: Snapshot-deleted event received, Machine ID = {%s}, Snapshot ID = {%s}\n",
             uMachineID.toString().toUtf8().constData(), uSnapshotID.toString().toUtf8().constData()));

    /* Gather previously used UIMedium IDs: */
    QList<QUuid> previousUIMediumIDs;
    calculateCachedUsage(uMachineID, previousUIMediumIDs, false /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
             previousUIMediumIDs.isEmpty() ? "<empty>" : toStringList(previousUIMediumIDs).join(", ").toUtf8().constData()));

    /* Gather currently used CMediums and their IDs: */
    CMediumMap currentCMediums;
    QList<QUuid> currentCMediumIDs;
    calculateActualUsage(uMachineID, currentCMediums, currentCMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
             currentCMediumIDs.isEmpty() ? "<empty>" : toStringList(currentCMediumIDs).join(", ").toUtf8().constData()));

    /* Update everything: */
    recacheFromCachedUsage(previousUIMediumIDs);
    recacheFromActualUsage(currentCMediums, currentCMediumIDs);

    LogRel2(("GUI: UIMediumEnumerator: Snapshot-deleted event processed, Machine ID = {%s}, Snapshot ID = {%s}\n",
             uMachineID.toString().toUtf8().constData(), uSnapshotID.toString().toUtf8().constData()));
}

#else /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

void UIMediumEnumerator::sltHandleStorageControllerChange(const QUuid &uMachineId, const QString &strControllerName)
{
    //printf("OnStorageControllerChanged: machine-id=%s, controller-name=%s\n",
    //       uMachineId.toString().toUtf8().constData(), strControllerName.toUtf8().constData());
    LogRel2(("GUI: UIMediumEnumerator: OnStorageControllerChanged event received, Medium ID = {%s}, Controller Name = {%s}\n",
             uMachineId.toString().toUtf8().constData(), strControllerName.toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleStorageDeviceChange(CMediumAttachment comAttachment, bool fRemoved, bool fSilent)
{
    //printf("OnStorageDeviceChanged: removed=%d, silent=%d\n",
    //       fRemoved, fSilent);
    LogRel2(("GUI: UIMediumEnumerator: OnStorageDeviceChanged event received, Removed = {%d}, Silent = {%d}\n",
             fRemoved, fSilent));

    /* Parse attachment: */
    parseAttachment(comAttachment);
}

void UIMediumEnumerator::sltHandleMediumChange(CMediumAttachment comAttachment)
{
    //printf("OnMediumChanged\n");
    LogRel2(("GUI: UIMediumEnumerator: OnMediumChanged event received\n"));

    /* Parse attachment: */
    parseAttachment(comAttachment);
}

void UIMediumEnumerator::sltHandleMediumConfigChange(CMedium comMedium)
{
    //printf("OnMediumConfigChanged\n");
    LogRel2(("GUI: UIMediumEnumerator: OnMediumConfigChanged event received\n"));

    /* Parse medium: */
    parseMedium(comMedium);
}

void UIMediumEnumerator::sltHandleMediumRegistered(const QUuid &uMediumId, KDeviceType enmMediumType, bool fRegistered)
{
    //printf("OnMediumRegistered: medium-id=%s, medium-type=%d, registered=%d\n",
    //       uMediumId.toString().toUtf8().constData(), enmMediumType, fRegistered);
    //printf(" Medium to recache: %s\n",
    //       uMediumId.toString().toUtf8().constData());
    LogRel2(("GUI: UIMediumEnumerator: OnMediumRegistered event received, Medium ID = {%s}, Medium type = {%d}, Registered = {%d}\n",
             uMediumId.toString().toUtf8().constData(), enmMediumType, fRegistered));

    /* New medium registered: */
    if (fRegistered)
    {
        /* Make sure this medium isn't already cached: */
        if (!medium(uMediumId).isNull())
        {
            /* This medium can be known because of async event nature. Currently medium registration event
             * comes very late and other even unrealted events can come before it and request for this
             * particular medium enumeration, so we just ignore that and enumerate this UIMedium again. */
            LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} is cached already and will be enumerated..\n",
                     uMediumId.toString().toUtf8().constData()));
            createMediumEnumerationTask(m_media.value(uMediumId));
        }
        else
        {
            /* Get VBox for temporary usage, it will cache the error info: */
            CVirtualBox comVBox = vboxGlobal().virtualBox();
            /* Open existing medium, this API can be used to open known medium as well, using ID as location for that: */
            CMedium comMedium = comVBox.OpenMedium(uMediumId.toString(), enmMediumType, KAccessMode_ReadWrite, false);

            /* Show error message if necessary: */
            if (!comVBox.isOk())
                msgCenter().cannotOpenKnownMedium(comVBox, uMediumId);
            else
            {
                /* Create new UIMedium: */
                const UIMedium guiMedium(comMedium, UIMediumDefs::mediumTypeToLocal(comMedium.GetDeviceType()));
                const QUuid &uUIMediumKey = guiMedium.key();

                /* Cache corresponding UIMedium: */
                m_media.insert(uUIMediumKey, guiMedium);
                LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} is now cached and will be enumerated..\n",
                         uUIMediumKey.toString().toUtf8().constData()));

                /* And notify listeners: */
                emit sigMediumCreated(uUIMediumKey);

                /* Enumerate corresponding UIMedium: */
                createMediumEnumerationTask(m_media.value(uMediumId));
            }
        }
    }
    /* Old medium unregistered: */
    else
    {
        /* Make sure this medium is still cached: */
        if (medium(uMediumId).isNull())
        {
            LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} was not currently cached!\n",
                     uMediumId.toString().toUtf8().constData()));
            /// @todo is this a valid case?
            AssertFailed();
        }
        else
        {
            /* Forget corresponding UIMedium: */
            m_media.remove(uMediumId);
            LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} is no more cached!\n",
                     uMediumId.toString().toUtf8().constData()));

            /* And notify listeners: */
            emit sigMediumDeleted(uMediumId);
        }
    }
}

#endif /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

void UIMediumEnumerator::sltHandleMediumEnumerationTaskComplete(UITask *pTask)
{
    /* Make sure that is one of our tasks: */
    if (pTask->type() != UITask::Type_MediumEnumeration)
        return;
    AssertReturnVoid(m_tasks.contains(pTask));

    /* Get enumerated UIMedium: */
    const UIMedium guiMedium = pTask->property("medium").value<UIMedium>();
    const QUuid uMediumKey = guiMedium.key();
    LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} enumerated\n",
             uMediumKey.toString().toUtf8().constData()));

    /* Remove task from internal set: */
    m_tasks.remove(pTask);

    /* Make sure such UIMedium still exists: */
    if (!m_media.contains(uMediumKey))
    {
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} already deleted by a third party\n",
                 uMediumKey.toString().toUtf8().constData()));
        return;
    }

    /* Check if UIMedium ID was changed: */
    const QUuid uMediumID = guiMedium.id();
    /* UIMedium ID was changed to nullID: */
    if (uMediumID == UIMedium::nullID())
    {
        /* Delete this UIMedium: */
        m_media.remove(uMediumKey);
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} closed and deleted (after enumeration)\n",
                 uMediumKey.toString().toUtf8().constData()));

        /* And notify listener about delete: */
        emit sigMediumDeleted(uMediumKey);
    }
    /* UIMedium ID was changed to something proper: */
    else if (uMediumID != uMediumKey)
    {
        /* We have to reinject enumerated UIMedium: */
        m_media.remove(uMediumKey);
        m_media[uMediumID] = guiMedium;
        m_media[uMediumID].setKey(uMediumID);
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} has it changed to {%s}\n",
                 uMediumKey.toString().toUtf8().constData(),
                 uMediumID.toString().toUtf8().constData()));

        /* And notify listener about delete/create: */
        emit sigMediumDeleted(uMediumKey);
        emit sigMediumCreated(uMediumID);
    }
    /* UIMedium ID was not changed: */
    else
    {
        /* Just update enumerated UIMedium: */
        m_media[uMediumID] = guiMedium;
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} updated\n",
                 uMediumID.toString().toUtf8().constData()));

        /* And notify listener about update: */
        emit sigMediumEnumerated(uMediumID);
    }

    /* If there are no more tasks we know about: */
    if (m_tasks.isEmpty())
    {
        /* Notify listener: */
        LogRel(("GUI: UIMediumEnumerator: Medium-enumeration finished!\n"));
        m_fMediumEnumerationInProgress = false;
        emit sigMediumEnumerationFinished();
    }
}

void UIMediumEnumerator::createMediumEnumerationTask(const UIMedium &guiMedium)
{
    /* Prepare medium-enumeration task: */
    UITask *pTask = new UITaskMediumEnumeration(guiMedium);
    /* Append to internal set: */
    m_tasks << pTask;
    /* Post into global thread-pool: */
    vboxGlobal().threadPool()->enqueueTask(pTask);
}

void UIMediumEnumerator::addNullMediumToMap(UIMediumMap &media)
{
    /* Insert NULL UIMedium to the passed media map.
     * Get existing one from the previous map if any. */
    const UIMedium guiMedium = m_media.contains(UIMedium::nullID())
                             ? m_media[UIMedium::nullID()]
                             : UIMedium();
    media.insert(UIMedium::nullID(), guiMedium);
}

void UIMediumEnumerator::addMediaToMap(const CMediumVector &inputMedia, UIMediumMap &outputMedia)
{
    /* Iterate through passed inputMedia vector: */
    foreach (const CMedium &comMedium, inputMedia)
    {
        /* If VBoxGlobal is cleaning up, abort immediately: */
        if (VBoxGlobal::isCleaningUp())
            break;

        /* Insert UIMedium to the passed media map.
         * Get existing one from the previous map if any.
         * Create on the basis of comMedium otherwise. */
        const QUuid uMediumID = comMedium.GetId();
        const UIMedium guiMedium = m_media.contains(uMediumID)
                                 ? m_media.value(uMediumID)
                                 : UIMedium(comMedium, UIMediumDefs::mediumTypeToLocal(comMedium.GetDeviceType()));
        outputMedia.insert(guiMedium.id(), guiMedium);

        /* Insert comMedium children into map as well: */
        addMediaToMap(comMedium.GetChildren(), outputMedia);
    }
}

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS

void UIMediumEnumerator::calculateCachedUsage(const QUuid &uMachineID,
                                              QList<QUuid> &previousUIMediumIDs,
                                              const bool fTakeIntoAccountCurrentStateOnly) const
{
    /* For each the cached UIMedium we have: */
    foreach (const QUuid &uUIMediumID, mediumIDs())
    {
        /* Get corresponding UIMedium: */
        const UIMedium &guiMedium = m_media[uUIMediumID];
        /* Get the list of the machines this UIMedium attached to.
         * Take into account current-state only if necessary. */
        const QList<QUuid> &machineIDs = fTakeIntoAccountCurrentStateOnly
                                       ? guiMedium.curStateMachineIds()
                                       : guiMedium.machineIds();
        /* Add this UIMedium ID to previous usage if necessary: */
        if (machineIDs.contains(uMachineID))
            previousUIMediumIDs.append(uUIMediumID);
    }
}

void UIMediumEnumerator::calculateActualUsage(const QUuid &uMachineID,
                                              CMediumMap &currentCMediums,
                                              QList<QUuid> &currentCMediumIDs,
                                              const bool fTakeIntoAccountCurrentStateOnly) const
{
    /* Search for corresponding machine: */
    CMachine comMachine = vboxGlobal().virtualBox().FindMachine(uMachineID.toString());
    if (comMachine.isNull())
    {
        /* Usually means the machine is already gone, not harmful. */
        return;
    }

    /* Calculate actual usage starting from root-snapshot if necessary: */
    if (!fTakeIntoAccountCurrentStateOnly)
        calculateActualUsage(comMachine.FindSnapshot(QString()), currentCMediums, currentCMediumIDs);

    /* Calculate actual usage for current machine state: */
    calculateActualUsage(comMachine, currentCMediums, currentCMediumIDs);
}

void UIMediumEnumerator::calculateActualUsage(const CSnapshot &comSnapshot,
                                              CMediumMap &currentCMediums,
                                              QList<QUuid> &currentCMediumIDs) const
{
    /* Check passed snapshot: */
    if (comSnapshot.isNull())
        return;

    /* Calculate actual usage for passed snapshot machine: */
    calculateActualUsage(comSnapshot.GetMachine(), currentCMediums, currentCMediumIDs);

    /* Iterate through passed snapshot children: */
    foreach (const CSnapshot &comChildSnapshot, comSnapshot.GetChildren())
        calculateActualUsage(comChildSnapshot, currentCMediums, currentCMediumIDs);
}

void UIMediumEnumerator::calculateActualUsage(const CMachine &comMachine,
                                              CMediumMap &currentCMediums,
                                              QList<QUuid> &currentCMediumIDs) const
{
    /* Check passed machine: */
    AssertReturnVoid(!comMachine.isNull());

    /* For each the attachment machine have: */
    foreach (const CMediumAttachment &comAttachment, comMachine.GetMediumAttachments())
    {
        /* Get corresponding CMedium: */
        CMedium comMedium = comAttachment.GetMedium();
        if (!comMedium.isNull())
        {
            /* Make sure that CMedium was not yet closed: */
            const QUuid &uCMediumID = comMedium.GetId();
            if (comMedium.isOk() && !uCMediumID.isNull())
            {
                /* Add this CMedium to current usage: */
                currentCMediums.insert(uCMediumID, comMedium);
                currentCMediumIDs.append(uCMediumID);
            }
        }
    }
}

void UIMediumEnumerator::recacheFromCachedUsage(const QList<QUuid> &previousUIMediumIDs)
{
    /* For each of previously used UIMedium ID: */
    foreach (const QUuid &uUIMediumID, previousUIMediumIDs)
    {
        /* Make sure this ID still in our map: */
        if (m_media.contains(uUIMediumID))
        {
            /* Get corresponding UIMedium: */
            UIMedium &guiMedium = m_media[uUIMediumID];

            /* If corresponding CMedium still exists: */
            CMedium comMedium = guiMedium.medium();
            if (!comMedium.GetId().isNull() && comMedium.isOk())
            {
                /* Refresh UIMedium parent first of all: */
                guiMedium.updateParentID();
                /* Enumerate corresponding UIMedium: */
                createMediumEnumerationTask(guiMedium);
            }
            /* If corresponding CMedium was closed already: */
            else
            {
                /* Uncache corresponding UIMedium: */
                m_media.remove(uUIMediumID);
                LogRel2(("GUI: UIMediumEnumerator:  Medium with key={%s} uncached\n",
                         uUIMediumID.toString().toUtf8().constData()));

                /* And notify listeners: */
                emit sigMediumDeleted(uUIMediumID);
            }
        }
    }
}

void UIMediumEnumerator::recacheFromActualUsage(const CMediumMap &currentCMediums,
                                                const QList<QUuid> &currentCMediumIDs)
{
    /* For each of currently used CMedium ID: */
    foreach (const QUuid &uCMediumID, currentCMediumIDs)
    {
        /* If that ID is not in our map: */
        if (!m_media.contains(uCMediumID))
        {
            /* Create new UIMedium: */
            const CMedium &comMedium = currentCMediums[uCMediumID];
            const UIMedium guiMedium(comMedium, UIMediumDefs::mediumTypeToLocal(comMedium.GetDeviceType()));
            const QUuid &uUIMediumKey = guiMedium.key();

            /* Cache created UIMedium: */
            m_media.insert(uUIMediumKey, guiMedium);
            LogRel2(("GUI: UIMediumEnumerator:  Medium with key={%s} cached\n",
                     uUIMediumKey.toString().toUtf8().constData()));

            /* And notify listeners: */
            emit sigMediumCreated(uUIMediumKey);
        }

        /* Enumerate corresponding UIMedium: */
        createMediumEnumerationTask(m_media[uCMediumID]);
    }
}

#else /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */

void UIMediumEnumerator::parseAttachment(CMediumAttachment &comAttachment)
{
    /* Make sure attachment is valid: */
    if (comAttachment.isNull())
    {
        LogRel2(("GUI: UIMediumEnumerator:  Attachment is NULL!\n"));
        /// @todo is this possible case?
        AssertFailed();
    }
    else
    {
        /* Acquire attachment medium: */
        CMedium comMedium = comAttachment.GetMedium();
        if (!comAttachment.isOk())
        {
            LogRel2(("GUI: UIMediumEnumerator:  Unable to acquire attachment medium!\n"));
            msgCenter().cannotAcquireAttachmentMedium(comAttachment);
        }
        else
        {
            /* Parse medium: */
            parseMedium(comMedium);
        }
    }
}

void UIMediumEnumerator::parseMedium(CMedium &comMedium)
{
    /* Make sure medium is valid: */
    if (comMedium.isNull())
    {
        /* This medium is NULL by some reason, the obvious case when this
         * can happen is when optical/floppy device is created empty. */
        LogRel2(("GUI: UIMediumEnumerator:  Medium is NULL!\n"));
    }
    else
    {
        /* Acquire medium ID: */
        const QUuid uMediumId = comMedium.GetId();
        if (!comMedium.isOk())
        {
            LogRel2(("GUI: UIMediumEnumerator:  Unable to acquire medium ID!\n"));
            msgCenter().cannotAcquireMediumAttribute(comMedium);
        }
        else
        {
            //printf(" Medium to recache: %s\n", uMediumId.toString().toUtf8().constData());

            /* Make sure this medium is already cached: */
            if (medium(uMediumId).isNull())
            {
                /* This medium isn't cached by some reason, which can be different.
                 * One of such reasons is when config-changed event comes earlier than
                 * corresponding registration event. For now we are ignoring that at all. */
                LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} isn't cached yet!\n",
                         uMediumId.toString().toUtf8().constData()));
            }
            else
            {
                /* Enumerate corresponding UIMedium: */
                LogRel2(("GUI: UIMediumEnumerator:  Medium {%s} will be enumerated..\n",
                         uMediumId.toString().toUtf8().constData()));
                createMediumEnumerationTask(m_media.value(uMediumId));
            }
        }
    }
}

#endif /* VBOX_GUI_WITH_NEW_MEDIA_EVENTS */


#include "UIMediumEnumerator.moc"
