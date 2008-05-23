/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIAbstractDialog class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __QIAbstractDialog_h__
#define __QIAbstractDialog_h__

/* Qt includes */
#include <QMainWindow>

class QSizeGrip;
class QPushButton;

/**
 *  QMainWindow reimplementation used as abstract interface in different dialog
 *  based on QMainWindow widget allows supporting main-menu & status-bar.
 */
class QIAbstractDialog : public QMainWindow
{
    Q_OBJECT;

public:

    QIAbstractDialog (QWidget *aParent = 0, Qt::WindowFlags aFlags = 0);

protected:

    void initializeDialog();

    virtual bool eventFilter (QObject *aObject, QEvent *aEvent);
    virtual void resizeEvent (QResizeEvent *aEvent);

private:

    QPushButton* searchDefaultButton();

    QSizeGrip   *mSizeGrip;
    QPushButton *mDefaultButton;
};

#endif // __QIAbstractDialog_h__

