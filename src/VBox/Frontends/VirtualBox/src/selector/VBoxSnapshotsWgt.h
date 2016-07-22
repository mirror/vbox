/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotsWgt class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxSnapshotsWgt_h__
#define __VBoxSnapshotsWgt_h__

/* Qt includes: */
#include <QTimer>
#include <QIcon>

/* GUI includes: */
#include "VBoxSnapshotsWgt.gen.h"
#include "VBoxGlobal.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CMachine.h"

/* Local forwards */
class SnapshotWgtItem;

/* Snapshot age format */
enum SnapshotAgeFormat
{
    AgeInSeconds,
    AgeInMinutes,
    AgeInHours,
    AgeInDays,
    AgeMax
};

class VBoxSnapshotsWgt : public QIWithRetranslateUI <QWidget>, public Ui::VBoxSnapshotsWgt
{
    Q_OBJECT;

public:

    VBoxSnapshotsWgt (QWidget *aParent);

    void setMachine (const CMachine &aMachine);

    /** Returns cached snapshot-item icon depending on @a fOnline flag. */
    const QIcon& snapshotItemIcon(bool fOnline) { return !fOnline ? m_offlineSnapshotIcon : m_onlineSnapshotIcon; }

protected:

    void retranslateUi();

private slots:

    /* Snapshot tree slots: */
    void onCurrentChanged (QTreeWidgetItem *aItem = 0);
    void onContextMenuRequested (const QPoint &aPoint);
    void onItemChanged (QTreeWidgetItem *aItem);
    void sltItemDoubleClicked(QTreeWidgetItem *pItem);

    /* Snapshot functionality slots: */
    void sltTakeSnapshot();
    void sltRestoreSnapshot(bool fSuppressNonCriticalWarnings = false);
    void sltDeleteSnapshot();
    void sltShowSnapshotDetails();
    void sltCloneSnapshot();

    /* Main API event handlers: */
    void machineDataChanged(QString strId);
    void machineStateChanged(QString strId, KMachineState state);
    void sessionStateChanged(QString strId, KSessionState state);

    void updateSnapshotsAge();

private:

    /* Snapshot private functions: */
    bool takeSnapshot();
    //bool restoreSnapshot();
    //bool deleteSnapshot();
    //bool showSnapshotDetails();

    void refreshAll();
    SnapshotWgtItem* findItem (const QString &aSnapshotId);
    SnapshotWgtItem* curStateItem();
    void populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *aItem);
    SnapshotAgeFormat traverseSnapshotAge (QTreeWidgetItem *aParentItem);

    CMachine         mMachine;
    QString          mMachineId;
    KSessionState    mSessionState;
    SnapshotWgtItem *mCurSnapshotItem;
    bool             mEditProtector;

    QActionGroup    *mSnapshotActionGroup;
    QActionGroup    *mCurStateActionGroup;

    QAction         *mRestoreSnapshotAction;
    QAction         *mDeleteSnapshotAction;
    QAction         *mShowSnapshotDetailsAction;
    QAction         *mTakeSnapshotAction;
    QAction         *mCloneSnapshotAction;

    QTimer          mAgeUpdateTimer;

    bool            m_fShapshotOperationsAllowed;

    /** Pointer to cached snapshot-item pixmap for 'offline' state. */
    QIcon           m_offlineSnapshotIcon;
    /** Pointer to cached snapshot-item pixmap for 'online' state. */
    QIcon           m_onlineSnapshotIcon;
};

#endif // __VBoxSnapshotsWgt_h__
