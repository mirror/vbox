/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotsWgt class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxSnapshotsWgt_h__
#define __VBoxSnapshotsWgt_h__

#include "VBoxSnapshotsWgt.gen.h"
#include "VBoxGlobal.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QUuid>

class SnapshotWgtItem;

class QMenu;

class VBoxSnapshotsWgt : public QIWithRetranslateUI<QWidget>,
                         public Ui::VBoxSnapshotsWgt
{
    Q_OBJECT;

public:

    VBoxSnapshotsWgt (QWidget *aParent);

    void setMachine (const CMachine &aMachine);

protected:

    void retranslateUi();

private slots:

    void onCurrentChanged (QTreeWidgetItem *aNewItem,
                           QTreeWidgetItem *aOldItem = 0);
    void onContextMenuRequested (const QPoint &aPoint);
    void onItemChanged (QTreeWidgetItem *aItem, int aColumn);

    void discardSnapshot();
    void takeSnapshot();
    void discardCurState();
    void discardCurSnapAndState();
    void showSnapshotDetails();

    void machineDataChanged (const VBoxMachineDataChangeEvent &aE);
    void machineStateChanged (const VBoxMachineStateChangeEvent &aE);
    void sessionStateChanged (const VBoxSessionStateChangeEvent &aE);
    void snapshotChanged (const VBoxSnapshotEvent &aE);

private:

    void refreshAll (bool aKeepSelected = true);
    SnapshotWgtItem* findItem (const QUuid &aSnapshotId);
    SnapshotWgtItem* curStateItem();
    void populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *aItem);

    CMachine         mMachine;
    QUuid            mMachineId;
    KSessionState    mSessionState;
    SnapshotWgtItem *mCurSnapshotItem;
    QMenu           *mContextMenu;
    bool             mContextMenuDirty;
    bool             mEditProtector;

    QActionGroup    *mSnapshotActionGroup;
    QActionGroup    *mCurStateActionGroup;

    QAction         *mDiscardSnapshotAction;
    QAction         *mTakeSnapshotAction;
    QAction         *mRevertToCurSnapAction;
    QAction         *mDiscardCurSnapAndStateAction;
    QAction         *mShowSnapshotDetailsAction;
};

#endif // __VBoxSnapshotsWgt_h__

