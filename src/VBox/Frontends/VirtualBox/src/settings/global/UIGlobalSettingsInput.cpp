/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInput class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UIAutoCaptureKeyboardEditor.h"
#include "UICommon.h"
#include "UIGlobalSettingsInput.h"
#include "UIHostComboEditor.h"
#include "UIShortcutConfigurationEditor.h"
#include "UIShortcutPool.h"
#include "UIExtraDataManager.h"


/** Global settings: Input page data structure. */
struct UIDataSettingsGlobalInput
{
    /** Constructs cache. */
    UIDataSettingsGlobalInput()
        : m_fAutoCapture(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalInput &other) const
    {
        return    true
               && (m_shortcuts == other.m_shortcuts)
               && (m_fAutoCapture == other.m_fAutoCapture)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalInput &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalInput &other) const { return !equal(other); }

    /** Holds the shortcut configuration list. */
    UIShortcutConfigurationList  m_shortcuts;
    /** Holds whether the keyboard auto-capture is enabled. */
    bool                         m_fAutoCapture;
};


UIGlobalSettingsInput::UIGlobalSettingsInput()
    : m_pCache(0)
    , m_pEditorShortcutConfiguration(0)
    , m_pLabelInputExtended(0)
    , m_pEditorAutoCaptureKeyboard(0)
{
    prepare();
}

UIGlobalSettingsInput::~UIGlobalSettingsInput()
{
    cleanup();
}

void UIGlobalSettingsInput::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalInput oldData;
    UIShortcutConfigurationList list;
    list << UIShortcutConfigurationItem(UIHostCombo::hostComboCacheKey(),
                                        QString(),
                                        tr("Host Key Combination"),
                                        gEDataManager->hostKeyCombination(),
                                        QString());
    const QMap<QString, UIShortcut> &shortcuts = gShortcutPool->shortcuts();
    const QList<QString> shortcutKeys = shortcuts.keys();
    foreach (const QString &strShortcutKey, shortcutKeys)
    {
        const UIShortcut &shortcut = shortcuts.value(strShortcutKey);
        list << UIShortcutConfigurationItem(strShortcutKey,
                                            shortcut.scope(),
                                            UICommon::removeAccelMark(shortcut.description()),
                                            shortcut.primaryToNativeText(),
                                            shortcut.defaultSequence().toString(QKeySequence::NativeText));
    }
    oldData.m_shortcuts = list;
    oldData.m_fAutoCapture = gEDataManager->autoCaptureEnabled();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsInput::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalInput &oldData = m_pCache->base();
    m_pEditorShortcutConfiguration->load(oldData.m_shortcuts);
    m_pEditorAutoCaptureKeyboard->setValue(oldData.m_fAutoCapture);

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsInput::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalInput newData = m_pCache->base();

    /* Cache new data: */
    m_pEditorShortcutConfiguration->save(newData.m_shortcuts);
    newData.m_fAutoCapture = m_pEditorAutoCaptureKeyboard->value();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsInput::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsInput::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Check VirtualBox Manager page for unique shortcuts: */
    if (!m_pEditorShortcutConfiguration->isShortcutsUniqueManager())
    {
        UIValidationMessage message;
        message.first = UICommon::removeAccelMark(m_pEditorShortcutConfiguration->tabNameManager());
        message.second << tr("Some items have the same shortcuts assigned.");
        messages << message;
        fPass = false;
    }

    /* Check Virtual Runtime page for unique shortcuts: */
    if (!m_pEditorShortcutConfiguration->isShortcutsUniqueRuntime())
    {
        UIValidationMessage message;
        message.first = UICommon::removeAccelMark(m_pEditorShortcutConfiguration->tabNameRuntime());
        message.second << tr("Some items have the same shortcuts assigned.");
        messages << message;
        fPass = false;
    }

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsInput::retranslateUi()
{
    m_pLabelInputExtended->setText(tr("Extended Features:"));
}

void UIGlobalSettingsInput::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalInput;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsInput::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        /* Prepare 'shortcut configuration' editor: */
        m_pEditorShortcutConfiguration = new UIShortcutConfigurationEditor(this);
        if (m_pEditorShortcutConfiguration)
            pLayoutMain->addWidget(m_pEditorShortcutConfiguration, 0, 0, 1, 2);

        /* Prepare input extended label: */
        m_pLabelInputExtended = new QLabel(this);
        if (m_pLabelInputExtended)
        {
            m_pLabelInputExtended->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelInputExtended, 1, 0);
        }
        /* Prepare 'auto capture keyboard' editor: */
        m_pEditorAutoCaptureKeyboard = new UIAutoCaptureKeyboardEditor(this);
        if (m_pEditorAutoCaptureKeyboard)
            pLayoutMain->addWidget(m_pEditorAutoCaptureKeyboard, 1, 1);
    }
}

void UIGlobalSettingsInput::prepareConnections()
{
    connect(m_pEditorShortcutConfiguration, &UIShortcutConfigurationEditor::sigValueChanged,
            this, &UIGlobalSettingsInput::revalidate);
}

void UIGlobalSettingsInput::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsInput::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save settings from cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalInput &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalInput &newData = m_pCache->data();

        /* Save new host-combo shortcut from cache: */
        const UIShortcutConfigurationItem fakeHostComboItem(UIHostCombo::hostComboCacheKey(), QString(), QString(), QString(), QString());
        const int iHostComboItemBase = UIShortcutSearchFunctor<UIShortcutConfigurationItem>()(oldData.m_shortcuts, fakeHostComboItem);
        const int iHostComboItemData = UIShortcutSearchFunctor<UIShortcutConfigurationItem>()(newData.m_shortcuts, fakeHostComboItem);
        const QString strHostComboBase = iHostComboItemBase != -1 ? oldData.m_shortcuts.at(iHostComboItemBase).currentSequence() : QString();
        const QString strHostComboData = iHostComboItemData != -1 ? newData.m_shortcuts.at(iHostComboItemData).currentSequence() : QString();
        if (strHostComboData != strHostComboBase)
            gEDataManager->setHostKeyCombination(strHostComboData);

        /* Save other new shortcuts from cache: */
        QMap<QString, QString> sequencesBase;
        QMap<QString, QString> sequencesData;
        foreach (const UIShortcutConfigurationItem &item, oldData.m_shortcuts)
            sequencesBase.insert(item.key(), item.currentSequence());
        foreach (const UIShortcutConfigurationItem &item, newData.m_shortcuts)
            sequencesData.insert(item.key(), item.currentSequence());
        if (sequencesData != sequencesBase)
            gShortcutPool->setOverrides(sequencesData);

        /* Save other new things from cache: */
        if (newData.m_fAutoCapture != oldData.m_fAutoCapture)
            gEDataManager->setAutoCaptureEnabled(newData.m_fAutoCapture);
    }
    /* Return result: */
    return fSuccess;
}
