/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootTable class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QScrollBar>

/* GUI includes: */
# include "UIBootTable.h"
# include "UIConverter.h"
# include "UIIconPool.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIBootTableItem implementation.                                                                                        *
*********************************************************************************************************************************/

UIBootTableItem::UIBootTableItem(KDeviceType enmType)
    : m_enmType(enmType)
{
    setCheckState(Qt::Unchecked);
    switch(enmType)
    {
        case KDeviceType_Floppy:   setIcon(UIIconPool::iconSet(":/fd_16px.png")); break;
        case KDeviceType_DVD:      setIcon(UIIconPool::iconSet(":/cd_16px.png")); break;
        case KDeviceType_HardDisk: setIcon(UIIconPool::iconSet(":/hd_16px.png")); break;
        case KDeviceType_Network:  setIcon(UIIconPool::iconSet(":/nw_16px.png")); break;
        default: break; /* Shut up, MSC! */
    }
    retranslateUi();
}

KDeviceType UIBootTableItem::type() const
{
    return m_enmType;
}

void UIBootTableItem::retranslateUi()
{
    setText(gpConverter->toString(m_enmType));
}


/*********************************************************************************************************************************
*   Class UIBootTable implementation.                                                                                            *
*********************************************************************************************************************************/

UIBootTable::UIBootTable(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QListWidget>(pParent)
{
    prepare();
}

void UIBootTable::adjustSizeToFitContent()
{
    const int iH = 2 * frameWidth();
    const int iW = iH;
    setFixedSize(sizeHintForColumn(0) + iW,
                 sizeHintForRow(0) * count() + iH);
}

void UIBootTable::sltMoveItemUp()
{
    QModelIndex index = currentIndex();
    moveItemTo(index, index.row() - 1);
}

void UIBootTable::sltMoveItemDown()
{
    QModelIndex index = currentIndex();
    moveItemTo(index, index.row() + 2);
}

void UIBootTable::retranslateUi()
{
    for (int i = 0; i < count(); ++i)
        static_cast<UIBootTableItem*>(item(i))->retranslateUi();

    adjustSizeToFitContent();
}

void UIBootTable::dropEvent(QDropEvent *pEvent)
{
    /* Call to base-class: */
    QListWidget::dropEvent(pEvent);
    /* Separately notify listeners: */
    emit sigRowChanged(currentRow());
}

QModelIndex UIBootTable::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers fModifiers)
{
    if (fModifiers.testFlag(Qt::ControlModifier))
    {
        switch (cursorAction)
        {
            case QAbstractItemView::MoveUp:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, index.row() - 1);
            }
            case QAbstractItemView::MoveDown:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, index.row() + 2);
            }
            case QAbstractItemView::MovePageUp:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, qMax(0, index.row() - verticalScrollBar()->pageStep()));
            }
            case QAbstractItemView::MovePageDown:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, qMin(model()->rowCount(), index.row() + verticalScrollBar()->pageStep() + 1));
            }
            case QAbstractItemView::MoveHome:
                return moveItemTo(currentIndex(), 0);
            case QAbstractItemView::MoveEnd:
                return moveItemTo(currentIndex(), model()->rowCount());
            default:
                break;
        }
    }
    return QListWidget::moveCursor(cursorAction, fModifiers);
}

void UIBootTable::prepare()
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDropIndicatorShown(true);
    setUniformItemSizes(true);
    connect(this, &UIBootTable::currentRowChanged,
            this, &UIBootTable::sigRowChanged);
}

QModelIndex UIBootTable::moveItemTo(const QModelIndex &index, int row)
{
    /* Check validity: */
    if (!index.isValid())
        return QModelIndex();

    /* Check sanity: */
    if (row < 0 || row > model()->rowCount())
        return QModelIndex();

    QPersistentModelIndex oldIndex(index);
    UIBootTableItem *pItem = static_cast<UIBootTableItem*>(itemFromIndex(oldIndex));
    insertItem(row, new UIBootTableItem(*pItem));
    QPersistentModelIndex newIndex = model()->index(row, 0);
    delete takeItem(oldIndex.row());
    setCurrentRow(newIndex.row());
    return QModelIndex(newIndex);
}
