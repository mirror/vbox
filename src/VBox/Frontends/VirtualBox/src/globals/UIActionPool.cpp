/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPool class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHelpEvent>
# include <QToolTip>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIActionPool.h"
# include "UIActionPoolSelector.h"
# include "UIActionPoolRuntime.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIShortcutPool.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UIExtraDataManager.h"
#  include "UINetworkManager.h"
#  include "UIUpdateManager.h"
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QEvent extension
  * representing action-activation event. */
class ActivateActionEvent : public QEvent
{
public:

    /** Constructs @a pAction event. */
    ActivateActionEvent(QAction *pAction)
        : QEvent((QEvent::Type)ActivateActionEventType)
        , m_pAction(pAction)
    {}

    /** Returns the action this event corresponds to. */
    QAction *action() const { return m_pAction; }

private:

    /** Holds the action this event corresponds to. */
    QAction *m_pAction;
};


/*********************************************************************************************************************************
*   Class UIMenu implementation.                                                                                                 *
*********************************************************************************************************************************/

UIMenu::UIMenu()
    : m_fShowToolTip(false)
#ifdef VBOX_WS_MAC
    , m_fConsumable(false)
    , m_fConsumed(false)
#endif
{
}

bool UIMenu::event(QEvent *pEvent)
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
            if (pAction && m_fShowToolTip)
                QToolTip::showText(pHelpEvent->globalPos(), pAction->toolTip());
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QMenu::event(pEvent);
}


/*********************************************************************************************************************************
*   Class UIAction implementation.                                                                                               *
*********************************************************************************************************************************/

UIAction::UIAction(UIActionPool *pParent, UIActionType enmType)
    : QAction(pParent)
    , m_enmType(enmType)
    , m_pActionPool(pParent)
    , m_enmActionPoolType(pParent->type())
    , m_iState(0)
    , m_fShortcutHidden(false)
{
    /* By default there is no specific menu role.
     * It will be set explicitly later. */
    setMenuRole(QAction::NoRole);

#ifdef VBOX_WS_MAC
    /* Make sure each action notifies it's parent about hovering: */
    connect(this, &UIAction::hovered,
            static_cast<UIActionPool*>(parent()), &UIActionPool::sltActionHovered);
#endif
}

UIMenu *UIAction::menu() const
{
    return QAction::menu() ? qobject_cast<UIMenu*>(QAction::menu()) : 0;
}

UIActionPolymorphicMenu *UIAction::toActionPolymorphicMenu()
{
    return qobject_cast<UIActionPolymorphicMenu*>(this);
}

void UIAction::setIcon(int iState, const QIcon &icon)
{
    m_icons.resize(iState + 1);
    m_icons[iState] = icon;
    updateIcon();
}

void UIAction::setIcon(const QIcon &icon)
{
    setIcon(0, icon);
}

void UIAction::setName(const QString &strName)
{
    /* Remember internal name: */
    m_strName = strName;
    /* Update text according new name: */
    updateText();
}

void UIAction::setShortcut(const QKeySequence &shortcut)
{
    /* Only for selector's action-pool: */
    if (m_enmActionPoolType == UIActionPoolType_Selector)
    {
        /* If shortcut is visible: */
        if (!m_fShortcutHidden)
            /* Call to base-class: */
            QAction::setShortcut(shortcut);
        /* Remember shortcut: */
        m_shortcut = shortcut;
    }
    /* Update text according new shortcut: */
    updateText();
}

void UIAction::showShortcut()
{
    m_fShortcutHidden = false;
    if (!m_shortcut.isEmpty())
        QAction::setShortcut(m_shortcut);
}

void UIAction::hideShortcut()
{
    m_fShortcutHidden = true;
    if (!shortcut().isEmpty())
        QAction::setShortcut(QKeySequence());
}

QString UIAction::nameInMenu() const
{
    /* Action-name format depends on action-pool type: */
    switch (m_enmActionPoolType)
    {
        /* Unchanged name for Selector UI: */
        case UIActionPoolType_Selector: return name();
        /* Filtered name for Runtime UI: */
        case UIActionPoolType_Runtime: return VBoxGlobal::removeAccelMark(name());
    }
    /* Nothing by default: */
    return QString();
}

void UIAction::updateIcon()
{
    QAction::setIcon(m_icons.value(m_iState));
}

void UIAction::updateText()
{
    /* Action-text format depends on action-pool type: */
    switch (m_enmActionPoolType)
    {
        /* The same as menu name for Selector UI: */
        case UIActionPoolType_Selector:
            setText(nameInMenu());
            break;
        /* With shortcut appended for Runtime UI: */
        case UIActionPoolType_Runtime:
            setText(vboxGlobal().insertKeyToActionText(nameInMenu(),
                                                       gShortcutPool->shortcut(actionPool(), this).toString()));
            break;
    }
}


