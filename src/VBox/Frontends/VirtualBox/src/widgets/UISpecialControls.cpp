/* $Id$ */
/** @file
 * VBox Qt GUI - UISpecialControls implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
# include <QHBoxLayout>
# ifndef VBOX_DARWIN_USE_NATIVE_CONTROLS
#  include <QBitmap>
#  include <QMouseEvent>
#  include <QPainter>
#  include <QSignalMapper>
# endif

/* GUI includes: */
# include "UIIconPool.h"
# include "UISpecialControls.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS


/*********************************************************************************************************************************
*   Class UIMiniCancelButton implementation.                                                                                     *
*********************************************************************************************************************************/

UIMiniCancelButton::UIMiniCancelButton(QWidget *pParent /* = 0 */)
    : QAbstractButton(pParent)
{
    setShortcut(QKeySequence(Qt::Key_Escape));
    m_pButton = new UICocoaButton(this, UICocoaButton::CancelButton);
    connect(m_pButton, SIGNAL(clicked()), this, SIGNAL(clicked()));
    setFixedSize(m_pButton->size());
}

void UIMiniCancelButton::resizeEvent(QResizeEvent *)
{
    m_pButton->resize(size());
}


/*********************************************************************************************************************************
*   Class UIHelpButton implementation.                                                                                           *
*********************************************************************************************************************************/

UIHelpButton::UIHelpButton(QWidget *pParent /* = 0 */)
    : QPushButton(pParent)
{
    setShortcut(QKeySequence(QKeySequence::HelpContents));
    m_pButton = new UICocoaButton(this, UICocoaButton::HelpButton);
    connect(m_pButton, SIGNAL(clicked()), this, SIGNAL(clicked()));
    setFixedSize(m_pButton->size());
}


#else /* !VBOX_DARWIN_USE_NATIVE_CONTROLS */


/*********************************************************************************************************************************
*   Class UIMiniCancelButton implementation.                                                                                     *
*********************************************************************************************************************************/

UIMiniCancelButton::UIMiniCancelButton(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QIToolButton>(pParent)
{
    setAutoRaise(true);
    setFocusPolicy(Qt::TabFocus);
    setShortcut(QKeySequence(Qt::Key_Escape));
    setIcon(UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_DialogCancel));
}


/*********************************************************************************************************************************
*   Class UIHelpButton implementation.                                                                                           *
*********************************************************************************************************************************/

/* From: src/gui/styles/qmacstyle_mac.cpp */
static const int PushButtonLeftOffset = 6;
static const int PushButtonTopOffset = 4;
static const int PushButtonRightOffset = 12;
static const int PushButtonBottomOffset = 4;

UIHelpButton::UIHelpButton(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QPushButton>(pParent)
{
# ifdef VBOX_WS_MAC
    m_pButtonPressed = false;
    m_pNormalPixmap = new QPixmap(":/help_button_normal_mac_22px.png");
    m_pPressedPixmap = new QPixmap(":/help_button_pressed_mac_22px.png");
    m_size = m_pNormalPixmap->size();
    m_pMask = new QImage(m_pNormalPixmap->mask().toImage());
    m_BRect = QRect(PushButtonLeftOffset,
                    PushButtonTopOffset,
                    m_size.width(),
                    m_size.height());
# endif /* VBOX_WS_MAC */

    /* Apply language settings: */
    retranslateUi();
}

void UIHelpButton::initFrom(QPushButton *pOther)
{
    /* Copy settings from pOther: */
    setIcon(pOther->icon());
    setText(pOther->text());
    setShortcut(pOther->shortcut());
    setFlat(pOther->isFlat());
    setAutoDefault(pOther->autoDefault());
    setDefault(pOther->isDefault());

    /* Apply language settings: */
    retranslateUi();
}

void UIHelpButton::retranslateUi()
{
    QPushButton::setText(tr("&Help"));
    if (QPushButton::shortcut().isEmpty())
        QPushButton::setShortcut(QKeySequence::HelpContents);
}

# ifdef VBOX_WS_MAC
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

void UIHelpButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawPixmap(PushButtonLeftOffset, PushButtonTopOffset, m_pButtonPressed ? *m_pPressedPixmap: *m_pNormalPixmap);
}

bool UIHelpButton::hitButton(const QPoint &position) const
{
    if (m_BRect.contains(position))
        return m_pMask->pixel(position.x() - PushButtonLeftOffset,
                              position.y() - PushButtonTopOffset) == 0xff000000;
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

void UIHelpButton::leaveEvent(QEvent *pEvent)
{
    QPushButton::leaveEvent(pEvent);
    m_pButtonPressed = false;
    update();
}
# endif /* VBOX_WS_MAC */


#endif /* !VBOX_DARWIN_USE_NATIVE_CONTROLS */

