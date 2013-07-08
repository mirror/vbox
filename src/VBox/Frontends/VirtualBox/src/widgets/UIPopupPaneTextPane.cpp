/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupPaneTextPane class implementation
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
#include <QLabel>
#include <QCheckBox>

/* GUI includes: */
#include "UIPopupPaneTextPane.h"
#include "UIAnimationFramework.h"

UIPopupPaneTextPane::UIPopupPaneTextPane(QWidget *pParent, const QString &strText, bool fProposeAutoConfirmation)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_iLayoutMargin(0)
    , m_iLayoutSpacing(10)
    , m_strText(strText)
    , m_pLabel(0)
    , m_iDesiredLabelWidth(-1)
    , m_pAutoConfirmCheckBox(0)
    , m_fProposeAutoConfirmation(fProposeAutoConfirmation)
    , m_fFocused(false)
    , m_pAnimation(0)
{
    /* Prepare: */
    prepare();
}

void UIPopupPaneTextPane::setText(const QString &strText)
{
    /* Make sure the text has changed: */
    if (m_pLabel->text() == strText)
        return;

    /* Fetch new text: */
    m_strText = strText;
    m_pLabel->setText(m_strText);

    /* Update size-hint: */
    updateSizeHint();
}

void UIPopupPaneTextPane::setDesiredWidth(int iDesiredWidth)
{
    /* Make sure the desired-width has changed: */
    if (m_iDesiredLabelWidth == iDesiredWidth)
        return;

    /* Fetch new desired-width: */
    m_iDesiredLabelWidth = iDesiredWidth;

    /* Update size-hint: */
    updateSizeHint();
}

void UIPopupPaneTextPane::setProposeAutoConfirmation(bool fPropose)
{
    /* Make sure the auto-confirmation-proposal has changed: */
    if (m_fProposeAutoConfirmation == fPropose)
        return;

    /* Fetch new auto-confirmation-proposal: */
    m_fProposeAutoConfirmation = fPropose;

    /* Update size-hint: */
    updateSizeHint();
}

bool UIPopupPaneTextPane::autoConfirmationProposed() const
{
    return m_fProposeAutoConfirmation;
}

bool UIPopupPaneTextPane::isAutoConfirmed() const
{
    return autoConfirmationProposed() &&
           m_pAutoConfirmCheckBox &&
           m_pAutoConfirmCheckBox->isChecked();
}

QSize UIPopupPaneTextPane::minimumSizeHint() const
{
    /* Check if desired-width set: */
    if (m_iDesiredLabelWidth >= 0)
        /* Dependent size-hint: */
        return m_minimumSizeHint;
    /* Golden-rule size-hint by default: */
    return QWidget::minimumSizeHint();
}

void UIPopupPaneTextPane::setMinimumSizeHint(const QSize &minimumSizeHint)
{
    /* Make sure the size-hint has changed: */
    if (m_minimumSizeHint == minimumSizeHint)
        return;

    /* Fetch new size-hint: */
    m_minimumSizeHint = minimumSizeHint;

    /* Notify parent popup-pane: */
    emit sigSizeHintChanged();
}

void UIPopupPaneTextPane::layoutContent()
{
    /* Variables: */
    const int iWidth = width();
    const int iHeight = height();
    const int iLabelWidth = m_labelSizeHint.width();
    const int iLabelHeight = m_labelSizeHint.height();

    /* Label: */
    m_pLabel->move(m_iLayoutMargin, m_iLayoutMargin);
    m_pLabel->resize(qMin(iWidth, iLabelWidth), qMin(iHeight, iLabelHeight));

    /* Check-box: */
    if (m_fProposeAutoConfirmation)
    {
        /* Variables: */
        const int iCheckBoxWidth = m_checkBoxSizeHint.width();
        const int iCheckBoxHeight = m_checkBoxSizeHint.height();
        const int iCheckBoxY = m_iLayoutMargin + iLabelHeight + m_iLayoutSpacing;
        /* Layout check-box: */
        if (iHeight - m_iLayoutMargin - iCheckBoxHeight - iCheckBoxY >= 0)
        {
            m_pAutoConfirmCheckBox->move(m_iLayoutMargin, iCheckBoxY);
            m_pAutoConfirmCheckBox->resize(iCheckBoxWidth, iCheckBoxHeight);
            if (m_pAutoConfirmCheckBox->isHidden())
                m_pAutoConfirmCheckBox->show();
        }
        else if (!m_pAutoConfirmCheckBox->isHidden())
            m_pAutoConfirmCheckBox->hide();
    }
    else if (!m_pAutoConfirmCheckBox->isHidden())
        m_pAutoConfirmCheckBox->hide();
}