/*********************************************************************************************************************************
*   Class UIActionMenu implementation.                                                                                           *
*********************************************************************************************************************************/

UIActionMenu::UIActionMenu(UIActionPool *pParent,
                           const QString &strIcon, const QString &strIconDisabled)
    : UIAction(pParent, UIActionType_Menu)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
    prepare();
}

UIActionMenu::UIActionMenu(UIActionPool *pParent,
                           const QIcon &icon)
    : UIAction(pParent, UIActionType_Menu)
{
    if (!icon.isNull())
        setIcon(icon);
    prepare();
}

void UIActionMenu::setShowToolTip(bool fShowToolTip)
{
    qobject_cast<UIMenu*>(menu())->setShowToolTip(fShowToolTip);
}

void UIActionMenu::prepare()
{
    /* Create menu: */
    setMenu(new UIMenu);
    AssertPtrReturnVoid(menu());
    {
        /* Prepare menu: */
        connect(menu(), &UIMenu::aboutToShow,
                actionPool(), &UIActionPool::sltHandleMenuPrepare);
    }
}

void UIActionMenu::updateText()
{
    setText(nameInMenu());
}


/*********************************************************************************************************************************
*   Class UIActionSimple implementation.                                                                                         *
*********************************************************************************************************************************/

UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QString &strIcon /* = QString() */, const QString &strIconDisabled /* = QString() */)
    : UIAction(pParent, UIActionType_Simple)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
}

UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QString &strIconNormal, const QString &strIconSmall,
                               const QString &strIconNormalDisabled, const QString &strIconSmallDisabled)
    : UIAction(pParent, UIActionType_Simple)
{
    setIcon(UIIconPool::iconSetFull(strIconNormal, strIconSmall, strIconNormalDisabled, strIconSmallDisabled));
}

UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QIcon &icon)
    : UIAction(pParent, UIActionType_Simple)
{
    setIcon(icon);
}


/*********************************************************************************************************************************
*   Class UIActionToggle implementation.                                                                                         *
*********************************************************************************************************************************/

UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QString &strIcon /* = QString() */, const QString &strIconDisabled /* = QString() */)
    : UIAction(pParent, UIActionType_Toggle)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
    prepare();
}

UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QString &strIconOn, const QString &strIconOff,
                               const QString &strIconOnDisabled, const QString &strIconOffDisabled)
    : UIAction(pParent, UIActionType_Toggle)
{
    setIcon(UIIconPool::iconSetOnOff(strIconOn, strIconOff, strIconOnDisabled, strIconOffDisabled));
    prepare();
}

UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QIcon &icon)
    : UIAction(pParent, UIActionType_Toggle)
{
    if (!icon.isNull())
        setIcon(icon);
    prepare();
}

void UIActionToggle::prepare()
{
    setCheckable(true);
}


/*********************************************************************************************************************************
*   Class UIActionPolymorphicMenu implementation.                                                                                *
*********************************************************************************************************************************/

UIActionPolymorphicMenu::UIActionPolymorphicMenu(UIActionPool *pParent,
                                                 const QString &strIcon, const QString &strIconDisabled)
    : UIAction(pParent, UIActionType_PolymorphicMenu)
    , m_pMenu(0)
    , m_iState(0)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
    prepare();
}

UIActionPolymorphicMenu::UIActionPolymorphicMenu(UIActionPool *pParent,
                                                 const QString &strIconNormal, const QString &strIconSmall,
                                                 const QString &strIconNormalDisabled, const QString &strIconSmallDisabled)
    : UIAction(pParent, UIActionType_PolymorphicMenu)
    , m_pMenu(0)
    , m_iState(0)
{
    if (!strIconNormal.isNull())
        setIcon(UIIconPool::iconSetFull(strIconNormal, strIconSmall, strIconNormalDisabled, strIconSmallDisabled));
    prepare();
}

UIActionPolymorphicMenu::UIActionPolymorphicMenu(UIActionPool *pParent,
                                                 const QIcon &icon)
    : UIAction(pParent, UIActionType_PolymorphicMenu)
    , m_pMenu(0)
    , m_iState(0)
{
    if (!icon.isNull())
        setIcon(icon);
    prepare();
}

UIActionPolymorphicMenu::~UIActionPolymorphicMenu()
{
    /* Hide menu: */
    hideMenu();
    /* Delete menu: */
    delete m_pMenu;
    m_pMenu = 0;
}

void UIActionPolymorphicMenu::setShowToolTip(bool fShowToolTip)
{
    qobject_cast<UIMenu*>(menu())->setShowToolTip(fShowToolTip);
}

