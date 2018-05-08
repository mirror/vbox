/* $Id$ */
/** @file
 * VBox Qt GUI - UIWindowMenuManager class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWindowMenuManager_h___
#define ___UIWindowMenuManager_h___

/* Qt includes: */
#include <QHash>
#include <QObject>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QMenu;
class UIMenuHelper;

/** Singleton QObject extension
  * used as Mac OS X 'Window' menu Manager. */
class SHARED_LIBRARY_STUFF UIWindowMenuManager : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

public:

    /** Creates instance. */
    static void create();
    /** Destroyes instance. */
    static void destroy();
    /** Returns current instance. */
    static UIWindowMenuManager *instance() { return s_pInstance; }

    /** Creates 'Window' menu for passed @a pWindow. */
    QMenu *createMenu(QWidget *pWindow);
    /** Destroys 'Window' menu for passed @a pWindow. */
    void destroyMenu(QWidget *pWindow);

    /** Adds @a pWindow to all 'Window' menus. */
    void addWindow(QWidget *pWindow);
    /** Removes @a pWindow from all 'Window' menus. */
    void removeWindow(QWidget *pWindow);

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

protected:

    /** Constructs 'Window' menu Manager. */
    UIWindowMenuManager();
    /** Destructs 'Window' menu Manager. */
    ~UIWindowMenuManager();

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private:

    /** Holds the static instance. */
    static UIWindowMenuManager *s_pInstance;

    /** Holds the list of the registered window references. */
    QList<QWidget*>  m_windows;

    /** Holds the hash of the registered menu-helper instances. */
    QHash<QWidget*, UIMenuHelper*>  m_helpers;
};

/** Singleton 'Window' menu Manager 'official' name. */
#define gpWindowMenuManager UIWindowMenuManager::instance()

#endif /* !___UIWindowMenuManager_h___ */
