/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIBootTable class implementation
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global include */
#include <QScrollBar>

/* Local includes */
#include "UIBootTable.h"
#include "UIConverter.h"

UIBootTableItem::UIBootTableItem(KDeviceType type)
  : m_type(type)
{
    setCheckState(Qt::Unchecked);
    switch(type)
    {
        case KDeviceType_Floppy:
        {
            setIcon(QIcon(":fd_16px.png"));
            break;
        }
        case KDeviceType_DVD:
        {
            setIcon(QIcon(":cd_16px.png"));
            break;
        }
        case KDeviceType_HardDisk:
        {
            setIcon(QIcon(":hd_16px.png"));
            break;
        }
        case KDeviceType_Network:
        {
            setIcon(QIcon(":nw_16px.png"));
            break;
        }
    }
    retranslateUi();
}

KDeviceType UIBootTableItem::type() const
{
    return m_type;
}

void UIBootTableItem::retranslateUi()
{
    setText(gpConverter->toString(m_type));
}

UIBootTable::UIBootTable(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QListWidget>(pParent)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDropIndicatorShown(true);
    setUniformItemSizes(true);
    connect(this, SIGNAL(currentRowChanged(int)),
            this, SIGNAL(sigRowChanged(int)));
}

void UIBootTable::adjustSizeToFitContent()
{
    int h = 2 * frameWidth();
    int w = h;
#if QT_VERSION < 0x040700
# ifdef Q_WS_MAC
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    h += top + bottom;
    w += left + right;
# else /* Q_WS_MAC */
    w += 4;
# endif /* !Q_WS_MAC */
#endif /* QT_VERSION < 0x040700 */
    setFixedSize(sizeHintForColumn(0) + w,
                 sizeHintForRow(0) * count() + h);
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
    QListWidget::dropEvent(pEvent);
    emit sigRowChanged(currentRow());
}

QModelIndex UIBootTable::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    if (modifiers.testFlag(Qt::ControlModifier))
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
    return QListWidget::moveCursor(cursorAction, modifiers);
}

QModelIndex UIBootTable::moveItemTo(const QModelIndex &index, int row)
{
    if (!index.isValid())
        return QModelIndex();

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