void UIActionPolymorphicMenu::showMenu()
{
    /* Show menu if necessary: */
    if (!menu())
        setMenu(m_pMenu);
}

void UIActionPolymorphicMenu::hideMenu()
{
    /* Hide menu if necessary: */
    if (menu())
        setMenu(0);
}

void UIActionPolymorphicMenu::prepare()
{
    /* Create menu: */
    m_pMenu = new UIMenu;
    AssertPtrReturnVoid(m_pMenu);
    {
        /* Prepare menu: */
        connect(m_pMenu, &UIMenu::aboutToShow,
                actionPool(), &UIActionPool::sltHandleMenuPrepare);
        /* Show menu: */
        showMenu();
    }
}

void UIActionPolymorphicMenu::updateText()
{
    setText(nameInMenu());
}


/** Menu action extension, used as 'Application' menu class. */
class UIActionMenuApplication : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuApplication(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
#ifdef VBOX_WS_MAC
        menu()->setConsumable(true);
#endif
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuType_Application;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuType_Application);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuBar(UIExtraDataMetaDefs::MenuType_Application);
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
#ifdef VBOX_WS_MAC
        setName(QApplication::translate("UIActionPool", "&VirtualBox"));
#else
        setName(QApplication::translate("UIActionPool", "&File"));
#endif
    }
};


/** Simple action extension, used as 'Close' action class. */
class UIActionSimplePerformClose : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimplePerformClose(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuApplicationActionType_Close;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_Close);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_Close);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Close");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType actionPoolType) const /* override */
    {
        switch (actionPoolType)
        {
            case UIActionPoolType_Selector: break;
            case UIActionPoolType_Runtime: return QKeySequence("Q");
        }
        return QKeySequence();
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Close..."));
        setStatusTip(QApplication::translate("UIActionPool", "Close the virtual machine"));
    }
};

#ifdef VBOX_WS_MAC
/** Menu action extension, used as 'Window' menu class. */
class UIActionMenuWindow : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuWindow(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuType_Window;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuType_Window);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuBar(UIExtraDataMetaDefs::MenuType_Window);
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Window"));
    }
};


/** Simple action extension, used as 'Minimize' action class. */
class UIActionSimpleMinimize : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleMinimize(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuWindowActionType_Minimize;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuWindowActionType_Minimize);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuWindow(UIExtraDataMetaDefs::MenuWindowActionType_Minimize);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Minimize");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Minimize"));
        setStatusTip(QApplication::translate("UIActionPool", "Minimize active window"));
    }
};
#endif /* VBOX_WS_MAC */


/** Menu action extension, used as 'Help' menu class. */
class UIActionMenuHelp : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuHelp(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuType_Help;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuType_Help);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuBar(UIExtraDataMetaDefs::MenuType_Help);
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Help"));
    }
};


/** Simple action extension, used as 'Contents' action class. */
class UIActionSimpleContents : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleContents(UIActionPool *pParent)
        : UIActionSimple(pParent, UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_DialogHelp))
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuHelpActionType_Contents;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_Contents);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Contents);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Help");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType actionPoolType) const /* override */
    {
        switch (actionPoolType)
        {
            case UIActionPoolType_Selector: return QKeySequence(QKeySequence::HelpContents);
            case UIActionPoolType_Runtime: break;
        }
        return QKeySequence();
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Contents..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show help contents"));
    }
};


/** Simple action extension, used as 'Web Site' action class. */
class UIActionSimpleWebSite : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleWebSite(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/site_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuHelpActionType_WebSite;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_WebSite);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_WebSite);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Web");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&VirtualBox Web Site..."));
        setStatusTip(QApplication::translate("UIActionPool", "Open the browser and go to the VirtualBox product web site"));
    }
};


/** Simple action extension, used as 'Bug Tracker' action class. */
class UIActionSimpleBugTracker : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleBugTracker(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/site_bugtracker_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuHelpActionType_BugTracker;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_BugTracker);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_BugTracker);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("BugTracker");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&VirtualBox Bug Tracker..."));
        setStatusTip(QApplication::translate("UIActionPool", "Open the browser and go to the VirtualBox product bug tracker"));
    }
};


/** Simple action extension, used as 'Forums' action class. */
class UIActionSimpleForums : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleForums(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/site_forum_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuHelpActionType_Forums;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_Forums);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Forums);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Forums");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&VirtualBox Forums..."));
        setStatusTip(QApplication::translate("UIActionPool", "Open the browser and go to the VirtualBox product forums"));
    }
};


/** Simple action extension, used as 'Oracle' action class. */
class UIActionSimpleOracle : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleOracle(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/site_oracle_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuHelpActionType_Oracle;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_Oracle);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Oracle);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Oracle");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Oracle Web Site..."));
        setStatusTip(QApplication::translate("UIActionPool", "Open the browser and go to the Oracle web site"));
    }
};


