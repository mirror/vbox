/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumEnumerator class declaration.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumEnumerator_h___
#define ___UIMediumEnumerator_h___

/* Qt includes: */
#include <QObject>
#include <QSet>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIMedium.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIThreadPool;
class UITask;

/* Typedefs: */
typedef QMap<QUuid, CMedium> CMediumMap;

/* Medium-enumerator prototype.
 * Manages access to medium information using thread-pool interface. */
class SHARED_LIBRARY_STUFF UIMediumEnumerator : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

signals:

    /* Notifiers: Medium-operations stuff: */
    void sigMediumCreated(const QUuid &aMediumID);
    void sigMediumDeleted(const QUuid &aMediumID);

    /* Notifiers: Medium-enumeration stuff: */
    void sigMediumEnumerationStarted();
    void sigMediumEnumerated(const QUuid &aMediumID);
    void sigMediumEnumerationFinished();

public:

    /** Constructs medium-enumerator object. */
    UIMediumEnumerator();

    /* API: Medium-access stuff: */
    QList<QUuid> mediumIDs() const;
    UIMedium medium(const QUuid &aMediumID);
    void createMedium(const UIMedium &medium);
    void deleteMedium(const QUuid &aMediumID);

    /* API: Medium-enumeration stuff: */
    bool isMediumEnumerationInProgress() const { return m_fMediumEnumerationInProgress; }
    void enumerateMedia(const CMediumVector &mediaList = CMediumVector());
    void refreshMedia();

private slots:

    /** Handles machine-data-change and snapshot-change events. */
    void sltHandleMachineUpdate(const QUuid &aMachineID);
    /** Handles machine-[un]registration events. */
    void sltHandleMachineRegistration(const QUuid &aMachineID, const bool fRegistered);
    /** Handles snapshot-deleted events. */
    void sltHandleSnapshotDeleted(const QUuid &aMachineID, const QUuid &aSnapshotID);

    /* Handler: Medium-enumeration stuff: */
    void sltHandleMediumEnumerationTaskComplete(UITask *pTask);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /* Helpers: Medium-enumeration stuff: */
    void createMediumEnumerationTask(const UIMedium &medium);
    void addNullMediumToMap(UIMediumMap &media);
    void addMediaToMap(const CMediumVector &inputMedia, UIMediumMap &outputMedia);

    /* Helpers: Medium re-caching stuff: */
    void calculateCachedUsage(const QUuid &aMachineID, QList<QUuid> &previousUIMediumIDs, const bool fTakeIntoAccountCurrentStateOnly) const;
    void calculateActualUsage(const QUuid &strMachineID, CMediumMap &currentCMediums, QList<QUuid> &currentCMediumIDs, const bool fTakeIntoAccountCurrentStateOnly) const;
    void calculateActualUsage(const CSnapshot &snapshot, CMediumMap &currentCMediums, QList<QUuid> &currentCMediumIDs) const;
    void calculateActualUsage(const CMachine &machine, CMediumMap &currentCMediums, QList<QUuid> &currentCMediumIDs) const;
    void recacheFromCachedUsage(const QList<QUuid> &previousUIMediumIDs);
    void recacheFromActualUsage(const CMediumMap &currentCMediums, const QList<QUuid> &currentCMediumIDs);

    /* Variables: */
    bool m_fMediumEnumerationInProgress;
    QSet<UITask*> m_tasks;
    UIMediumMap m_media;
};

#endif /* !___UIMediumEnumerator_h___ */
