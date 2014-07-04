/** @file
 * VBox Qt GUI - QIArrowButtonSwitch class declaration.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIArrowButtonSwitch_h___
#define ___QIArrowButtonSwitch_h___

/* Qt includes: */
#include <QMap>
#include <QIcon>

/* GUI includes: */
#include "QIRichToolButton.h"

/** QIRichToolButton extension
  * representing arrow tool-button with text-label,
  * can be used as collaps/expand switch in various places. */
class QIArrowButtonSwitch : public QIRichToolButton
{
    Q_OBJECT;

public:

    /** Button states. */
    enum ButtonState { ButtonState_Collapsed, ButtonState_Expanded };

    /** Constructor, passes @a pParent to the QIRichToolButton constructor. */
    QIArrowButtonSwitch(QWidget *pParent = 0);

    /** Defines the @a icon for the @a buttonState. */
    void setIconForButtonState(ButtonState buttonState, const QIcon &icon);

    /** Returns whether button-state is ButtonState_Expanded. */
    bool isExpanded() const { return m_buttonState == ButtonState_Expanded; }

protected slots:

    /** Button-click handler. */
    virtual void sltButtonClicked();

protected:

    /** Key-press-event handler. */
    virtual void keyPressEvent(QKeyEvent *pEvent);

private:

    /** Updates icon according button-state. */
    void updateIcon();

    /** Holds the button-state. */
    ButtonState m_buttonState;
    /** Holds icons for button-states. */
    QMap<ButtonState, QIcon> m_icons;
};

#endif /* !___QIArrowButtonSwitch_h___ */
