/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITableView class declaration.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QITableView_h___
#define ___QITableView_h___

/* Qt includes: */
#include <QTableView>

/** QTableView extension providing GUI with advanced functionality. */
class QITableView : public QTableView
{
    Q_OBJECT;

signals:

    /** Notifies listeners about index changed from @a previous to @a current. */
    void sigCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

public:

    /** Constructs table-view on the basis of passed @a pParent. */
    QITableView(QWidget *pParent = 0);

protected:

    /** Handles index change from @a previous to @a current. */
    virtual void currentChanged(const QModelIndex &current, const QModelIndex &previous) /* override */;
};

#endif /* !___QITableView_h___ */

