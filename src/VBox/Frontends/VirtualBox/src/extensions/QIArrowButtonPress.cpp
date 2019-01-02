/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIArrowButtonPress class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QKeyEvent>

/* GUI includes: */
#include "QIArrowButtonPress.h"


QIArrowButtonPress::QIArrowButtonPress(QIArrowButtonPress::ButtonType enmButtonType,
                                       QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QIRichToolButton>(pParent)
    , m_enmButtonType(enmButtonType)
{
    /* Retranslate UI: */
    retranslateUi();
}

void QIArrowButtonPress::retranslateUi()
{
    /* Retranslate: */
    switch (m_enmButtonType)
    {
        case ButtonType_Back: setText(tr("&Back")); break;
        case ButtonType_Next: setText(tr("&Next")); break;
        default: break;
    }
}

void QIArrowButtonPress::keyPressEvent(QKeyEvent *pEvent)
{
    /* Handle different keys: */
    switch (pEvent->key())
    {
        /* Animate-click for the Space key: */
        case Qt::Key_PageUp:   if (m_enmButtonType == ButtonType_Next) return animateClick(); break;
        case Qt::Key_PageDown: if (m_enmButtonType == ButtonType_Back) return animateClick(); break;
        default: break;
    }
    /* Call to base-class: */
    QIWithRetranslateUI<QIRichToolButton>::keyPressEvent(pEvent);
}
