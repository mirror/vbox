/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxBootTable class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef __VBoxBootTable_h__
#define __VBoxBootTable_h__

/* Global includes */
#include <QTreeWidget>

/**
 *  QTreeWidget class reimplementation to use as boot items table.
 *  It has one unsorted column without header.
 *  Keymapping handlers for ctrl-up & ctrl-down are translated into
 *  boot-items up/down moving signals.
 *  Emits itemToggled() signal when the item changed.
 */
class VBoxBootTable : public QTreeWidget
{
    Q_OBJECT;

public:

    VBoxBootTable(QWidget *pParent);

signals:

    void moveItemUp();
    void moveItemDown();
    void itemToggled();

protected:

    void keyPressEvent(QKeyEvent *pEvent);

private slots:

    void onItemChanged();
};

#endif // __VBoxBootTable_h__

