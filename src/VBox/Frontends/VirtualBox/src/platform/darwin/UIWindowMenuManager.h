/* $Id$ */
/** @file
 * VBox Qt GUI - UIWindowMenuManager class declaration.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
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
#include <QObject>
#include <QHash>

/* Forward declarations: */
class UIMenuHelper;
class QMenu;

/** Singleton QObject extension
  * used as Mac OS X 'Window' menu Manager. */
class UIWindowMenuManager: public QObject
{
    Q_OBJECT;

public:

    /** Static constructor and instance provider. */
    static UIWindowMenuManager *instance(QWidget *pParent = 0);
    /** Static destructor. */
    static void destroy();

    /** Creates 'Window' menu for passed @a pWindow. */
    QMenu *createMenu(QWidget *pWindow);
    /** Destroys 'Window' menu for passed @a pWindow. */
    void destroyMenu(QWidget *pWindow);

    /** Adds @a pWindow to all 'Window' menus. */
    void addWindow(QWidget *pWindow);
    /** Removes @a pWindow from all 'Window' menus. */
    void removeWindow(QWidget *pWindow);

    /** Handles translation event. */
    void retranslateUi();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    bool eventFilter(QObject *pObj, QEvent *pEvent);

private:

    /** Constructs 'Window' menu Manager. */
    UIWindowMenuManager(QWidget *pParent = 0);
    /** Destructs 'Window' menu Manager. */
    ~UIWindowMenuManager();

    /** Holds the static instance. */
    static UIWindowMenuManager *m_pInstance;

    /** Holds the passed parent reference. */
    QWidget *m_pParent;

    /** Holds the list of the registered window references. */
    QList<QWidget*> m_regWindows;

    /** Holds the hash of the registered menu-helper instances. */
    QHash<QWidget*, UIMenuHelper*> m_helpers;
};

#endif /* !___UIWindowMenuManager_h___ */

