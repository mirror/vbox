/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPool class declaration
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

#ifndef __UIActionPool_h__
#define __UIActionPool_h__

/* Global includes: */
#include <QAction>
#include <QMenu>

/* Local includes: */
#include "QIWithRetranslateUI.h"

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
    UIActionIndex_Simple_Help,
    UIActionIndex_Simple_Web,
    UIActionIndex_Simple_ResetWarnings,
    UIActionIndex_Simple_NetworkAccessManager,
#ifdef VBOX_WITH_REGISTRATION
    UIActionIndex_Simple_Register,
#endif /* VBOX_WITH_REGISTRATION */
    UIActionIndex_Simple_Update,
    UIActionIndex_Simple_About,

    /* Maximum index: */
    UIActionIndex_Max
};

/* Basic abstract QAction reimplemetation, extending interface: */
class UIActionInterface : public QIWithRetranslateUI3<QAction>
{
    Q_OBJECT;

public:

    UIActionType type() const { return m_type; }
    virtual void setState(int /* iState */) {}
    virtual void updateAppearance() {}

protected:

    UIActionInterface(QObject *pParent, UIActionType type);

    QString menuText(const QString &strText);

private:

    UIActionType m_type;
};

/* Basic QMenu reimplemetation, extending interface: */
class UIMenuInterface : public QMenu
{
    Q_OBJECT;

public:

    UIMenuInterface();

    void setShowToolTips(bool fShowToolTips) { m_fShowToolTips = fShowToolTips; }
    bool isToolTipsShown() const { return m_fShowToolTips; }

private:

    bool event(QEvent *pEvent);

    bool m_fShowToolTips;
};

/* Abstract extention for UIActionInterface, describing 'simple' action type: */
class UISimpleAction : public UIActionInterface
{
    Q_OBJECT;

protected:

    UISimpleAction(QObject *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString());
    UISimpleAction(QObject *pParent,
                   const QSize &normalSize, const QSize &smallSize,
                   const QString &strNormalIcon, const QString &strSmallIcon,
                   const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UISimpleAction(QObject *pParent, const QIcon& icon);
};

/* Abstract extention for UIActionInterface, describing 'state' action type: */
class UIStateAction : public UIActionInterface
{
    Q_OBJECT;

protected:

    UIStateAction(QObject *pParent,
                  const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIStateAction(QObject *pParent,
                  const QSize &normalSize, const QSize &smallSize,
                  const QString &strNormalIcon, const QString &strSmallIcon,
                  const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UIStateAction(QObject *pParent, const QIcon& icon);
    void setState(int iState) { m_iState = iState; retranslateUi(); }

    int m_iState;
};

/* Abstract extention for UIActionInterface, describing 'toggle' action type: */
class UIToggleAction : public UIActionInterface
{
    Q_OBJECT;

protected:

    UIToggleAction(QObject *pParent, const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIToggleAction(QObject *pParent,
                   const QSize &normalSize, const QSize &smallSize,
                   const QString &strNormalIcon, const QString &strSmallIcon,
                   const QString &strNormalIconDis = QString(), const QString &strSmallIconDis = QString());
    UIToggleAction(QObject *pParent, const QString &strIconOn, const QString &strIconOff, const QString &strIconOnDis, const QString &strIconOffDis);
    UIToggleAction(QObject *pParent, const QIcon &icon);
    void updateAppearance() { sltUpdateAppearance(); }

private slots:

    void sltUpdateAppearance();

private:

    void init();
};

/* Abstract extention for UIActionInterface, describing 'menu' action type: */
class UIMenuAction : public UIActionInterface
{
    Q_OBJECT;

protected:

    UIMenuAction(QObject *pParent, const QString &strIcon = QString(), const QString &strIconDis = QString());
    UIMenuAction(QObject *pParent, const QIcon &icon);
};

/* Action pool types: */
enum UIActionPoolType
{
    UIActionPoolType_Selector,
    UIActionPoolType_Runtime
};

/* Singleton action pool: */
class UIActionPool : public QObject
{
    Q_OBJECT;

public:

    /* Singleton methods: */
    static UIActionPool* instance();

    /* Constructor/destructor: */
    UIActionPool(UIActionPoolType type);
    ~UIActionPool();

    /* Prepare/cleanup: */
    void prepare();
    void cleanup();

    /* Return corresponding action: */
    UIActionInterface* action(int iIndex) const { return m_pool[iIndex]; }

    /* Helping stuff: */
    void recreateMenus() { createMenus(); }
    bool processHotKey(const QKeySequence &key);

    /* Action pool type: */
    UIActionPoolType type() const { return m_type; }

protected:

    /* Virtual helping stuff: */
    virtual void createActions();
    virtual void createMenus();
    virtual void destroyPool();

    /* Event handler: */
    bool event(QEvent *pEvent);

    /* Instance: */
    static UIActionPool *m_pInstance;

    /* Action pool type: */
    UIActionPoolType m_type;

    /* Actions pool itself: */
    QMap<int, UIActionInterface*> m_pool;
};

#define gActionPool UIActionPool::instance()

#endif // __UIActionPool_h__

