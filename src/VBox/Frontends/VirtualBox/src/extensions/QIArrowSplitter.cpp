/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowSplitter class implementation
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
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
#include "VBoxGlobal.h"
#include "QIArrowSplitter.h"

/* Qt includes */
#include <QHBoxLayout>
#include <QKeyEvent>

QIArrowSplitter::QIArrowSplitter (QWidget *aChild, QWidget *aParent)
    : QWidget (aParent)
    , mMainLayout (new QVBoxLayout (this))
    , mSwitchButton (new QIArrowButtonSwitch())
    , mBackButton (new QIArrowButtonPress (false, tr ("&Back")))
    , mNextButton (new QIArrowButtonPress (true,  tr ("&Next")))
    , mChild (aChild)
{
    /* Setup main-layout */
    VBoxGlobal::setLayoutMargin (mMainLayout, 0);

    /* Setup buttons */
    mBackButton->setVisible (false);
    mNextButton->setVisible (false);

    /* Setup connections */
    connect (mSwitchButton, SIGNAL (clicked()), this, SLOT (toggleWidget()));
    connect (mBackButton, SIGNAL (clicked()), this, SIGNAL (showBackDetails()));
    connect (mNextButton, SIGNAL (clicked()), this, SIGNAL (showNextDetails()));

    /* Setup button layout */
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    VBoxGlobal::setLayoutMargin (buttonLayout, 0);
    buttonLayout->setSpacing (0);
    buttonLayout->addWidget (mSwitchButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget (mBackButton);
    buttonLayout->addWidget (mNextButton);

    /* Append layout with children */
    mMainLayout->addLayout (buttonLayout);
    mMainLayout->addWidget (mChild);

    /* Install event-filter */
    qApp->installEventFilter (this);
}

void QIArrowSplitter::setMultiPaging (bool aMultiPage)
{
    mBackButton->setVisible (aMultiPage);
    mNextButton->setVisible (aMultiPage);
}

void QIArrowSplitter::setButtonEnabled (bool aNext, bool aEnabled)
{
    aNext ? mNextButton->setEnabled (aEnabled)
          : mBackButton->setEnabled (aEnabled);
}

void QIArrowSplitter::setName (const QString &aName)
{
    mSwitchButton->setText (aName);
    emit sigSizeChanged();
}

void QIArrowSplitter::toggleWidget()
{
    mChild->setVisible (mSwitchButton->isExpanded());
    emit sigSizeChanged();
}

bool QIArrowSplitter::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only parent window children */
    if (!(aObject == window() || window()->children().contains (aObject)))
        return QWidget::eventFilter (aObject, aEvent);

    /* Do not process QIArrowButtonSwitch & QIArrowButtonPress children */
    if (aObject == mSwitchButton ||
        aObject == mBackButton ||
        aObject == mNextButton ||
        mSwitchButton->children().contains (aObject) ||
        mBackButton->children().contains (aObject) ||
        mNextButton->children().contains (aObject))
        return QWidget::eventFilter (aObject, aEvent);

    /* Process some keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        switch (kEvent->key())
        {
            case Qt::Key_Plus:
            {
                if (!mSwitchButton->isExpanded())
                    mSwitchButton->animateClick();
                break;
            }
            case Qt::Key_Minus:
            {
                if (mSwitchButton->isExpanded())
                    mSwitchButton->animateClick();
                break;
            }
            case Qt::Key_PageUp:
            {
                if (mNextButton->isEnabled())
                    mNextButton->animateClick();
                break;
            }
            case Qt::Key_PageDown:
            {
                if (mBackButton->isEnabled())
                    mBackButton->animateClick();
                break;
            }
        }
    }

    /* Default one handler */
    return QWidget::eventFilter (aObject, aEvent);
}

