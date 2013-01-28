/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIShortcuts class declaration
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIShortcuts_h__
#define __UIShortcuts_h__

/* Qt includes: */
#include <QMap>

/* GUI includes: */
#include "VBoxGlobal.h"

/* Forward declarations: */
class UIActionPool;
class UIAction;

/* Shortcut descriptor: */
class UIShortcut
{
public:

    /* Constructor: */
    UIShortcut(const QString &strDescription = QString(),
               const QKeySequence &sequence = QKeySequence())
        : m_strDescription(strDescription), m_sequence(sequence) {}

    /* API: Description stuff: */
    void setDescription(const QString &strDescription);
    const QString& description() const;

    /* API: Sequence stuff: */
    void setSequence(const QKeySequence &sequence);
    const QKeySequence& sequence() const;

    /* API: Conversion stuff: */
    QString toString() const;

private:

    /* Variables: */
    QString m_strDescription;
    QKeySequence m_sequence;
};

/* Singleton shortcut pool: */
class UIShortcutPool : public QObject
{
    Q_OBJECT;

public:

    /* API: Singleton stuff: */
    static UIShortcutPool* instance();
    static void create();
    static void destroy();

    /* API: Shortcut stuff: */
    UIShortcut& shortcut(UIActionPool *pActionPool, UIAction *pAction);
    UIShortcut& shortcut(const QString &strPoolID, const QString &strActionID);

    /* API: Action-pool stuff: */
    void applyShortcuts(UIActionPool *pActionPool);

private:

    /* Constructor/destructor: */
    UIShortcutPool();
    ~UIShortcutPool();

    /* Prepare/cleanup helpers: */
    void prepare();
    void cleanup() {}

    /* Helpers: Shortcuts stuff: */
    void loadDefaults();
    void loadOverrides();
    void parseOverrides(const QStringList &overrides, const QString &strTemplate);

    /* Helper: Shortcut stuff: */
    UIShortcut& shortcut(const QString &strShortcutKey);

    /* Variables: */
    static UIShortcutPool *m_pInstance;
    static const QString m_strShortcutKeyTemplate;
    QMap<QString, UIShortcut> m_shortcuts;
};

#define gShortcutPool UIShortcutPool::instance()

#endif /* __UIShortcuts_h__ */

