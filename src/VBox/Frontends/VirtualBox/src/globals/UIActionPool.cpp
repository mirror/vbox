/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPool class implementation
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QHelpEvent>
#include <QToolTip>

/* Local includes: */
#include "UIActionPool.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UISelectorShortcuts.h"
#include "UIMachineShortcuts.h"

/* Action activation event: */
class ActivateActionEvent : public QEvent
{
public:

    ActivateActionEvent(QAction *pAction)
        : QEvent((QEvent::Type)VBoxDefs::ActivateActionEventType)
        , m_pAction(pAction) {}
    QAction* action() const { return m_pAction; }

private:

    QAction *m_pAction;
};

/* UIActionInterface stuff: */
UIActionInterface::UIActionInterface(QObject *pParent, UIActionType type)
    : QIWithRetranslateUI3<QAction>(pParent)
    , m_type(type)
{
    /* By default there is no specific menu role.
     * It will be set explicitly later. */
    setMenuRole(QAction::NoRole);
}

QString UIActionInterface::menuText(const QString &strText)
{
    return vboxGlobal().isVMConsoleProcess() ? VBoxGlobal::removeAccelMark(strText) : strText;
}

/* UIMenuInterface stuff: */
UIMenuInterface::UIMenuInterface()
    : m_fShowToolTips(false)
{
}

bool UIMenuInterface::event(QEvent *pEvent)
{
    /* Handle particular event-types: */
    switch (pEvent->type())
    {
        /* Tool-tip request handler: */
        case QEvent::ToolTip:
        {
            /* Get current help-event: */
            QHelpEvent *pHelpEvent = static_cast<QHelpEvent*>(pEvent);
            /* Get action which caused help-event: */
            QAction *pAction = actionAt(pHelpEvent->pos());
            /* If action present => show action's tool-tip if needed: */
            if (pAction && m_fShowToolTips)
                QToolTip::showText(pHelpEvent->globalPos(), pAction->toolTip());
            break;
        }
        default:
            break;
    }
    /* Base-class event-handler: */
    return QMenu::event(pEvent);
}

/* UISimpleAction stuff: */
UISimpleAction::UISimpleAction(QObject *pParent, const QString &strIcon, const QString &strIconDis)
    : UIActionInterface(pParent, UIActionType_Simple)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDis));
}

UISimpleAction::UISimpleAction(QObject *pParent,
                               const QSize &normalSize, const QSize &smallSize,
                               const QString &strNormalIcon, const QString &strSmallIcon,
                               const QString &strNormalIconDis, const QString &strSmallIconDis)
    : UIActionInterface(pParent, UIActionType_Simple)
{
    setIcon(UIIconPool::iconSetFull(normalSize, smallSize, strNormalIcon, strSmallIcon, strNormalIconDis, strSmallIconDis));
}

UISimpleAction::UISimpleAction(QObject *pParent, const QIcon& icon)
    : UIActionInterface(pParent, UIActionType_Simple)
{
    if (!icon.isNull())
        setIcon(icon);
}

/* UIStateAction stuff: */
UIStateAction::UIStateAction(QObject *pParent, const QString &strIcon, const QString &strIconDis)
    : UIActionInterface(pParent, UIActionType_State)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDis));
}

UIStateAction::UIStateAction(QObject *pParent,
                             const QSize &normalSize, const QSize &smallSize,
                             const QString &strNormalIcon, const QString &strSmallIcon,
                             const QString &strNormalIconDis, const QString &strSmallIconDis)
    : UIActionInterface(pParent, UIActionType_State)
{
    setIcon(UIIconPool::iconSetFull(normalSize, smallSize, strNormalIcon, strSmallIcon, strNormalIconDis, strSmallIconDis));
}

UIStateAction::UIStateAction(QObject *pParent, const QIcon& icon)
    : UIActionInterface(pParent, UIActionType_State)
{
    if (!icon.isNull())
        setIcon(icon);
}

