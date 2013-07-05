/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupPane class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QPainter>

/* GUI includes: */
#include "UIPopupPane.h"
#include "UIPopupPaneTextPane.h"
#include "UIPopupPaneButtonPane.h"
#include "UIAnimationFramework.h"
#include "QIMessageBox.h"

UIPopupPane::UIPopupPane(QWidget *pParent,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions,
                         bool fProposeAutoConfirmation)
    : QWidget(pParent)
    , m_iLayoutMargin(10), m_iLayoutSpacing(5)
    , m_strMessage(strMessage), m_strDetails(strDetails)
    , m_fProposeAutoConfirmation(fProposeAutoConfirmation)
    , m_buttonDescriptions(buttonDescriptions)
    , m_fHovered(false)
    , m_iDefaultOpacity(180)
    , m_iHoveredOpacity(250)
    , m_iOpacity(m_iDefaultOpacity)
    , m_fFocused(false)
    , m_pTextPane(0), m_pButtonPane(0)
{
    /* Prepare: */
    prepare();
}

void UIPopupPane::setMessage(const QString &strMessage)
{
    /* Make sure somthing changed: */
    if (m_strMessage == strMessage)
        return;

    /* Fetch new message: */
    m_strMessage = strMessage;
    m_pTextPane->setText(m_strMessage);
}

void UIPopupPane::setDetails(const QString &strDetails)
{
    /* Make sure somthing changed: */
    if (m_strDetails == strDetails)
        return;

    /* Fetch new details: */
    m_strDetails = strDetails;
}

void UIPopupPane::setProposeAutoConfirmation(bool fPropose)
{
    /* Make sure something changed: */
    if (m_fProposeAutoConfirmation == fPropose)
        return;

    /* Fetch new auto-confirmation proposal: */
    m_fProposeAutoConfirmation = fPropose;
    m_pTextPane->setProposeAutoConfirmation(m_fProposeAutoConfirmation);
}

void UIPopupPane::setDesiredWidth(int iWidth)
{
    /* Make sure text-pane exists: */
    if (!m_pTextPane)
        return;

    /* Propagate desired width to the text-pane we have: */
    m_pTextPane->setDesiredWidth(iWidth - 2 * m_iLayoutMargin
                                        - m_pButtonPane->minimumSizeHint().width());
}

int UIPopupPane::minimumWidthHint() const
{
    /* Prepare width hint: */
    int iWidthHint = 0;

    /* Take into account layout: */
    iWidthHint += 2 * m_iLayoutMargin;
    {
        /* Take into account widgets: */
        iWidthHint += m_pTextPane->minimumSizeHint().width();
        iWidthHint += m_iLayoutSpacing;
        iWidthHint += m_pButtonPane->minimumSizeHint().width();
    }

    /* Return width hint: */
    return iWidthHint;
}

int UIPopupPane::minimumHeightHint() const
{
    /* Prepare height hint: */
    int iHeightHint = 0;

    /* Take into account layout: */
    iHeightHint += 2 * m_iLayoutMargin;
    {
        /* Take into account widgets: */
        const int iTextPaneHeight = m_pTextPane->minimumSizeHint().height();
        const int iButtonBoxHeight = m_pButtonPane->minimumSizeHint().height();
        iHeightHint += qMax(iTextPaneHeight, iButtonBoxHeight);
    }

    /* Return height hint: */
    return iHeightHint;
}

QSize UIPopupPane::minimumSizeHint() const
{
    /* Wrap reimplemented getters: */
    return QSize(minimumWidthHint(), minimumHeightHint());
}

void UIPopupPane::layoutContent()
{
    /* Variables: */
    const int iWidth = width();
    const int iHeight = height();
    const QSize buttonPaneMinimumSizeHint = m_pButtonPane->minimumSizeHint();
    const int iButtonPaneMinimumWidth = buttonPaneMinimumSizeHint.width();
    const int iButtonPaneMinimumHeight = buttonPaneMinimumSizeHint.height();
    const int iTextPaneWidth = iWidth - 2 * m_iLayoutMargin - m_iLayoutSpacing - iButtonPaneMinimumWidth;
    const int iTextPaneHeight = m_pTextPane->minimumSizeHint().height();
    const int iMaximumHeight = qMax(iTextPaneHeight, iButtonPaneMinimumHeight);
    const int iMinimumHeight = qMin(iTextPaneHeight, iButtonPaneMinimumHeight);
    const int iHeightShift = (iMaximumHeight - iMinimumHeight) / 2;
    const bool fTextPaneShifted = iTextPaneHeight < iButtonPaneMinimumHeight;

    /* Text-pane: */
    m_pTextPane->move(m_iLayoutMargin,
                      fTextPaneShifted ? m_iLayoutMargin + iHeightShift : m_iLayoutMargin);
    m_pTextPane->resize(iTextPaneWidth,
                        iTextPaneHeight);
    m_pTextPane->layoutContent();
    /* Button-box: */
    m_pButtonPane->move(m_iLayoutMargin + iTextPaneWidth + m_iLayoutSpacing,
                        m_iLayoutMargin);
    m_pButtonPane->resize(iButtonPaneMinimumWidth,
                          iHeight - 2 * m_iLayoutMargin);
}