/** Simple action extension, used as 'Reset Warnings' action class. */
class UIActionSimpleResetWarnings : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleResetWarnings(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/reset_warnings_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuApplicationActionType_ResetWarnings;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_ResetWarnings);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_ResetWarnings);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ResetWarnings");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Reset All Warnings"));
        setStatusTip(QApplication::translate("UIActionPool", "Go back to showing all suppressed warnings and messages"));
    }
};


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
/** Simple action extension, used as 'Network Access Manager' action class. */
class UIActionSimpleNetworkAccessManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleNetworkAccessManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/download_manager_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuApplicationActionType_NetworkAccessManager;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_NetworkAccessManager);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_NetworkAccessManager);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("NetworkAccessManager");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Network Operations Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Network Operations Manager window"));
    }
};


/** Simple action extension, used as 'Check for Updates' action class. */
class UIActionSimpleCheckForUpdates : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleCheckForUpdates(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/refresh_16px.png", ":/refresh_disabled_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuApplicationActionType_CheckForUpdates;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_CheckForUpdates);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_CheckForUpdates);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Update");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "C&heck for Updates..."));
        setStatusTip(QApplication::translate("UIActionPool", "Check for a new VirtualBox version"));
    }
};
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */


/** Simple action extension, used as 'About' action class. */
class UIActionSimpleAbout : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleAbout(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/about_16px.png")
    {
        setMenuRole(QAction::AboutRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
#ifdef VBOX_WS_MAC
        return UIExtraDataMetaDefs::MenuApplicationActionType_About;
#else
        return UIExtraDataMetaDefs::MenuHelpActionType_About;
#endif
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
#ifdef VBOX_WS_MAC
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_About);
#else
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_About);
#endif
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
#ifdef VBOX_WS_MAC
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_About);
#else
        return actionPool()->isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_About);
#endif
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("About");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&About VirtualBox..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display a window with product information"));
    }
};


/** Simple action extension, used as 'Preferences' action class. */
class UIActionSimplePreferences : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimplePreferences(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/global_settings_16px.png")
    {
        setMenuRole(QAction::PreferencesRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const /* override */
    {
        return UIExtraDataMetaDefs::MenuApplicationActionType_Preferences;
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const /* override */
    {
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_Preferences);
    }
    /** Returns whether action is allowed. */
    virtual bool isAllowed() const /* override */
    {
        return actionPool()->isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_Preferences);
    }

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Preferences");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        switch (actionPool()->type())
        {
            case UIActionPoolType_Selector: return QKeySequence("Ctrl+G");
            case UIActionPoolType_Runtime: break;
        }
        return QKeySequence();
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Preferences...", "global preferences window"));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global preferences window"));
    }
};


/** Menu action extension, used as 'Log' menu class. */
class UIActionMenuSelectorLog : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLog(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("LogViewerMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Log Viewer"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the log menu"));
    }
};

