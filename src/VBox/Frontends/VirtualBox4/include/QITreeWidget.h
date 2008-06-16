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

#ifndef __QITreeWidget_h__
#define __QITreeWidget_h__

/* Qt includes */
#include <QTreeWidget>

class QITreeWidget: public QTreeWidget
{
    Q_OBJECT

public:

    QITreeWidget (QWidget *aParent = 0);

    void setSupportedDropActions (Qt::DropActions aAction);

signals:

    void itemRightClicked (const QPoint &aPos, QTreeWidgetItem *aItem, int aColumn);

protected:

    void mousePressEvent (QMouseEvent *aEvent);
    virtual Qt::DropActions supportedDropActions () const;

    /* Protected member vars */
    Qt::DropActions mSupportedDropActions;
};

#endif /* __QITreeWidget_h__ */

