/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootTable class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIBootTable_h___
#define ___UIBootTable_h___

/* Qt includes: */
#include <QListWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"


/** QListWidgetItem extension for our UIBootTable. */
class SHARED_LIBRARY_STUFF UIBootTableItem : public QListWidgetItem
{
public:

    /** Constructs boot-table item of passed @a enmType. */
    UIBootTableItem(KDeviceType enmType);

    /** Returns the item type. */
    KDeviceType type() const;

    /** Performs item translation. */
    void retranslateUi();

private:

    /** Holds the item type. */
    KDeviceType m_enmType;

};


/** QListWidget subclass used as system settings boot-table. */
class SHARED_LIBRARY_STUFF UIBootTable : public QIWithRetranslateUI<QListWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about current table row changed.
      * @note  Same as base-class currentRowChanged but in wider cases. */
    void sigRowChanged(int iRow);

public:

    /** Constructs boot-table passing @a pParent to the base-class. */
    UIBootTable(QWidget *pParent = 0);

    /** Adjusts table size to fit contents. */
    void adjustSizeToFitContent();

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

    /** Moves item with passed @a index to specified @a iRow. */
    QModelIndex moveItemTo(const QModelIndex &index, int iRow);
};


#endif /* !___UIBootTable_h___ */
