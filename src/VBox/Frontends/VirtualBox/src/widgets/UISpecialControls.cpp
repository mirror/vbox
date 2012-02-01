/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSpecialButtons implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox includes */
#include "UIIconPool.h"
#include "UISpecialControls.h"

/* Global includes */
#include <QHBoxLayout>

#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS

/********************************************************************************
 *
 * A mini cancel button in the native Cocoa version.
 *
 ********************************************************************************/
UIMiniCancelButton::UIMiniCancelButton(QWidget *pParent /* = 0 */)
  : QAbstractButton(pParent)
{
    setShortcut(QKeySequence(Qt::Key_Escape));
    m_pButton = new UICocoaButton(UICocoaButton::CancelButton, this);
    connect(m_pButton, SIGNAL(clicked()),
            this, SIGNAL(clicked()));
    setFixedSize(m_pButton->size());
}

void UIMiniCancelButton::resizeEvent(QResizeEvent * /* pEvent */)
{
    m_pButton->resize(size());
}

/********************************************************************************
 *
 * A rest button in the native Cocoa version.
 *
 ********************************************************************************/
UIResetButton::UIResetButton(QWidget *pParent /* = 0 */)
  : QAbstractButton(pParent)
{
    m_pButton = new UICocoaButton(UICocoaButton::ResetButton, this);
    connect(m_pButton, SIGNAL(clicked()),
            this, SIGNAL(clicked()));
    setFixedSize(m_pButton->size());
}

void UIResetButton::resizeEvent(QResizeEvent * /* pEvent */)
{
    m_pButton->resize(size());
}

/********************************************************************************
 *
 * A help button in the native Cocoa version.
 *
 ********************************************************************************/
UIHelpButton::UIHelpButton(QWidget *pParent /* = 0 */)
  : QPushButton(pParent)
{
    setShortcut(QKeySequence(QKeySequence::HelpContents));
    m_pButton = new UICocoaButton(UICocoaButton::HelpButton, this);
    connect(m_pButton, SIGNAL(clicked()),
            this, SIGNAL(clicked()));
    setFixedSize(m_pButton->size());
}

/********************************************************************************
 *
 * A segmented button in the native Cocoa version.
 *
 ********************************************************************************/
UIRoundRectSegmentedButton::UIRoundRectSegmentedButton(int cCount, QWidget *pParent /* = 0 */)
  : UICocoaSegmentedButton(cCount, UICocoaSegmentedButton::RoundRectSegment, pParent)
{
}

UITexturedSegmentedButton::UITexturedSegmentedButton(int cCount, QWidget *pParent /* = 0 */)
  : UICocoaSegmentedButton(cCount, UICocoaSegmentedButton::TexturedRoundedSegment, pParent)
{
}

/********************************************************************************
 *
 * A search field in the native Cocoa version.
 *
 ********************************************************************************/
UISearchField::UISearchField(QWidget *pParent /* = 0 */)
  : UICocoaSearchField(pParent)
{
}

#else /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

/* Qt includes */
#include <QPainter>
#include <QBitmap>
#include <QMouseEvent>
#include <QSignalMapper>

/********************************************************************************
 *
 * A mini cancel button for the other OS's.
 *
 ********************************************************************************/
UIMiniCancelButton::UIMiniCancelButton(QWidget *pParent /* = 0 */)
  : QIWithRetranslateUI<QIToolButton>(pParent)
{
    setAutoRaise(true);
    setFocusPolicy(Qt::TabFocus);
    setShortcut(QKeySequence(Qt::Key_Escape));
    setIcon(UIIconPool::defaultIcon(UIIconPool::DialogCancelIcon));
}

/********************************************************************************
 *
 * A help button for the other OS's.
 *
 ********************************************************************************/
/* From: src/gui/styles/qmacstyle_mac.cpp */
static const int PushButtonLeftOffset = 6;
static const int PushButtonTopOffset = 4;
static const int PushButtonRightOffset = 12;
static const int PushButtonBottomOffset = 4;

