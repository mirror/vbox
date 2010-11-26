/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIWindowMenuManager class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWindowMenuManager_h__
#define __UIWindowMenuManager_h__

/* Global includes */
#include <QObject>
#include <QHash>

/* Local forward declarations */
class UIMenuHelper;

/* Global forward declarations */
class QMenu;

class UIWindowMenuManager: public QObject
{
    Q_OBJECT;

public:

    static UIWindowMenuManager *instance(QWidget *pParent = 0);
    static void destroy();

    QMenu *createMenu(QWidget *pWindow);
    void destroyMenu(QWidget *pWindow);

    void addWindow(QWidget *pWindow);
    void removeWindow(QWidget *pWindow);

    void retranslateUi();

protected:

    bool eventFilter(QObject *pObj, QEvent *pEvent);

private:

    UIWindowMenuManager(QWidget *pParent = 0);
    ~UIWindowMenuManager();

    /* Private member vars */
    static UIWindowMenuManager *m_pInstance;
    QWidget *m_pParent;
    QList<QWidget*> m_regWindows;
    QHash<QWidget*, UIMenuHelper*> m_helpers;
};

#endif /* __UIWindowMenuManager_h__ */

