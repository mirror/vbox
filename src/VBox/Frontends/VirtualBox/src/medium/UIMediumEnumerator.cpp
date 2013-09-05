/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMediumEnumerator class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include "UIMediumEnumerator.h"
#include "UIThreadPool.h"
#include "UIVirtualBoxEventHandler.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CMediumAttachment.h"


/* GUI task prototype: Medium enumeration.
 * Extends UITask interface to be executed by the UIThreadWorker. */
class UITaskMediumEnumeration : public UITask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UITaskMediumEnumeration(const UIMedium &medium)
        : UITask(QVariant::fromValue(medium))
    {}

private:

    /* Helper: Run stuff: */
    void run();
};


UIMediumEnumerator::UIMediumEnumerator(ulong uWorkerCount /*= 3*/, ulong uWorkerTimeout /*= 5000*/)
    : m_pThreadPool(0)
    , m_fMediumEnumerationInProgress(false)
{
    /* Allow UIMedium to be used in inter-thread signals: */
    qRegisterMetaType<UIMedium>();

    /* Prepare Main event handlers: */
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(sltHandleMachineUpdate(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)), this, SLOT(sltHandleMachineUpdate(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltHandleMachineUpdate(QString)));

    /* Prepare thread-pool: */
    m_pThreadPool = new UIThreadPool(uWorkerCount, uWorkerTimeout);
    connect(m_pThreadPool, SIGNAL(sigTaskComplete(UITask*)), this, SLOT(sltHandleMediumEnumerationTaskComplete(UITask*)));
}

UIMediumEnumerator::~UIMediumEnumerator()
{
    /* Delete thread-pool: */
    delete m_pThreadPool;
    m_pThreadPool = 0;

    /* Delete all the tasks: */
    while (!m_tasks.isEmpty())
        delete m_tasks.takeFirst();
}

QList<QString> UIMediumEnumerator::mediumIDs() const
{
    /* Return keys of current medium-map: */
    return m_mediums.keys();
}

UIMedium UIMediumEnumerator::medium(const QString &strMediumID)
{
    /* Search through current medium-map for the medium with passed ID: */
    if (m_mediums.contains(strMediumID))
        return m_mediums[strMediumID];
    /* Return NULL medium otherwise: */
    return UIMedium();
}

void UIMediumEnumerator::createMedium(const UIMedium &medium)
{
    /* Get medium ID: */
    QString strMediumID = medium.id();
    LogRelFlow(("UIMediumEnumerator: Medium with ID={%s} created.\n", strMediumID.toAscii().constData()));

    /* Make sure medium doesn't exists already: */
    AssertReturnVoid(!m_mediums.contains(strMediumID));

    /* Insert medium: */
    m_mediums[strMediumID] = medium;

    /* Notify listener: */
    emit sigMediumCreated(strMediumID);
}

void UIMediumEnumerator::updateMedium(const UIMedium &medium)
{
    /* Get medium ID: */
    QString strMediumID = medium.id();
    LogRelFlow(("UIMediumEnumerator: Medium with ID={%s} updated.\n", strMediumID.toAscii().constData()));

    /* Make sure medium still exists: */
    AssertReturnVoid(m_mediums.contains(strMediumID));

    /* Update medium: */
    m_mediums[strMediumID] = medium;

    /* Notify listener: */
    emit sigMediumUpdated(strMediumID);
}

void UIMediumEnumerator::deleteMedium(const QString &strMediumID)
{
    LogRelFlow(("UIMediumEnumerator: Medium with ID={%s} removed.\n", strMediumID.toAscii().constData()));

    /* Make sure medium still exists: */
    AssertReturnVoid(m_mediums.contains(strMediumID));

    /* Remove medium: */
    m_mediums.remove(strMediumID);

    /* Notify listener: */
    emit sigMediumDeleted(strMediumID);
}

