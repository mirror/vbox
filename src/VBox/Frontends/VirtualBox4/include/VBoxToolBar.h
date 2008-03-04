/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxToolBar class declaration & implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxToolBar_h__
#define __VBoxToolBar_h__

/* Qt includes */
#include <QToolBar>
#include <QToolButton>
#include <QMainWindow>
#include <QContextMenuEvent>
#ifdef Q_WS_MAC
//# include "VBoxAquaStyle.h"
#endif

/**
 *  The VBoxToolBar class is a simple QToolBar reimplementation to disable
 *  its built-in context menu and add some default behavior we need.
 */
class VBoxToolBar : public QToolBar
{
public:

    VBoxToolBar (QMainWindow *aMainWindow, QWidget *aParent)
        : QToolBar (aParent),
          mMainWindow (aMainWindow)
    {
        setFloatable (false);
        setMovable (false);
    };

    /** Reimplements and does nothing to disable the context menu */
    void contextMenuEvent (QContextMenuEvent *) {};

    /**
     *  Substitutes for QMainWindow::setUsesBigPixmaps() when QMainWindow is
     *  not used (otherwise just redirects the call to #mainWindow()).
     */
    void setUsesBigPixmaps (bool enable)
    {
        QSize bigSize(32, 32);

        if (mMainWindow)
            mMainWindow->setIconSize (bigSize);
        else
            setIconSize (bigSize);
    }

    void setUsesTextLabel (bool enable)
    {
        if (mMainWindow)
            mMainWindow->setToolButtonStyle (Qt::ToolButtonTextUnderIcon);
        else
            setToolButtonStyle (Qt::ToolButtonTextUnderIcon);
    }

#ifdef Q_WS_MAC
    /** 
     * This is a temporary hack, we'll set the style globally later. 
     */
#warning port me
    void setMacStyle() 
    {
        /* self */
//        QStyle *qs = &VBoxAquaStyle::instance();
//        setStyle(qs);
//
//        /* the buttons */
//        QObjectList *list = queryList ("QToolButton");
//        QObjectListIt it (*list);
//        QObject *obj;
//        while ((obj = it.current()) != 0)
//        {
//            QToolButton *btn = ::qt_cast <QToolButton *> (obj);
//            btn->setStyle (&VBoxAquaStyle::instance());
//            ++ it;
//        }
//        delete list;

        /** @todo the separator */
    }
#endif 
private:
    QMainWindow *mMainWindow;
};

#endif // __VBoxToolBar_h__
