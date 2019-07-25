/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootListWidget class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIBootListWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIBootListWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QListWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QLabel;
class UIToolBar;


/** Boot item data structure. */
struct UIBootItemData
{
    /** Constructs boot item data. */
    UIBootItemData()
        : m_enmType(KDeviceType_Null)
        , m_fEnabled(false)
    {}

    /** Returns whether @a another passed data is equal to this one. */
    bool operator==(const UIBootItemData &another) const
    {
        return true
               && (m_enmType == another.m_enmType)
               && (m_fEnabled == another.m_fEnabled)
               ;
    }

    /** Holds the boot device type. */
    KDeviceType  m_enmType;
    /** Holds whether the boot device enabled. */
    bool         m_fEnabled;
};
typedef QList<UIBootItemData> UIBootItemDataList;


/** QListWidgetItem extension for our UIBootListWidget. */
class SHARED_LIBRARY_STUFF UIBootListWidgetItem : public QListWidgetItem
{
public:

    /** Constructs boot-table item of passed @a enmType. */
    UIBootListWidgetItem(KDeviceType enmType);

    /** Returns the item type. */
    KDeviceType type() const;

    /** Performs item translation. */
    void retranslateUi();

private:

    /** Holds the item type. */
    KDeviceType m_enmType;
};


/** QListWidget subclass used as system settings boot-table. */
class SHARED_LIBRARY_STUFF UIBootListWidget : public QIWithRetranslateUI<QListWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about current table row changed.
      * @note  Same as base-class currentRowChanged but in wider cases. */
    void sigRowChanged(int iRow);

public:

    /** Constructs boot-table passing @a pParent to the base-class. */
    UIBootListWidget(QWidget *pParent = 0);

    /** Defines @a bootItems list. */
    void setBootItems(const UIBootItemDataList &bootItems);
    /** Returns boot item list. */
    UIBootItemDataList bootItems() const;

public slots:

    /** Moves current item up. */
    void sltMoveItemUp();
    /** Moves current item down. */
    void sltMoveItemDown();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles drop @a pEvent. */
    virtual void dropEvent(QDropEvent *pEvent) /* override */;

    /** Returns a QModelIndex object pointing to the next object in the view,
      * based on the given @a cursorAction and keyboard @a fModifiers. */
    virtual QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction,
                                   Qt::KeyboardModifiers fModifiers) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts table size to fit contents. */
    void adjustSizeToFitContent();

    /** Moves item with passed @a index to specified @a iRow. */
    QModelIndex moveItemTo(const QModelIndex &index, int iRow);
};


/** QWidget subclass used as boot order editor. */
class SHARED_LIBRARY_STUFF UIBootOrderEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs boot order editor passing @a pParent to the base-class.
      * @param  fWithLabel  Brings whether we should add label ourselves. */
    UIBootOrderEditor(QWidget *pParent = 0, bool fWithLabel = false);

    /** Defines editor @a guiValue. */
    void setValue(const UIBootItemDataList &guiValue);
    /** Returns editor value. */
    UIBootItemDataList value() const;

protected:

    /** Preprocesses Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles current boot item change. */
    void sltHandleCurrentBootItemChange();

private:

    /** Prepares all. */
    void prepare();

    /** Updates action availability: */
    void updateActionAvailability();

    /** Holds whether descriptive label should be created. */
    bool  m_fWithLabel;

    /** Holds the label instance. */
    QLabel           *m_pLabel;
    /** Holds the table instance. */
    UIBootListWidget *m_pTable;
    /** Holds the toolbar instance. */
    UIToolBar        *m_pToolbar;
    /** Holds the move up action. */
    QAction          *m_pMoveUp;
    /** Holds the move down action. */
    QAction          *m_pMoveDown;
};


#endif /* !FEQT_INCLUDED_SRC_widgets_UIBootListWidget_h */
