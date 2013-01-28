/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIShortcuts class implementation
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

/* GUI includes: */
#include "UIShortcuts.h"
#include "UIActionPool.h"

void UIShortcut::setDescription(const QString &strDescription)
{
    m_strDescription = strDescription;
}

const QString& UIShortcut::description() const
{
    return m_strDescription;
}

void UIShortcut::setSequence(const QKeySequence &sequence)
{
    m_sequence = sequence;
}

const QKeySequence& UIShortcut::sequence() const
{
    return m_sequence;
}

QString UIShortcut::toString() const
{
    return m_sequence.toString();
}

UIShortcutPool* UIShortcutPool::m_pInstance = 0;

const QString UIShortcutPool::m_strShortcutKeyTemplate = QString("%1/%2");

UIShortcutPool* UIShortcutPool::instance()
{
    return m_pInstance;
}

void UIShortcutPool::create()
{
    /* Check that instance do NOT exists: */
    if (m_pInstance)
        return;

    /* Create instance: */
    new UIShortcutPool;

    /* Prepare instance: */
    m_pInstance->prepare();
}

void UIShortcutPool::destroy()
{
    /* Check that instance exists: */
    if (!m_pInstance)
        return;

    /* Cleanup instance: */
    m_pInstance->cleanup();

    /* Delete instance: */
    delete m_pInstance;
}

UIShortcut& UIShortcutPool::shortcut(UIActionPool *pActionPool, UIAction *pAction)
{
    /* Compose shortcut key: */
    QString strShortcutKey(m_strShortcutKeyTemplate.arg(pActionPool->shortcutsExtraDataID(),
                                                        pAction->shortcutExtraDataID()));
    /* Return existing if any: */
    if (m_shortcuts.contains(strShortcutKey))
        return shortcut(strShortcutKey);
    /* Create and return new one: */
    UIShortcut &newShortcut = m_shortcuts[strShortcutKey];
    newShortcut.setDescription(pAction->name());
    newShortcut.setSequence(pAction->defaultShortcut(pActionPool->type()));
    return newShortcut;
}

UIShortcut& UIShortcutPool::shortcut(const QString &strPoolID, const QString &strActionID)
{
    /* Return if present, autocreate if necessary: */
    return shortcut(m_strShortcutKeyTemplate.arg(strPoolID, strActionID));
}

void UIShortcutPool::applyShortcuts(UIActionPool *pActionPool)
{
    /* For each the action of the passed action-pool: */
    foreach (UIAction *pAction, pActionPool->actions())
    {
        /* Compose full shortcut key: */
        QString strShortcutKey = m_strShortcutKeyTemplate.arg(pActionPool->shortcutsExtraDataID(),
                                                              pAction->shortcutExtraDataID());
        /* If shortcut key is already known: */
        if (m_shortcuts.contains(strShortcutKey))
        {
            /* Get corresponding shortcut: */
            UIShortcut &existingShortcut = m_shortcuts[strShortcutKey];
            /* Copy the sequence from the shortcut to the action: */
            if (pActionPool->type() != UIActionPoolType_Runtime)
                pAction->setShortcut(existingShortcut.sequence());
            /* Copy the description from the action to the shortcut if necessary: */
            if (existingShortcut.description().isNull())
                existingShortcut.setDescription(pAction->name());
        }
        /* If shortcut key is NOT known yet: */
        else
        {
            /* Create corresponding shortcut: */
            UIShortcut &newShortcut = m_shortcuts[strShortcutKey];
            /* Copy the action's default to both the shortcut & the action: */
            newShortcut.setSequence(pAction->defaultShortcut(pActionPool->type()));
            if (pActionPool->type() != UIActionPoolType_Runtime)
                pAction->setShortcut(newShortcut.sequence());
            /* Copy the description from the action to the shortcut: */
            newShortcut.setDescription(pAction->name());
        }
    }
}

UIShortcutPool::UIShortcutPool()
{
    /* Prepare instance: */
    if (!m_pInstance)
        m_pInstance = this;
}

UIShortcutPool::~UIShortcutPool()
{
    /* Cleanup instance: */
    if (m_pInstance == this)
        m_pInstance = 0;
}

void UIShortcutPool::prepare()
{
    /* Load defaults: */
    loadDefaults();
    /* Load overrides: */
    loadOverrides();
}

void UIShortcutPool::loadDefaults()
{
    /* Runtime shortcut key template: */
    QString strRuntimeShortcutKeyTemplate(m_strShortcutKeyTemplate.arg(GUI_Input_MachineShortcuts));
    /* Default shortcut for the Runtime Popup Menu invokation: */
    m_shortcuts.insert(strRuntimeShortcutKeyTemplate.arg("PopupMenu"),
                       UIShortcut(QApplication::translate("UIActonPool", "Invoke popup menu"), QString("Home")));
}

void UIShortcutPool::loadOverrides()
{
    /* Selector shortcut prefix: */
    QString strSelectorShortcutPrefix(GUI_Input_SelectorShortcuts);
    /* Selector shortcut key template: */
    QString strSelectorShortcutKeyTemplate(m_strShortcutKeyTemplate.arg(strSelectorShortcutPrefix));
    /* Runtime shortcut prefix: */
    QString strRuntimeShortcutPrefix(GUI_Input_MachineShortcuts);
    /* Runtime shortcut key template: */
    QString strRuntimeShortcutKeyTemplate(m_strShortcutKeyTemplate.arg(strRuntimeShortcutPrefix));

    /* Iterate over all the selector records: */
    parseOverrides(vboxGlobal().virtualBox().GetExtraDataStringList(strSelectorShortcutPrefix), strSelectorShortcutKeyTemplate);
    /* Iterate over all the runtime records: */
    parseOverrides(vboxGlobal().virtualBox().GetExtraDataStringList(strRuntimeShortcutPrefix), strRuntimeShortcutKeyTemplate);
}

void UIShortcutPool::parseOverrides(const QStringList &overrides, const QString &strTemplate)
{
    /* Iterate over all the selector records: */
    foreach (const QString &strKeyValuePair, overrides)
    {
        /* Make sure record structure is valid: */
        int iDelimiterPosition = strKeyValuePair.indexOf('=');
        if (iDelimiterPosition < 0)
            continue;

        /* Get shortcut ID/value: */
        QString strShortcutID = strKeyValuePair.left(iDelimiterPosition);
        QString strShortcutValue = strKeyValuePair.right(strKeyValuePair.length() - iDelimiterPosition - 1);

        /* Compose corresponding shortcut key: */
        QString strShortcutKey(strTemplate.arg(strShortcutID));
        /* Modify map with composed key/value: */
        if (!m_shortcuts.contains(strShortcutKey))
            m_shortcuts.insert(strShortcutKey, UIShortcut(QString(), strShortcutValue));
        else
        {
            /* Get corresponding value: */
            UIShortcut &shortcut = m_shortcuts[strShortcutKey];
            /* Check if corresponding shortcut overriden by value: */
            if (shortcut.toString().compare(strShortcutValue, Qt::CaseInsensitive) != 0)
            {
                /* Shortcut unassigned? */
                if (strShortcutValue.compare("None", Qt::CaseInsensitive) == 0)
                    shortcut.setSequence(QKeySequence());
                /* Or reassigned? */
                else
                    shortcut.setSequence(QKeySequence(strShortcutValue));
            }
        }
    }
}

UIShortcut& UIShortcutPool::shortcut(const QString &strShortcutKey)
{
    return m_shortcuts[strShortcutKey];
}

