/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QITableView class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QITableView_h__
#define __QITableView_h__

/* Global includes */
#include <QTableView>

/* QITableView class which extends standard QTableView's functionality. */
class QITableView: public QTableView
{
    Q_OBJECT;

public:

    QITableView(QWidget *pParent = 0);

signals:

    void sigCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

protected slots:

    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
};

#endif /* __QITableView_h__ */

