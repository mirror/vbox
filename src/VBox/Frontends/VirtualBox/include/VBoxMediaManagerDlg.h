/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxMediaManagerDlg class declaration
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

#ifndef __VBoxMediaManagerDlg_h__
#define __VBoxMediaManagerDlg_h__

#include "VBoxMediaManagerDlg.gen.h"
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"
#include "VBoxDefs.h"
#include "VBoxMediaComboBox.h"

class MediaItem;
class VBoxToolBar;
class VBoxProgressBar;

class VBoxMediaManagerDlg : public QIWithRetranslateUI2<QIMainDialog>,
                            public Ui::VBoxMediaManagerDlg
{
    Q_OBJECT;

    enum TabIndex { HDTab = 0, CDTab, FDTab };
    enum ItemAction { ItemAction_Added, ItemAction_Updated, ItemAction_Removed };
    enum Action { Action_Select, Action_Edit, Action_Remove, Action_Release };

public:

    VBoxMediaManagerDlg (QWidget *aParent = NULL,
                         Qt::WindowFlags aFlags = Qt::Dialog);

    void setup (VBoxDefs::MediaType aType, bool aDoSelect,
                bool aRefresh = true,
                const CMachine &aSessionMachine = CMachine(),
                const QString &aSelectId = QString::null,
                bool aShowDiffs = true);

    static void showModeless (QWidget *aParent = NULL, bool aRefresh = true);

    QString selectedId() const;
    QString selectedLocation() const;

    bool showDiffs() const { return mShowDiffs; };
    bool inAttachMode() const { return !mSessionMachine.isNull(); };

public slots:

    void refreshAll();

protected:

    void retranslateUi();
    virtual void closeEvent (QCloseEvent *aEvent);
    virtual bool eventFilter (QObject *aObject, QEvent *aEvent);

private slots:

    void mediumAdded (const VBoxMedium &aMedium);
    void mediumUpdated (const VBoxMedium &aMedium);
    void mediumRemoved (VBoxDefs::MediaType aType, const QString &aId);

    void mediumEnumStarted();
    void mediumEnumerated (const VBoxMedium &aMedium);
    void mediumEnumFinished (const VBoxMediaList &aList);

    void doNewMedium();
    void doAddMedium();
    void doRemoveMedium();
    void doReleaseMedium();

    bool releaseMediumFrom (const VBoxMedium &aMedium, const QString &aMachineId);

    void processCurrentChanged (int index = -1);
    void processCurrentChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem = 0);
    void processDoubleClick (QTreeWidgetItem *aItem, int aColumn);
    void showContextMenu (const QPoint &aPos);

    void machineStateChanged (const VBoxMachineStateChangeEvent &aEvent);

    void makeRequestForAdjustTable();
    void performTablesAdjustment();

private:

    QTreeWidget* treeWidget (VBoxDefs::MediaType aType) const;
    VBoxDefs::MediaType currentTreeWidgetType() const;
    QTreeWidget* currentTreeWidget() const;

    QTreeWidgetItem* selectedItem (const QTreeWidget *aTree) const;
    MediaItem* toMediaItem (QTreeWidgetItem *aItem) const;

    void setCurrentItem (QTreeWidget *aTree, QTreeWidgetItem *aItem);

    void addMediumToList (const QString &aLocation, VBoxDefs::MediaType aType);

    MediaItem* createHardDiskItem (QTreeWidget *aTree, const VBoxMedium &aMedium) const;

    void updateTabIcons (MediaItem *aItem, ItemAction aAction);

    MediaItem* searchItem (QTreeWidget *aTree, const QString &aId) const;

    bool checkMediumFor (MediaItem *aItem, Action aAction);

    bool checkDndUrls (const QList<QUrl> &aUrls) const;
    void addDndUrls (const QList<QUrl> &aUrls);

    void clearInfoPanes();
    void prepareToRefresh (int aTotal = 0);

    QString formatPaneText (const QString &aText, bool aCompact = true, const QString &aElipsis = "middle");

    /* Private member vars */
    /* Window status */
    bool mDoSelect;
    static VBoxMediaManagerDlg *mModelessDialog;
    VBoxProgressBar *mProgressBar;

    /* The global VirtualBox instance */
    CVirtualBox mVBox;

    /* Type if we are in the select modus */
    int mType;

    bool mShowDiffs : 1;
    bool mSetupMode : 1;

    /* Icon definitions */
    QIcon mHardDiskIcon;
    QIcon mDVDImageIcon;
    QIcon mFloppyImageIcon;

    /* Menu & Toolbar */
    QMenu       *mActionsContextMenu;
    QMenu       *mActionsMenu;
    VBoxToolBar *mActionsToolBar;
    QAction     *mNewAction;
    QAction     *mAddAction;
    QAction     *mEditAction;
    QAction     *mRemoveAction;
    QAction     *mReleaseAction;
    QAction     *mRefreshAction;

    /* Machine */
    CMachine mSessionMachine;
    QString  mSessionMachineId;
    bool mHardDisksInaccessible;
    bool mDVDImagesInaccessible;
    bool mFloppyImagesInaccessible;
    QString mHDSelectedId;
    QString mDVDSelectedId;
    QString mFloppySelectedId;
};

#endif /* __VBoxMediaManagerDlg_h__ */

