/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsParallel class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
typedef UISettingsCache<UIDataSettingsMachineParallelPort> UICacheSettingsMachineParallelPort;

/* Machine settings / Parallel page / Ports data: */
struct UIDataSettingsMachineParallel
{
    /* Default constructor: */
    UIDataSettingsMachineParallel() {}
    /* Operators: */
    bool operator==(const UIDataSettingsMachineParallel& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsMachineParallel& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineParallel, UICacheSettingsMachineParallelPort> UICacheSettingsMachineParallel;

class UIMachineSettingsParallel : public QIWithRetranslateUI<QWidget>,
                               public Ui::UIMachineSettingsParallel
{
    Q_OBJECT;

public:

    UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent);

    void polishTab();

    void fetchPortData(const UICacheSettingsMachineParallelPort &portCache);
    void uploadPortData(UICacheSettingsMachineParallelPort &portCache);

    /* API: Validation stuff: */
#ifdef VBOX_WITH_NEW_SETTINGS_VALIDATOR
    void setValidator(UIPageValidator *pValidator);
#else /* VBOX_WITH_NEW_SETTINGS_VALIDATOR */
    void setValidator(QIWidgetValidator *pValidator);
#endif /* !VBOX_WITH_NEW_SETTINGS_VALIDATOR */

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbParallelToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);

private:

    /* Variable: Validation stuff: */
#ifdef VBOX_WITH_NEW_SETTINGS_VALIDATOR
    UIPageValidator *m_pValidator;
#else /* VBOX_WITH_NEW_SETTINGS_VALIDATOR */
    QIWidgetValidator *m_pValidator;
#endif /* !VBOX_WITH_NEW_SETTINGS_VALIDATOR */

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

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* Page changed: */
    bool changed() const { return m_cache.wasChanged(); }

    /* API: Validation stuff: */
#ifdef VBOX_WITH_NEW_SETTINGS_VALIDATOR
    void setValidator(UIPageValidator *pValidator);
#else /* VBOX_WITH_NEW_SETTINGS_VALIDATOR */
    void setValidator(QIWidgetValidator *pValidator);
#endif /* !VBOX_WITH_NEW_SETTINGS_VALIDATOR */
    bool revalidate (QString &aWarning, QString &aTitle);

    void retranslateUi();

private:

    void polishPage();

    /* Variable: Validation stuff: */
#ifdef VBOX_WITH_NEW_SETTINGS_VALIDATOR
    UIPageValidator *m_pValidator;
#else /* VBOX_WITH_NEW_SETTINGS_VALIDATOR */
    QIWidgetValidator *m_pValidator;
#endif /* !VBOX_WITH_NEW_SETTINGS_VALIDATOR */

    QITabWidget *mTabWidget;

    /* Cache: */
    UICacheSettingsMachineParallel m_cache;
};

#endif // __UIMachineSettingsParallel_h__

