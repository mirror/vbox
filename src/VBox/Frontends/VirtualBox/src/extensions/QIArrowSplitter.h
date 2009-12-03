/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowSplitter class declaration
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

#ifndef __QIArrowSplitter_h__
#define __QIArrowSplitter_h__

/* VBox includes */
#include "QIArrowButtonPress.h"
#include "QIArrowButtonSwitch.h"

/* Qt includes */
#include <QWidget>
#include <QHBoxLayout>

/* VBox forwards */
class QIArrowButtonPress;
class QIArrowButtonSwitch;

/* Qt forwards */
class QVBoxLayout;

/** @class QIArrowSplitter
 *
 *  The QIArrowSplitter class is a folding widget placeholder.
 *
 */
class QIArrowSplitter : public QWidget
{
    Q_OBJECT;

public:

    QIArrowSplitter (QWidget *aChild, QWidget *aParent = 0);

    void setMultiPaging (bool aMultiPage);

    void setButtonEnabled (bool aNext, bool aEnabled);

    void setName (const QString &aName);

public slots:

    void toggleWidget();

signals:

    void showBackDetails();
    void showNextDetails();

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    void relayout();

    QVBoxLayout *mMainLayout;
    QIArrowButtonSwitch *mSwitchButton;
    QIArrowButtonPress *mBackButton;
    QIArrowButtonPress *mNextButton;
    QWidget *mChild;
};

#endif
