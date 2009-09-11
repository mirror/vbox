/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIRichToolButton class declaration
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef __QIRichToolButton_h__
#define __QIRichToolButton_h__

/* Qt includes */
#include <QLabel>
#include <QWidget>
#include <QToolButton>

/** @class QIRichToolButton
 *
 *  The QIRichToolButton class is a tool-botton with separate text-label.
 *
 */
class QIRichToolButton : public QWidget
{
    Q_OBJECT;

public:

    QIRichToolButton (const QString &aName = QString::null, QWidget *aParent = 0);

    void animateClick() { mButton->animateClick(); }
    void setText (const QString &aName) { mLabel->setText (aName); }

signals:

    void clicked();

protected slots:

    virtual void buttonClicked() { emit clicked(); }

protected:

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void paintEvent (QPaintEvent *aEvent);

    QToolButton *mButton;
    QLabel *mLabel;
};

#endif
