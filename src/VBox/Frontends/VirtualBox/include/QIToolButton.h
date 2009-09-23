/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIToolButton class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef __QIToolButton_h__
#define __QIToolButton_h__

/* Qt includes */
#include <QToolButton>

class QIToolButton: public QToolButton
{
    Q_OBJECT;

public:
    QIToolButton (QWidget *aParent = 0)
      : QToolButton (aParent)
    {
#ifdef Q_WS_MAC
        setStyleSheet ("QToolButton { border: 0px none black; }");
#endif /* Q_WS_MAC */
    }
};

#endif /* __QIToolButton_h__ */

