/* $Id$ */
/** @file
 * VBox Qt GUI - QIListView, QIItemDelegate class declarations.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIListView_h__
#define __QIListView_h__

/* Qt includes */
#include <QListView>
#include <QItemDelegate>

class QIListView: public QListView
{
    Q_OBJECT;

public:
    QIListView (QWidget *aParent = 0);

protected slots:
    void focusChanged (QWidget *aOld, QWidget *aNow);
};

class QIItemDelegate: public QItemDelegate
{
public:
    QIItemDelegate (QObject *aParent = 0)
        : QItemDelegate (aParent) {}

protected:
    void drawBackground (QPainter *aPainter, const QStyleOptionViewItem &aOption,
                         const QModelIndex &aIndex) const;
};

#endif /* __QIListView_h__ */

