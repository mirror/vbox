/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIBar class implementation
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "UIBar.h"
#include "UIImageTools.h"
#include "VBoxGlobal.h"

/* Global includes */
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QPixmapCache>

UIBar::UIBar(QWidget *pParent /* = 0 */)
  : QWidget(pParent)
  , m_pContentWidget(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 2, 0, 2);
}

void UIBar::setContentWidget(QWidget *pWidget)
{
    QLayout *pLayout = layout();
    if (m_pContentWidget)
        pLayout->removeWidget(m_pContentWidget);
    pLayout->addWidget(pWidget);
//    pLayout->setAlignment(pWidget, Qt::AlignCenter);
    m_pContentWidget = pWidget;
}

QWidget* UIBar::contentWidget() const
{
    return m_pContentWidget;
}

QSize UIBar::sizeHint() const
{
    return QSize(0, 0);
}

void UIBar::paintEvent(QPaintEvent *pEvent)
{
    QPainter painter(this);
    painter.setClipRect(pEvent->rect());

#ifdef Q_WS_MAC
    paintContentDarwin(&painter);
#else /* Q_WS_MAC */
    paintContent(&painter);
#endif /* !Q_WS_MAC */
}

#ifdef Q_WS_MAC

void UIBar::paintContentDarwin(QPainter *pPainter)
{
    QSize s = size();
    QLinearGradient lg(0, 1, 0, s.height() - 2);
    lg.setColorAt(0, QColor(233, 233, 233));
    lg.setColorAt(1, QColor(208, 208, 208));
//    pPainter->setPen(QColor(64, 64, 64));
//    pPainter->drawLine(0, 0, s.width(), 0);
    pPainter->setPen(QColor(147, 147, 147));
    pPainter->drawLine(0, s.height()-1, s.width(), s.height()-1);
    pPainter->setPen(Qt::NoPen);
    pPainter->setBrush(lg);
    pPainter->drawRect(0, 1, s.width(), s.height()- 2);
}

#else /* Q_WS_MAC */

void UIBar::paintContent(QPainter *pPainter)
{
    QSize s = size();
    QPalette pal = palette();
    QColor base = pal.color(QPalette::Active, QPalette::Window);
    QLinearGradient lg(0, 1, 0, s.height() - 2);
    lg.setColorAt(0, base);
    lg.setColorAt(.49, base.darker(102));
    lg.setColorAt(.50, base.darker(104));
    lg.setColorAt(1., base.darker(106));
    pPainter->setPen(base.darker(60));
    pPainter->drawLine(0, 0, s.width(), 0);
    pPainter->setPen(base.darker(125));
    pPainter->drawLine(0, s.height()-1, s.width(), s.height()-1);
    pPainter->setPen(Qt::NoPen);
    pPainter->setBrush(lg);
    pPainter->drawRect(0, 1, s.width(), s.height()-2);
}

#endif /* !Q_WS_MAC */

UIMainBar::UIMainBar(QWidget *pParent /* = 0 */)
  : UIBar(pParent)
  , m_fShowBetaLabel(false)
{
    /* Check for beta versions */
    if (vboxGlobal().isBeta())
        m_fShowBetaLabel = true;
}

void UIMainBar::paintEvent(QPaintEvent *pEvent)
{
    UIBar::paintEvent(pEvent);
    if (m_fShowBetaLabel)
    {
        QPixmap betaLabel;
        const QString key("vbox:betaLabelSleeve");
        if (!QPixmapCache::find(key, betaLabel))
        {
            betaLabel = ::betaLabelSleeve();
            QPixmapCache::insert(key, betaLabel);
        }
        QSize s = size();
        QPainter painter(this);
        painter.setClipRect(pEvent->rect());
        painter.drawPixmap(s.width() - betaLabel.width(), 0, betaLabel);
    }
}

