/** @file
 * VBox Qt GUI - UIActionPool class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIActionPool_h___
#define ___UIActionPool_h___

/* Qt includes: */
#include <QAction>
#include <QMenu>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIActionPolymorphic;
class UIActionPool;
class UIActionPoolRuntime;
class UIActionPoolSelector;


/** Action-pool types. */
enum UIActionPoolType
{
    UIActionPoolType_Selector,
    UIActionPoolType_Runtime
};

/** Action types. */
enum UIActionType
{
    UIActionType_Menu,
    UIActionType_Simple,
    UIActionType_Toggle,
    UIActionType_Polymorphic
};

/** Action indexes. */
enum UIActionIndex
{
    /* Various actions: */
    UIActionIndex_Simple_Preferences,
    UIActionIndex_Simple_LogDialog,

    /* 'Help' menu actions: */
    UIActionIndex_Menu_Help,
    UIActionIndex_Simple_Contents,
    UIActionIndex_Simple_WebSite,
    UIActionIndex_Simple_ResetWarnings,
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    UIActionIndex_Simple_NetworkAccessManager,
    UIActionIndex_Simple_CheckForUpdates,
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    UIActionIndex_Simple_About,

    /* Maximum index: */
    UIActionIndex_Max
};


/** QMenu extension
  * allowing to show tool-tips. */
class UIMenu : public QMenu
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIMenu();

    /** Defines whether tool-tip should be shown. */
    void setShowToolTip(bool fShowToolTips) { m_fShowToolTip = fShowToolTips; }

protected:

    /** General event handler. */
    virtual bool event(QEvent *pEvent);

private:

    /** Holds whether tool-tip should be shown. */
    bool m_fShowToolTip;
};


/** Abstract QAction extension. */
class UIAction : public QAction
{
    Q_OBJECT;

public:

    /** Returns action type. */
    UIActionType type() const { return m_type; }

    /** Returns action-pool this action belongs to. */
    UIActionPool* actionPool() const { return m_pActionPool; }

    /** Casts action to polymorphic-action. */
    UIActionPolymorphic* toActionPolymorphic();

    /** Returns current action name. */
    const QString& name() const { return m_strName; }
    /** Defines current action name. */
    void setName(const QString &strName);

    /** Returns extra-data ID to save keyboard shortcut under. */
    virtual QString shortcutExtraDataID() const { return QString(); }
    /** Returns default keyboard shortcut for this action. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const { return QKeySequence(); }

    /** Defines current keyboard shortcut for this action. */
    void setShortcut(const QKeySequence &shortcut);
    /** Make action show keyboard shortcut. */
    void showShortcut();
    /** Make action hide keyboard shortcut. */
    void hideShortcut();

    /** Retranslates action. */
    virtual void retranslateUi() = 0;

protected:

    /** Constructor. */
    UIAction(UIActionPool *pParent, UIActionType type);

    /** Returns current action name in menu. */
    QString nameInMenu() const;

    /** Updates action text accordingly. */
    virtual void updateText();

private:

    /** Holds the action type. */
    UIActionType m_type;

    /** Holds the reference to the action-pool this action belongs to. */
    UIActionPool *m_pActionPool;
    /** Holds the type of the action-pool this action belongs to. */
    UIActionPoolType m_actionPoolType;

    /** Holds the action name. */
    QString m_strName;
    /** Holds the action shortcut. */
    QKeySequence m_shortcut;
    /** Holds whether action shortcut hidden. */
    bool m_fShortcutHidden;
};


/** Abstract UIAction extension for 'Menu' action type. */
class UIActionMenu : public UIAction
{
    Q_OBJECT;

protected:

    /** Constructor, taking normal icon name and name for disabled analog. */
    UIActionMenu(UIActionPool *pParent,
                 const QString &strIcon = QString(), const QString &strIconDis = QString());
    /** Constructor, taking copy of existing icon. */
    UIActionMenu(UIActionPool *pParent,
                 const QIcon &icon);

    /** Defines whether tool-tip should be shown. */
    void setShowToolTip(bool fShowToolTip);

private:

    /** Updates action text accordingly. */
    virtual void updateText();
};


/** Abstract UIAction extension for 'Simple' action type. */
class UIActionSimple : public UIAction
{
    Q_OBJECT;

protected:

    /** Constructor, taking normal icon name and name for disabled analog. */
    UIActionSimple(UIActionPool *pParent,
                   const QString &strIcon = QString(), const QString &strIconDisabled = QString());
    /** Constructor, taking normal, small icon names and names for disabled analogs. */
    UIActionSimple(UIActionPool *pParent,
                   const QString &strIconNormal, const QString &strIconSmall,
                   const QString &strIconNormalDisabled, const QString &strIconSmallDisabled);
    /** Constructor, taking copy of existing icon. */
    UIActionSimple(UIActionPool *pParent,
                   const QIcon& icon);
};


