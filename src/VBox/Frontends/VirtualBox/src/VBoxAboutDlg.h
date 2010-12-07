/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxAboutDlg class declaration
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

#ifndef __VBoxAboutDlg_h__
#define __VBoxAboutDlg_h__

/* Global includes */
#include <QPixmap>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

/* Forward declarations */
class QEvent;

/* VBox about dialog */
class VBoxAboutDlg: public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /* Constructor: */
    VBoxAboutDlg(QWidget* pParent, const QString &strVersion);

protected:

    /* Event handlers: */
    bool event(QEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);
    void mouseReleaseEvent(QMouseEvent *pEvent);

    /* Language stuff: */
    void retranslateUi();

private:

    /* Variables: */
    QString m_strAboutText;
    QString m_strVersion;
    QPixmap m_bgImage;
};

#endif /* __VBoxAboutDlg_h__ */

