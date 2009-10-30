/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialog class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#ifndef __QIDialog_h__
#define __QIDialog_h__

/* Qt includes */
#include <QDialog>

/* Qt forwards declarations */
class QEventLoop;

class QIDialog: public QDialog
{
    Q_OBJECT;

public:
    QIDialog (QWidget *aParent = 0, Qt::WindowFlags aFlags = 0);
    void setVisible (bool aVisible);

public slots:
    int exec (bool aShow = true);

protected:
    void showEvent (QShowEvent *aEvent);

private:
    /* Private member vars */
    bool mPolished;
    QEventLoop *mEventLoop;
};

#endif /* __QIDialog_h__ */

