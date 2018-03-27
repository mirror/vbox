/* $Id$ */
/** @file
 * VBox Qt GUI - UIShortcutPool class declaration.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIShortcutPool_h___
#define ___UIShortcutPool_h___

/* Qt includes: */
#include <QMap>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QKeySequence;
class QString;
class UIActionPool;
class UIAction;


/** Shortcut descriptor prototype. */
class UIShortcut
{
public:

    /** Constructs empty shortcut descriptor. */
    UIShortcut()
        : m_strDescription(QString())
        , m_sequence(QKeySequence())
        , m_defaultSequence(QKeySequence())
    {}
    /** Constructs shortcut descriptor.
      * @param  strDescription   Brings the shortcut description.
      * @param  sequence         Brings the shortcut sequence.
      * @param  defaultSequence  Brings the default shortcut sequence. */
    UIShortcut(const QString &strDescription,
               const QKeySequence &sequence,
               const QKeySequence &defaultSequence)
        : m_strDescription(strDescription)
        , m_sequence(sequence)
        , m_defaultSequence(defaultSequence)
    {}

    /** Defines the shortcut @a strDescription. */
    void setDescription(const QString &strDescription);
    /** Returns the shortcut description. */
    const QString &description() const;

    /** Defines the shortcut @a sequence. */
    void setSequence(const QKeySequence &sequence);
    /** Returns the shortcut sequence. */
    const QKeySequence &sequence() const;

    /** Defines the default shortcut @a sequence. */
    void setDefaultSequence(const QKeySequence &sequence);
    /** Returns the default shortcut sequence. */
    const QKeySequence &defaultSequence() const;

    /** Converts shortcut sequence to string. */
    QString toString() const;

private:

    /** Holds the shortcut description. */
    QString      m_strDescription;
    /** Holds the shortcut sequence. */
    QKeySequence m_sequence;
    /** Holds the default shortcut sequence. */
    QKeySequence m_defaultSequence;
};


/** QObject extension used as shortcut pool singleton. */
class UIShortcutPool : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

signals:

    /** Notifies about Selector UI shortcuts changed. */
    void sigSelectorShortcutsReloaded();
    /** Notifies about Runtime UI shortcuts changed. */
    void sigMachineShortcutsReloaded();

public:

    /** Returns singleton instance. */
    static UIShortcutPool *instance() { return s_pInstance; }
    /** Creates singleton instance. */
    static void create();
    /** Destroys singleton instance. */
    static void destroy();

    /** Returns shortcuts of particular @a pActionPool for specified @a pAction. */
    UIShortcut &shortcut(UIActionPool *pActionPool, UIAction *pAction);
    /** Returns shortcuts of action-pool with @a strPoolID for action with @a strActionID. */
    UIShortcut &shortcut(const QString &strPoolID, const QString &strActionID);
    /** Returns all the shortcuts. */
    const QMap<QString, UIShortcut> &shortcuts() const { return m_shortcuts; }
    /** Defines shortcut overrides. */
    void setOverrides(const QMap<QString, QString> &overrides);

    /** Applies shortcuts for specified @a pActionPool. */
    void applyShortcuts(UIActionPool *pActionPool);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Reloads Selector UI shortcuts. */
    void sltReloadSelectorShortcuts();
    /** Reloads Runtime UI shortcuts. */
    void sltReloadMachineShortcuts();

private:

    /** Constructs shortcut pool. */
    UIShortcutPool();
    /** Destructs shortcut pool. */
    ~UIShortcutPool();

    /** Prepares all. */
    void prepare();
    /** Prepares connections. */
    void prepareConnections();

    /** Cleanups all. */
    void cleanup() {}

    /** Loads defaults. */
    void loadDefaults();
    /** Loads defaults for pool with specified @a strPoolExtraDataID. */
    void loadDefaultsFor(const QString &strPoolExtraDataID);
    /** Loads overrides. */
    void loadOverrides();
    /** Loads overrides for pool with specified @a strPoolExtraDataID. */
    void loadOverridesFor(const QString &strPoolExtraDataID);
    /** Saves overrides. */
    void saveOverrides();
    /** Saves overrides for pool with specified @a strPoolExtraDataID. */
    void saveOverridesFor(const QString &strPoolExtraDataID);

    /** Returns shortcut with specified @a strShortcutKey. */
    UIShortcut &shortcut(const QString &strShortcutKey);

    /** Holds the singleton instance. */
    static UIShortcutPool *s_pInstance;
    /** Shortcut key template. */
    static const QString   s_strShortcutKeyTemplate;
    /** Shortcut key template for Runtime UI. */
    static const QString   s_strShortcutKeyTemplateRuntime;

    /** Holds the pool shortcuts. */
    QMap<QString, UIShortcut> m_shortcuts;
};

/** Singleton Shortcut Pool 'official' name. */
#define gShortcutPool UIShortcutPool::instance()


#endif /* !___UIShortcutPool_h___ */

