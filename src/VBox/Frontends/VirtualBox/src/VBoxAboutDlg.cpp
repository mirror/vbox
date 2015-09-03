/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxAboutDlg class implementation.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QEvent>
# include <QLabel>
# include <QPainter>

/* GUI includes: */
# include "UIConverter.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "VBoxAboutDlg.h"
# include "VBoxGlobal.h"

/* Other VBox includes: */
# include <iprt/path.h>
# include <VBox/version.h> /* VBOX_VENDOR */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

VBoxAboutDlg::VBoxAboutDlg(QWidget *pParent, const QString &strVersion)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_strVersion(strVersion)
    , m_pLabel(0)
{
    /* Prepare: */
    prepare();
}

bool VBoxAboutDlg::event(QEvent *pEvent)
{
    /* Set fixed-size for dialog: */
    if (pEvent->type() == QEvent::Polish)
        setFixedSize(m_size);
    /* Call to base-class: */
    return QIDialog::event(pEvent);
}

void VBoxAboutDlg::paintEvent(QPaintEvent* /* pEvent */)
{
    QPainter painter(this);
    /* Draw About-VirtualBox background image: */
    painter.drawPixmap(0, 0, m_pixmap);
}

void VBoxAboutDlg::retranslateUi()
{
    setWindowTitle(tr("VirtualBox - About"));
    const QString strAboutText = tr("VirtualBox Graphical User Interface");
#ifdef VBOX_BLEEDING_EDGE
    const QString strVersionText = "EXPERIMENTAL build %1 - " + QString(VBOX_BLEEDING_EDGE);
#else /* !VBOX_BLEEDING_EDGE */
    const QString strVersionText = tr("Version %1");
#endif /* !VBOX_BLEEDING_EDGE */
#if VBOX_OSE
    m_strAboutText = strAboutText + " " + strVersionText.arg(m_strVersion) + "\n" +
                     QString("%1 2004-" VBOX_C_YEAR " " VBOX_VENDOR).arg(QChar(0xa9));
#else /* !VBOX_OSE */
    m_strAboutText = strAboutText + "\n" + strVersionText.arg(m_strVersion);
#endif /* !VBOX_OSE */
    m_strAboutText = m_strAboutText + "\n" + QString("Copyright %1 2015 Oracle and/or its affiliates. All rights reserved.").arg(QChar(0xa9));
    AssertPtrReturnVoid(m_pLabel);
    m_pLabel->setText(m_strAboutText);
}

void VBoxAboutDlg::prepare()
{
    /* Delete dialog on close: */
    setAttribute(Qt::WA_DeleteOnClose);

    /* Choose default image: */
    QString strPath(":/about.png");

    /* Branding: Use a custom about splash picture if set: */
    const QString strSplash = vboxGlobal().brandingGetKey("UI/AboutSplash");
    if (vboxGlobal().brandingIsActive() && !strSplash.isEmpty())
    {
        char szExecPath[1024];
        RTPathExecDir(szExecPath, 1024);
        QString strTmpPath = QString("%1/%2").arg(szExecPath).arg(strSplash);
        if (QFile::exists(strTmpPath))
            strPath = strTmpPath;
    }

    /* Load image: */
    const QIcon icon = UIIconPool::iconSet(strPath);
    m_size = icon.availableSizes().first();
    m_pixmap = icon.pixmap(m_size);

    /* Prepares main-layout: */
    prepareMainLayout();

    /* Translate: */
    retranslateUi();
}

void VBoxAboutDlg::prepareMainLayout()
{
    /* Create main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pMainLayout);
    {
        /* Create label for version text: */
        m_pLabel = new QLabel;
        AssertPtrReturnVoid(m_pLabel);
        {
            /* Prepare label for version text: */
            QPalette palette;
            /* Branding: Set a different text color (because splash also could be white),
             * otherwise use white as default color: */
            const QString strColor = vboxGlobal().brandingGetKey("UI/AboutTextColor");
            if (!strColor.isEmpty())
                palette.setColor(QPalette::WindowText, QColor(strColor).name());
            else
                palette.setColor(QPalette::WindowText, Qt::black);
            m_pLabel->setPalette(palette);
            m_pLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_pLabel->setFont(font());

            /* Add label to the main-layout: */
            pMainLayout->addWidget(m_pLabel);
            pMainLayout->setAlignment(m_pLabel, Qt::AlignRight | Qt::AlignBottom);
        }
    }
}

