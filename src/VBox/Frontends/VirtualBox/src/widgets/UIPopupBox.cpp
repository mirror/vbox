/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupBox class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include "UIPopupBox.h"
#ifdef Q_WS_MAC
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

/* Global includes */
#include <QApplication>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

UIPopupBox::UIPopupBox(QWidget *pParent)
  : QWidget(pParent)
  , m_fLinkEnabled(false)
  , m_pContentWidget(0)
  , m_fOpen(true)
  , m_pLabelPath(0)
  , m_aw(9)
  , m_fHeaderHover(false)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(10, 5, 5, 5);
    QHBoxLayout *pTitleLayout = new QHBoxLayout();
    m_pTitleIcon = new QLabel(this);
    m_pTitleLabel = new QLabel(this);
    connect(m_pTitleLabel, SIGNAL(linkActivated(const QString)),
            this, SIGNAL(titleClicked(const QString)));
    pTitleLayout->addWidget(m_pTitleIcon);
    pTitleLayout->addWidget(m_pTitleLabel, Qt::AlignLeft);
    pMainLayout->addLayout(pTitleLayout);

    m_arrowPath.lineTo(m_aw / 2.0, m_aw / 2.0);
    m_arrowPath.lineTo(m_aw, 0);

//    setMouseTracking(true);
    qApp->installEventFilter(this);
}

UIPopupBox::~UIPopupBox()
{
    if (m_pLabelPath)
        delete m_pLabelPath;
}

void UIPopupBox::setTitle(const QString& strTitle)
{
    m_strTitle = strTitle;
    updateHover(true);
    recalc();
}

QString UIPopupBox::title() const
{
    return m_strTitle;
}

void UIPopupBox::setTitleIcon(const QIcon& icon)
{
    m_icon = icon;
    updateHover(true);
    recalc();
}

QIcon UIPopupBox::titleIcon() const
{
    return m_icon;
}

void UIPopupBox::setTitleLink(const QString& strLink)
{
    m_strLink = strLink;
}

QString UIPopupBox::titleLink() const
{
    return m_strLink;
}

void UIPopupBox::setTitleLinkEnabled(bool fEnabled)
{
    m_fLinkEnabled = fEnabled;
}

bool UIPopupBox::isTitleLinkEnabled() const
{
    return m_fLinkEnabled;
}

void UIPopupBox::setContentWidget(QWidget *pWidget)
{
    if (m_pContentWidget)
    {
        layout()->removeWidget(m_pContentWidget);
//        m_pContentWidget->removeEventFilter(this);
    }
    m_pContentWidget = pWidget;
//    m_pContentWidget->installEventFilter(this);
//    m_pContentWidget->setMouseTracking(true);
    layout()->addWidget(pWidget);
    recalc();
}

QWidget* UIPopupBox::contentWidget() const
{
    return m_pContentWidget;
}

void UIPopupBox::setOpen(bool fOpen)
{
    /* Do not do anything if already done: */
    if (m_fOpen == fOpen)
        return;

    /* Store new value: */
    m_fOpen = fOpen;

    /* Update content widget if present or this itself: */
    if (m_pContentWidget)
        m_pContentWidget->setVisible(m_fOpen);
    else
        update();

    /* Notify listeners about content widget visibility: */
    if (m_pContentWidget && m_pContentWidget->isVisible())
        emit sigUpdateContentWidget();
}

void UIPopupBox::toggleOpen()
{
    /* Switch 'opened' state: */
    setOpen(!m_fOpen);

    /* Notify listeners about toggling: */
    emit toggled(m_fOpen);
}

bool UIPopupBox::isOpen() const
{
    return m_fOpen;
}

bool UIPopupBox::eventFilter(QObject * /* pWatched */, QEvent *pEvent)
{
    QEvent::Type type = pEvent->type();
    if (   type == QEvent::MouseMove
        || type == QEvent::Wheel
        || type == QEvent::Resize
        || type == QEvent::Enter
        || type == QEvent::Leave)
        updateHover();
    return false;
}

void UIPopupBox::resizeEvent(QResizeEvent *pEvent)
{
    updateHover();
    recalc();
    QWidget::resizeEvent(pEvent);
}

void UIPopupBox::mouseDoubleClickEvent(QMouseEvent * /* pEvent */)
{
    toggleOpen();
}

void UIPopupBox::mouseMoveEvent(QMouseEvent *pEvent)
{
    updateHover();
    QWidget::mouseMoveEvent(pEvent);
}

void UIPopupBox::wheelEvent(QWheelEvent *pEvent)
{
    updateHover();
    QWidget::wheelEvent(pEvent);
}

