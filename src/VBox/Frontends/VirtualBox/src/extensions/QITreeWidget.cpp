/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QITreeWidget class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
# include <QPainter>
# include <QResizeEvent>

/* GUI includes: */
# include "QITreeWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class QITreeWidgetItem implementation.                                                                                       *
*********************************************************************************************************************************/

/* static */
QITreeWidgetItem *QITreeWidgetItem::toItem(QTreeWidgetItem *pItem)
{
    /* Make sure alive QITreeWidgetItem passed: */
    if (!pItem || pItem->type() != QITreeWidgetItem::ItemType)
        return 0;

    /* Return casted QITreeWidgetItem: */
    return static_cast<QITreeWidgetItem*>(pItem);
}

/* static */
const QITreeWidgetItem *QITreeWidgetItem::toItem(const QTreeWidgetItem *pItem)
{
    /* Make sure alive QITreeWidgetItem passed: */
    if (!pItem || pItem->type() != QITreeWidgetItem::ItemType)
        return 0;

    /* Return casted QITreeWidgetItem: */
    return static_cast<const QITreeWidgetItem*>(pItem);
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidget *pTreeWidget)
    : QTreeWidgetItem(pTreeWidget, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidgetItem *pTreeWidgetItem)
    : QTreeWidgetItem(pTreeWidgetItem, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidget *pTreeWidget, const QStringList &strings)
    : QTreeWidgetItem(pTreeWidget, strings, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidgetItem *pTreeWidgetItem, const QStringList &strings)
    : QTreeWidgetItem(pTreeWidgetItem, strings, ItemType)
{
}

QITreeWidget *QITreeWidgetItem::parentTree() const
{
    /* Return the parent tree if any: */
    return treeWidget() ? qobject_cast<QITreeWidget*>(treeWidget()) : 0;
}

QITreeWidgetItem *QITreeWidgetItem::parentItem() const
{
    /* Return the parent item if any: */
    return QTreeWidgetItem::parent() ? toItem(QTreeWidgetItem::parent()) : 0;
}

QITreeWidgetItem *QITreeWidgetItem::childItem(int iIndex) const
{
    /* Return the child item with iIndex if any: */
    return QTreeWidgetItem::child(iIndex) ? toItem(QTreeWidgetItem::child(iIndex)) : 0;
}


/*********************************************************************************************************************************
*   Class QITreeWidget implementation.                                                                                           *
*********************************************************************************************************************************/

QITreeWidget::QITreeWidget(QWidget *pParent)
    : QTreeWidget(pParent)
{
}

void QITreeWidget::setSizeHintForItems(const QSize &sizeHint)
{
    /* Pass the sizeHint to all the top-level items: */
    for (int i = 0; i < topLevelItemCount(); ++i)
        topLevelItem(i)->setSizeHint(0, sizeHint);
}

int QITreeWidget::childCount() const
{
    /* Return the number of children: */
    return invisibleRootItem()->childCount();
}

QITreeWidgetItem *QITreeWidget::childItem(int iIndex) const
{
    /* Return the child item with iIndex if any: */
    return invisibleRootItem()->child(iIndex) ? QITreeWidgetItem::toItem(invisibleRootItem()->child(iIndex)) : 0;
}

void QITreeWidget::paintEvent(QPaintEvent *pEvent)
{
    /* Create item painter: */
    QPainter painter;
    painter.begin(viewport());

    /* Notify listeners about painting: */
    QTreeWidgetItemIterator it(this);
    while (*it)
    {
        emit painted(*it, &painter);
        ++it;
    }

    /* Close item painter: */
    painter.end();

    /* Call to base-class: */
    QTreeWidget::paintEvent(pEvent);
}

void QITreeWidget::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QTreeWidget::resizeEvent(pEvent);

    /* Notify listeners about resizing: */
    emit resized(pEvent->size(), pEvent->oldSize());
}

