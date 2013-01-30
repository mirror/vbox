/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIExtraDataEventHandler class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIExtraDataEventHandler_h__
#define __UIExtraDataEventHandler_h__

/* COM includes: */
#include "CEventListener.h"

/* Forward declarations: */
class UIExtraDataEventHandlerPrivate;

class UIExtraDataEventHandler: public QObject
{
    Q_OBJECT;

public:
    static UIExtraDataEventHandler* instance();
    static void destroy();

signals:
    /* Specialized extra data signals */
    void sigGUILanguageChange(QString strLang);
    void sigSelectorShortcutsChanged();
    void sigMachineShortcutsChanged();
#ifdef VBOX_GUI_WITH_SYSTRAY
    void sigMainWindowCountChange(int count);
    void sigCanShowTrayIcon(bool fEnabled);
    void sigTrayIconChange(bool fEnabled);
#endif /* VBOX_GUI_WITH_SYSTRAY */
#ifdef RT_OS_DARWIN
    void sigPresentationModeChange(bool fEnabled);
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

private:
    UIExtraDataEventHandler();
    ~UIExtraDataEventHandler();

    /* Private member vars */
    static UIExtraDataEventHandler *m_pInstance;
    CEventListener m_mainEventListener;
    UIExtraDataEventHandlerPrivate *m_pHandler;
};

#define gEDataEvents UIExtraDataEventHandler::instance()

#endif /* !__UIExtraDataEventHandler_h__ */

