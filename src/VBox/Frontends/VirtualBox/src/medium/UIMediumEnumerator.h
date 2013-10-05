/** @file
 * VBox Qt GUI - UIMediumEnumerator class declaration.
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

#ifndef ___UIMediumEnumerator_h___
#define ___UIMediumEnumerator_h___

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UIMedium.h"

/* Forward declarations: */
class UIThreadPool;
class UITask;

/* Medium-enumerator prototype.
 * Manages access to medium information using thread-pool interface. */
class UIMediumEnumerator : public QObject
{
    Q_OBJECT;

signals:

    /* Notifiers: Medium-operations stuff: */
    void sigMediumCreated(const QString &strMediumID);
    void sigMediumUpdated(const QString &strMediumID);
    void sigMediumDeleted(const QString &strMediumID);

    /* Notifiers: Medium-enumeration stuff: */
    void sigMediumEnumerationStarted();
    void sigMediumEnumerated(const QString &strMediumID);
    void sigMediumEnumerationFinished();

public:

    /* Constructor/destructor: */
    UIMediumEnumerator(ulong uWorkerCount = 3, ulong uWorkerTimeout = 5000);
    ~UIMediumEnumerator();

    /* API: Medium-access stuff: */
    QList<QString> mediumIDs() const;
    UIMedium medium(const QString &strMediumID);
    void createMedium(const UIMedium &medium);
    void updateMedium(const UIMedium &medium);
    void deleteMedium(const QString &strMediumID);

    /* API: Medium-enumeration stuff: */
    bool isMediumEnumerationInProgress() const { return m_fMediumEnumerationInProgress; }
    void enumerateMediums();

private slots:

    /* Handler: Machine stuff: */
    void sltHandleMachineUpdate(QString strMachineID);

    /* Handler: Medium-enumeration stuff: */
    void sltHandleMediumEnumerationTaskComplete(UITask *pTask);

private:

    /* Helpers: Medium-enumeration stuff: */
    void createMediumEnumerationTask(const UIMedium &medium);
    void addNullMediumToMap(UIMediumMap &mediums);
    void addMediumsToMap(const CMediumVector &inputMediums, UIMediumMap &outputMediums, UIMediumType mediumType);
    void addHardDisksToMap(const CMediumVector &inputMediums, UIMediumMap &outputMediums);

    /* Variables: */
    UIThreadPool *m_pThreadPool;
    bool m_fMediumEnumerationInProgress;
    QList<UITask*> m_tasks;
    UIMediumMap m_mediums;
};

#endif /* !___UIMediumEnumerator_h___ */

