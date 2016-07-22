/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITreeWidget class implementation.
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

#ifndef __QITreeWidget_h__
#define __QITreeWidget_h__

/* Global includes */
#include <QTreeWidget>

/*
 * QTreeWidget class which extends standard QTreeWidget's functionality.
 */
class QITreeWidget: public QTreeWidget
{
    Q_OBJECT;

public:

    QITreeWidget (QWidget *aParent = 0);

    void setSizeHintForItems(const QSize &sizeHint);

signals:

    void painted (QTreeWidgetItem *aItem, QPainter *aPainter);
    void resized (const QSize &aSize, const QSize &aOldSize);

protected:

    void paintEvent (QPaintEvent *aEvent);
    void resizeEvent (QResizeEvent *aEvent);
};

#endif /* __QITreeWidget_h__ */