void UIPopupBox::enterEvent(QEvent *pEvent)
{
    updateHover();
    QWidget::enterEvent(pEvent);
}

void UIPopupBox::leaveEvent(QEvent *pEvent)
{
    updateHover();
    QWidget::leaveEvent(pEvent);
}

void UIPopupBox::paintEvent(QPaintEvent *pEvent)
{
    QPainter painter(this);
    painter.setClipRect(pEvent->rect());

    QPalette pal = palette();
    painter.setClipPath(*m_pLabelPath);
    QColor base = pal.color(QPalette::Active, QPalette::Window);
    QRect rect = QRect(QPoint(0, 0), size()).adjusted(0, 0, -1, -1);
    /* Base background */
    painter.fillRect(QRect(QPoint(0, 0), size()), pal.brush(QPalette::Active, QPalette::Base));
    /* Top header background */
    QLinearGradient lg(rect.x(), rect.y(), rect.x(), rect.y() + 2 * 5 + m_pTitleLabel->sizeHint().height());
    lg.setColorAt(0, base.darker(95));
    lg.setColorAt(1, base.darker(110));
    int theight = rect.height();
    if (m_fOpen)
        theight = 2 * 5 + m_pTitleLabel->sizeHint().height();
    painter.fillRect(QRect(rect.x(), rect.y(), rect.width(), theight), lg);
    /* Outer round rectangle line */
    painter.setClipping(false);
    painter.strokePath(*m_pLabelPath, base.darker(110));
    /* Arrow */
    if (m_fHeaderHover)
    {
        painter.setBrush(base.darker(106));
        painter.setPen(QPen(base.darker(128), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QSizeF s = m_arrowPath.boundingRect().size();
        if (m_fOpen)
        {
            painter.translate(rect.x() + rect.width() - s.width() - 10, rect.y() + theight / 2 + s.height() / 2);
            /* Flip */
            painter.scale(1, -1);
        }
        else
            painter.translate(rect.x() + rect.width() - s.width() - 10, rect.y() + theight / 2 - s.height() / 2 + 1);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawPath(m_arrowPath);
    }
}

void UIPopupBox::updateHover(bool fForce /* = false */)
{
    bool fOld = m_fHeaderHover;
//    QPoint bl = mapFromGlobal(QCursor::pos());
//    printf("%d %d\n", bl.x(), bl.y());
    if (   m_pLabelPath
        && m_pLabelPath->contains(mapFromGlobal(QCursor::pos())))
//        if (underMouse())
        m_fHeaderHover = true;
    else
        m_fHeaderHover = false;

    if (   !m_fLinkEnabled
        || m_strLink.isEmpty())
    {
        m_pTitleLabel->setText(QString("<b>%1</b>").arg(m_strTitle));
    }
    if (   fForce
        || fOld != m_fHeaderHover)
    {
        QPalette pal = m_pTitleLabel->palette();
        m_pTitleLabel->setText(QString("<b><a style=\"text-decoration: none; color: %1\" href=\"%2\">%3</a></b>")
                               .arg(m_fHeaderHover ? pal.color(QPalette::Link).name() : pal.color(QPalette::WindowText).name())
                               .arg(m_strLink)
                               .arg(m_strTitle));

        QPixmap i = m_icon.pixmap(16, 16);
#ifdef Q_WS_MAC
        /* todo: fix this */
//        if (!m_fHeaderHover)
//            i = QPixmap::fromImage(toGray(i.toImage()));
#endif /* Q_WS_MAC */
        m_pTitleIcon->setPixmap(i);
        update();
    }
}

void UIPopupBox::recalc()
{
    if (m_pLabelPath)
        delete m_pLabelPath;
    QRect rect = QRect(QPoint(0, 0), size()).adjusted(0, 0, -1, -1);
    int d = 18; // 22
    m_pLabelPath = new QPainterPath(QPointF(rect.x() + rect.width() - d, rect.y()));
    m_pLabelPath->arcTo(QRectF(rect.x(), rect.y(), d, d), 90, 90);
    m_pLabelPath->arcTo(QRectF(rect.x(), rect.y() + rect.height() - d, d, d), 180, 90);
    m_pLabelPath->arcTo(QRectF(rect.x() + rect.width() - d, rect.y() + rect.height() - d, d, d), 270, 90);
    m_pLabelPath->arcTo(QRectF(rect.x() + rect.width() - d, rect.y(), d, d), 0, 90);
    m_pLabelPath->closeSubpath();
    update();
}