UIHelpButton::UIHelpButton(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QPushButton>(pParent)
{
#ifdef Q_WS_MAC
    m_pButtonPressed = false;
    m_pNormalPixmap = new QPixmap(":/help_button_normal_mac_22px.png");
    m_pPressedPixmap = new QPixmap(":/help_button_pressed_mac_22px.png");
    m_size = m_pNormalPixmap->size();
    m_pMask = new QImage(m_pNormalPixmap->mask().toImage());
    m_BRect = QRect(PushButtonLeftOffset,
                    PushButtonTopOffset,
                    m_size.width(),
                    m_size.height());
#endif /* Q_WS_MAC */
    /* Applying language settings */
    retranslateUi();
}

void UIHelpButton::initFrom(QPushButton *pOther)
{
    setIcon(pOther->icon());
    setText(pOther->text());
    setShortcut(pOther->shortcut());
    setFlat(pOther->isFlat());
    setAutoDefault(pOther->autoDefault());
    setDefault(pOther->isDefault());
    /* Applying language settings */
    retranslateUi();
}

void UIHelpButton::retranslateUi()
{
    QPushButton::setText(tr("&Help"));
    if (QPushButton::shortcut().isEmpty())
        QPushButton::setShortcut(QKeySequence::HelpContents);
}

#ifdef Q_WS_MAC
UIHelpButton::~UIHelpButton()
{
    delete m_pNormalPixmap;
    delete m_pPressedPixmap;
    delete m_pMask;
}

QSize UIHelpButton::sizeHint() const
{
    return QSize(m_size.width() + PushButtonLeftOffset + PushButtonRightOffset,
                 m_size.height() + PushButtonTopOffset + PushButtonBottomOffset);
}

void UIHelpButton::paintEvent(QPaintEvent * /* pEvent */)
{
    QPainter painter(this);
    painter.drawPixmap(PushButtonLeftOffset, PushButtonTopOffset, m_pButtonPressed ? *m_pPressedPixmap: *m_pNormalPixmap);
}

bool UIHelpButton::hitButton(const QPoint &pos) const
{
    if (m_BRect.contains(pos))
        return m_pMask->pixel(pos.x() - PushButtonLeftOffset,
                              pos.y() - PushButtonTopOffset) == 0xff000000;
    else
        return false;
}

void UIHelpButton::mousePressEvent(QMouseEvent *pEvent)
{
    if (hitButton(pEvent->pos()))
        m_pButtonPressed = true;
    QPushButton::mousePressEvent(pEvent);
    update();
}

void UIHelpButton::mouseReleaseEvent(QMouseEvent *pEvent)
{
    QPushButton::mouseReleaseEvent(pEvent);
    m_pButtonPressed = false;
    update();
}

void UIHelpButton::leaveEvent(QEvent * pEvent)
{
    QPushButton::leaveEvent(pEvent);
    m_pButtonPressed = false;
    update();
}
#endif /* Q_WS_MAC */

/********************************************************************************
 *
 * A segmented button for the other OS's.
 *
 ********************************************************************************/
UIRoundRectSegmentedButton::UIRoundRectSegmentedButton(int aCount, QWidget *pParent /* = 0 */)
  : QWidget(pParent)
{
    m_pSignalMapper = new QSignalMapper(this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    for (int i=0; i < aCount; ++i)
    {
        QIToolButton *button = new QIToolButton(this);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::TabFocus);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_pButtons.append(button);
        layout->addWidget(button);
        connect(button, SIGNAL(clicked()),
                m_pSignalMapper, SLOT(map()));
        m_pSignalMapper->setMapping(button, i);
    }
    connect(m_pSignalMapper, SIGNAL(mapped(int)),
            this, SIGNAL(clicked(int)));

}

UIRoundRectSegmentedButton::~UIRoundRectSegmentedButton()
{
    delete m_pSignalMapper;
    qDeleteAll(m_pButtons);
}

void UIRoundRectSegmentedButton::setTitle(int iSegment, const QString &aTitle)
{
    m_pButtons.at(iSegment)->setText(aTitle);
}

void UIRoundRectSegmentedButton::setToolTip(int iSegment, const QString &strTip)
{
    m_pButtons.at(iSegment)->setToolTip(strTip);
}

void UIRoundRectSegmentedButton::setIcon(int iSegment, const QIcon &icon)
{
    m_pButtons.at(iSegment)->setIcon(icon);
}

void UIRoundRectSegmentedButton::setEnabled(int iSegment, bool fEnabled)
{
    m_pButtons.at(iSegment)->setEnabled(fEnabled);
}

void UIRoundRectSegmentedButton::animateClick(int iSegment)
{
    m_pButtons.at(iSegment)->animateClick();
}

UITexturedSegmentedButton::UITexturedSegmentedButton(int cCount, QWidget *pParent /* = 0 */)
  : UIRoundRectSegmentedButton(cCount, pParent)
{
    for (int i=0; i < m_pButtons.size(); ++i)
    {
        m_pButtons.at(i)->setAutoExclusive(true);
        m_pButtons.at(i)->setCheckable(true);
    }
}

/********************************************************************************
 *
 * A search field  for the other OS's.
 *
 ********************************************************************************/
UISearchField::UISearchField(QWidget *pParent /* = 0 */)
  : QLineEdit(pParent)
{
    m_baseBrush = palette().base();
}

void UISearchField::markError()
{
    QPalette pal = palette();
    QColor c(Qt::red);
    c.setAlphaF(0.3);
    pal.setBrush(QPalette::Base, c);
    setPalette(pal);
}

void UISearchField::unmarkError()
{
    QPalette pal = palette();
    pal.setBrush(QPalette::Base, m_baseBrush);
    setPalette(pal);
}

#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

