/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIAbstractDialog class implementation
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

#include "QIAbstractDialog.h"

/* Qt includes */
#include <QApplication>
#include <QSizeGrip>
#include <QPushButton>
#include <QStatusBar>
#include <QKeyEvent>

QIAbstractDialog::QIAbstractDialog (QWidget *aParent,
                                    Qt::WindowFlags aFlags)
    : QMainWindow (aParent, aFlags)
{
}

void QIAbstractDialog::initializeDialog()
{
    /* Search the default button */
    mDefaultButton = searchDefaultButton();

    /* Statusbar initially disabled */
    setStatusBar (NULL);

    /* Setup size grip */
    mSizeGrip = new QSizeGrip (centralWidget());
    mSizeGrip->resize (mSizeGrip->sizeHint());

    qApp->installEventFilter (this);
}

bool QIAbstractDialog::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!isActiveWindow())
        return QMainWindow::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        /* Auto-default button focus-in processor used to move the "default"
         * button property into the currently focused button */
        case QEvent::FocusIn:
        {
            if (aObject->inherits ("QPushButton") &&
                qobject_cast<QPushButton*> (aObject)->autoDefault())
            {
                qobject_cast<QPushButton*> (aObject)->
                    setDefault (aObject != mDefaultButton);
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject == mDefaultButton);
            }
            break;
        }
        /* Auto-default button focus-out processor used to remove the "default"
         * button property from the previously focused button */
        case QEvent::FocusOut:
        {
            if (aObject->inherits ("QPushButton") &&
                qobject_cast<QPushButton*> (aObject)->autoDefault())
            {
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject != mDefaultButton);
                qobject_cast<QPushButton*> (aObject)->
                    setDefault (aObject == mDefaultButton);
            }
            break;
        }
        /* Processing Enter&Escape keys as it done for QDialog */
        case QEvent::KeyPress:
        {
            QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);
            if (!aObject->inherits ("QPushButton") &&
                (e->QInputEvent::modifiers() == 0 ||
                 (e->QInputEvent::modifiers() & Qt::KeypadModifier &&
                  e->key() == Qt::Key_Enter)))
            {
                switch (e->key())
                {
                    /* Processing the return keypress for the auto-default button */
                    case Qt::Key_Enter:
                    case Qt::Key_Return:
                    {
                        QPushButton *currentDefault = searchDefaultButton();
                        if (currentDefault)
                            currentDefault->animateClick();
                        break;
                    }
                    /* Processing the escape keypress as the close dialog action */
                    case Qt::Key_Escape:
                    {
                        close();
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        default:
            break;
    }
    return QMainWindow::eventFilter (aObject, aEvent);
}

void QIAbstractDialog::resizeEvent (QResizeEvent *aEvent)
{
    QMainWindow::resizeEvent (aEvent);

    /* Adjust the size-grip location for the current resize event */
    mSizeGrip->move (centralWidget()->rect().bottomRight() -
                     QPoint (mSizeGrip->rect().width() - 1,
                     mSizeGrip->rect().height() - 1));
}

QPushButton* QIAbstractDialog::searchDefaultButton()
{
    /* This mechanism is used for searching the default dialog button
     * and similar the same mechanism in Qt::QDialog inner source */
    QPushButton *button = 0;
    QList<QPushButton*> list = findChildren<QPushButton*>();
    foreach (QObject *aObj, list)
    {
        button = qobject_cast<QPushButton*> (aObj);
        if (button->isDefault())
            break;
    }
    return button;
}

