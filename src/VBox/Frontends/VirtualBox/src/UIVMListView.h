/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMItem, UIVMItemModel, UIVMListView, UIVMItemPainter class declarations
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMListView_h__
#define __UIVMListView_h__

#include "VBoxGlobal.h"
#include "QIListView.h"

/* Qt includes */
#include <QAbstractListModel>
#include <QDateTime>

class VBoxSelectorWnd;

class UIVMItem
{
public:

    UIVMItem(const CMachine &aMachine);
    virtual ~UIVMItem();

    CMachine machine() const { return m_machine; }

    QString name() const { return m_strName; }
    QIcon osIcon() const { return m_fAccessible ? vboxGlobal().vmGuestOSTypeIcon(m_strOSTypeId) : QPixmap(":/os_other.png"); }
    QString id() const { return m_strId; }

    QString sessionStateName() const;
    QIcon sessionStateIcon() const { return m_fAccessible ? vboxGlobal().toIcon(m_state) : QPixmap(":/state_aborted_16px.png"); }

    QString snapshotName() const { return m_strSnapshotName; }
    ULONG snapshotCount() const { return m_cSnaphot; }

    QString toolTipText() const;

    bool accessible() const { return m_fAccessible; }
    const CVirtualBoxErrorInfo &accessError() const { return m_accessError; }
    KMachineState state() const { return m_state; }
    KSessionState sessionState() const { return m_sessionState; }

    bool recache();

    bool canSwitchTo() const;
    bool switchTo();

private:

    /* Private member vars */
    CMachine m_machine;

    /* Cached machine data (to minimize server requests) */
    QString m_strId;
    QString m_strSettingsFile;

    bool m_fAccessible;
    CVirtualBoxErrorInfo m_accessError;

    QString m_strName;
    QString m_strSnapshotName;
    KMachineState m_state;
    QDateTime m_lastStateChange;
    KSessionState m_sessionState;
    QString m_strOSTypeId;
    ULONG m_cSnaphot;

    ULONG m_pid;
};

/* Make the pointer of this class public to the QVariant framework */
Q_DECLARE_METATYPE(UIVMItem *);

class UIVMItemModel: public QAbstractListModel
{
    Q_OBJECT;

public:
    enum { SnapShotDisplayRole = Qt::UserRole,
           SnapShotFontRole,
           SessionStateDisplayRole,
           SessionStateDecorationRole,
           SessionStateFontRole,
           UIVMItemPtrRole };

    UIVMItemModel(QObject *aParent = 0)
        :QAbstractListModel(aParent) {}

    void addItem(UIVMItem *aItem);
    void removeItem(UIVMItem *aItem);
    void refreshItem(UIVMItem *aItem);

    void itemChanged(UIVMItem *aItem);

    void clear();

    UIVMItem *itemById(const QString &aId) const;
    UIVMItem *itemByRow(int aRow) const;
    QModelIndex indexById(const QString &aId) const;

    int rowById(const QString &aId) const;;

    void sort(Qt::SortOrder aOrder = Qt::AscendingOrder) { sort(0, aOrder); }

    /* The following are necessary model implementations */
    void sort(int aColumn, Qt::SortOrder aOrder = Qt::AscendingOrder);

    int rowCount(const QModelIndex &aParent = QModelIndex()) const;

    QVariant data(const QModelIndex &aIndex, int aRole) const;
    QVariant headerData(int aSection, Qt::Orientation aOrientation,
                        int aRole = Qt::DisplayRole) const;

    bool removeRows(int aRow, int aCount, const QModelIndex &aParent = QModelIndex());

private:
    static bool UIVMItemNameCompareLessThan(UIVMItem* aItem1, UIVMItem* aItem2);
    static bool UIVMItemNameCompareGreaterThan(UIVMItem* aItem1, UIVMItem* aItem2);

    /* Private member vars */
    QList<UIVMItem *> m_VMItemList;
};

class UIVMListView: public QIListView
{
    Q_OBJECT;

public:
    UIVMListView(QWidget *aParent = 0);

    void selectItemByRow(int row);
    void selectItemById(const QString &aID);
    void ensureSomeRowSelected(int aRowHint);
    UIVMItem * selectedItem() const;

    void ensureCurrentVisible();

signals:
    void currentChanged();
    void activated();

protected slots:
    void selectionChanged(const QItemSelection &aSelected, const QItemSelection &aDeselected);
    void currentChanged(const QModelIndex &aCurrent, const QModelIndex &aPrevious);
    void dataChanged(const QModelIndex &aTopLeft, const QModelIndex &aBottomRight);

protected:
    bool selectCurrent();
};

class UIVMItemPainter: public QIItemDelegate
{
public:
    UIVMItemPainter(QObject *aParent = 0)
      : QIItemDelegate(aParent), m_Margin(8), m_Spacing(m_Margin * 3 / 2) {}

    QSize sizeHint(const QStyleOptionViewItem &aOption,
                   const QModelIndex &aIndex) const;

    void paint(QPainter *aPainter, const QStyleOptionViewItem &aOption,
               const QModelIndex &aIndex) const;

private:
    void paintContent(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void blendContent(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    inline QFontMetrics fontMetric(const QModelIndex &aIndex, int aRole) const { return QFontMetrics(aIndex.data(aRole).value<QFont>()); }
    inline QIcon::Mode iconMode(QStyle::State aState) const
    {
        if (!(aState & QStyle::State_Enabled))
            return QIcon::Disabled;
        if (aState & QStyle::State_Selected)
            return QIcon::Selected;
        return QIcon::Normal;
    }
    inline QIcon::State iconState(QStyle::State aState) const { return aState & QStyle::State_Open ? QIcon::On : QIcon::Off; }

    QRect rect(const QStyleOptionViewItem &aOption,
               const QModelIndex &aIndex, int aRole) const;

    void calcLayout(const QModelIndex &aIndex,
                    QRect *aOSType, QRect *aVMName, QRect *aShot,
                    QRect *aStateIcon, QRect *aState) const;

    /* Private member vars */
    int m_Margin;
    int m_Spacing;
};

#endif /* __UIVMListView_h__ */

