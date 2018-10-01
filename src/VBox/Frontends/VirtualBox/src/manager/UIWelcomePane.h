/* $Id$ */
/** @file
 * VBox Qt GUI - UIWelcomePane class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWelcomePane_h___
#define ___UIWelcomePane_h___

/* Qt includes: */
#include <QIcon>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QLabel;

/** QWidget subclass holding Welcome information about VirtualBox. */
class UIWelcomePane : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs Welcome pane passing @a pParent to the base-class. */
    UIWelcomePane(QWidget *pParent = 0);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Updates pixmap. */
    void updatePixmap();

    /** Holds the icon instance. */
    QIcon  m_icon;

    /** Holds the text label instance. */
    QLabel *m_pLabelText;
    /** Holds the icon label instance. */
    QLabel *m_pLabelIcon;
};

#endif /* !___UIWelcomePane_h___ */
