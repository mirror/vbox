/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsParallel class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineSettingsParallel_h___
#define ___UIMachineSettingsParallel_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsParallel.gen.h"

/* Forward declarations: */
class UIMachineSettingsParallelPage;
class QITabWidget;


/** Machine settings: Parallel Port tab data structure. */
struct UIDataSettingsMachineParallelPort
{
    /** Constructs data. */
    UIDataSettingsMachineParallelPort()
        : m_iSlot(-1)
        , m_fPortEnabled(false)
        , m_uIRQ(0)
        , m_uIOBase(0)
        , m_strPath(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineParallelPort &other) const
    {
        return true
               && (m_iSlot == other.m_iSlot)
               && (m_fPortEnabled == other.m_fPortEnabled)
               && (m_uIRQ == other.m_uIRQ)
               && (m_uIOBase == other.m_uIOBase)
               && (m_strPath == other.m_strPath)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineParallelPort &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineParallelPort &other) const { return !equal(other); }

    /** Holds the parallel port slot number. */
    int      m_iSlot;
    /** Holds whether the parallel port is enabled. */
    bool     m_fPortEnabled;
    /** Holds the parallel port IRQ. */
    ulong    m_uIRQ;
    /** Holds the parallel port IO base. */
    ulong    m_uIOBase;
    /** Holds the parallel port path. */
    QString  m_strPath;
};
typedef UISettingsCache<UIDataSettingsMachineParallelPort> UISettingsCacheMachineParallelPort;


/** Machine settings: Parallel page data structure. */
struct UIDataSettingsMachineParallel
{
    /** Constructs data. */
    UIDataSettingsMachineParallel() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineParallel & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineParallel & /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineParallel, UISettingsCacheMachineParallelPort> UISettingsCacheMachineParallel;


/** Machine settings: Parallel Port tab. */
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


/** Machine settings: Parallel page. */
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

#endif /* !___UIMachineSettingsParallel_h___ */

