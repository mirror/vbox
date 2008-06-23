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

#include <QTreeWidget>

/*
 * QTreeWidget class which extends standard QTreeWidget's functionality.
 */
class QITreeWidget: public QTreeWidget
{
    Q_OBJECT;

public:

    /*
     * There are two allowed QTreeWidgetItem types which may be used with
     * QITreeWidget: basic and complex.
     * Complex type used in every place where the particular item have to
     * be separately repainted with it's own content.
     * Basic are used in all other places.
     */
    enum
    {
        BasicItemType   = QTreeWidgetItem::UserType + 1,
        ComplexItemType = QTreeWidgetItem::UserType + 2
    };

    QITreeWidget (QWidget *aParent = 0);

    void setSupportedDropActions (Qt::DropActions aAction);

protected:

    virtual Qt::DropActions supportedDropActions () const;

    void paintEvent (QPaintEvent *);

    /* Protected member vars */
    Qt::DropActions mSupportedDropActions;
};

/*
 * Interface for more complex items which requires special repainting
 * routine inside QITreeWidget's viewport.
 */
class ComplexTreeWidgetItem : public QTreeWidgetItem
{
public:

    ComplexTreeWidgetItem (QTreeWidget *aParent)
        : QTreeWidgetItem  (aParent, QITreeWidget::ComplexItemType) {}

    virtual void paintItem (QPainter *aPainter) = 0;
};

#endif /* __QITreeWidget_h__ */

