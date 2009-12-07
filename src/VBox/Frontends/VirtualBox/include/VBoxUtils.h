/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBoxUtils_h___
#define ___VBoxUtils_h___

#include <iprt/types.h>

/* Qt includes */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/**
 *  Simple class that filters out all key presses and releases
 *  got while the Alt key is pressed. For some very strange reason,
 *  QLineEdit accepts those combinations that are not used as accelerators,
 *  and inserts the corresponding characters to the entry field.
 */
class QIAltKeyFilter : protected QObject
{
    Q_OBJECT;

public:

    QIAltKeyFilter (QObject *aParent) :QObject (aParent) {}

    void watchOn (QObject *aObject) { aObject->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /* aObject */, QEvent *aEvent)
    {
        if (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::KeyRelease)
        {
            QKeyEvent *pEvent = static_cast<QKeyEvent *> (aEvent);
            if (pEvent->modifiers() & Qt::AltModifier)
                return true;
        }
        return false;
    }
};

/**
 *  Simple class which simulates focus-proxy rule redirecting widget
 *  assigned shortcut to desired widget.
 */
class QIFocusProxy : protected QObject
{
    Q_OBJECT;

public:

    QIFocusProxy (QWidget *aFrom, QWidget *aTo)
        : QObject (aFrom), mFrom (aFrom), mTo (aTo)
    {
        mFrom->installEventFilter (this);
    }

protected:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject == mFrom && aEvent->type() == QEvent::Shortcut)
        {
            mTo->setFocus();
            return true;
        }
        return QObject::eventFilter (aObject, aEvent);
    }

    QWidget *mFrom;
    QWidget *mTo;
};

/**
 *  QTextEdit reimplementation to feat some extended requirements.
 */
class QRichTextEdit : public QTextEdit
{
    Q_OBJECT;

public:

    QRichTextEdit (QWidget *aParent) : QTextEdit (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextEdit::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

/**
 *  QTextBrowser reimplementation to feat some extended requirements.
 */
class QRichTextBrowser : public QTextBrowser
{
    Q_OBJECT;

public:

    QRichTextBrowser (QWidget *aParent) : QTextBrowser (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextBrowser::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

#endif // !___VBoxUtils_h___

