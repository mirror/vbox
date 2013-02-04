/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIStatusBar class declaration
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

#ifndef __QIStatusBar_h__
#define __QIStatusBar_h__

/* Qt includes */
#include <QStatusBar>

/**
 *  The QIStatusBar class is a replacement of QStatusBar with disabling
 *  drawing of sunken borders around every widget on the status bar.
 */
class QIStatusBar : public QStatusBar
{
    Q_OBJECT;

public:

    QIStatusBar (QWidget *aParent = 0);

protected slots:

    void rememberLastMessage (const QString &aMsg) { mMessage = aMsg; }

protected:

    QString mMessage;
};

#endif // __QIStatusBar_h__

