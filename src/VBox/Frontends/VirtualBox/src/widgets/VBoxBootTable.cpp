/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxBootTable class implementation
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
#include <QHeaderView>
#include <QKeyEvent>

/* Local includes */
#include "VBoxBootTable.h"
#include "VBoxGlobal.h"

VBoxBootTable::VBoxBootTable(QWidget *pParent)
    : QTreeWidget(pParent)
{
    header()->hide();
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(onItemChanged()));
}

void VBoxBootTable::onItemChanged()
{
    emit itemToggled();
}

void VBoxBootTable::keyPressEvent(QKeyEvent *pEvent)
{
    if (pEvent->QInputEvent::modifiers() == Qt::ControlModifier)
    {
        switch (pEvent->key())
        {
            case Qt::Key_Up:
                emit moveItemUp();
                return;
            case Qt::Key_Down:
                emit moveItemDown();
                return;
            default:
                break;
        }
    }
    QTreeWidget::keyPressEvent(pEvent);
}

