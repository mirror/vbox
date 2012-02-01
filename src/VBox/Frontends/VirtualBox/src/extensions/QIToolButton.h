/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIToolButton class declaration
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIToolButton_h__
#define __QIToolButton_h__

/* Global includes: */
#include <QToolButton>

/* QToolButton reimplementation: */
class QIToolButton: public QToolButton
{
    Q_OBJECT;

public:

    QIToolButton(QWidget *pParent = 0)
        : QToolButton(pParent)
    {
#ifdef Q_WS_MAC
        setStyleSheet("QToolButton { border: 0px none black; margin: 2px 4px 0px 4px; } QToolButton::menu-indicator {image: none;}");
#endif /* Q_WS_MAC */
    }

    void removeBorder()
    {
        setStyleSheet("QToolButton { border: 0px }");
    }
};

#endif /* __QIToolButton_h__ */

