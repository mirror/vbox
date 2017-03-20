/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsParallel class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsParallel_h__
#define __UIMachineSettingsParallel_h__

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsParallel.gen.h"

/* Forward declarations: */
class UIMachineSettingsParallelPage;
class QITabWidget;

/* Machine settings / Parallel page / Port data: */
struct UIDataSettingsMachineParallelPort
{
    /* Default constructor: */
    UIDataSettingsMachineParallelPort()
        : m_iSlot(-1)
        , m_fPortEnabled(false)
        , m_uIRQ(0)
        , m_uIOBase(0)
        , m_strPath(QString()) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineParallelPort &other) const
    {
        return (m_iSlot == other.m_iSlot) &&
               (m_fPortEnabled == other.m_fPortEnabled) &&
               (m_uIRQ == other.m_uIRQ) &&
               (m_uIOBase == other.m_uIOBase) &&
               (m_strPath == other.m_strPath);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineParallelPort &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineParallelPort &other) const { return !equal(other); }
    /* Variables: */
    int m_iSlot;
    bool m_fPortEnabled;
    ulong m_uIRQ;
    ulong m_uIOBase;
    QString m_strPath;
};
typedef UISettingsCache<UIDataSettingsMachineParallelPort> UISettingsCacheMachineParallelPort;

/* Machine settings / Parallel page / Ports data: */
struct UIDataSettingsMachineParallel
{
    /* Default constructor: */
    UIDataSettingsMachineParallel() {}
    /* Operators: */
    bool operator==(const UIDataSettingsMachineParallel& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsMachineParallel& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineParallel, UISettingsCacheMachineParallelPort> UISettingsCacheMachineParallel;

class UIMachineSettingsParallel : public QIWithRetranslateUI<QWidget>,
                               public Ui::UIMachineSettingsParallel
{
    Q_OBJECT;

public:

    UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent);

    void polishTab();

    void fetchPortData(const UISettingsCacheMachineParallelPort &portCache);
    void uploadPortData(UISettingsCacheMachineParallelPort &portCache);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbParallelToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    UIMachineSettingsParallelPage *m_pParent;
    int m_iSlot;
};

/* Machine settings / Parallel page: */
class UIMachineSettingsParallelPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    UIMachineSettingsParallelPage();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Returns whether the page content was changed. */
    bool changed() const { return m_cache.wasChanged(); }

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Handles translation event. */
    void retranslateUi();

private:

    void polishPage();

    QITabWidget *mTabWidget;

    /* Cache: */
    UISettingsCacheMachineParallel m_cache;
};

#endif // __UIMachineSettingsParallel_h__

