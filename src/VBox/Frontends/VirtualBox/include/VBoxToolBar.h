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

#ifndef ___VBoxToolBar_h___
#define ___VBoxToolBar_h___

#include <QGlobalStatic> /* for Q_WS_MAC */
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif

/* Qt includes */
#include <QLayout>
#include <QMainWindow>
#include <QToolBar>

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
        , mMainWindow (qobject_cast <QMainWindow*> (aParent))
    {
        setFloatable (false);
        setMovable (false);

        /* Remove that ugly frame panel around the toolbar.
         * Doing that currently for Cleanlooks & Windows styles. */
        if (qobject_cast <QCleanlooksStyle*> (QToolBar::style()) ||
            qobject_cast <QWindowsStyle*> (QToolBar::style()))
            setStyleSheet ("QToolBar { border: 0px none black; }");

        if (layout())
            layout()->setContentsMargins (0, 0, 0, 0);;

        setContextMenuPolicy (Qt::NoContextMenu);
    }

#ifdef Q_WS_MAC
    void setMacToolbar()
    {
        if (mMainWindow)
        {
            mMainWindow->setUnifiedTitleAndToolBarOnMac (true);
#ifndef QT_MAC_USE_COCOA
            WindowRef window = ::darwinToNativeWindow (this);
            EventHandlerUPP eventHandler = ::NewEventHandlerUPP (VBoxToolBar::macEventFilter);
            EventTypeSpec eventTypes[2];
            eventTypes[0].eventClass = kEventClassMouse;
            eventTypes[0].eventKind  = kEventMouseDown;
            eventTypes[1].eventClass = kEventClassMouse;
            eventTypes[1].eventKind  = kEventMouseUp;
            InstallWindowEventHandler (window, eventHandler,
                                       RT_ELEMENTS (eventTypes), eventTypes,
                                       NULL, NULL);
#endif /* !QT_MAC_USE_COCOA */
        }
    }

#ifndef QT_MAC_USE_COCOA
    static pascal OSStatus macEventFilter (EventHandlerCallRef aNextHandler,
                                           EventRef aEvent, void * /* aUserData */)
    {
        UInt32 eclass = GetEventClass (aEvent);
        if (eclass == kEventClassMouse)
        {
            WindowPartCode partCode;
            GetEventParameter (aEvent, kEventParamWindowPartCode, typeWindowPartCode, NULL, sizeof (WindowPartCode), NULL, &partCode);
            UInt32 ekind = GetEventKind (aEvent);
            if (partCode == 15 ||
                partCode == 4)
                if(ekind == kEventMouseDown || ekind == kEventMouseUp)
                {
                    EventMouseButton button = 0;
                    GetEventParameter (aEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof (button), NULL, &button);
                    if (button != kEventMouseButtonPrimary)
                        return noErr;
                }
        }
        return CallNextEventHandler (aNextHandler, aEvent);
    }
#endif /* !QT_MAC_USE_COCOA */

    void setShowToolBarButton (bool aShow)
    {
        ::darwinSetShowsToolbarButton (this, aShow);
    }
#endif /* Q_WS_MAC */

    void setUsesTextLabel (bool aEnable)
    {
        Qt::ToolButtonStyle tbs = Qt::ToolButtonTextUnderIcon;
        if (!aEnable)
            tbs = Qt::ToolButtonIconOnly;

        if (mMainWindow)
            mMainWindow->setToolButtonStyle (tbs);
        else
            setToolButtonStyle (tbs);
    }

private:

    QMainWindow *mMainWindow;
};

#endif // !___VBoxToolBar_h___

