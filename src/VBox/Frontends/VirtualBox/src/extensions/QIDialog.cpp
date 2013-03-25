/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialog class implementation
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "QIDialog.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

QIDialog::QIDialog(QWidget *pParent /* = 0 */, Qt::WindowFlags flags /* = 0 */)
    : QDialog(pParent, flags)
    , m_fPolished(false)
{
}

QIDialog::~QIDialog()
{
}

void QIDialog::showEvent(QShowEvent * /* pEvent */)
{
    /* Two thinks to do for fixed size dialogs on MacOS X:
     * 1. Make sure the layout is polished and have the right size
     * 2. Disable that _unnecessary_ size grip (Bug in Qt?) */
    QSizePolicy policy = sizePolicy();
    if ((policy.horizontalPolicy() == QSizePolicy::Fixed &&
         policy.verticalPolicy() == QSizePolicy::Fixed) ||
        (windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        adjustSize();
        setFixedSize(size());
#ifdef Q_WS_MAC
        ::darwinSetShowsResizeIndicator (this, false);
#endif /* Q_WS_MAC */
    }

    /* Polishing: */
    if (m_fPolished)
        return;
    m_fPolished = true;

    /* Explicit centering according to our parent: */
    VBoxGlobal::centerWidget(this, parentWidget(), false);
}

int QIDialog::exec(bool fShow /* = true */)
{
    /* Reset the result-code: */
    setResult(QDialog::Rejected);

    bool fWasDeleteOnClose = testAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_DeleteOnClose, false);
#if defined(Q_WS_MAC) && QT_VERSION >= 0x040500
    /* After 4.5 Qt changed the behavior of Sheets for the window/application
     * modal case. See "New Ways of Using Dialogs" in
     * http://doc.trolltech.com/qq/QtQuarterly30.pdf why. We want the old
     * behavior back, where all modal windows where shown as sheets. So make
     * the modal mode window, but be application modal in any case. */
    Qt::WindowModality winModality = windowModality();
    bool wasSetWinModality = testAttribute(Qt::WA_SetWindowModality);
    if ((windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        setWindowModality(Qt::WindowModal);
        setAttribute(Qt::WA_SetWindowModality, false);
    }
#endif /* defined(Q_WS_MAC) && QT_VERSION >= 0x040500 */
    /* The dialog has to modal in any case.
     * Save the current modality to restore it later. */
    bool wasShowModal = testAttribute(Qt::WA_ShowModal);
    setAttribute(Qt::WA_ShowModal, true);

    /* Create a local event-loop: */
    QEventLoop eventLoop;
    m_pEventLoop = &eventLoop;

    /* Show ourself if requested: */
    if (fShow)
        show();

    /* Guard ourself for the case
     * we destroyed ourself in our event-loop: */
    QPointer<QIDialog> guard = this;

    /* Start the blocking event-loop: */
    eventLoop.exec();

    /* Are we still valid? */
    if (guard.isNull())
        return QDialog::Rejected;

    m_pEventLoop = 0;

    /* Save the result-code early (we can delete ourself on close): */
    QDialog::DialogCode resultCode = (QDialog::DialogCode)result();

#if defined(Q_WS_MAC) && QT_VERSION >= 0x040500
    /* Restore old modality mode: */
    if ((windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        setWindowModality(winModality);
        setAttribute(Qt::WA_SetWindowModality, wasSetWinModality);
    }
#endif /* defined(Q_WS_MAC) && QT_VERSION >= 0x040500 */

    /* Set the old show modal attribute */
    setAttribute (Qt::WA_ShowModal, wasShowModal);
    /* Delete ourself if we should do that on close: */
    if (fWasDeleteOnClose)
        delete this;

    /* Return the result-code: */
    return resultCode;
}

void QIDialog::setVisible(bool fVisible)
{
    /* Call to base-class: */
    QDialog::setVisible(fVisible);

    /* Exit from the event-loop if
     * 1. there is any and
     * 2. we are changing our state from visible to invisible: */
    if (m_pEventLoop && !fVisible)
        m_pEventLoop->exit();
}

