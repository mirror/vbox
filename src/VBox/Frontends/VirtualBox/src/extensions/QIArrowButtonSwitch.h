/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowButtonSwitch class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIArrowButtonSwitch_h__
#define __QIArrowButtonSwitch_h__

/* VBox includes */
#include "QIRichToolButton.h"

/* VBox forwards */
class QIRichToolButton;


/** @class QIArrowButtonSwitch
 *
 *  The QIArrowButtonSwitch class is an arrow tool-button with text-label,
 *  used as collaps/expand switch in QIMessageBox class.
 *
 */
class QIArrowButtonSwitch : public QIRichToolButton
{
    Q_OBJECT;

public:

    QIArrowButtonSwitch (QWidget *aParent = 0);
    QIArrowButtonSwitch (const QString &aName, QWidget *aParent = 0);

    bool isExpanded() const { return mIsExpanded; }

private slots:

    void buttonClicked();

private:

    void updateIcon();
    bool eventFilter (QObject *aObject, QEvent *aEvent);

    bool mIsExpanded;
};

#endif

