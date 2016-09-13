/* $Id$ */
/** @file
 * VBox Qt GUI - UISnapshotPane class declaration.
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

#ifndef ___UISnapshotPane_h___
#define ___UISnapshotPane_h___

/* Qt includes: */
#include <QTimer>
#include <QIcon>

/* GUI includes: */
#include "UISnapshotPane.gen.h"
#include "VBoxGlobal.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CMachine.h"

/* Forward declarations: */
class SnapshotWgtItem;


/** Snapshot age format. */
enum SnapshotAgeFormat
{
    AgeInSeconds,
    AgeInMinutes,
    AgeInHours,
    AgeInDays,
    AgeMax
};


/** QWidget extension providing GUI with the pane to control snapshot related functionality. */
class UISnapshotPane : public QIWithRetranslateUI <QWidget>, public Ui::UISnapshotPane
{
    Q_OBJECT;

public:

    /** Constructs snapshot pane passing @a aParent to the base-class. */
    UISnapshotPane(QWidget *aParent);

    /** Defines the @a aMachine to be parsed. */
    void setMachine (const CMachine &aMachine);

    /** Returns cached snapshot-item icon depending on @a fOnline flag. */
    const QIcon& snapshotItemIcon(bool fOnline) { return !fOnline ? m_offlineSnapshotIcon : m_onlineSnapshotIcon; }

protected:

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /** @name Tree-view handlers
      * @{ */
        /** Handles cursor change to @a aItem. */
        void onCurrentChanged (QTreeWidgetItem *aItem = 0);
        /** Handles context menu request for @a aPoint. */
        void onContextMenuRequested (const QPoint &aPoint);
        /** Handles modification for @a aItem. */
        void onItemChanged (QTreeWidgetItem *aItem);
        /** Handles double-click for @a pItem. */
        void sltItemDoubleClicked(QTreeWidgetItem *pItem);
    /** @} */

    /** @name Snapshot operations
      * @{ */
        /** Proposes to take a snapshot. */
        void sltTakeSnapshot();
        /** Proposes to restore the snapshot. */
        void sltRestoreSnapshot(bool fSuppressNonCriticalWarnings = false);
        /** Proposes to delete the snapshot. */
        void sltDeleteSnapshot();
        /** Displays the snapshot details dialog. */
        void sltShowSnapshotDetails();
        /** Proposes to clone the snapshot. */
        void sltCloneSnapshot();
    /** @} */

    /** @name Main event handlers
      * @{ */
        /** Handles machine data change for machine with @a strId. */
        void machineDataChanged(QString strId);
        /** Handles machine @a state change for machine with @a strId. */
        void machineStateChanged(QString strId, KMachineState state);
        /** Handles session @a state change for machine with @a strId. */
        void sessionStateChanged(QString strId, KSessionState state);
    /** @} */

    /** @name Timer event handlers
      * @{ */
        /** Updates snapshots age. */
        void updateSnapshotsAge();
    /** @} */

private:

    /** @name Snapshot operations
      * @{ */
        /** Proposes to take a snapshot. */
        bool takeSnapshot();
        /** Proposes to restore the snapshot. */
        //bool restoreSnapshot();
        /** Proposes to delete the snapshot. */
        //bool deleteSnapshot();
        /** Displays the snapshot details dialog. */
        //bool showSnapshotDetails();
        /** Proposes to clone the snapshot. */
        //bool cloneSnapshot();
    /** @} */

    /** Refreshes everything. */
    void refreshAll();

    /** Searches for an item with corresponding @a aSnapshotId. */
    SnapshotWgtItem* findItem (const QString &aSnapshotId);
    /** Returns the "current state" item. */
    SnapshotWgtItem* curStateItem();

    /** Populates snapshot items for corresponding @a aSnapshot using @a aItem as parent. */
    void populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *aItem);

    /** Searches for smallest snapshot age starting with @a aParentItem as parent. */
    SnapshotAgeFormat traverseSnapshotAge (QTreeWidgetItem *aParentItem);

    /** Holds the machine COM wrapper. */
    CMachine         mMachine;
    /** Holds the machine ID. */
    QString          mMachineId;
    /** Holds the cached session state. */
    KSessionState    mSessionState;
    /** Holds the current snapshot item reference. */
    SnapshotWgtItem *mCurSnapshotItem;
    /** Holds the snapshot item editing protector. */
    bool             mEditProtector;

    /** Holds the snapshot item action group instance. */
    QActionGroup    *mSnapshotActionGroup;
    /** Holds the current item action group instance. */
    QActionGroup    *mCurStateActionGroup;

    /** Holds the snapshot restore action instance. */
    QAction         *mRestoreSnapshotAction;
    /** Holds the snapshot delete action instance. */
    QAction         *mDeleteSnapshotAction;
    /** Holds the show snapshot details action instance. */
    QAction         *mShowSnapshotDetailsAction;
    /** Holds the snapshot take action instance. */
    QAction         *mTakeSnapshotAction;
    /** Holds the snapshot clone action instance. */
    QAction         *mCloneSnapshotAction;

    /** Holds the snapshot age update timer. */
    QTimer           mAgeUpdateTimer;

    /** Holds whether the snapshot operations are allowed. */
    bool             m_fShapshotOperationsAllowed;

    /** Holds the cached snapshot-item pixmap for 'offline' state. */
    QIcon            m_offlineSnapshotIcon;
    /** Holds the cached snapshot-item pixmap for 'online' state. */
    QIcon            m_onlineSnapshotIcon;
};

#endif /* !___UISnapshotPane_h___ */

