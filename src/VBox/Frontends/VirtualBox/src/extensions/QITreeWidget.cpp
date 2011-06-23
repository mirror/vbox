/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QITreeWidget class implementation
 */

/*
 * Copyright (C) 2008-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QPainter>
#include <QResizeEvent>

/* Local includes */
#include "QITreeWidget.h"

QITreeWidget::QITreeWidget (QWidget *aParent)
    : QTreeWidget (aParent)
{
}

void QITreeWidget::setSizeHintForItems(const QSize &sizeHint)
{
    for (int i = 0; i < topLevelItemCount(); ++i)
        topLevelItem(i)->setSizeHint(0, sizeHint);
}

void QITreeWidget::paintEvent (QPaintEvent *aEvent)
{
    /* Opens Items Painter */
    QPainter painter;
    painter.begin (viewport());

    /* Notify connected objects about painting */
    QTreeWidgetItemIterator it (this);
    while (*it)
    {
        emit painted (*it, &painter);
        ++ it;
    }

    /* Close Items Painter */
    painter.end();

    /* Base-class paint-event */
    QTreeWidget::paintEvent (aEvent);
}

void QITreeWidget::resizeEvent (QResizeEvent *aEvent)
{
    /* Base-class resize-event */
    QTreeWidget::resizeEvent (aEvent);

    /* Notify connected objects about resize */
    emit resized (aEvent->size(), aEvent->oldSize());
}

