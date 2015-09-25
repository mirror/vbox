/* $Id$ */
/** @file
 * VBox Qt GUI - UIWindowMenuManager class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIWindowMenuManager.h"

/* Qt includes: */
# include <QApplication>
# include <QMenu>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** QObject extension
  * used as Mac OS X 'Window' menu helper. */
class UIMenuHelper: public QObject
{
    Q_OBJECT;

public:

    /** Constructs menu-helper on the basis of passed @a winList. */
    UIMenuHelper(const QList<QWidget*> &winList)
    {
        m_pWindowMenu = new QMenu(0);
        m_pGroup = new QActionGroup(this);
        m_pGroup->setExclusive(true);
        m_pMinimizeAction = new QAction(this);
        m_pWindowMenu->addAction(m_pMinimizeAction);
        connect(m_pMinimizeAction, SIGNAL(triggered(bool)),
                this, SLOT(minimizeActive(bool)));
        /* Make sure all already available windows are properly registered on
         * this menu. */
        for (int i=0; i < winList.size(); ++i)
            addWindow(winList.at(i));

        retranslateUi();
    }

    /** Destructs menu-helper. */
    ~UIMenuHelper()
    {
        delete m_pWindowMenu;
        qDeleteAll(m_regWindows);
    }

    /** Returns 'Window' menu. */
    QMenu *menu() const { return m_pWindowMenu; }

    /** Adds window into 'Window' menu. */
    QAction* addWindow(QWidget *pWindow)
    {
        QAction *pAction = 0;
        if (    pWindow
            && !m_regWindows.contains(pWindow->windowTitle()))
        {
            if (m_regWindows.size() < 2)
                m_pWindowMenu->addSeparator();
            /* The main window always first */
            pAction = new QAction(this);
            pAction->setText(pWindow->windowTitle());
            pAction->setMenuRole(QAction::NoRole);
            pAction->setData(qVariantFromValue(pWindow));
            pAction->setCheckable(true);
            /* The first registered one is always considered as the main window */
            if (m_regWindows.size() == 0)
                pAction->setShortcut(QKeySequence("Ctrl+0"));
            m_pGroup->addAction(pAction);
            connect(pAction, SIGNAL(triggered(bool)),
                    this, SLOT(raiseSender(bool)));
            m_pWindowMenu->addAction(pAction);
            m_regWindows[pWindow->windowTitle()] = pAction;
        }
        return pAction;
    }

    /** Removes window from 'Window' menu. */
    void removeWindow(QWidget *pWindow)
    {
        if (m_regWindows.contains(pWindow->windowTitle()))
        {
            delete m_regWindows[pWindow->windowTitle()];
            m_regWindows.remove(pWindow->windowTitle());
        }
    }

    /** Handles translation event. */
    void retranslateUi()
    {
        m_pWindowMenu->setTitle(tr("&Window"));
        m_pMinimizeAction->setText(tr("Minimize"));
        m_pMinimizeAction->setShortcut(QKeySequence("Ctrl+M"));
    }

    /** Updates toggle action states according to passed @a pActive. */
    void updateStatus(QWidget *pActive)
    {
        m_pMinimizeAction->setEnabled(pActive != 0);
        if (pActive)
        {
            if (m_regWindows.contains(pActive->windowTitle()))
                m_regWindows[pActive->windowTitle()]->setChecked(true);
        }
        else
        {
            if (QAction *pChecked = m_pGroup->checkedAction())
                pChecked->setChecked(false);
        }

    }

private slots:

    /** Handles request to minimize active-window. */
    void minimizeActive(bool /* fToggle */)
    {
        if (QWidget *pActive = qApp->activeWindow())
            pActive->showMinimized();
    }

    /** Handles request to raise sender window. */
    void raiseSender(bool /* fToggle */)
    {
        if (QAction *pAction= qobject_cast<QAction*>(sender()))
        {
            if (QWidget *pWidget = qVariantValue<QWidget*>(pAction->data()))
            {
                pWidget->show();
                pWidget->raise();
                pWidget->activateWindow();
            }
        }
    }

private:

