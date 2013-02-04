/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowButtonPress class declaration
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

#ifndef __QIArrowButtonPress_h__
#define __QIArrowButtonPress_h__

/* VBox includes */
#include "QIRichToolButton.h"

/* VBox forwards */
class QIRichToolButton;

/** @class QIArrowButtonPress
 *
 *  The QIArrowButtonPress class is an arrow tool-button with text-label,
 *  used as back/next buttons in QIMessageBox class.
 *
 */
class QIArrowButtonPress : public QIRichToolButton
{
    Q_OBJECT;

public:

    QIArrowButtonPress (QWidget *aParent = 0);
    QIArrowButtonPress (bool aNext, const QString &aName, QWidget *aParent = 0);

    void setNext (bool aNext) { mNext = aNext; }

private:

    void updateIcon();
    bool eventFilter (QObject *aObject, QEvent *aEvent);

    bool mNext;
};

#endif

