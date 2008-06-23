/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QITreeWidget class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "QITreeWidget.h"

#include <QPainter>

QITreeWidget::QITreeWidget (QWidget *aParent)
    : QTreeWidget (aParent)
{
}

void QITreeWidget::setSupportedDropActions (Qt::DropActions aAction)
{
    mSupportedDropActions = aAction;
}

Qt::DropActions QITreeWidget::supportedDropActions() const
{
    return mSupportedDropActions;
}

void QITreeWidget::paintEvent (QPaintEvent *aEvent)
{
    /* Painter for items */
    QPainter painter (viewport());

    /* Here we let the items make some painting inside the viewport. */
    QTreeWidgetItemIterator it (this);
    while (*it)
    {
        switch ((*it)->type())
        {
            case ComplexItemType:
            {
                /* Let the ComplexItem paint itself */
                ComplexTreeWidgetItem *i = static_cast<ComplexTreeWidgetItem*> (*it);
                i->paintItem (&painter);
                break;
            }
            case BasicItemType:
            {
                /* Do nothing for BasicItem */
                break;
            }
            default:
            {
                /* Wrong item is used */
                break;
            }
        }
        ++ it;
    }

    QTreeWidget::paintEvent (aEvent);
}

