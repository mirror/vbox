/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxDiskImageManagerDlg class declaration
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

#ifndef __VBoxDiskImageManagerDlg_h__
#define __VBoxDiskImageManagerDlg_h__

#include "VBoxDiskImageManagerDlg.gen.h"
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"
#include "VBoxDefs.h"
#include "VBoxMediaComboBox.h"

class DiskImageItem;
class VBoxToolBar;
class VBoxProgressBar;

class VBoxDiskImageManagerDlg : public QIWithRetranslateUI2<QIMainDialog>,
                                public Ui::VBoxDiskImageManagerDlg
{
    Q_OBJECT;

    enum TabIndex { HDTab = 0, CDTab, FDTab };

public:

    VBoxDiskImageManagerDlg (QWidget *aParent = 0,
                             Qt::WindowFlags aFlags = Qt::Dialog);

    void setup (int aType, bool aDoSelect, const QUuid &aTargetVMId = QUuid(),
                bool aRefresh = true, CMachine aMachine = CMachine(),
                const QUuid &aHdId = QUuid(),
                const QUuid &aCdId = QUuid(),
                const QUuid &aFdId = QUuid());

    static void showModeless (QWidget *aParent = NULL, bool aRefresh = true);

    QUuid selectedUuid() const;
    QString selectedPath() const;

    static QString composeHdToolTip (CHardDisk &aHd, VBoxMedia::Status aStatus,
                                     DiskImageItem *aItem = 0);
    static QString composeCdToolTip (CDVDImage &aCd, VBoxMedia::Status aStatus,
                                     DiskImageItem *aItem = 0);
    static QString composeFdToolTip (CFloppyImage &aFd, VBoxMedia::Status aStatus,
                                     DiskImageItem *aItem = 0);

public slots:

    void refreshAll();

protected:

    void retranslateUi();
    virtual void closeEvent (QCloseEvent *aEvent);
    virtual bool eventFilter (QObject *aObject, QEvent *aEvent);

private slots:

    void mediaAdded (const VBoxMedia &aMedia);
    void mediaUpdated (const VBoxMedia &aMedia);
    void mediaRemoved (VBoxDefs::DiskType aType, const QUuid &aId);

    void mediaEnumStarted();
    void mediaEnumerated (const VBoxMedia &aMedia, int aIndex);
    void mediaEnumFinished (const VBoxMediaList &aList);

    void newImage();
    void addImage();
    void removeImage();
    void releaseImage();

    void releaseDisk (const QUuid &aMachineId, const QUuid &aItemId,
                      VBoxDefs::DiskType aDiskType);

    void processCurrentChanged (int index = -1);
    void processCurrentChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem = 0);
    void processDoubleClick (QTreeWidgetItem *aItem, int aColumn);
    void showContextMenu (const QPoint &aPos);

private:

    QTreeWidget* treeWidget (VBoxDefs::DiskType aType) const;
    VBoxDefs::DiskType currentTreeWidgetType() const;
    QTreeWidget* currentTreeWidget() const;

    QTreeWidgetItem* selectedItem (const QTreeWidget *aTree) const;
    DiskImageItem* toDiskImageItem (QTreeWidgetItem *aItem) const;

    void setCurrentItem (QTreeWidget *aTree, QTreeWidgetItem *aItem);

    void addImageToList (const QString &aSource, VBoxDefs::DiskType aDiskType);
    DiskImageItem* createImageNode (QTreeWidget *aTree, DiskImageItem *aRoot,
                                    const VBoxMedia &aMedia) const;

    DiskImageItem* createHdItem (QTreeWidget *aTree, const VBoxMedia &aMedia) const;
    DiskImageItem* createCdItem (QTreeWidget *aTree, const VBoxMedia &aMedia) const;
    DiskImageItem* createFdItem (QTreeWidget *aTree, const VBoxMedia &aMedia) const;

    void updateHdItem (DiskImageItem *aItem, const VBoxMedia &aMedia) const;
    void updateCdItem (DiskImageItem *aItem, const VBoxMedia &aMedia) const;
    void updateFdItem (DiskImageItem *aItem, const VBoxMedia &aMedia) const;

    DiskImageItem* searchItem (QTreeWidget *aTree, const QUuid &aId) const;
    DiskImageItem* searchItem (QTreeWidget *aTree, VBoxMedia::Status aStatus) const;

    bool checkImage (DiskImageItem *aItem);

    bool checkDndUrls (const QList<QUrl> &aUrls) const;
    void addDndUrls (const QList<QUrl> &aUrls);

    void clearInfoPanes();
    void prepareToRefresh (int aTotal = 0);

    void makeWarningMark (DiskImageItem *aItem, VBoxMedia::Status aStatus,
                          VBoxDefs::DiskType aType) const;

    static QString DVDImageUsage (const QUuid &aId, QString &aSnapshotUsage);
    static QString FloppyImageUsage (const QUuid &aId, QString &aSnapshotUsage);
    static void DVDImageSnapshotUsage (const QUuid &aId, const CSnapshot &aSnapshot, QString &aUsage);
    static void FloppyImageSnapshotUsage (const QUuid &aId, const CSnapshot &aSnapshot, QString &aUsage);

    /* Private member vars */
    /* Window status */
    bool mDoSelect;
    static VBoxDiskImageManagerDlg *mModelessDialog;
    VBoxProgressBar  *mProgressBar;

    /* The global VirtualBox instance */
    CVirtualBox mVBox;

    /* Type if we are in the select modus */
    int mType;

    /* Icon definitions */
    QIcon mIconInaccessible;
    QIcon mIconErroneous;
    QIcon mIconHD;
    QIcon mIconCD;
    QIcon mIconFD;

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
    CMachine mMachine;
    QUuid mTargetVMId;
    QUuid mHdSelectedId;
    QUuid mCdSelectedId;
    QUuid mFdSelectedId;
};

#endif /* __VBoxDiskImageManagerDlg_h__ */

