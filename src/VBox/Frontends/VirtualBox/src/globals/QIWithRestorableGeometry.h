/* $Id$ */
/** @file
 * VBox Qt GUI - QIWithRestorableGeometry class declaration.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_QIWithRestorableGeometry_h
#define FEQT_INCLUDED_SRC_globals_QIWithRestorableGeometry_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>
#include <QRect>
#include <QResizeEvent>

/* GUI includes: */
#include "UILibraryDefs.h"
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

/** Template with geometry saving/restoring capabilities. */
template <class Base>
class QIWithRestorableGeometry : public Base
{
public:

    /** Constructs main window passing @a pParent and @a enmFlags to base-class. */
    QIWithRestorableGeometry(QWidget *pParent = 0, Qt::WindowFlags enmFlags = 0)
        : Base(pParent, enmFlags)
    {}

protected:

    /** Handles move @a pEvent. */
    virtual void moveEvent(QMoveEvent *pEvent) /* override */
    {
        /* Call to base-class: */
        QMainWindow::moveEvent(pEvent);

#ifdef VBOX_WS_X11
        /* Prevent further handling if fake screen detected: */
        if (gpDesktop->isFakeScreenDetected())
            return;
#endif

        /* Prevent handling for yet/already invisible window or if window is in minimized state: */
        if (this->isVisible() && (this->windowState() & Qt::WindowMinimized) == 0)
        {
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
            /* Use the old approach for OSX/Win: */
            m_geometry.moveTo(this->frameGeometry().x(), this->frameGeometry().y());
#else
            /* Use the new approach otherwise: */
            m_geometry.moveTo(this->geometry().x(), this->geometry().y());
#endif
        }
    }

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */
    {
        /* Call to base-class: */
        QMainWindow::resizeEvent(pEvent);

#ifdef VBOX_WS_X11
        /* Prevent handling if fake screen detected: */
        if (gpDesktop->isFakeScreenDetected())
            return;
#endif

        /* Prevent handling for yet/already invisible window or if window is in minimized state: */
        if (this->isVisible() && (this->windowState() & Qt::WindowMinimized) == 0)
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            m_geometry.setSize(pResizeEvent->size());
        }
    }

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const { return false; }

    /** Restores the window geometry to passed @a rect. */
    void restoreGeometry(const QRect &rect)
    {
        m_geometry = rect;
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
        /* Use the old approach for OSX/Win: */
        this->move(m_geometry.topLeft());
        this->resize(m_geometry.size());
#else
        /* Use the new approach otherwise: */
        UICommon::setTopLevelGeometry(this, m_geometry);
#endif

        /* Maximize (if necessary): */
        if (shouldBeMaximized())
            this->showMaximized();
    }

    /** Returns current window geometry. */
    QRect currentGeometry() const
    {
        return m_geometry;
    }

    /** Returns whether the window is currently maximized. */
    bool isCurrentlyMaximized() const
    {
#ifdef VBOX_WS_MAC
        return ::darwinIsWindowMaximized(unconst(this));
#else
        return this->isMaximized();
#endif
    }

private:

    /** Holds the cached window geometry. */
    QRect m_geometry;
};

/** Explicit QIWithRestorableGeometry instantiation for QMainWindow class.
  * @note  On Windows it's important that all template cases are instantiated just once across
  *        the linking space. In case we have particular template case instantiated from both
  *        library and executable sides, - we have multiple definition case and need to strictly
  *        ask compiler to do it just once and link such cases against library only.
  *        I would also note that it would be incorrect to just make whole the template exported
  *        to library because latter can have lack of required instantiations (current case). */
template class SHARED_LIBRARY_STUFF QIWithRestorableGeometry<QMainWindow>;

#endif /* !FEQT_INCLUDED_SRC_globals_QIWithRestorableGeometry_h */