    /** Holds the 'Window' menu instance. */
    QMenu *m_pWindowMenu;
    /** Holds the action group instance. */
    QActionGroup *m_pGroup;
    /** Holds the 'Minimize' action instance. */
    QAction *m_pMinimizeAction;
    /** Holds the hash of the registered menu-helper instances. */
    QHash<QString, QAction*> m_regWindows;
};

/* static */
UIWindowMenuManager *UIWindowMenuManager::m_pInstance = 0;

/* static */
UIWindowMenuManager *UIWindowMenuManager::instance(QWidget *pParent /* = 0 */)
{
    if (!m_pInstance)
        m_pInstance = new UIWindowMenuManager(pParent);

    return m_pInstance;
}

/* static */
void UIWindowMenuManager::destroy()
{
    if (!m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = 0;
    }
}

QMenu *UIWindowMenuManager::createMenu(QWidget *pWindow)
{
    UIMenuHelper *pHelper = new UIMenuHelper(m_regWindows);

    m_helpers[pWindow] = pHelper;

    return pHelper->menu();
}

void UIWindowMenuManager::destroyMenu(QWidget *pWindow)
{
    if (m_helpers.contains(pWindow))
    {
        delete m_helpers[pWindow];
        m_helpers.remove(pWindow);
    }
}

void UIWindowMenuManager::addWindow(QWidget *pWindow)
{
    m_regWindows.append(pWindow);
    QHash<QWidget*, UIMenuHelper*>::const_iterator i = m_helpers.constBegin();
    while (i != m_helpers.constEnd())
    {
        i.value()->addWindow(pWindow);
        ++i;
    }
}

void UIWindowMenuManager::removeWindow(QWidget *pWindow)
{
    QHash<QWidget*, UIMenuHelper*>::const_iterator i = m_helpers.constBegin();
    while (i != m_helpers.constEnd())
    {
        i.value()->removeWindow(pWindow);
        ++i;
    }
    m_regWindows.removeAll(pWindow);
}

void UIWindowMenuManager::retranslateUi()
{
    QHash<QWidget*, UIMenuHelper*>::const_iterator i = m_helpers.constBegin();
    while (i != m_helpers.constEnd())
    {
        i.value()->retranslateUi();
        ++i;
    }
}

bool UIWindowMenuManager::eventFilter(QObject *pObj, QEvent *pEvent)
{
    QEvent::Type type = pEvent->type();
#if defined(VBOX_OSE) || (QT_VERSION < 0x040700)
    /* Stupid Qt: Qt doesn't check if a window is minimized when a command is
     * executed. This leads to strange behaviour. The minimized window is
     * partly restored, but not usable. As a workaround we raise the parent
     * window before we let execute the command.
     * Note: fixed in our local Qt build since 4.7.0. */
    if (type == QEvent::Show)
    {
        QWidget *pWidget = (QWidget*)pObj;
        if (   pWidget->parentWidget()
            && pWidget->parentWidget()->isMinimized())
        {
            pWidget->parentWidget()->show();
            pWidget->parentWidget()->raise();
            pWidget->parentWidget()->activateWindow();
        }
    }
#endif /* defined(VBOX_OSE) || (QT_VERSION < 0x040700) */
    /* We need to track several events which leads to different window
     * activation and change the menu items in that case. */
    if (   type == QEvent::ActivationChange
        || type == QEvent::WindowActivate
        || type == QEvent::WindowDeactivate
        || type == QEvent::WindowStateChange
        || type == QEvent::Show
        || type == QEvent::Close
        || type == QEvent::Hide)
    {
        QWidget *pActive = qApp->activeWindow();
        QHash<QWidget*, UIMenuHelper*>::const_iterator i = m_helpers.constBegin();
        while (i != m_helpers.constEnd())
        {
            i.value()->updateStatus(pActive);
            ++i;
        }
    }
    /* Change the strings in all registers window menus */
    if (   type == QEvent::LanguageChange
        && pObj == m_pParent)
        retranslateUi();

    return false;
}

UIWindowMenuManager::UIWindowMenuManager(QWidget *pParent /* = 0 */)
  : QObject(pParent)
  , m_pParent(pParent)
{
    qApp->installEventFilter(this);
}

UIWindowMenuManager::~UIWindowMenuManager()
{
    qDeleteAll(m_helpers);
}

#include "UIWindowMenuManager.moc"