void UIPopupPane::sltButtonClicked(int iButtonID)
{
    done(iButtonID & AlertButtonMask);
}

void UIPopupPane::prepare()
{
    /* Install 'hover' animation for 'opacity' property: */
    UIAnimation::installPropertyAnimation(this, "opacity", "defaultOpacity", "hoveredOpacity",
                                          SIGNAL(sigHoverEnter()), SIGNAL(sigHoverLeave()));
    /* Prepare content: */
    prepareContent();
}

void UIPopupPane::prepareContent()
{
    /* Prepare this: */
    installEventFilter(this);
    /* Create message-label: */
    m_pTextPane = new UIPopupPaneTextPane(this);
    {
        /* Prepare label: */
        connect(m_pTextPane, SIGNAL(sigSizeHintChanged()),
                this, SIGNAL(sigSizeHintChanged()));
        m_pTextPane->installEventFilter(this);
        m_pTextPane->setText(m_strMessage);
        m_pTextPane->setProposeAutoConfirmation(m_fProposeAutoConfirmation);
    }
    /* Create button-box: */
    m_pButtonPane = new UIPopupPaneButtonPane(this);
    {
        /* Prepare button-box: */
        connect(m_pButtonPane, SIGNAL(sigButtonClicked(int)),
                this, SLOT(sltButtonClicked(int)));
        m_pButtonPane->installEventFilter(this);
        m_pButtonPane->setButtons(m_buttonDescriptions);
    }

    /* Prepare focus rules: */
    setFocusPolicy(Qt::StrongFocus);
    m_pTextPane->setFocusPolicy(Qt::StrongFocus);
    m_pButtonPane->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_pButtonPane);
    m_pTextPane->setFocusProxy(m_pButtonPane);
}

bool UIPopupPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Something is hovered: */
        case QEvent::HoverEnter:
        case QEvent::Enter:
        {
            /* Hover pane if not yet hovered: */
            if (!m_fHovered)
            {
                m_fHovered = true;
                emit sigHoverEnter();
            }
            break;
        }
        /* Nothing is hovered: */
        case QEvent::Leave:
        {
            /* Unhover pane if hovered but not focused: */
            if (pWatched == this && m_fHovered && !m_fFocused)
            {
                m_fHovered = false;
                emit sigHoverLeave();
            }
            break;
        }
        /* Pane is focused: */
        case QEvent::FocusIn:
        {
            /* Focus pane if not focused: */
            if (!m_fFocused)
            {
                m_fFocused = true;
                emit sigFocusEnter();
            }
            break;
        }
        /* Pane is unfocused: */
        case QEvent::FocusOut:
        {
            /* Unfocus and unhover pane: */
            if (m_fFocused)
            {
                m_fFocused = false;
                emit sigFocusLeave();
                m_fHovered = false;
                emit sigHoverLeave();
            }
            break;
        }
        /* Default case: */
        default: break;
    }
    /* Do not filter anything: */
    return false;
}

void UIPopupPane::paintEvent(QPaintEvent*)
{
    /* Compose painting rectangle: */
    const QRect rect(0, 0, width(), height());

    /* Create painter: */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Configure painter clipping: */
    QPainterPath path;
    int iDiameter = 6;
    QSizeF arcSize(2 * iDiameter, 2 * iDiameter);
    path.moveTo(iDiameter, 0);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, 0), 90, 90);
    path.lineTo(path.currentPosition().x(), rect.height() - iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(0, -iDiameter), 180, 90);
    path.lineTo(rect.width() - iDiameter, path.currentPosition().y());
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, -2 * iDiameter), 270, 90);
    path.lineTo(path.currentPosition().x(), iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-2 * iDiameter, -iDiameter), 0, 90);
    path.closeSubpath();
    painter.setClipPath(path);

    /* Fill with background: */
    QColor currentColor(palette().color(QPalette::Window));
    QColor newColor1(currentColor.red(), currentColor.green(), currentColor.blue(), opacity());
    QColor newColor2 = newColor1.darker(115);
    QLinearGradient headerGradient(rect.topLeft(), rect.topRight());
    headerGradient.setColorAt(0, newColor1);
    headerGradient.setColorAt(1, newColor2);
    painter.fillRect(rect, headerGradient);
}

void UIPopupPane::done(int iResultCode)
{
    /* Close the window: */
    close();

    /* Was the popup auto-confirmed? */
    if (m_pTextPane->isAutoConfirmed())
        iResultCode |= AlertOption_AutoConfirmed;

    /* Notify listeners: */
    emit sigDone(iResultCode);
}

