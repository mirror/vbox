/* $Id$ */
/** @file
 * VBox Qt GUI - QIMainWindow class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
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
#include <QResizeEvent>

/* GUI includes: */
#include "QIMainWindow.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif
#ifdef VBOX_WS_X11
# include "UICommon.h"
# include "UIDesktopWidgetWatchdog.h"
#endif

/* Other VBox includes: */
#ifdef VBOX_WS_MAC
# include "iprt/cpp/utils.h"
#endif


QIMainWindow::QIMainWindow(QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = 0 */)
    : QMainWindow(pParent, enmFlags)
{
}

void QIMainWindow::moveEvent(QMoveEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindow::moveEvent(pEvent);

#ifdef VBOX_WS_X11
    /* Prevent further handling if fake screen detected: */
    if (gpDesktop->isFakeScreenDetected())
        return;
#endif

    /* Prevent handling for yet/already invisible window or if window is in minimized state: */
    if (isVisible() && (windowState() & Qt::WindowMinimized) == 0)
    {
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
        /* Use the old approach for OSX/Win: */
        m_geometry.moveTo(frameGeometry().x(), frameGeometry().y());
#else
        /* Use the new approach otherwise: */
        m_geometry.moveTo(geometry().x(), geometry().y());
#endif
    }
}

void QIMainWindow::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindow::resizeEvent(pEvent);

#ifdef VBOX_WS_X11
    /* Prevent handling if fake screen detected: */
    if (gpDesktop->isFakeScreenDetected())
        return;
#endif

    /* Prevent handling for yet/already invisible window or if window is in minimized state: */
    if (isVisible() && (windowState() & Qt::WindowMinimized) == 0)
    {
        QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
        m_geometry.setSize(pResizeEvent->size());
    }
}

void QIMainWindow::restoreGeometry(const QRect &rect)
{
    m_geometry = rect;
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /* Use the old approach for OSX/Win: */
    move(m_geometry.topLeft());
    resize(m_geometry.size());
#else
    /* Use the new approach otherwise: */
    UICommon::setTopLevelGeometry(this, m_geometry);
#endif

    /* Maximize (if necessary): */
    if (shouldBeMaximized())
        showMaximized();
}

QRect QIMainWindow::currentGeometry() const
{
    return m_geometry;
}

bool QIMainWindow::isCurrentlyMaximized() const
{
#ifdef VBOX_WS_MAC
    return ::darwinIsWindowMaximized(unconst(this));
#else
    return isMaximized();
#endif
}