/* UIToggleAction stuff: */
UIToggleAction::UIToggleAction(QObject *pParent, const QString &strIcon, const QString &strIconDis)
    : UIActionInterface(pParent, UIActionType_Toggle)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDis));
    init();
}

UIToggleAction::UIToggleAction(QObject *pParent,
                               const QSize &normalSize, const QSize &smallSize,
                               const QString &strNormalIcon, const QString &strSmallIcon,
                               const QString &strNormalIconDis, const QString &strSmallIconDis)
    : UIActionInterface(pParent, UIActionType_Toggle)
{
    setIcon(UIIconPool::iconSetFull(normalSize, smallSize, strNormalIcon, strSmallIcon, strNormalIconDis, strSmallIconDis));
    init();
}

UIToggleAction::UIToggleAction(QObject *pParent,
               const QString &strIconOn, const QString &strIconOff,
               const QString &strIconOnDis, const QString &strIconOffDis)
    : UIActionInterface(pParent, UIActionType_Toggle)
{
    setIcon(UIIconPool::iconSetOnOff(strIconOn, strIconOff, strIconOnDis, strIconOffDis));
    init();
}

UIToggleAction::UIToggleAction(QObject *pParent, const QIcon &icon)
    : UIActionInterface(pParent, UIActionType_Toggle)
{
    if (!icon.isNull())
        setIcon(icon);
    init();
}

void UIToggleAction::sltUpdateAppearance()
{
    retranslateUi();
}

void UIToggleAction::init()
{
    setCheckable(true);
    connect(this, SIGNAL(toggled(bool)), this, SLOT(sltUpdateAppearance()));
}

/* UIMenuAction stuff: */
UIMenuAction::UIMenuAction(QObject *pParent, const QString &strIcon, const QString &strIconDis)
    : UIActionInterface(pParent, UIActionType_Menu)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDis));
    setMenu(new UIMenuInterface);
}

UIMenuAction::UIMenuAction(QObject *pParent, const QIcon &icon)
    : UIActionInterface(pParent, UIActionType_Menu)
{
    if (!icon.isNull())
        setIcon(icon);
    setMenu(new UIMenuInterface);
}

class MenuHelpAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuHelpAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(menuText(QApplication::translate("UIActionPool", "&Help")));
    }
};

class ShowHelpAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowHelpAction(QObject *pParent)
        : UISimpleAction(pParent, UIIconPool::defaultIcon(UIIconPool::DialogHelpIcon))
    {
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::HelpShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::HelpShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(menuText(QApplication::translate("UIMessageCenter", "&Contents...")));
        setStatusTip(QApplication::translate("UIMessageCenter", "Show the online help contents"));
    }
};

class ShowWebAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowWebAction(QObject *pParent)
        : UISimpleAction(pParent, ":/site_16px.png")
    {
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::WebShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::WebShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "&VirtualBox Web Site...")), gMS->shortcut(UIMachineShortcuts::WebShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Open the browser and go to the VirtualBox product web site"));
    }
};

class PerformResetWarningsAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformResetWarningsAction(QObject *pParent)
        : UISimpleAction(pParent, ":/reset_16px.png")
    {
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::ResetWarningsShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::ResetWarningsShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "&Reset All Warnings")), gMS->shortcut(UIMachineShortcuts::ResetWarningsShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Go back to showing all suppressed warnings and messages"));
    }
};

class ShowNetworkAccessManagerAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowNetworkAccessManagerAction(QObject *pParent)
        : UISimpleAction(pParent, ":/nw_16px.png", ":/nw_disabled_16px.png")
    {
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::NetworkAccessManager));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::NetworkAccessManager));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "&Network Operations Manager...")), gMS->shortcut(UIMachineShortcuts::NetworkAccessManager)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Show Network Operations Manager"));
    }
};

