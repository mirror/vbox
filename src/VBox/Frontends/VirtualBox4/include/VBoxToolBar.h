/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxToolBar class declaration & implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __VBoxToolBar_h__
#define __VBoxToolBar_h__

/* Qt includes */
#include <QToolBar>
#include <QMainWindow>

/* Note: This styles are available on _all_ platforms. */
#include <QCleanlooksStyle>
#include <QWindowsStyle>

/**
 *  The VBoxToolBar class is a simple QToolBar reimplementation to disable
 *  its built-in context menu and add some default behavior we need.
 */
class VBoxToolBar : public QToolBar
{

public:

    VBoxToolBar (QWidget *aParent)
        : QToolBar (aParent)
        , mMainWindow (qobject_cast<QMainWindow*> (aParent))
    {
        setFloatable (false);
        setMovable (false);
        if (layout())
            layout()->setContentsMargins (0, 0, 0, 0);;
        
        setContextMenuPolicy (Qt::NoContextMenu);

        /* Remove that ugly frame panel around the toolbar. */
        /* I'm not sure if we should do this generally on linux for that mass
         * of KDE styles. But maybe some of them are based on CleanLooks so
         * they are looking ok also. */
        QStyle *style = NULL;
        if (!style)
            /* Check for cleanlooks style */
            style = qobject_cast<QCleanlooksStyle*> (QToolBar::style());
        if (!style)
            /* Check for windows style */
            style = qobject_cast<QWindowsStyle*> (QToolBar::style());
        if (style)
            setStyleSheet ("QToolBar { border: 0px none black; }");
    }


    /**
     *  Substitutes for QMainWindow::setUsesBigPixmaps() when QMainWindow is
     *  not used (otherwise just redirects the call to #mainWindow()).
     */
    void setUsesBigPixmaps (bool enable)
    {
        QSize size (24, 24);
        if (!enable)
            size = QSize (16, 16);

        if (mMainWindow)
            mMainWindow->setIconSize (size);
        else
            setIconSize (size);
    }

    void setUsesTextLabel (bool enable)
    {
        Qt::ToolButtonStyle tbs = Qt::ToolButtonTextUnderIcon;
        if (!enable)
            tbs = Qt::ToolButtonIconOnly;

        if (mMainWindow)
            mMainWindow->setToolButtonStyle (tbs);
        else
            setToolButtonStyle (tbs);
    }

private:

    QMainWindow *mMainWindow;
};

#endif // __VBoxToolBar_h__