/** Simple action extension, used as 'Toggle Pane Find' action class. */
class UIActionMenuSelectorLogTogglePaneFind : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogTogglePaneFind(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/log_viewer_find_22px.png", ":/log_viewer_find_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleLogFind");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+F");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Find"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with searching options"));
        setToolTip(tr("Open Find Pane (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Toggle Pane Filter' action class. */
class UIActionMenuSelectorLogTogglePaneFilter : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogTogglePaneFilter(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/log_viewer_filter_22px.png", ":/log_viewer_filter_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleLogFilter");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+T");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Filter"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with filtering options"));
        setToolTip(tr("Open Filter Pane (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Toggle Pane Bookmark' action class. */
class UIActionMenuSelectorLogTogglePaneBookmark : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogTogglePaneBookmark(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/log_viewer_bookmark_22px.png", ":/log_viewer_bookmark_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleLogBookmark");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+D");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Bookmark"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with bookmarking options"));
        setToolTip(tr("Open Bookmark Pane (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Toggle Pane Settings' action class. */
class UIActionMenuSelectorLogTogglePaneSettings : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogTogglePaneSettings(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/log_viewer_settings_22px.png", ":/log_viewer_settings_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleLogSettings");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Settings"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with log viewer settings"));
        setToolTip(tr("Open Settings Pane (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Refresh' action class. */
class UIActionMenuSelectorLogPerformRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogPerformRefresh(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/log_viewer_refresh_22px.png", ":/log_viewer_refresh_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RefreshLog");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Refresh"));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh selected virtual machine log"));
        setToolTip(tr("Refresh Virtual Machine Log (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Save' action class. */
class UIActionMenuSelectorLogPerformSave : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorLogPerformSave(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/log_viewer_save_22px.png", ":/log_viewer_save_disabled_22px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SaveLog");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+S");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Save..."));
        setStatusTip(QApplication::translate("UIActionPool", "Save selected virtual machine log"));
        setToolTip(tr("Save Virtual Machine Log (%1)").arg(shortcut().toString()));
    }
};


/*********************************************************************************************************************************
*   Class UIActionPool implementation.                                                                                           *
*********************************************************************************************************************************/

/* static */
UIActionPool *UIActionPool::create(UIActionPoolType enmType)
{
    UIActionPool *pActionPool = 0;
    switch (enmType)
    {
        case UIActionPoolType_Selector: pActionPool = new UIActionPoolSelector; break;
        case UIActionPoolType_Runtime: pActionPool = new UIActionPoolRuntime; break;
        default: AssertFailedReturn(0);
    }
    AssertPtrReturn(pActionPool, 0);
    pActionPool->prepare();
    return pActionPool;
}

/* static */
void UIActionPool::destroy(UIActionPool *pActionPool)
{
    AssertPtrReturnVoid(pActionPool);
    pActionPool->cleanup();
    delete pActionPool;
}

/* static */
void UIActionPool::createTemporary(UIActionPoolType enmType)
{
    UIActionPool *pActionPool = 0;
    switch (enmType)
    {
        case UIActionPoolType_Selector: pActionPool = new UIActionPoolSelector(true); break;
        case UIActionPoolType_Runtime: pActionPool = new UIActionPoolRuntime(true); break;
        default: AssertFailedReturnVoid();
    }
    AssertPtrReturnVoid(pActionPool);
    pActionPool->prepare();
    pActionPool->cleanup();
    delete pActionPool;
}

UIActionPool::UIActionPool(UIActionPoolType enmType, bool fTemporary /* = false */)
    : m_enmType(enmType)
    , m_fTemporary(fTemporary)
{
}

UIActionPoolRuntime *UIActionPool::toRuntime()
{
    return qobject_cast<UIActionPoolRuntime*>(this);
}

UIActionPoolSelector *UIActionPool::toSelector()
{
    return qobject_cast<UIActionPoolSelector*>(this);
}

bool UIActionPool::isAllowedInMenuBar(UIExtraDataMetaDefs::MenuType enmType) const
{
    foreach (const UIExtraDataMetaDefs::MenuType &enmRestriction, m_restrictedMenus.values())
        if (enmRestriction & enmType)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuBar(UIActionRestrictionLevel enmLevel, UIExtraDataMetaDefs::MenuType enmRestriction)
{
    m_restrictedMenus[enmLevel] = enmRestriction;
    updateMenus();
}

bool UIActionPool::isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType enmType) const
{
    foreach (const UIExtraDataMetaDefs::MenuApplicationActionType &enmRestriction, m_restrictedActionsMenuApplication.values())
        if (enmRestriction & enmType)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuApplication(UIActionRestrictionLevel enmLevel, UIExtraDataMetaDefs::MenuApplicationActionType enmRestriction)
{
    m_restrictedActionsMenuApplication[enmLevel] = enmRestriction;
    m_invalidations << UIActionIndex_M_Application;
}

#ifdef VBOX_WS_MAC
bool UIActionPool::isAllowedInMenuWindow(UIExtraDataMetaDefs::MenuWindowActionType enmType) const
{
    foreach (const UIExtraDataMetaDefs::MenuWindowActionType &enmRestriction, m_restrictedActionsMenuWindow.values())
        if (enmRestriction & enmType)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuWindow(UIActionRestrictionLevel enmLevel, UIExtraDataMetaDefs::MenuWindowActionType enmRestriction)
{
    m_restrictedActionsMenuWindow[enmLevel] = enmRestriction;
    m_invalidations << UIActionIndex_M_Window;
}
#endif /* VBOX_WS_MAC */

bool UIActionPool::isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType enmType) const
{
    foreach (const UIExtraDataMetaDefs::MenuHelpActionType &enmRestriction, m_restrictedActionsMenuHelp.values())
        if (enmRestriction & enmType)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuHelp(UIActionRestrictionLevel enmLevel, UIExtraDataMetaDefs::MenuHelpActionType enmRestriction)
{
    m_restrictedActionsMenuHelp[enmLevel] = enmRestriction;
    m_invalidations << UIActionIndex_Menu_Help;
}

void UIActionPool::sltHandleMenuPrepare()
{
    /* Make sure menu is valid: */
    AssertPtrReturnVoid(sender());
    UIMenu *pMenu = qobject_cast<UIMenu*>(sender());
    AssertPtrReturnVoid(pMenu);
    /* Make sure action is valid: */
    AssertPtrReturnVoid(pMenu->menuAction());
    UIAction *pAction = qobject_cast<UIAction*>(pMenu->menuAction());
    AssertPtrReturnVoid(pAction);

    /* Determine action index: */
    const int iIndex = m_pool.key(pAction);

    /* Update menu if necessary: */
    updateMenu(iIndex);

    /* Notify listeners about menu prepared: */
    emit sigNotifyAboutMenuPrepare(iIndex, pMenu);
}

#ifdef VBOX_WS_MAC
void UIActionPool::sltActionHovered()
{
    /* Acquire sender action: */
    UIAction *pAction = qobject_cast<UIAction*>(sender());
    AssertPtrReturnVoid(pAction);
    //printf("Action hovered: {%s}\n", pAction->name().toUtf8().constData());

    /* Notify listener about action hevering: */
    emit sigActionHovered(pAction);
}
#endif /* VBOX_WS_MAC */

void UIActionPool::prepare()
{
    /* Prepare pool: */
    preparePool();
    /* Prepare connections: */
    prepareConnections();
    /* Update configuration: */
    updateConfiguration();
    /* Update shortcuts: */
    updateShortcuts();
}

void UIActionPool::preparePool()
{
    /* Create 'Application' actions: */
    m_pool[UIActionIndex_M_Application] = new UIActionMenuApplication(this);
#ifdef VBOX_WS_MAC
    m_pool[UIActionIndex_M_Application_S_About] = new UIActionSimpleAbout(this);
#endif
    m_pool[UIActionIndex_M_Application_S_Preferences] = new UIActionSimplePreferences(this);
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    m_pool[UIActionIndex_M_Application_S_NetworkAccessManager] = new UIActionSimpleNetworkAccessManager(this);
    m_pool[UIActionIndex_M_Application_S_CheckForUpdates] = new UIActionSimpleCheckForUpdates(this);
#endif
    m_pool[UIActionIndex_M_Application_S_ResetWarnings] = new UIActionSimpleResetWarnings(this);
    m_pool[UIActionIndex_M_Application_S_Close] = new UIActionSimplePerformClose(this);

#ifdef VBOX_WS_MAC
    /* Create 'Window' actions: */
    m_pool[UIActionIndex_M_Window] = new UIActionMenuWindow(this);
    m_pool[UIActionIndex_M_Window_S_Minimize] = new UIActionSimpleMinimize(this);
#endif

    /* Create 'Help' actions: */
    m_pool[UIActionIndex_Menu_Help] = new UIActionMenuHelp(this);
    m_pool[UIActionIndex_Simple_Contents] = new UIActionSimpleContents(this);
    m_pool[UIActionIndex_Simple_WebSite] = new UIActionSimpleWebSite(this);
    m_pool[UIActionIndex_Simple_BugTracker] = new UIActionSimpleBugTracker(this);
    m_pool[UIActionIndex_Simple_Forums] = new UIActionSimpleForums(this);
    m_pool[UIActionIndex_Simple_Oracle] = new UIActionSimpleOracle(this);
#ifndef VBOX_WS_MAC
    m_pool[UIActionIndex_Simple_About] = new UIActionSimpleAbout(this);
#endif

    /* Create 'Log Viewer' actions: */
    m_pool[UIActionIndex_M_LogViewerWindow] = new UIActionMenuSelectorLog(this);
    m_pool[UIActionIndex_M_LogViewer] = new UIActionMenuSelectorLog(this);
    m_pool[UIActionIndex_M_LogViewer_T_Find] = new UIActionMenuSelectorLogTogglePaneFind(this);
    m_pool[UIActionIndex_M_LogViewer_T_Filter] = new UIActionMenuSelectorLogTogglePaneFilter(this);
    m_pool[UIActionIndex_M_LogViewer_T_Bookmark] = new UIActionMenuSelectorLogTogglePaneBookmark(this);
    m_pool[UIActionIndex_M_LogViewer_T_Settings] = new UIActionMenuSelectorLogTogglePaneSettings(this);
    m_pool[UIActionIndex_M_LogViewer_S_Refresh] = new UIActionMenuSelectorLogPerformRefresh(this);
    m_pool[UIActionIndex_M_LogViewer_S_Save] = new UIActionMenuSelectorLogPerformSave(this);

    /* Prepare update-handlers for known menus: */
#ifdef VBOX_WS_MAC
    m_menuUpdateHandlers[UIActionIndex_M_Application].ptf = &UIActionPool::updateMenuApplication;
    m_menuUpdateHandlers[UIActionIndex_M_Window].ptf = &UIActionPool::updateMenuWindow;
#endif
    m_menuUpdateHandlers[UIActionIndex_Menu_Help].ptf = &UIActionPool::updateMenuHelp;
    m_menuUpdateHandlers[UIActionIndex_M_LogViewerWindow].ptf = &UIActionPool::updateMenuLogViewerWindow;
    m_menuUpdateHandlers[UIActionIndex_M_LogViewer].ptf = &UIActionPool::updateMenuLogViewer;

    /* Invalidate all known menus: */
    m_invalidations.unite(m_menuUpdateHandlers.keys().toSet());

    /* Retranslate finally: */
    retranslateUi();
}

void UIActionPool::prepareConnections()
{
    /* 'Application' menu connections: */
#ifdef VBOX_WS_MAC
    connect(action(UIActionIndex_M_Application_S_About), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowHelpAboutDialog, Qt::UniqueConnection);
#endif
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    connect(action(UIActionIndex_M_Application_S_NetworkAccessManager), &UIAction::triggered,
            gNetworkManager, &UINetworkManager::show, Qt::UniqueConnection);
    connect(action(UIActionIndex_M_Application_S_CheckForUpdates), &UIAction::triggered,
            gUpdateManager, &UIUpdateManager::sltForceCheck, Qt::UniqueConnection);
#endif
    connect(action(UIActionIndex_M_Application_S_ResetWarnings), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltResetSuppressedMessages, Qt::UniqueConnection);

    /* 'Help' menu connections: */
    connect(action(UIActionIndex_Simple_Contents), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowHelpHelpDialog, Qt::UniqueConnection);
    connect(action(UIActionIndex_Simple_WebSite), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowHelpWebDialog, Qt::UniqueConnection);
    connect(action(UIActionIndex_Simple_BugTracker), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowBugTracker, Qt::UniqueConnection);
    connect(action(UIActionIndex_Simple_Forums), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowForums, Qt::UniqueConnection);
    connect(action(UIActionIndex_Simple_Oracle), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowOracle, Qt::UniqueConnection);
#ifndef VBOX_WS_MAC
    connect(action(UIActionIndex_Simple_About), &UIAction::triggered,
            &msgCenter(), &UIMessageCenter::sltShowHelpAboutDialog, Qt::UniqueConnection);
#endif
}

void UIActionPool::cleanupPool()
{
    /* Cleanup pool: */
    qDeleteAll(m_pool);
}

void UIActionPool::cleanup()
{
    /* Cleanup pool: */
    cleanupPool();
}

void UIActionPool::updateShortcuts()
{
    gShortcutPool->applyShortcuts(this);
}

bool UIActionPool::processHotKey(const QKeySequence &key)
{
    /* Iterate through the whole list of keys: */
    foreach (const int &iKey, m_pool.keys())
    {
        /* Get current action: */
        UIAction *pAction = m_pool.value(iKey);
        /* Skip menus/separators: */
        if (pAction->type() == UIActionType_Menu)
            continue;
        /* Get the hot-key of the current action: */
        const QString strHotKey = gShortcutPool->shortcut(this, pAction).toString();
        if (pAction->isEnabled() && pAction->isAllowed() && !strHotKey.isEmpty())
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

void UIActionPool::updateConfiguration()
{
    /* Recache common action restrictions: */
    // Nothing here for now..

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Recache update action restrictions: */
    bool fUpdateAllowed = gEDataManager->applicationUpdateEnabled();
    if (!fUpdateAllowed)
    {
        m_restrictedActionsMenuApplication[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::MenuApplicationActionType)
            (m_restrictedActionsMenuApplication[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::MenuApplicationActionType_CheckForUpdates);
    }
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Update menus: */
    updateMenus();
}

void UIActionPool::updateMenu(int iIndex)
{
    /* Update if menu with such index is invalidated and there is update-handler: */
    if (m_invalidations.contains(iIndex) && m_menuUpdateHandlers.contains(iIndex))
        (this->*(m_menuUpdateHandlers.value(iIndex).ptf))();
}

void UIActionPool::updateMenuApplication()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndex_M_Application)->menu();
    AssertPtrReturnVoid(pMenu);
#ifdef VBOX_WS_MAC
    AssertReturnVoid(pMenu->isConsumable());
#endif
    /* Clear contents: */
#ifdef VBOX_WS_MAC
    if (!pMenu->isConsumed())
#endif
        pMenu->clear();

    /* Separator: */
    bool fSeparator = false;

#ifdef VBOX_WS_MAC
    /* 'About' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Application_S_About)) || fSeparator;
#endif

    /* 'Preferences' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Application_S_Preferences)) || fSeparator;

#ifndef VBOX_WS_MAC
    /* Separator: */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }
#endif

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* 'Network Manager' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Application_S_NetworkAccessManager)) || fSeparator;
#endif
    /* 'Reset Warnings' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Application_S_ResetWarnings)) || fSeparator;

#ifndef VBOX_WS_MAC
    /* Separator: */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }
#endif

    /* 'Close' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Application_S_Close)) || fSeparator;

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_M_Application);
}

#ifdef VBOX_WS_MAC
void UIActionPool::updateMenuWindow()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndex_M_Window)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Separator: */
    bool fSeparator = false;

    /* 'Minimize' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_Window_S_Minimize)) || fSeparator;

    /* Separator: */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* This menu always remains invalid.. */
}
#endif /* VBOX_WS_MAC */

void UIActionPool::updateMenuHelp()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndex_Menu_Help)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Contents' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_Contents)) || fSeparator;
    /* 'Web Site' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_WebSite)) || fSeparator;
    /* 'Bug Tracker' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_BugTracker)) || fSeparator;
    /* 'Forums' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_Forums)) || fSeparator;
    /* 'Oracle' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_Oracle)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

#ifndef VBOX_WS_MAC
    /* 'About' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_Simple_About)) || fSeparator;;
#endif

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_Menu_Help);
}

void UIActionPool::updateMenuLogViewerWindow()
{
    /* Update corresponding menu: */
    updateMenuLogViewerWrapper(action(UIActionIndex_M_LogViewerWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_M_LogViewerWindow);
}

void UIActionPool::updateMenuLogViewer()
{
    /* Update corresponding menu: */
    updateMenuLogViewerWrapper(action(UIActionIndex_M_LogViewer)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_M_LogViewer);
}

void UIActionPool::updateMenuLogViewerWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Save' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_S_Save)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Find' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_T_Find)) || fSeparator;
    /* 'Filter' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_T_Filter)) || fSeparator;
    /* 'Bookmarks' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_T_Bookmark)) || fSeparator;
    /* 'Settings' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_T_Settings)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Refresh' action: */
    fSeparator = addAction(pMenu, action(UIActionIndex_M_LogViewer_S_Refresh)) || fSeparator;;
}

