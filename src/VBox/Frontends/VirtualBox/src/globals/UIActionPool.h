/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPool class declaration
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

#ifndef __UIActionPool_h__
#define __UIActionPool_h__

/* Qt includes: */
#include <QAction>
#include <QMenu>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIActionState;
class UIActionPool;

/* Action pool types: */
enum UIActionPoolType
{
    UIActionPoolType_Selector,
    UIActionPoolType_Runtime
};

/* Action types: */
enum UIActionType
{
    UIActionType_Simple,
    UIActionType_State,
    UIActionType_Toggle,
    UIActionType_Menu
};

/* Action keys: */
enum UIActionIndex
{
    /* Various dialog actions: */
    UIActionIndex_Simple_LogDialog,

    /* 'Help' menu actions: */
    UIActionIndex_Menu_Help,
    UIActionIndex_Simple_Contents,
    UIActionIndex_Simple_WebSite,
    UIActionIndex_Simple_ResetWarnings,
    UIActionIndex_Simple_NetworkAccessManager,
    UIActionIndex_Simple_CheckForUpdates,
    UIActionIndex_Simple_About,

    /* Maximum index: */
    UIActionIndex_Max
};

/* Basic abstract QAction reimplemetation, extending interface: */
class UIAction : public QIWithRetranslateUI3<QAction>
{
    Q_OBJECT;

public:

    /* API: RTTI: */
    UIActionType type() const { return m_type; }

    /* API: Parent stuff: */
    UIActionPool* actionPool() const { return m_pActionPool; }

    /* API: Update stuff: */
    virtual void update() {}

    /* API: Cast stuff: */
    UIActionState* toStateAction();

    /* API: Name stuff: */
    const QString& name() const { return m_strName; }
    void setName(const QString &strName);

    /* API: Shortcut stuff: */
    virtual QString shortcutExtraDataID() const { return QString(); }
    virtual QKeySequence defaultShortcut(UIActionPoolType) const { return QKeySequence(); }
    void setShortcut(const QKeySequence &shortcut);
    void showShortcut();
    void hideShortcut();

protected:

    /* Constructor: */
    UIAction(UIActionPool *pParent, UIActionType type);

    /* Protected API: Menu stuff: */
    QString nameInMenu() const;

private:

    /* Helper: Text stuff: */
    void updateText();

    /* Variables: */
    UIActionPool *m_pActionPool;
    UIActionType m_type;
    UIActionPoolType m_actionPoolType;
    QString m_strName;
    QKeySequence m_shortcut;
    bool m_fShortcutHidden;
};

/* Basic QMenu reimplemetation, extending interface: */
class UIMenu : public QMenu
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMenu();

    /* API: Tool-tip stuff: */
    void setShowToolTips(bool fShowToolTips) { m_fShowToolTips = fShowToolTips; }
    bool isToolTipsShown() const { return m_fShowToolTips; }

private:

    /* Helper: Event stuff: */
    bool event(QEvent *pEvent);

    /* Variables: */
    bool m_fShowToolTips;
};

/* Abstract extention for UIAction, describing 'simple' action type: */
class UIActionSimple : public UIAction
{
    Q_OBJECT;

protected:

    /* Constructors: */
    UIActionSimple(UIActionPool *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIActionSimple(UIActionPool *pParent,
                   const QSize &normalSize, const QSize &smallSize,
                   const QString &strNormalIcon, const QString &strSmallIcon,
                   const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UIActionSimple(UIActionPool *pParent,
                   const QIcon& icon);
};

/* Abstract extention for UIAction, describing 'state' action type: */
class UIActionState : public UIAction
{
    Q_OBJECT;

public:

    /* API: State stuff: */
    void setState(int iState) { m_iState = iState; retranslateUi(); }

protected:

    /* Constructors: */
    UIActionState(UIActionPool *pParent,
                  const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIActionState(UIActionPool *pParent,
                  const QSize &normalSize, const QSize &smallSize,
                  const QString &strNormalIcon, const QString &strSmallIcon,
                  const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UIActionState(UIActionPool *pParent,
                  const QIcon& icon);

    /* Variables: */
    int m_iState;
};

/* Abstract extention for UIAction, describing 'toggle' action type: */
class UIActionToggle : public UIAction
{
    Q_OBJECT;

protected:

    /* Constructors: */
    UIActionToggle(UIActionPool *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIActionToggle(UIActionPool *pParent,
                   const QSize &normalSize, const QSize &smallSize,
                   const QString &strNormalIcon, const QString &strSmallIcon,
                   const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UIActionToggle(UIActionPool *pParent,
                   const QString &strIconOn, const QString &strIconOff, const QString &strIconOnDis, const QString &strIconOffDis);
    UIActionToggle(UIActionPool *pParent,
                   const QIcon &icon);

    /* API reimplementation: Update stuff: */
    void update() { sltUpdate(); }

private slots:

    /* Handler: Update stuff: */
    void sltUpdate();

private:

    /* Helper: Prepare stuff: */
    void init();
};

/* Abstract extention for UIAction, describing 'menu' action type: */
class UIActionMenu : public UIAction
{
    Q_OBJECT;

protected:

    /* Constructors: */
    UIActionMenu(UIActionPool *pParent,
                 const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIActionMenu(UIActionPool *pParent,
                 const QIcon &icon);
};

/* Singleton action pool: */
class UIActionPool : public QObject
{
    Q_OBJECT;

public:

    /* API: Singleton stuff: */
    static UIActionPool* instance();
    static void create(UIActionPoolType type);
    static void destroy();

    /* API: Shortcut pool helper stuff: */
    static void createTemporary(UIActionPoolType type);

    /* API: RTTI: */
    UIActionPoolType type() const { return m_type; }

    /* API: Action stuff: */
    UIAction* action(int iIndex) const { return m_pool[iIndex]; }
    QList<UIAction*> actions() const { return m_pool.values(); }

    /* API: Shortcuts stuff: */
    virtual QString shortcutsExtraDataID() const = 0;

    /* API: Prepare stuff: */
    void recreateMenus() { createMenus(); }

    /* API: Hot-key handling stuff: */
    bool processHotKey(const QKeySequence &key);

protected slots:

    /* Handler: Shortcuts stuff: */
    void sltApplyShortcuts();

protected:

    /* Constructor/destructor: */
    UIActionPool(UIActionPoolType type);
    ~UIActionPool();

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Virtual helpers: Prepare/cleanup stuff: */
    virtual void createActions();
    virtual void createMenus();
    virtual void destroyPool();

    /* Helper: Event stuff: */
    bool event(QEvent *pEvent);

    /* Instance: */
    static UIActionPool *m_pInstance;
    /* Action pool type: */
    UIActionPoolType m_type;
    /* Actions pool itself: */
    QMap<int, UIAction*> m_pool;
};

#define gActionPool UIActionPool::instance()

#endif /* __UIActionPool_h__ */

