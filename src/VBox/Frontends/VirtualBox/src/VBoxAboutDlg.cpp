/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxAboutDlg class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
# include <QDir>
# include <QEvent>
# include <QPainter>
# include <iprt/path.h>
# include <VBox/version.h> /* VBOX_VENDOR */

/* Local includes */
# include "VBoxAboutDlg.h"
# include "VBoxGlobal.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

VBoxAboutDlg::VBoxAboutDlg(QWidget *pParent, const QString &strVersion)
    : QIWithRetranslateUI2<QIDialog>(pParent, Qt::CustomizeWindowHint | Qt::WindowTitleHint)
    , m_strVersion(strVersion)
{
    /* Delete dialog on close: */
    setAttribute(Qt::WA_DeleteOnClose);

    /* Choose default image: */
    QString strPath(":/about.png");

    /* Branding: Use a custom about splash picture if set: */
    QString strSplash = vboxGlobal().brandingGetKey("UI/AboutSplash");
    if (vboxGlobal().brandingIsActive() && !strSplash.isEmpty())
    {
        char szExecPath[1024];
        RTPathExecDir(szExecPath, 1024);
        QString strTmpPath = QString("%1/%2").arg(szExecPath).arg(strSplash);
        if (QFile::exists(strTmpPath))
            strPath = strTmpPath;
    }

    /* Assign image: */
    m_bgImage.load(strPath);

    /* Translate: */
    retranslateUi();
}

bool VBoxAboutDlg::event(QEvent *pEvent)
{
    if (pEvent->type() == QEvent::Polish)
        setFixedSize(m_bgImage.size());
    if (pEvent->type() == QEvent::WindowDeactivate)
        close();
    return QIDialog::event(pEvent);
}

void VBoxAboutDlg::paintEvent(QPaintEvent* /* pEvent */)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_bgImage);
    painter.setFont(font());

    /* Branding: Set a different text color (because splash also could be white),
                 otherwise use white as default color: */
    QString strColor = vboxGlobal().brandingGetKey("UI/AboutTextColor");
    if (!strColor.isEmpty())
        painter.setPen(QColor(strColor).name());
    else
        painter.setPen(Qt::black);
#if VBOX_OSE
    painter.drawText(QRect(0, 400, 600, 32),
                     Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                     m_strAboutText);
#else /* VBOX_OSE */
    painter.drawText(QRect(271, 370, 360, 72),
                     Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
                     m_strAboutText);
#endif /* VBOX_OSE */
}

void VBoxAboutDlg::mouseReleaseEvent(QMouseEvent* /* pEvent */)
{
    /* Close the dialog on mouse button release: */
    close();
}

void VBoxAboutDlg::retranslateUi()
{
    setWindowTitle(tr("VirtualBox - About"));
    QString strAboutText =  tr("VirtualBox Graphical User Interface");
#ifdef VBOX_BLEEDING_EDGE
    QString strVersionText = "EXPERIMENTAL build %1 - " + QString(VBOX_BLEEDING_EDGE);
#else
    QString strVersionText = tr("Version %1");
#endif
#if VBOX_OSE
    m_strAboutText = strAboutText + " " + strVersionText.arg(m_strVersion) + "\n" +
                     QString("%1 2004-" VBOX_C_YEAR " " VBOX_VENDOR).arg(QChar(0xa9));
#else /* VBOX_OSE */
    m_strAboutText = strAboutText + "\n" + strVersionText.arg(m_strVersion);
#endif /* VBOX_OSE */
}

