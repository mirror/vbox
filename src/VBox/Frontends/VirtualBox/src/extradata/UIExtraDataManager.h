/** @file
 * VBox Qt GUI - UIExtraDataManager class declaration.
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

#ifndef ___UIExtraDataManager_h___
#define ___UIExtraDataManager_h___

/* Qt includes: */
#include <QObject>

/* COM includes: */
#include "CEventListener.h"

/* Forward declarations: */
class UIExtraDataEventHandler;

class UIExtraDataManager: public QObject
{
    Q_OBJECT;

public:
    static UIExtraDataManager* instance();
    static void destroy();

signals:
    /* Specialized extra data signals */
    void sigGUILanguageChange(QString strLang);
    void sigSelectorShortcutsChanged();
    void sigMachineShortcutsChanged();
    void sigHidLedsSyncStateChanged(bool fEnabled);
#ifdef RT_OS_DARWIN
    void sigPresentationModeChange(bool fEnabled);
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

private:
    UIExtraDataManager();
    ~UIExtraDataManager();

    /* Private member vars */
    static UIExtraDataManager *m_pInstance;
    CEventListener m_mainEventListener;
    UIExtraDataEventHandler *m_pHandler;
};

#define gEDataManager UIExtraDataManager::instance()

#endif /* !___UIExtraDataManager_h___ */
