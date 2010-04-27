/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxAboutDlg class declaration
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxAboutDlg_h__
#define __VBoxAboutDlg_h__

#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

/* Qt includes */
#include <QPixmap>

class QEvent;

class VBoxAboutDlg: public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:
    VBoxAboutDlg (QWidget* aParent, const QString &aVersion);

protected:
    bool event (QEvent *aEvent);
    void paintEvent (QPaintEvent *aEvent);
    void mouseReleaseEvent (QMouseEvent *aEvent);

    void retranslateUi();

private:
    QString mAboutText;
    QString mVersion;
    QPixmap mBgImage;
};

#endif /* __VBoxAboutDlg_h__ */

