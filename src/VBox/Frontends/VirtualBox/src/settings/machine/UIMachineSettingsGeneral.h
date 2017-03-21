/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsGeneral class declaration.
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

#ifndef ___UIMachineSettingsGeneral_h___
#define ___UIMachineSettingsGeneral_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsGeneral.gen.h"
#include "UIAddDiskEncryptionPasswordDialog.h"


/** Machine settings: General page data structure. */
struct UIDataSettingsMachineGeneral
{
    /** Constructs data. */
    UIDataSettingsMachineGeneral()
        : m_strName(QString())
        , m_strGuestOsTypeId(QString())
        , m_strSnapshotsFolder(QString())
        , m_strSnapshotsHomeDir(QString())
        , m_clipboardMode(KClipboardMode_Disabled)
        , m_dndMode(KDnDMode_Disabled)
        , m_strDescription(QString())
        , m_fEncryptionEnabled(false)
        , m_fEncryptionCipherChanged(false)
        , m_fEncryptionPasswordChanged(false)
        , m_iEncryptionCipherIndex(-1)
        , m_strEncryptionPassword(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineGeneral &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_strGuestOsTypeId == other.m_strGuestOsTypeId)
               && (m_strSnapshotsFolder == other.m_strSnapshotsFolder)
               && (m_strSnapshotsHomeDir == other.m_strSnapshotsHomeDir)
               && (m_clipboardMode == other.m_clipboardMode)
               && (m_dndMode == other.m_dndMode)
               && (m_strDescription == other.m_strDescription)
               && (m_fEncryptionEnabled == other.m_fEncryptionEnabled)
               && (m_fEncryptionCipherChanged == other.m_fEncryptionCipherChanged)
               && (m_fEncryptionPasswordChanged == other.m_fEncryptionPasswordChanged)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineGeneral &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineGeneral &other) const { return !equal(other); }

    /** Holds the VM name. */
    QString  m_strName;
    /** Holds the VM OS type ID. */
    QString  m_strGuestOsTypeId;

    /** Holds the VM snapshot folder. */
    QString         m_strSnapshotsFolder;
    /** Holds the default VM snapshot folder. */
    QString         m_strSnapshotsHomeDir;
    /** Holds the VM shared clipboard mode. */
    KClipboardMode  m_clipboardMode;
    /** Holds the VM drag&drop mode. */
    KDnDMode        m_dndMode;

    /** Holds the VM description. */
    QString  m_strDescription;

    /** Holds whether the encryption is enabled. */
    bool                   m_fEncryptionEnabled;
    /** Holds whether the encryption cipher was changed. */
    bool                   m_fEncryptionCipherChanged;
    /** Holds whether the encryption password was changed. */
    bool                   m_fEncryptionPasswordChanged;
    /** Holds the encryption cipher index. */
    int                    m_iEncryptionCipherIndex;
    /** Holds the encryption password. */
    QString                m_strEncryptionPassword;
    /** Holds the encrypted medium ids. */
    EncryptedMediumMap     m_encryptedMediums;
    /** Holds the encryption passwords. */
    EncryptionPasswordMap  m_encryptionPasswords;
};
typedef UISettingsCache<UIDataSettingsMachineGeneral> UISettingsCacheMachineGeneral;


/** Machine settings: General page. */
class UIMachineSettingsGeneral : public UISettingsPageMachine,
                                 public Ui::UIMachineSettingsGeneral
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIMachineSettingsGeneral();

    /** Returns the VM OS type ID. */
    CGuestOSType guestOSType() const;
    /** Returns whether 64bit OS type ID is selected. */
    bool is64BitOSTypeSelected() const;
#ifdef VBOX_WITH_VIDEOHWACCEL
    /** Returns whether Windows OS type ID is selected. */
    bool isWindowsOSTypeSelected() const;
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /** Defines whether HW virtualization extension is enabled. */
    void setHWVirtExEnabled(bool fEnabled);

protected:

    /** Loads data into the cache from the corresponding external object(s).
      * @note This task COULD be performed in other than GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into the corresponding widgets from the cache,
      * @note This task SHOULD be performed in GUI thread only! */
    void getFromCache();

    /** Returns whether the page was changed: */
    bool changed() const { return m_cache.wasChanged(); }

    /** Saves the data from the corresponding widgets into the cache,
      * @note This task SHOULD be performed in GUI thread only! */
    void putToCache();
    /** Save data from the cache into the corresponding external object(s).
      * @note This task COULD be performed in other than GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Validation routine. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Tab-order assignment routine. */
    void setOrderAfter(QWidget *aWidget);

    /** Translation routine. */
    void retranslateUi();

private slots:

    /** Marks the encryption cipher as changed. */
    void sltMarkEncryptionCipherChanged() { m_fEncryptionCipherChanged = true; }
    /** Marks the encryption cipher and password as changed. */
    void sltMarkEncryptionPasswordChanged() { m_fEncryptionCipherChanged = true; m_fEncryptionPasswordChanged = true; }

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare 'Basic' tab routine. */
    void prepareTabBasic();
    /** Prepare 'Advanced' tab routine. */
    void prepareTabAdvanced();
    /** Prepare 'Description' tab routine. */
    void prepareTabDescription();
    /** Prepare 'Encryption' tab routine. */
    void prepareTabEncryption();

    /** Polish routine. */
    void polishPage();

    /** Holds the page cache. */
    UISettingsCacheMachineGeneral m_cache;

    /** Holds whether HW virtualization extension is enabled. */
    bool m_fHWVirtExEnabled;

    /** Holds whether the encryption cipher was changed.
      * We are holding that argument here because we do not know
      * the old <i>cipher</i> for sure to compare the new one with. */
    bool m_fEncryptionCipherChanged;
    /** Holds whether the encryption password was changed.
      * We are holding that argument here because we do not know
      * the old <i>password</i> at all to compare the new one with. */
    bool m_fEncryptionPasswordChanged;

    /** Holds the hard-coded encryption cipher list.
      * We are hard-coding it because there is no place we can get it from. */
    QStringList m_encryptionCiphers;
};

#endif /* !___UIMachineSettingsGeneral_h___ */