void UIPopupPaneTextPane::sltFocusEnter()
{
    /* Ignore if already focused: */
    if (m_fFocused)
        return;

    /* Update focus state: */
    m_fFocused = true;

    /* Notify listeners: */
    emit sigFocusEnter();
}

void UIPopupPaneTextPane::sltFocusLeave()
{
    /* Ignore if already unfocused: */
    if (!m_fFocused)
        return;

    /* Update focus state: */
    m_fFocused = false;

    /* Notify listeners: */
    emit sigFocusLeave();
}

void UIPopupPaneTextPane::prepare()
{
    /* Prepare content: */
    prepareContent();
    /* Prepare animation: */
    prepareAnimation();

    /* Update size-hint: */
    updateSizeHint();
}

void UIPopupPaneTextPane::prepareContent()
{
    /* Create label: */
    m_pLabel = new QLabel(this);
    {
        /* Prepare label: */
        QFont currentFont = m_pLabel->font();
#ifdef Q_WS_MAC
        currentFont.setPointSize(currentFont.pointSize() - 2);
#else /* Q_WS_MAC */
        currentFont.setPointSize(currentFont.pointSize() - 1);
#endif /* !Q_WS_MAC */
        m_pLabel->setFont(currentFont);
        m_pLabel->setWordWrap(true);
        m_pLabel->setFocusPolicy(Qt::NoFocus);
        m_pLabel->setText(m_strText);
    }

    /* Create check-box: */
    m_pAutoConfirmCheckBox = new QCheckBox(this);
    {
        /* Prepare check-box: */
        QFont currentFont = m_pAutoConfirmCheckBox->font();
#ifdef Q_WS_MAC
        currentFont.setPointSize(currentFont.pointSize() - 2);
#else /* Q_WS_MAC */
        currentFont.setPointSize(currentFont.pointSize() - 1);
#endif /* !Q_WS_MAC */
        m_pAutoConfirmCheckBox->setFont(currentFont);
        m_pAutoConfirmCheckBox->setFocusPolicy(Qt::NoFocus);
    }

    /* Translate UI finally: */
    retranslateUi();
}

void UIPopupPaneTextPane::prepareAnimation()
{
    /* Propagate parent signals: */
    connect(parent(), SIGNAL(sigFocusEnter()), this, SLOT(sltFocusEnter()));
    connect(parent(), SIGNAL(sigFocusLeave()), this, SLOT(sltFocusLeave()));
    /* Install geometry animation for 'minimumSizeHint' property: */
    m_pAnimation = UIAnimation::installPropertyAnimation(this, "minimumSizeHint", "collapsedSizeHint", "expandedSizeHint",
                                                         SIGNAL(sigFocusEnter()), SIGNAL(sigFocusLeave()));
}

void UIPopupPaneTextPane::retranslateUi()
{
    /* Translate auto-confirm check-box: */
    m_pAutoConfirmCheckBox->setText(QApplication::translate("UIMessageCenter", "Do not show this message again"));
}

void UIPopupPaneTextPane::updateSizeHint()
{
    /* Recalculate collapsed size-hint: */
    {
        /* Collapsed size-hint contains only one-text-line label: */
        QFontMetrics fm(m_pLabel->font(), m_pLabel);
        m_collapsedSizeHint = QSize(m_iDesiredLabelWidth, fm.height());
    }

    /* Recalculate expanded size-hint: */
    {
        /* Recalculate label size-hint: */
        m_labelSizeHint = QSize(m_iDesiredLabelWidth, m_pLabel->heightForWidth(m_iDesiredLabelWidth));
        /* Recalculate check-box size-hint: */
        m_checkBoxSizeHint = m_fProposeAutoConfirmation ? m_pAutoConfirmCheckBox->sizeHint() : QSize();
        /* Expanded size-hint contains full-size label: */
        m_expandedSizeHint = m_labelSizeHint;
        /* Expanded size-hint can contain check-box: */
        if (m_checkBoxSizeHint.isValid())
        {
            m_expandedSizeHint.setWidth(qMax(m_expandedSizeHint.width(), m_checkBoxSizeHint.width()));
            m_expandedSizeHint.setHeight(m_expandedSizeHint.height() + m_iLayoutSpacing + m_checkBoxSizeHint.height());
        }
    }

    /* Update current size-hint: */
    m_minimumSizeHint = m_fFocused ? m_expandedSizeHint : m_collapsedSizeHint;

    /* Update animation: */
    if (m_pAnimation)
        m_pAnimation->update();

    /* Notify parent popup-pane: */
    emit sigSizeHintChanged();
}