#ifdef VBOX_WITH_REGISTRATION
class PerformRegisterAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformRegisterAction(QObject *pParent)
        : UISimpleAction(pParent, ":/register_16px.png", ":/register_disabled_16px.png")
    {
        setEnabled(vboxGlobal().virtualBox().
                   GetExtraData(VBoxDefs::GUI_RegistrationDlgWinID).isEmpty());
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::RegisterShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::RegisterShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "R&egister VirtualBox...")), gMS->shortcut(UIMachineShortcuts::RegisterShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Open VirtualBox registration form"));
    }
};
#endif /* VBOX_WITH_REGISTRATION */

class PerformUpdateAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformUpdateAction(QObject *pParent)
        : UISimpleAction(pParent, ":/refresh_16px.png", ":/refresh_disabled_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::UpdateShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::UpdateShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "C&heck for Updates...")), gMS->shortcut(UIMachineShortcuts::UpdateShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Check for a new VirtualBox version"));
    }
};

class ShowAboutAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowAboutAction(QObject *pParent)
        : UISimpleAction(pParent, ":/about_16px.png")
    {
        setMenuRole(QAction::AboutRole);
        switch (gActionPool->type())
        {
            case UIActionPoolType_Selector:
                setShortcut(gSS->keySequence(UISelectorShortcuts::AboutShortcut));
                break;
            case UIActionPoolType_Runtime:
                setShortcut(gMS->keySequence(UIMachineShortcuts::AboutShortcut));
                break;
        }
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(menuText(QApplication::translate("UIMessageCenter", "&About VirtualBox...")), gMS->shortcut(UIMachineShortcuts::AboutShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Show a dialog with product information"));
    }
};

/* UIActionPool stuff: */
UIActionPool* UIActionPool::m_pInstance = 0;

/* static */
UIActionPool* UIActionPool::instance()
{
    return m_pInstance;
}

UIActionPool::UIActionPool(UIActionPoolType type)
    : m_type(type)
{
    /* Prepare instance: */
    if (!m_pInstance)
        m_pInstance = this;
}

UIActionPool::~UIActionPool()
{
    /* Cleanup instance: */
    if (m_pInstance == this)
        m_pInstance = 0;
}

void UIActionPool::prepare()
{
    /* Create actions: */
    createActions();
    /* Create menus: */
    createMenus();
}

void UIActionPool::cleanup()
{
    /* Destroy pool: */
    destroyPool();
}

bool UIActionPool::processHotKey(const QKeySequence &key)
{
    /* Get the list of keys: */
    QList<int> keys = m_pool.keys();
    /* Iterate through the whole list of keys: */
    for (int i = 0; i < keys.size(); ++i)
    {
        /* Get current action: */
        UIActionInterface *pAction = m_pool[keys[i]];
        /* Skip menus/separators: */
        if (pAction->type() == UIActionType_Menu)
            continue;
        /* Get the hot key of the current action: */
        QString strHotKey = VBoxGlobal::extractKeyFromActionText(pAction->text());
        if (pAction->isEnabled() && pAction->isVisible() && !strHotKey.isEmpty())
        {
            if (key.matches(QKeySequence(strHotKey)) == QKeySequence::ExactMatch)
            {
                /* We asynchronously post a special event instead of calling
                 * pAction->trigger() directly, to let key presses and
                 * releases be processed correctly by Qt first.
                 * Note: we assume that nobody will delete the menu item
                 * corresponding to the key sequence, so that the pointer to
                 * menu data posted along with the event will remain valid in
                 * the event handler, at least until the main window is closed. */
                QApplication::postEvent(this, new ActivateActionEvent(pAction));
                return true;
            }
        }
    }
    return false;
}

void UIActionPool::createActions()
{
    /* 'Help' actions: */
    m_pool[UIActionIndex_Simple_Help] = new ShowHelpAction(this);
    m_pool[UIActionIndex_Simple_Web] = new ShowWebAction(this);
    m_pool[UIActionIndex_Simple_ResetWarnings] = new PerformResetWarningsAction(this);
    m_pool[UIActionIndex_Simple_NetworkAccessManager] = new ShowNetworkAccessManagerAction(this);
#ifdef VBOX_WITH_REGISTRATION
    m_pool[UIActionIndex_Simple_Register] = new PerformRegisterAction(this);
#endif /* VBOX_WITH_REGISTRATION */
    m_pool[UIActionIndex_Simple_Update] = new PerformUpdateAction(this);
    m_pool[UIActionIndex_Simple_About] = new ShowAboutAction(this);
}

void UIActionPool::createMenus()
{
    /* On Mac OS X, all QMenu's are consumed by Qt after they are added to another QMenu or a QMenuBar.
     * This means we have to recreate all QMenus when creating a new QMenuBar.
     * For simplicity we doing this on all platforms right now. */

    /* Recreate 'help' menu items as well.
     * This makes sure they are removed also from the Application menu: */
    if (m_pool[UIActionIndex_Simple_Help])
        delete m_pool[UIActionIndex_Simple_Help];
    m_pool[UIActionIndex_Simple_Help] = new ShowHelpAction(this);
    if (m_pool[UIActionIndex_Simple_Web])
        delete m_pool[UIActionIndex_Simple_Web];
    m_pool[UIActionIndex_Simple_Web] = new ShowWebAction(this);
    if (m_pool[UIActionIndex_Simple_ResetWarnings])
        delete m_pool[UIActionIndex_Simple_ResetWarnings];
    m_pool[UIActionIndex_Simple_ResetWarnings] = new PerformResetWarningsAction(this);
    if (m_pool[UIActionIndex_Simple_NetworkAccessManager])
        delete m_pool[UIActionIndex_Simple_NetworkAccessManager];
    m_pool[UIActionIndex_Simple_NetworkAccessManager] = new ShowNetworkAccessManagerAction(this);
#ifdef VBOX_WITH_REGISTRATION
    if (m_pool[UIActionIndex_Simple_Register])
        delete m_pool[UIActionIndex_Simple_Register]
    m_pool[UIActionIndex_Simple_Register] = new PerformRegisterAction(this);
#endif /* VBOX_WITH_REGISTRATION */
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
    /* For whatever reason, Qt doesn't fully remove items with a
     * ApplicationSpecificRole from the application menu. Although the QAction
     * itself is deleted, a dummy entry is leaved back in the menu.
     * Hiding before deletion helps. */
    m_pool[UIActionIndex_Simple_Update]->setVisible(false);
#endif
#if !(defined(Q_WS_MAC) && (QT_VERSION < 0x040700))
    if (m_pool[UIActionIndex_Simple_Update])
        delete m_pool[UIActionIndex_Simple_Update];
    m_pool[UIActionIndex_Simple_Update] = new PerformUpdateAction(this);
    if (m_pool[UIActionIndex_Simple_About])
        delete m_pool[UIActionIndex_Simple_About];
    m_pool[UIActionIndex_Simple_About] = new ShowAboutAction(this);
#endif

    /* 'Help' menu itself: */
    if (m_pool[UIActionIndex_Menu_Help])
        delete m_pool[UIActionIndex_Menu_Help];
    m_pool[UIActionIndex_Menu_Help] = new MenuHelpAction(this);
}

void UIActionPool::destroyPool()
{
    /* Get the list of keys: */
    QList<int> keys = m_pool.keys();
    /* Delete all the items of the map: */
    for (int i = 0; i < keys.size(); ++i)
        delete m_pool[keys[i]];
}

bool UIActionPool::event(QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        case VBoxDefs::ActivateActionEventType:
        {
            /* Process specific event: */
            ActivateActionEvent *pActionEvent = static_cast<ActivateActionEvent*>(pEvent);
            pActionEvent->action()->trigger();
            pEvent->accept();
            return true;
        }
        default:
            break;
    }
    /* Pass to the base-class: */
    return QObject::event(pEvent);
}

#include "UIActionPool.moc"

