/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIStatusBar class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __QIStatusBar_h__
#define __QIStatusBar_h__

/* Qt includes */
#include <QStatusBar>

class QPaintEvent;

class QIStatusBar : public QStatusBar
{
    Q_OBJECT

public:

    QIStatusBar (QWidget *parent = 0);

protected:

    virtual void paintEvent (QPaintEvent *);

protected slots:

    void rememberLastMessage (const QString &msg) { message = msg; }

protected:

    QString message;
};

#endif // __QIStatusBar_h__