void UIMediumEnumerator::enumerateMediums()
{
    /* Make sure we are not already in progress: */
    AssertReturnVoid(!m_fMediumEnumerationInProgress);

    /* Compose new map of all currently known mediums & their children.
     * While composing we are using data from already existing mediums. */
    UIMediumMap mediums;
    addNullMediumToMap(mediums);
    addHardDisksToMap(vboxGlobal().virtualBox().GetHardDisks(), mediums);
    addMediumsToMap(vboxGlobal().host().GetDVDDrives(), mediums, UIMediumType_DVD);
    addMediumsToMap(vboxGlobal().virtualBox().GetDVDImages(), mediums, UIMediumType_DVD);
    addMediumsToMap(vboxGlobal().host().GetFloppyDrives(), mediums, UIMediumType_Floppy);
    addMediumsToMap(vboxGlobal().virtualBox().GetFloppyImages(), mediums, UIMediumType_Floppy);
    m_mediums = mediums;

    /* Notify listener: */
    LogRelFlow(("UIMediumEnumerator: Medium-enumeration started...\n"));
    m_fMediumEnumerationInProgress = true;
    emit sigMediumEnumerationStarted();

    /* Start enumeration for all the new mediums: */
    QList<QString> mediumIDs = m_mediums.keys();
    foreach (QString strMediumID, mediumIDs)
        createMediumEnumerationTask(m_mediums[strMediumID]);
}

void UIMediumEnumerator::sltHandleMachineUpdate(QString strMachineID)
{
    LogRelFlow(("UIMediumEnumerator: Machine event received, ID = %s\n", strMachineID.toAscii().constData()));

    /* Compose a map of previous usage: */
    QStringList oldUsage;
    foreach (const QString &strMediumID, mediumIDs())
    {
        const UIMedium &uimedium = m_mediums[strMediumID];
        const QList<QString> &machineIDs = uimedium.machineIds();
        if (machineIDs.contains(strMachineID))
            oldUsage << strMediumID;
    }
    LogRelFlow(("UIMediumEnumerator:  Old usage: %s\n", oldUsage.isEmpty() ? "<empty>" : oldUsage.join(", ").toAscii().constData()));

    /* Compose a map of current usage: */
    QStringList newUsage;
    QMap<QString, CMedium> newMediumMap;
    CMachine machine = vboxGlobal().virtualBox().FindMachine(strMachineID);
    if (!machine.isNull())
    {
        CMediumAttachmentVector attachments = machine.GetMediumAttachments();
        foreach (const CMediumAttachment &attachment, attachments)
        {
            CMedium cmedium = attachment.GetMedium();
            if (cmedium.isNull())
                continue;
            QString strMediumID = cmedium.GetId();
            newMediumMap.insert(strMediumID, cmedium);
            newUsage << strMediumID;
        }
    }
    LogRelFlow(("UIMediumEnumerator:  New usage: %s\n", newUsage.isEmpty() ? "<empty>" : newUsage.join(", ").toAscii().constData()));

    /* Manipulations over the sets: */
    QSet<QString> oldSet = oldUsage.toSet();
    QSet<QString> newSet = newUsage.toSet();
    QSet<QString> excludedSet = oldSet - newSet;
    QSet<QString> includedSet = newSet - oldSet;
    QStringList excludedList = excludedSet.toList();
    QStringList includedList = includedSet.toList();
    if (!excludedList.isEmpty())
        LogRelFlow(("UIMediumEnumerator:  Items excluded from machine usage: %s\n", excludedList.join(", ").toAscii().constData()));
    if (!includedList.isEmpty())
        LogRelFlow(("UIMediumEnumerator:  Items included into machine usage: %s\n", includedList.join(", ").toAscii().constData()));

    /* For each of excluded items: */
    foreach (const QString &strExcludedMediumID, excludedList)
    {
        /* Make sure this medium still in our map: */
        if (!m_mediums.contains(strExcludedMediumID))
            continue;
        /* Remove UIMedium if it was closed already: */
        const UIMedium &uimedium = m_mediums[strExcludedMediumID];
        if (uimedium.medium().GetId() != strExcludedMediumID)
        {
            deleteMedium(strExcludedMediumID);
            continue;
        }
        /* Enumerate UIMedium if CMedium still exists: */
        createMediumEnumerationTask(m_mediums[strExcludedMediumID]);
    }

    /* For each of included items: */
    foreach (const QString &strIncludedMediumID, includedList)
    {
        /* Create UIMedium if it is not in our map: */
        if (!m_mediums.contains(strIncludedMediumID))
        {
            const CMedium &cmedium = newMediumMap[strIncludedMediumID];
            UIMedium uimedium(cmedium, UIMediumDefs::mediumTypeToLocal(cmedium.GetDeviceType()));
            createMedium(uimedium);
        }
        /* Enumerate UIMedium in any case: */
        createMediumEnumerationTask(m_mediums[strIncludedMediumID]);
    }

    LogRelFlow(("UIMediumEnumerator: Machine event processed, ID = %s\n", strMachineID.toAscii().constData()));
}

