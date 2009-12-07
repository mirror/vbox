/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxVMItem, VBoxVMModel, VBoxVMListView, VBoxVMItemPainter class declarations
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

#ifndef __VBoxVMListView_h__
#define __VBoxVMListView_h__

#include "VBoxGlobal.h"
#include "QIListView.h"

/* Qt includes */
#include <QAbstractListModel>
#include <QDateTime>

class VBoxSelectorWnd;

class VBoxVMItem
{
public:

    VBoxVMItem (const CMachine &aMachine);
    virtual ~VBoxVMItem();

    CMachine machine() const { return mMachine; }

    QString name() const { return mName; }
    QIcon osIcon() const { return mAccessible ? vboxGlobal().vmGuestOSTypeIcon (mOSTypeId) : QPixmap (":/os_other.png"); }
    QString id() const { return mId; }

    QString sessionStateName() const;
    QIcon sessionStateIcon() const { return mAccessible ? vboxGlobal().toIcon (mState) : QPixmap (":/state_aborted_16px.png"); }

    QString snapshotName() const { return mSnapshotName; }
    ULONG snapshotCount() const { return mSnapshotCount; }

    QString toolTipText() const;

    bool accessible() const { return mAccessible; }
    const CVirtualBoxErrorInfo &accessError() const { return mAccessError; }
    KMachineState state() const { return mState; }
    KSessionState sessionState() const { return mSessionState; }

    bool recache();

    bool canSwitchTo() const;
    bool switchTo();

private:

    /* Private member vars */
    CMachine mMachine;

    /* Cached machine data (to minimize server requests) */
    QString mId;
    QString mSettingsFile;

    bool mAccessible;
    CVirtualBoxErrorInfo mAccessError;

    QString mName;
    QString mSnapshotName;
    KMachineState mState;
    QDateTime mLastStateChange;
    KSessionState mSessionState;
    QString mOSTypeId;
    ULONG mSnapshotCount;

    ULONG mPid;
};

/* Make the pointer of this class public to the QVariant framework */
Q_DECLARE_METATYPE(VBoxVMItem *);

class VBoxVMModel: public QAbstractListModel
{
    Q_OBJECT;

public:
    enum { SnapShotDisplayRole = Qt::UserRole,
           SnapShotFontRole,
           SessionStateDisplayRole,
           SessionStateDecorationRole,
           SessionStateFontRole,
           VBoxVMItemPtrRole };

    VBoxVMModel(QObject *aParent = 0)
        :QAbstractListModel (aParent) {}

    void addItem (VBoxVMItem *aItem);
    void removeItem (VBoxVMItem *aItem);
    void refreshItem (VBoxVMItem *aItem);

    void itemChanged (VBoxVMItem *aItem);

    void clear();

    VBoxVMItem *itemById (const QString &aId) const;
    VBoxVMItem *itemByRow (int aRow) const;
    QModelIndex indexById (const QString &aId) const;

    int rowById (const QString &aId) const;;

    void sort (Qt::SortOrder aOrder = Qt::AscendingOrder) { sort (0, aOrder); }

    /* The following are necessary model implementations */
    void sort (int aColumn, Qt::SortOrder aOrder = Qt::AscendingOrder);

    int rowCount (const QModelIndex &aParent = QModelIndex()) const;

    QVariant data (const QModelIndex &aIndex, int aRole) const;
    QVariant headerData (int aSection, Qt::Orientation aOrientation,
                         int aRole = Qt::DisplayRole) const;

    bool removeRows (int aRow, int aCount, const QModelIndex &aParent = QModelIndex());

private:
    static bool VBoxVMItemNameCompareLessThan (VBoxVMItem* aItem1, VBoxVMItem* aItem2);
    static bool VBoxVMItemNameCompareGreaterThan (VBoxVMItem* aItem1, VBoxVMItem* aItem2);

    /* Private member vars */
    QList<VBoxVMItem *> mVMItemList;
};

class VBoxVMListView: public QIListView
{
    Q_OBJECT;

public:
    VBoxVMListView (QWidget *aParent = 0);

    void selectItemByRow (int row);
    void selectItemById (const QString &aID);
    void ensureSomeRowSelected (int aRowHint);
    VBoxVMItem * selectedItem() const;

    void ensureCurrentVisible();

signals:
    void currentChanged();
    void activated();

protected slots:
    void selectionChanged (const QItemSelection &aSelected, const QItemSelection &aDeselected);
    void currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious);
    void dataChanged (const QModelIndex &aTopLeft, const QModelIndex &aBottomRight);

protected:
    bool selectCurrent();
};

class VBoxVMItemPainter: public QIItemDelegate
{
public:
    VBoxVMItemPainter (QObject *aParent = 0)
      : QIItemDelegate (aParent), mMargin (8), mSpacing (mMargin * 3 / 2) {}

    QSize sizeHint (const QStyleOptionViewItem &aOption,
                    const QModelIndex &aIndex) const;

    void paint (QPainter *aPainter, const QStyleOptionViewItem &aOption,
                const QModelIndex &aIndex) const;

private:
    inline QFontMetrics fontMetric (const QModelIndex &aIndex, int aRole) const { return QFontMetrics (aIndex.data (aRole).value<QFont>()); }
    inline QIcon::Mode iconMode (QStyle::State aState) const
    {
        if (!(aState & QStyle::State_Enabled))
            return QIcon::Disabled;
        if (aState & QStyle::State_Selected)
            return QIcon::Selected;
        return QIcon::Normal;
    }
    inline QIcon::State iconState (QStyle::State aState) const { return aState & QStyle::State_Open ? QIcon::On : QIcon::Off; }

    QRect rect (const QStyleOptionViewItem &aOption,
                const QModelIndex &aIndex, int aRole) const;

    void calcLayout (const QModelIndex &aIndex,
                     QRect *aOSType, QRect *aVMName, QRect *aShot,
                     QRect *aStateIcon, QRect *aState) const;

    /* Private member vars */
    int mMargin;
    int mSpacing;
};

#endif /* __VBoxVMListView_h__ */