/** Abstract UIAction extension for 'Toggle' action type. */
class UIActionToggle : public UIAction
{
    Q_OBJECT;

protected:

    /** Constructor, taking normal icon name and name for disabled analog. */
    UIActionToggle(UIActionPool *pParent,
                   const QString &strIcon = QString(), const QString &strIconDisabled = QString());
    /** Constructor, taking normal on/off icon names and names for disabled analogs. */
    UIActionToggle(UIActionPool *pParent,
                   const QString &strIconOn, const QString &strIconOff,
                   const QString &strIconOnDisabled, const QString &strIconOffDisabled);
    /** Constructor, taking copy of existing icon. */
    UIActionToggle(UIActionPool *pParent,
                   const QIcon &icon);

private:

    /** Prepare routine. */
    void prepare();
};


/** Abstract UIAction extension for 'Polymorphic' action type. */
class UIActionPolymorphic : public UIAction
{
    Q_OBJECT;

public:

    /** Returns current action state. */
    int state() const { return m_iState; }
    /** Defines current action state. */
    void setState(int iState) { m_iState = iState; retranslateUi(); }

protected:

    /** Constructor, taking normal icon name and name for disabled analog. */
    UIActionPolymorphic(UIActionPool *pParent,
                  const QString &strIcon = QString(), const QString &strIconDisabled = QString());
    /** Constructor, taking normal, small icon names and names for disabled analogs. */
    UIActionPolymorphic(UIActionPool *pParent,
                  const QString &strIconNormal, const QString &strIconSmall,
                  const QString &strIconNormalDisabled, const QString &strIconSmallDisabled);
    /** Constructor, taking copy of existing icon. */
    UIActionPolymorphic(UIActionPool *pParent,
                  const QIcon& icon);

private:

    /** Holds current action state. */
    int m_iState;
};


/** Abstract QObject extension
  * representing action-pool singleton. */
class UIActionPool : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

public:

    /** Restriction levels. */
    enum UIActionRestrictionLevel
    {
        UIActionRestrictionLevel_Base,
        UIActionRestrictionLevel_Session,
        UIActionRestrictionLevel_Logic
    };

    /** Singleton instance access member. */
    static UIActionPool* instance();

    /** Static factory constructor. */
    static void create(UIActionPoolType type);
    /** Static factory destructor. */
    static void destroy();

    /** Static factory constructor (temporary),
      * used to initialize shortcuts-pool from action-pool of passed @a type. */
    static void createTemporary(UIActionPoolType type);

    /** Cast action-pool to Runtime one. */
    UIActionPoolRuntime* toRuntime();
    /** Cast action-pool to Selector one. */
    UIActionPoolSelector* toSelector();

    /** Returns action-pool type. */
    UIActionPoolType type() const { return m_type; }

    /** Returns the action for the passed @a iIndex. */
    UIAction* action(int iIndex) const { return m_pool.value(iIndex); }
    /** Returns all the actions action-pool contains. */
    QList<UIAction*> actions() const { return m_pool.values(); }

    /** Hot-key processing delegate. */
    bool processHotKey(const QKeySequence &key);

    /** Returns extra-data ID to save keyboard shortcuts under. */
    virtual QString shortcutsExtraDataID() const = 0;

    /** Returns the list of main menus. */
    virtual QList<QMenu*> menus() const = 0;

protected slots:

    /** Loads keyboard shortcuts of action-pool into shortcuts-pool. */
    void sltApplyShortcuts();

protected:

    /** Constructor of the action-pool of passed @a type. */
    UIActionPool(UIActionPoolType type);
    /** Destructor. */
    ~UIActionPool();

    /** Prepare routine. */
    void prepare();
    /** Prepare pool routine. */
    virtual void preparePool();
    /** Prepare connections routine. */
    virtual void prepareConnections() = 0;
    /** Cleanup connections routine. */
    virtual void cleanupConnections() {}
    /** Cleanup pool routine. */
    virtual void cleanupPool();
    /** Cleanup routine. */
    void cleanup();

    /** Update configuration routine. */
    virtual void updateConfiguration() = 0;

    /** General event handler. */
    virtual bool event(QEvent *pEvent);

    /** Holds the singleton action-pool instance. */
    static UIActionPool *m_pInstance;
    /** Holds the action-pool type. */
    UIActionPoolType m_type;
    /** Holds all the actions action-pool contains. */
    QMap<int, UIAction*> m_pool;
};

#define gpActionPool UIActionPool::instance()

#endif /* !___UIActionPool_h___ */