void UIActionPool::retranslateUi()
{
    /* Translate all the actions: */
    foreach (const int iActionPoolKey, m_pool.keys())
        m_pool[iActionPoolKey]->retranslateUi();
    /* Update shortcuts: */
    updateShortcuts();
}

bool UIActionPool::event(QEvent *pEvent)
{
    /* Depending on event-type: */
    switch ((UIEventType)pEvent->type())
    {
        case ActivateActionEventType:
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

bool UIActionPool::addAction(UIMenu *pMenu, UIAction *pAction, bool fReallyAdd /* = true */)
{
    /* Check if action is allowed: */
    const bool fIsActionAllowed = pAction->isAllowed();

#ifdef VBOX_WS_MAC
    /* Check if menu is consumable: */
    const bool fIsMenuConsumable = pMenu->isConsumable();
    /* Check if menu is NOT yet consumed: */
    const bool fIsMenuConsumed = pMenu->isConsumed();
#endif

    /* Make this action visible
     * depending on clearance state. */
    pAction->setVisible(fIsActionAllowed);

#ifdef VBOX_WS_MAC
    /* If menu is consumable: */
    if (fIsMenuConsumable)
    {
        /* Add action only if menu was not yet consumed: */
        if (!fIsMenuConsumed)
            pMenu->addAction(pAction);
    }
    /* If menu is NOT consumable: */
    else
#endif
    {
        /* Add action only if is allowed: */
        if (fIsActionAllowed && fReallyAdd)
            pMenu->addAction(pAction);
    }

    /* Return if action is allowed: */
    return fIsActionAllowed;
}

bool UIActionPool::addMenu(QList<QMenu*> &menuList, UIAction *pAction, bool fReallyAdd /* = true */)
{
    /* Check if action is allowed: */
    const bool fIsActionAllowed = pAction->isAllowed();

    /* Get action's menu: */
    UIMenu *pMenu = pAction->menu();

#ifdef VBOX_WS_MAC
    /* Check if menu is consumable: */
    const bool fIsMenuConsumable = pMenu->isConsumable();
    /* Check if menu is NOT yet consumed: */
    const bool fIsMenuConsumed = pMenu->isConsumed();
#endif

    /* Make this action visible
     * depending on clearance state. */
    pAction->setVisible(   fIsActionAllowed
#ifdef VBOX_WS_MAC
                        && !fIsMenuConsumable
#endif
                        );

#ifdef VBOX_WS_MAC
    /* If menu is consumable: */
    if (fIsMenuConsumable)
    {
        /* Add action's menu only if menu was not yet consumed: */
        if (!fIsMenuConsumed)
            menuList << pMenu;
    }
    /* If menu is NOT consumable: */
    else
#endif
    {
        /* Add action only if is allowed: */
        if (fIsActionAllowed && fReallyAdd)
            menuList << pMenu;
    }

    /* Return if action is allowed: */
    return fIsActionAllowed;
}


#include "UIActionPool.moc"