void UIMediumEnumerator::sltHandleMediumEnumerationTaskComplete(UITask *pTask)
{
    /* Make sure that is one of our tasks: */
    int iIndexOfTask = m_tasks.indexOf(pTask);
    AssertReturnVoid(iIndexOfTask != -1);

    /* Get medium: */
    UIMedium medium = pTask->data().value<UIMedium>();
    QString strMediumID = medium.id();
    LogRelFlow(("UIMediumEnumerator: Medium with ID={%s} enumerated.\n", strMediumID.toAscii().constData()));

    /* Delete task: */
    delete m_tasks.takeAt(iIndexOfTask);

    /* Make sure such medium still exists: */
    if (!m_mediums.contains(strMediumID))
        return;

    /* Update enumerated medium: */
    m_mediums[strMediumID] = medium;

    /* Notify listener: */
    emit sigMediumEnumerated(strMediumID);

    /* If there are no more tasks we know about: */
    if (m_tasks.isEmpty())
    {
        /* Notify listener: */
        LogRelFlow(("UIMediumEnumerator: Medium-enumeration finished!\n"));
        m_fMediumEnumerationInProgress = false;
        emit sigMediumEnumerationFinished();
    }
}

void UIMediumEnumerator::createMediumEnumerationTask(const UIMedium &medium)
{
    /* Prepare medium-enumeration task: */
    UITask *pTask = new UITaskMediumEnumeration(medium);
    /* Append to internal list: */
    m_tasks.append(pTask);
    /* Post into thread-pool: */
    m_pThreadPool->enqueueTask(pTask);
}

void UIMediumEnumerator::addNullMediumToMap(UIMediumMap &mediums)
{
    /* Insert NULL uimedium to the passed uimedium map.
     * Get existing one from the previous map if any. */
    QString strNullMediumID = UIMedium::nullID();
    UIMedium uimedium = m_mediums.contains(strNullMediumID) ? m_mediums[strNullMediumID] : UIMedium();
    mediums.insert(strNullMediumID, uimedium);
}

void UIMediumEnumerator::addMediumsToMap(const CMediumVector &inputMediums, UIMediumMap &outputMediums, UIMediumType mediumType)
{
    /* Insert hard-disks to the passed uimedium map.
     * Get existing one from the previous map if any. */
    foreach (CMedium medium, inputMediums)
    {
        /* Prepare uimedium on the basis of current medium: */
        QString strMediumID = medium.GetId();
        UIMedium uimedium = m_mediums.contains(strMediumID) ? m_mediums[strMediumID] :
                                                              UIMedium(medium, mediumType);

        /* Insert uimedium into map: */
        outputMediums.insert(uimedium.id(), uimedium);
    }
}

void UIMediumEnumerator::addHardDisksToMap(const CMediumVector &inputMediums, UIMediumMap &outputMediums)
{
    /* Insert hard-disks to the passed uimedium map.
     * Get existing one from the previous map if any. */
    foreach (CMedium medium, inputMediums)
    {
        /* Prepare uimedium on the basis of current medium: */
        QString strMediumID = medium.GetId();
        UIMedium uimedium = m_mediums.contains(strMediumID) ? m_mediums[strMediumID] :
                                                              UIMedium(medium, UIMediumType_HardDisk);

        /* Insert uimedium into map: */
        outputMediums.insert(uimedium.id(), uimedium);

        /* Insert medium children into map too: */
        addHardDisksToMap(medium.GetChildren(), outputMediums);
    }
}

void UITaskMediumEnumeration::run()
{
    /* Get medium: */
    UIMedium medium = m_data.value<UIMedium>();
    /* Enumerate it: */
    medium.blockAndQueryState();
    /* Put medium back: */
    m_data = QVariant::fromValue(medium);
}


#include "UIMediumEnumerator.moc"

