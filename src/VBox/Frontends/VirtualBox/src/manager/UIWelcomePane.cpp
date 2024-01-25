/* $Id$ */
/** @file
 * VBox Qt GUI - UIWelcomePane class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>
#include <QUrl>

/* GUI includes */
#include "QIRichTextLabel.h"
#include "QIWithRetranslateUI.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIIconPool.h"
#include "UIWelcomePane.h"


/*********************************************************************************************************************************
*   Class UIWelcomePane implementation.                                                                                          *
*********************************************************************************************************************************/

UIWelcomePane::UIWelcomePane(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLabelGreetings(0)
    , m_pLabelIcon(0)
{
    prepare();
}

bool UIWelcomePane::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::Show:
        case QEvent::ScreenChangeInternal:
        {
            /* Update pixmap: */
            updateTextLabels();
            updatePixmap();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QIWithRetranslateUI<QWidget>::event(pEvent);
}

void UIWelcomePane::retranslateUi()
{
    /* Translate greetings text: */
    m_pLabelGreetings->setText(tr("<h3>Welcome to VirtualBox!</h3>"
                                  "<p>The left part of application window contains global tools and "
                                  "lists all virtual machines and virtual machine groups on your computer. "
                                  "You can import, add and create new VMs using corresponding toolbar buttons. "
                                  "You can popup a tools of currently selected element using corresponding element button.</p>"
                                  "<p>You can press the <b>%1</b> key to get instant help, or visit "
                                  "<a href=https://www.virtualbox.org>www.virtualbox.org</a> "
                                  "for more information and latest news.</p>")
                                  .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
}

void UIWelcomePane::sltHandleLinkActivated(const QUrl &urlLink)
{
    uiCommon().openURL(urlLink.toString());
}

void UIWelcomePane::prepare()
{
    /* Prepare default welcome icon: */
    m_icon = UIIconPool::iconSet(":/tools_banner_global_200px.png");

    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Prepare welcome layout: */
        QHBoxLayout *pLayoutWelcome = new QHBoxLayout;
        if (pLayoutWelcome)
        {
            const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
            pLayoutWelcome->setContentsMargins(iL, 0, 0, 0);

            /* Prepare greetings label: */
            m_pLabelGreetings = new QIRichTextLabel(this);
            if (m_pLabelGreetings)
            {
                m_pLabelGreetings->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
                connect(m_pLabelGreetings, &QIRichTextLabel::sigLinkClicked, this, &UIWelcomePane::sltHandleLinkActivated);
                pLayoutWelcome->addWidget(m_pLabelGreetings);
            }

            /* Prepare icon label: */
            m_pLabelIcon = new QLabel(this);
            if (m_pLabelIcon)
            {
                m_pLabelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

                /* Add into layout: */
                pLayoutWelcome->addWidget(m_pLabelIcon);
                pLayoutWelcome->setAlignment(m_pLabelIcon, Qt::AlignHCenter | Qt::AlignTop);
            }

            /* Add into layout: */
            pMainLayout->addLayout(pLayoutWelcome);
        }

        /* Add stretch: */
        pMainLayout->addStretch();
    }

    /* Assign Help keyword: */
    uiCommon().setHelpKeyword(this, "intro-starting");

    /* Translate finally: */
    retranslateUi();
    /* Update stuff: */
    updateTextLabels();
    updatePixmap();
}

void UIWelcomePane::updateTextLabels()
{
    /* For all the text-labels: */
    QList<QIRichTextLabel*> labels = findChildren<QIRichTextLabel*>();
    if (!labels.isEmpty())
    {
        /* Make sure their minimum width is around 20% of the screen width: */
        const QSize screenGeometry = gpDesktop->screenGeometry(this).size();
        foreach (QIRichTextLabel *pLabel, labels)
            pLabel->setMinimumTextWidth(screenGeometry.width() * .2);
    }
}

void UIWelcomePane::updatePixmap()
{
    /* Assign corresponding icon: */
    if (!m_icon.isNull())
    {
        /* Check which size goes as the default one: */
        const QList<QSize> sizes = m_icon.availableSizes();
        const QSize defaultSize = sizes.isEmpty() ? QSize(200, 200) : sizes.first();
        /* Acquire device-pixel ratio: */
        const qreal fDevicePixelRatio = gpDesktop->devicePixelRatio(this);
        m_pLabelIcon->setPixmap(m_icon.pixmap(defaultSize, fDevicePixelRatio));
    }
}
