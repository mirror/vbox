/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsGeneral class implementation.
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
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITabWidget.h"
#include "QIWidgetValidator.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIFilePathSelector.h"
#include "UIMachineSettingsGeneral.h"
#include "UIModalWindowManager.h"
#include "UINameAndSystemEditor.h"
#include "UIProgressObject.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"
#include "CMedium.h"
#include "CMediumAttachment.h"
#include "CProgress.h"


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
    /** Holds the VM clipboard mode. */
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
    EncryptedMediumMap     m_encryptedMedia;
    /** Holds the encryption passwords. */
    EncryptionPasswordMap  m_encryptionPasswords;
};


UIMachineSettingsGeneral::UIMachineSettingsGeneral()
    : m_fHWVirtExEnabled(false)
    , m_fEncryptionCipherChanged(false)
    , m_fEncryptionPasswordChanged(false)
    , m_pCache(0)
    , m_pTabWidget(0)
    , m_pTabBasic(0)
    , m_pEditorNameAndSystem(0)
    , m_pTabAdvanced(0)
    , m_pLabelSnapshotFolder(0)
    , m_pEditorSnapshotFolder(0)
    , m_pLabelClipboard(0)
    , m_pComboClipboard(0)
    , m_pLabelDragAndDrop(0)
    , m_pComboDragAndDrop(0)
    , m_pTabDescription(0)
    , m_pEditorDescription(0)
    , m_pTabEncryption(0)
    , m_pCheckBoxEncryption(0)
    , m_pWidgetEncryptionSettings(0)
    , m_pLabelCipher(0)
    , m_pComboCipher(0)
    , m_pLabelEncryptionPassword(0)
    , m_pEditorEncryptionPassword(0)
    , m_pLabelEncryptionPasswordConfirm(0)
    , m_pEditorEncryptionPasswordConfirm(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsGeneral::~UIMachineSettingsGeneral()
{
    /* Cleanup: */
    cleanup();
}

CGuestOSType UIMachineSettingsGeneral::guestOSType() const
{
    AssertPtrReturn(m_pEditorNameAndSystem, CGuestOSType());
    return m_pEditorNameAndSystem->type();
}

bool UIMachineSettingsGeneral::is64BitOSTypeSelected() const
{
    AssertPtrReturn(m_pEditorNameAndSystem, false);
    return   m_pEditorNameAndSystem->type().isNotNull()
           ? m_pEditorNameAndSystem->type().GetIs64Bit()
           : false;
}

void UIMachineSettingsGeneral::setHWVirtExEnabled(bool fEnabled)
{
    /* Make sure hardware virtualization extension has changed: */
    if (m_fHWVirtExEnabled == fEnabled)
        return;

    /* Update hardware virtualization extension value: */
    m_fHWVirtExEnabled = fEnabled;

    /* Revalidate: */
    revalidate();
}

bool UIMachineSettingsGeneral::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old general data: */
    UIDataSettingsMachineGeneral oldGeneralData;

    /* Gather old 'Basic' data: */
    oldGeneralData.m_strName = m_machine.GetName();
    oldGeneralData.m_strGuestOsTypeId = m_machine.GetOSTypeId();

    /* Gather old 'Advanced' data: */
    oldGeneralData.m_strSnapshotsFolder = m_machine.GetSnapshotFolder();
    oldGeneralData.m_strSnapshotsHomeDir = QFileInfo(m_machine.GetSettingsFilePath()).absolutePath();
    oldGeneralData.m_clipboardMode = m_machine.GetClipboardMode();
    oldGeneralData.m_dndMode = m_machine.GetDnDMode();

    /* Gather old 'Description' data: */
    oldGeneralData.m_strDescription = m_machine.GetDescription();

    /* Gather old 'Encryption' data: */
    QString strCipher;
    bool fEncryptionCipherCommon = true;
    /* Prepare the map of the encrypted media: */
    EncryptedMediumMap encryptedMedia;
    foreach (const CMediumAttachment &attachment, m_machine.GetMediumAttachments())
    {
        /* Check hard-drive attachments only: */
        if (attachment.GetType() == KDeviceType_HardDisk)
        {
            /* Get the attachment medium base: */
            const CMedium comMedium = attachment.GetMedium();
            /* Check medium encryption attributes: */
            QString strCurrentCipher;
            const QString strCurrentPasswordId = comMedium.GetEncryptionSettings(strCurrentCipher);
            if (comMedium.isOk())
            {
                encryptedMedia.insert(strCurrentPasswordId, comMedium.GetId());
                if (strCurrentCipher != strCipher)
                {
                    if (strCipher.isNull())
                        strCipher = strCurrentCipher;
                    else
                        fEncryptionCipherCommon = false;
                }
            }
        }
    }
    oldGeneralData.m_fEncryptionEnabled = !encryptedMedia.isEmpty();
    oldGeneralData.m_fEncryptionCipherChanged = false;
    oldGeneralData.m_fEncryptionPasswordChanged = false;
    if (fEncryptionCipherCommon)
        oldGeneralData.m_iEncryptionCipherIndex = m_encryptionCiphers.indexOf(strCipher);
    if (oldGeneralData.m_iEncryptionCipherIndex == -1)
        oldGeneralData.m_iEncryptionCipherIndex = 0;
    oldGeneralData.m_encryptedMedia = encryptedMedia;

    /* Cache old general data: */
    m_pCache->cacheInitialData(oldGeneralData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsGeneral::getFromCache()
{
    /* Get old general data from the cache: */
    const UIDataSettingsMachineGeneral &oldGeneralData = m_pCache->base();

    /* We are doing that *now* because these combos have
     * dynamical content which depends on cashed value: */
    repopulateComboClipboardMode();
    repopulateComboDnDMode();

    /* Load old 'Basic' data from the cache: */
    AssertPtrReturnVoid(m_pEditorNameAndSystem);
    m_pEditorNameAndSystem->setName(oldGeneralData.m_strName);
    m_pEditorNameAndSystem->setTypeId(oldGeneralData.m_strGuestOsTypeId);

    /* Load old 'Advanced' data from the cache: */
    AssertPtrReturnVoid(m_pEditorSnapshotFolder);
    AssertPtrReturnVoid(m_pComboClipboard);
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    m_pEditorSnapshotFolder->setPath(oldGeneralData.m_strSnapshotsFolder);
    m_pEditorSnapshotFolder->setInitialPath(oldGeneralData.m_strSnapshotsHomeDir);
    const int iClipboardModePosition = m_pComboClipboard->findData(oldGeneralData.m_clipboardMode);
    m_pComboClipboard->setCurrentIndex(iClipboardModePosition == -1 ? 0 : iClipboardModePosition);
    const int iDnDModePosition = m_pComboDragAndDrop->findData(oldGeneralData.m_dndMode);
    m_pComboDragAndDrop->setCurrentIndex(iDnDModePosition == -1 ? 0 : iDnDModePosition);

    /* Load old 'Description' data from the cache: */
    AssertPtrReturnVoid(m_pEditorDescription);
    m_pEditorDescription->setPlainText(oldGeneralData.m_strDescription);

    /* Load old 'Encryption' data from the cache: */
    AssertPtrReturnVoid(m_pCheckBoxEncryption);
    AssertPtrReturnVoid(m_pComboCipher);
    m_pCheckBoxEncryption->setChecked(oldGeneralData.m_fEncryptionEnabled);
    m_pComboCipher->setCurrentIndex(oldGeneralData.m_iEncryptionCipherIndex);
    m_fEncryptionCipherChanged = oldGeneralData.m_fEncryptionCipherChanged;
    m_fEncryptionPasswordChanged = oldGeneralData.m_fEncryptionPasswordChanged;

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsGeneral::putToCache()
{
    /* Prepare new general data: */
    UIDataSettingsMachineGeneral newGeneralData;

    /* Gather new 'Basic' data: */
    AssertPtrReturnVoid(m_pEditorNameAndSystem);
    newGeneralData.m_strName = m_pEditorNameAndSystem->name();
    newGeneralData.m_strGuestOsTypeId = m_pEditorNameAndSystem->typeId();

    /* Gather new 'Advanced' data: */
    AssertPtrReturnVoid(m_pEditorSnapshotFolder);
    AssertPtrReturnVoid(m_pComboClipboard);
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    newGeneralData.m_strSnapshotsFolder = m_pEditorSnapshotFolder->path();
    newGeneralData.m_clipboardMode = m_pComboClipboard->currentData().value<KClipboardMode>();
    newGeneralData.m_dndMode = m_pComboDragAndDrop->currentData().value<KDnDMode>();

    /* Gather new 'Description' data: */
    AssertPtrReturnVoid(m_pEditorDescription);
    newGeneralData.m_strDescription = m_pEditorDescription->toPlainText().isEmpty() ?
                                      QString() : m_pEditorDescription->toPlainText();

    /* Gather new 'Encryption' data: */
    AssertPtrReturnVoid(m_pCheckBoxEncryption);
    AssertPtrReturnVoid(m_pComboCipher);
    AssertPtrReturnVoid(m_pEditorEncryptionPassword);
    newGeneralData.m_fEncryptionEnabled = m_pCheckBoxEncryption->isChecked();
    newGeneralData.m_fEncryptionCipherChanged = m_fEncryptionCipherChanged;
    newGeneralData.m_fEncryptionPasswordChanged = m_fEncryptionPasswordChanged;
    newGeneralData.m_iEncryptionCipherIndex = m_pComboCipher->currentIndex();
    newGeneralData.m_strEncryptionPassword = m_pEditorEncryptionPassword->text();
    newGeneralData.m_encryptedMedia = m_pCache->base().m_encryptedMedia;
    /* If encryption status, cipher or password is changed: */
    if (newGeneralData.m_fEncryptionEnabled != m_pCache->base().m_fEncryptionEnabled ||
        newGeneralData.m_fEncryptionCipherChanged != m_pCache->base().m_fEncryptionCipherChanged ||
        newGeneralData.m_fEncryptionPasswordChanged != m_pCache->base().m_fEncryptionPasswordChanged)
    {
        /* Ask for the disk encryption passwords if necessary: */
        if (!m_pCache->base().m_encryptedMedia.isEmpty())
        {
            /* Create corresponding dialog: */
            QWidget *pDlgParent = windowManager().realParentWindow(window());
            QPointer<UIAddDiskEncryptionPasswordDialog> pDlg =
                 new UIAddDiskEncryptionPasswordDialog(pDlgParent,
                                                       newGeneralData.m_strName,
                                                       newGeneralData.m_encryptedMedia);
            /* Execute it and acquire the result: */
            if (pDlg->exec() == QDialog::Accepted)
                newGeneralData.m_encryptionPasswords = pDlg->encryptionPasswords();
            /* Delete dialog if still valid: */
            if (pDlg)
                delete pDlg;
        }
    }

    /* Cache new general data: */
    m_pCache->cacheCurrentData(newGeneralData);
}

void UIMachineSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update general data and failing state: */
    setFailed(!saveGeneralData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsGeneral::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* 'Basic' tab validations: */
    message.first = UICommon::removeAccelMark(m_pTabWidget->tabText(0));
    message.second.clear();

    /* VM name validation: */
    AssertPtrReturn(m_pEditorNameAndSystem, false);
    if (m_pEditorNameAndSystem->name().trimmed().isEmpty())
    {
        message.second << tr("No name specified for the virtual machine.");
        fPass = false;
    }

    /* OS type & VT-x/AMD-v correlation: */
    if (is64BitOSTypeSelected() && !m_fHWVirtExEnabled)
    {
        message.second << tr("The virtual machine operating system hint is set to a 64-bit type. "
                             "64-bit guest systems require hardware virtualization, "
                             "so this will be enabled automatically if you confirm the changes.");
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* 'Encryption' tab validations: */
    message.first = UICommon::removeAccelMark(m_pTabWidget->tabText(3));
    message.second.clear();

    /* Encryption validation: */
    AssertPtrReturn(m_pCheckBoxEncryption, false);
    if (m_pCheckBoxEncryption->isChecked())
    {
        /* Encryption Extension Pack presence test: */
        CExtPackManager extPackManager = uiCommon().virtualBox().GetExtensionPackManager();
        if (!extPackManager.isNull() && !extPackManager.IsExtPackUsable(GUI_ExtPackName))
        {
            message.second << tr("You are trying to enable disk encryption for this virtual machine. "
                                 "However, this requires the <i>%1</i> to be installed. "
                                 "Please install the Extension Pack from the VirtualBox download site.")
                                 .arg(GUI_ExtPackName);
            fPass = false;
        }

        /* Cipher should be chosen if once changed: */
        AssertPtrReturn(m_pComboCipher, false);
        if (!m_pCache->base().m_fEncryptionEnabled ||
            m_fEncryptionCipherChanged)
        {
            if (m_pComboCipher->currentIndex() == 0)
                message.second << tr("Disk encryption cipher type not specified.");
            fPass = false;
        }

        /* Password should be entered and confirmed if once changed: */
        AssertPtrReturn(m_pEditorEncryptionPassword, false);
        AssertPtrReturn(m_pEditorEncryptionPasswordConfirm, false);
        if (!m_pCache->base().m_fEncryptionEnabled ||
            m_fEncryptionPasswordChanged)
        {
            if (m_pEditorEncryptionPassword->text().isEmpty())
                message.second << tr("Disk encryption password empty.");
            else
            if (m_pEditorEncryptionPassword->text() !=
                m_pEditorEncryptionPasswordConfirm->text())
                message.second << tr("Disk encryption passwords do not match.");
            fPass = false;
        }
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIMachineSettingsGeneral::setOrderAfter(QWidget *pWidget)
{
    /* 'Basic' tab: */
    AssertPtrReturnVoid(pWidget);
    AssertPtrReturnVoid(m_pTabWidget);
    AssertPtrReturnVoid(m_pTabWidget->focusProxy());
    AssertPtrReturnVoid(m_pEditorNameAndSystem);
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pEditorNameAndSystem);

    /* 'Advanced' tab: */
    AssertPtrReturnVoid(m_pEditorSnapshotFolder);
    AssertPtrReturnVoid(m_pComboClipboard);
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    setTabOrder(m_pEditorNameAndSystem, m_pEditorSnapshotFolder);
    setTabOrder(m_pEditorSnapshotFolder, m_pComboClipboard);
    setTabOrder(m_pComboClipboard, m_pComboDragAndDrop);

    /* 'Description' tab: */
    AssertPtrReturnVoid(m_pEditorDescription);
    setTabOrder(m_pComboDragAndDrop, m_pEditorDescription);
}

void UIMachineSettingsGeneral::retranslateUi()
{
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabBasic), QApplication::translate("UIMachineSettingsGeneral", "Basi&c"));
    m_pLabelSnapshotFolder->setText(QApplication::translate("UIMachineSettingsGeneral", "S&napshot Folder:"));
    m_pLabelClipboard->setText(QApplication::translate("UIMachineSettingsGeneral", "&Shared Clipboard:"));
    m_pComboClipboard->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Selects which clipboard data will be copied "
                                                       "between the guest and the host OS. This feature requires Guest Additions "
                                                       "to be installed in the guest OS."));
    m_pLabelDragAndDrop->setText(QApplication::translate("UIMachineSettingsGeneral", "D&rag'n'Drop:"));
    m_pComboDragAndDrop->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Selects which data will be copied between "
                                                         "the guest and the host OS by drag'n'drop. This feature requires Guest "
                                                         "Additions to be installed in the guest OS."));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabAdvanced), QApplication::translate("UIMachineSettingsGeneral", "A&dvanced"));
    m_pEditorDescription->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Holds the description of the virtual machine. "
                                                         "The description field is useful for commenting on configuration details "
                                                         "of the installed guest OS."));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabDescription), QApplication::translate("UIMachineSettingsGeneral", "D&escription"));
    m_pCheckBoxEncryption->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "When checked, disks attached to this "
                                                                "virtual machine will be encrypted."));
    m_pCheckBoxEncryption->setText(QApplication::translate("UIMachineSettingsGeneral", "En&able Disk Encryption"));
    m_pLabelCipher->setText(QApplication::translate("UIMachineSettingsGeneral", "Disk Encryption C&ipher:"));
    m_pComboCipher->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Selects the cipher to be used for encrypting "
                                                         "the virtual machine disks."));
    m_pLabelEncryptionPassword->setText(QApplication::translate("UIMachineSettingsGeneral", "E&nter New Password:"));
    m_pEditorEncryptionPassword->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Holds the encryption password "
                                                                      "for disks attached to this virtual machine."));
    m_pLabelEncryptionPasswordConfirm->setText(QApplication::translate("UIMachineSettingsGeneral", "C&onfirm New Password:"));
    m_pEditorEncryptionPasswordConfirm->setWhatsThis(QApplication::translate("UIMachineSettingsGeneral", "Confirms the disk encryption password."));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabEncryption), QApplication::translate("UIMachineSettingsGeneral", "Disk Enc&ryption"));

    /* Translate path selector: */
    AssertPtrReturnVoid(m_pEditorSnapshotFolder);
    m_pEditorSnapshotFolder->setWhatsThis(tr("Holds the path where snapshots of this "
                                 "virtual machine will be stored. Be aware that "
                                 "snapshots can take quite a lot of storage space."));

    /* Translate Clipboard mode combo: */
    AssertPtrReturnVoid(m_pComboClipboard);
    for (int iIndex = 0; iIndex < m_pComboClipboard->count(); ++iIndex)
    {
        const KClipboardMode enmType = m_pComboClipboard->currentData().value<KClipboardMode>();
        m_pComboClipboard->setItemText(iIndex, gpConverter->toString(enmType));
    }

    /* Translate Drag'n'drop mode combo: */
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    for (int iIndex = 0; iIndex < m_pComboDragAndDrop->count(); ++iIndex)
    {
        const KDnDMode enmType = m_pComboDragAndDrop->currentData().value<KDnDMode>();
        m_pComboDragAndDrop->setItemText(iIndex, gpConverter->toString(enmType));
    }

    /* Translate Cipher type combo: */
    AssertPtrReturnVoid(m_pComboCipher);
    m_pComboCipher->setItemText(0, tr("Leave Unchanged", "cipher type"));
}

void UIMachineSettingsGeneral::polishPage()
{
    /* Polish 'Basic' availability: */
    AssertPtrReturnVoid(m_pEditorNameAndSystem);
    m_pEditorNameAndSystem->setNameStuffEnabled(isMachineOffline() || isMachineSaved());
    m_pEditorNameAndSystem->setPathStuffEnabled(isMachineOffline());
    m_pEditorNameAndSystem->setOSTypeStuffEnabled(isMachineOffline());

    /* Polish 'Advanced' availability: */
    AssertPtrReturnVoid(m_pLabelSnapshotFolder);
    AssertPtrReturnVoid(m_pEditorSnapshotFolder);
    AssertPtrReturnVoid(m_pLabelClipboard);
    AssertPtrReturnVoid(m_pComboClipboard);
    AssertPtrReturnVoid(m_pLabelDragAndDrop);
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    m_pLabelSnapshotFolder->setEnabled(isMachineOffline());
    m_pEditorSnapshotFolder->setEnabled(isMachineOffline());
    m_pLabelClipboard->setEnabled(isMachineInValidMode());
    m_pComboClipboard->setEnabled(isMachineInValidMode());
    m_pLabelDragAndDrop->setEnabled(isMachineInValidMode());
    m_pComboDragAndDrop->setEnabled(isMachineInValidMode());

    /* Polish 'Description' availability: */
    AssertPtrReturnVoid(m_pEditorDescription);
    m_pEditorDescription->setEnabled(isMachineInValidMode());

    /* Polish 'Encryption' availability: */
    AssertPtrReturnVoid(m_pCheckBoxEncryption);
    AssertPtrReturnVoid(m_pWidgetEncryptionSettings);
    m_pCheckBoxEncryption->setEnabled(isMachineOffline());
    m_pWidgetEncryptionSettings->setEnabled(isMachineOffline() && m_pCheckBoxEncryption->isChecked());
}

void UIMachineSettingsGeneral::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineGeneral;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsGeneral::prepareWidgets()
{
    /* Prepare main layout: */
    QHBoxLayout *pLayoutMain = new QHBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare tab-widget: */
        m_pTabWidget = new QITabWidget(this);
        if (m_pTabWidget)
        {
            /* Prepare each tab separately: */
            prepareTabBasic();
            prepareTabAdvanced();
            prepareTabDescription();
            prepareTabEncryption();

            pLayoutMain->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsGeneral::prepareTabBasic()
{
    /* Prepare 'Basic' tab: */
    m_pTabBasic = new QWidget;
    if (m_pTabBasic)
    {
        /* Prepare 'Basic' tab layout: */
        QVBoxLayout *pLayoutBasic = new QVBoxLayout(m_pTabBasic);
        if (pLayoutBasic)
        {
            /* Prepare name and system editor: */
            m_pEditorNameAndSystem = new UINameAndSystemEditor(m_pTabBasic);
            if (m_pEditorNameAndSystem)
            {
                m_pEditorNameAndSystem->setNameFieldValidator(".+");
                pLayoutBasic->addWidget(m_pEditorNameAndSystem);
            }

            /* Add vertical strech: */
            pLayoutBasic->addStretch();
        }

        m_pTabWidget->addTab(m_pTabBasic, QString());
    }
}

void UIMachineSettingsGeneral::prepareTabAdvanced()
{
    /* Prepare 'Advanced' tab: */
    m_pTabAdvanced = new QWidget;
    if (m_pTabAdvanced)
    {
        /* Prepare 'Advanced' tab layout: */
        QGridLayout *pLayoutAdvanced = new QGridLayout(m_pTabAdvanced);
        if (pLayoutAdvanced)
        {
            pLayoutAdvanced->setColumnStretch(2, 1);
            pLayoutAdvanced->setRowStretch(3, 1);

            /* Prepare snapshot folder label: */
            m_pLabelSnapshotFolder = new QLabel(m_pTabAdvanced);
            if (m_pLabelSnapshotFolder)
            {
                m_pLabelSnapshotFolder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutAdvanced->addWidget(m_pLabelSnapshotFolder, 0, 0);
            }
            /* Prepare snapshot folder editor: */
            m_pEditorSnapshotFolder = new UIFilePathSelector(m_pTabAdvanced);
            if (m_pEditorSnapshotFolder)
            {
                if (m_pLabelSnapshotFolder)
                    m_pLabelSnapshotFolder->setBuddy(m_pEditorSnapshotFolder);
                pLayoutAdvanced->addWidget(m_pEditorSnapshotFolder, 0, 1, 1, 2);
            }

            /* Prepare clipboard label: */
            m_pLabelClipboard = new QLabel(m_pTabAdvanced);
            if (m_pLabelClipboard)
            {
                m_pLabelClipboard->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutAdvanced->addWidget(m_pLabelClipboard, 1, 0);
            }
            /* Prepare clipboard combo: */
            m_pComboClipboard = new QComboBox(m_pTabAdvanced);
            if (m_pComboClipboard)
            {
                if (m_pLabelClipboard)
                    m_pLabelClipboard->setBuddy(m_pComboClipboard);
                pLayoutAdvanced->addWidget(m_pComboClipboard, 1, 1);
            }

            /* Prepare drag&drop label: */
            m_pLabelDragAndDrop = new QLabel(m_pTabAdvanced);
            if (m_pLabelDragAndDrop)
            {
                m_pLabelDragAndDrop->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutAdvanced->addWidget(m_pLabelDragAndDrop, 2, 0);
            }
            /* Prepare drag&drop combo: */
            m_pComboDragAndDrop = new QComboBox(m_pTabAdvanced);
            if (m_pComboDragAndDrop)
            {
                if (m_pLabelDragAndDrop)
                    m_pLabelDragAndDrop->setBuddy(m_pComboDragAndDrop);
                pLayoutAdvanced->addWidget(m_pComboDragAndDrop, 2, 1);
            }
        }

        m_pTabWidget->addTab(m_pTabAdvanced, QString());
    }
}

void UIMachineSettingsGeneral::prepareTabDescription()
{
    /* Prepare 'Description' tab: */
    m_pTabDescription = new QWidget;
    if (m_pTabDescription)
    {
        /* Prepare 'Description' tab layout: */
        QVBoxLayout *pLayoutDescription = new QVBoxLayout(m_pTabDescription);
        if (pLayoutDescription)
        {
            /* Prepare description editor: */
            m_pEditorDescription = new QTextEdit(m_pTabDescription);
            if (m_pEditorDescription)
            {
                m_pEditorDescription->setObjectName(QStringLiteral("m_pEditorDescription"));
                m_pEditorDescription->setAcceptRichText(false);
#ifdef VBOX_WS_MAC
                m_pEditorDescription->setMinimumHeight(150);
#endif

                pLayoutDescription->addWidget(m_pEditorDescription);
            }
        }

        m_pTabWidget->addTab(m_pTabDescription, QString());
    }
}

void UIMachineSettingsGeneral::prepareTabEncryption()
{
    /* Prepare 'Encryption' tab: */
    m_pTabEncryption = new QWidget;
    if (m_pTabEncryption)
    {
        /* Prepare 'Encryption' tab layout: */
        QGridLayout *pLayoutEncryption = new QGridLayout(m_pTabEncryption);
        if (pLayoutEncryption)
        {
            pLayoutEncryption->setRowStretch(2, 1);

            /* Prepare encryption check-box: */
            m_pCheckBoxEncryption = new QCheckBox(m_pTabEncryption);
            if (m_pCheckBoxEncryption)
                pLayoutEncryption->addWidget(m_pCheckBoxEncryption, 0, 0, 1, 2);

            /* Prepare 20-px shifting spacer: */
            QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
            if (pSpacerItem)
                pLayoutEncryption->addItem(pSpacerItem, 1, 0);

            /* Prepare encryption settings widget: */
            m_pWidgetEncryptionSettings = new QWidget(m_pTabEncryption);
            if (m_pWidgetEncryptionSettings)
            {
                /* Prepare encryption settings widget layout: */
                QGridLayout *m_pLayoutEncryptionSettings = new QGridLayout(m_pWidgetEncryptionSettings);
                if (m_pLayoutEncryptionSettings)
                {
                    m_pLayoutEncryptionSettings->setContentsMargins(0, 0, 0, 0);

                    /* Prepare encryption cipher label: */
                    m_pLabelCipher = new QLabel(m_pWidgetEncryptionSettings);
                    if (m_pLabelCipher)
                    {
                        m_pLabelCipher->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
                        m_pLayoutEncryptionSettings->addWidget(m_pLabelCipher, 0, 0);
                    }
                    /* Prepare encryption cipher combo: */
                    m_pComboCipher = new QComboBox(m_pWidgetEncryptionSettings);
                    if (m_pComboCipher)
                    {
                        if (m_pLabelCipher)
                            m_pLabelCipher->setBuddy(m_pComboCipher);
                        m_encryptionCiphers << QString()
                                            << "AES-XTS256-PLAIN64"
                                            << "AES-XTS128-PLAIN64";
                        m_pComboCipher->addItems(m_encryptionCiphers);
                        m_pLayoutEncryptionSettings->addWidget(m_pComboCipher, 0, 1);
                    }

                    /* Prepare encryption password label: */
                    m_pLabelEncryptionPassword = new QLabel(m_pWidgetEncryptionSettings);
                    if (m_pLabelEncryptionPassword)
                    {
                        m_pLabelEncryptionPassword->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
                        m_pLayoutEncryptionSettings->addWidget(m_pLabelEncryptionPassword, 1, 0);
                    }
                    /* Prepare encryption password editor: */
                    m_pEditorEncryptionPassword = new QLineEdit(m_pWidgetEncryptionSettings);
                    if (m_pEditorEncryptionPassword)
                    {
                        if (m_pLabelEncryptionPassword)
                            m_pLabelEncryptionPassword->setBuddy(m_pEditorEncryptionPassword);
                        m_pEditorEncryptionPassword->setEchoMode(QLineEdit::Password);
                        m_pLayoutEncryptionSettings->addWidget(m_pEditorEncryptionPassword, 1, 1);
                    }

                    /* Prepare encryption confirm password label: */
                    m_pLabelEncryptionPasswordConfirm = new QLabel(m_pWidgetEncryptionSettings);
                    if (m_pLabelEncryptionPasswordConfirm)
                    {
                        m_pLabelEncryptionPasswordConfirm->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
                        m_pLayoutEncryptionSettings->addWidget(m_pLabelEncryptionPasswordConfirm, 2, 0);
                    }
                    /* Prepare encryption confirm password editor: */
                    m_pEditorEncryptionPasswordConfirm = new QLineEdit(m_pWidgetEncryptionSettings);
                    if (m_pEditorEncryptionPasswordConfirm)
                    {
                        if (m_pLabelEncryptionPasswordConfirm)
                            m_pLabelEncryptionPasswordConfirm->setBuddy(m_pEditorEncryptionPasswordConfirm);
                        m_pEditorEncryptionPasswordConfirm->setEchoMode(QLineEdit::Password);
                        m_pLayoutEncryptionSettings->addWidget(m_pEditorEncryptionPasswordConfirm, 2, 1);
                    }
                }

                pLayoutEncryption->addWidget(m_pWidgetEncryptionSettings, 1, 1);
            }
        }

        m_pTabWidget->addTab(m_pTabEncryption, QString());
    }
}

void UIMachineSettingsGeneral::prepareConnections()
{
    /* Configure 'Basic' connections: */
    connect(m_pEditorNameAndSystem, &UINameAndSystemEditor::sigOsTypeChanged,
            this, &UIMachineSettingsGeneral::revalidate);
    connect(m_pEditorNameAndSystem, &UINameAndSystemEditor::sigNameChanged,
            this, &UIMachineSettingsGeneral::revalidate);

    /* Configure 'Encryption' connections: */
    connect(m_pCheckBoxEncryption, &QCheckBox::toggled,
            m_pWidgetEncryptionSettings, &QWidget::setEnabled);
    connect(m_pCheckBoxEncryption, &QCheckBox::toggled,
            this, &UIMachineSettingsGeneral::revalidate);
    connect(m_pComboCipher, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsGeneral::sltMarkEncryptionCipherChanged);
    connect(m_pComboCipher, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsGeneral::revalidate);
    connect(m_pEditorEncryptionPassword, &QLineEdit::textEdited,
            this, &UIMachineSettingsGeneral::sltMarkEncryptionPasswordChanged);
    connect(m_pEditorEncryptionPassword, &QLineEdit::textEdited,
            this, &UIMachineSettingsGeneral::revalidate);
    connect(m_pEditorEncryptionPasswordConfirm, &QLineEdit::textEdited,
            this, &UIMachineSettingsGeneral::sltMarkEncryptionPasswordChanged);
    connect(m_pEditorEncryptionPasswordConfirm, &QLineEdit::textEdited,
            this, &UIMachineSettingsGeneral::revalidate);
}

void UIMachineSettingsGeneral::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsGeneral::repopulateComboClipboardMode()
{
    AssertPtrReturnVoid(m_pComboClipboard);
    {
        /* Clear combo first of all: */
        m_pComboClipboard->clear();

        /* Load currently supported Clipboard modes: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KClipboardMode> clipboardModes = comProperties.GetSupportedClipboardModes();
        /* Take into account currently cached value: */
        const KClipboardMode enmCachedValue = m_pCache->base().m_clipboardMode;
        if (!clipboardModes.contains(enmCachedValue))
            clipboardModes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KClipboardMode &enmMode, clipboardModes)
            m_pComboClipboard->addItem(gpConverter->toString(enmMode), QVariant::fromValue(enmMode));
    }
}

void UIMachineSettingsGeneral::repopulateComboDnDMode()
{
    AssertPtrReturnVoid(m_pComboDragAndDrop);
    {
        /* Clear combo first of all: */
        m_pComboDragAndDrop->clear();

        /* Load currently supported DnD modes: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KDnDMode> dndModes = comProperties.GetSupportedDnDModes();
        /* Take into account currently cached value: */
        const KDnDMode enmCachedValue = m_pCache->base().m_dndMode;
        if (!dndModes.contains(enmCachedValue))
            dndModes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KDnDMode &enmMode, dndModes)
            m_pComboDragAndDrop->addItem(gpConverter->toString(enmMode), QVariant::fromValue(enmMode));
    }
}

bool UIMachineSettingsGeneral::saveGeneralData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save general settings from the cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Save 'Basic' data from the cache: */
        if (fSuccess)
            fSuccess = saveBasicData();
        /* Save 'Advanced' data from the cache: */
        if (fSuccess)
            fSuccess = saveAdvancedData();
        /* Save 'Description' data from the cache: */
        if (fSuccess)
            fSuccess = saveDescriptionData();
        /* Save 'Encryption' data from the cache: */
        if (fSuccess)
            fSuccess = saveEncryptionData();
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsGeneral::saveBasicData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Basic' data from the cache: */
    if (fSuccess)
    {
        /* Get old general data from the cache: */
        const UIDataSettingsMachineGeneral &oldGeneralData = m_pCache->base();
        /* Get new general data from the cache: */
        const UIDataSettingsMachineGeneral &newGeneralData = m_pCache->data();

        /* Save machine OS type ID: */
        if (isMachineOffline() && newGeneralData.m_strGuestOsTypeId != oldGeneralData.m_strGuestOsTypeId)
        {
            if (fSuccess)
            {
                m_machine.SetOSTypeId(newGeneralData.m_strGuestOsTypeId);
                fSuccess = m_machine.isOk();
            }
            if (fSuccess)
            {
                // Must update long mode CPU feature bit when os type changed:
                CVirtualBox vbox = uiCommon().virtualBox();
                // Should we check global object getters?
                const CGuestOSType &comNewType = vbox.GetGuestOSType(newGeneralData.m_strGuestOsTypeId);
                m_machine.SetCPUProperty(KCPUPropertyType_LongMode, comNewType.GetIs64Bit());
                fSuccess = m_machine.isOk();
            }
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsGeneral::saveAdvancedData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Advanced' data from the cache: */
    if (fSuccess)
    {
        /* Get old general data from the cache: */
        const UIDataSettingsMachineGeneral &oldGeneralData = m_pCache->base();
        /* Get new general data from the cache: */
        const UIDataSettingsMachineGeneral &newGeneralData = m_pCache->data();

        /* Save machine clipboard mode: */
        if (fSuccess && newGeneralData.m_clipboardMode != oldGeneralData.m_clipboardMode)
        {
            m_machine.SetClipboardMode(newGeneralData.m_clipboardMode);
            fSuccess = m_machine.isOk();
        }
        /* Save machine D&D mode: */
        if (fSuccess && newGeneralData.m_dndMode != oldGeneralData.m_dndMode)
        {
            m_machine.SetDnDMode(newGeneralData.m_dndMode);
            fSuccess = m_machine.isOk();
        }
        /* Save machine snapshot folder: */
        if (fSuccess && isMachineOffline() && newGeneralData.m_strSnapshotsFolder != oldGeneralData.m_strSnapshotsFolder)
        {
            m_machine.SetSnapshotFolder(newGeneralData.m_strSnapshotsFolder);
            fSuccess = m_machine.isOk();
        }
        // VM name from 'Basic' data should go after the snapshot folder from the 'Advanced' data
        // as otherwise VM rename magic can collide with the snapshot folder one.
        /* Save machine name: */
        if (fSuccess && (isMachineOffline() || isMachineSaved()) && newGeneralData.m_strName != oldGeneralData.m_strName)
        {
            m_machine.SetName(newGeneralData.m_strName);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsGeneral::saveDescriptionData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Description' data from the cache: */
    if (fSuccess)
    {
        /* Get old general data from the cache: */
        const UIDataSettingsMachineGeneral &oldGeneralData = m_pCache->base();
        /* Get new general data from the cache: */
        const UIDataSettingsMachineGeneral &newGeneralData = m_pCache->data();

        /* Save machine description: */
        if (fSuccess && newGeneralData.m_strDescription != oldGeneralData.m_strDescription)
        {
            m_machine.SetDescription(newGeneralData.m_strDescription);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsGeneral::saveEncryptionData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Encryption' data from the cache: */
    if (fSuccess)
    {
        /* Get old general data from the cache: */
        const UIDataSettingsMachineGeneral &oldGeneralData = m_pCache->base();
        /* Get new general data from the cache: */
        const UIDataSettingsMachineGeneral &newGeneralData = m_pCache->data();

        /* Make sure it either encryption state is changed itself,
         * or the encryption was already enabled and either cipher or password is changed. */
        if (   isMachineOffline()
            && (   newGeneralData.m_fEncryptionEnabled != oldGeneralData.m_fEncryptionEnabled
                || (   oldGeneralData.m_fEncryptionEnabled
                    && (   newGeneralData.m_fEncryptionCipherChanged != oldGeneralData.m_fEncryptionCipherChanged
                        || newGeneralData.m_fEncryptionPasswordChanged != oldGeneralData.m_fEncryptionPasswordChanged))))
        {
            /* Get machine name for further activities: */
            QString strMachineName;
            if (fSuccess)
            {
                strMachineName = m_machine.GetName();
                fSuccess = m_machine.isOk();
            }
            /* Get machine attachments for further activities: */
            CMediumAttachmentVector attachments;
            if (fSuccess)
            {
                attachments = m_machine.GetMediumAttachments();
                fSuccess = m_machine.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));

            /* For each attachment: */
            for (int iIndex = 0; fSuccess && iIndex < attachments.size(); ++iIndex)
            {
                /* Get current attachment: */
                const CMediumAttachment &comAttachment = attachments.at(iIndex);

                /* Get attachment type for further activities: */
                KDeviceType enmType = KDeviceType_Null;
                if (fSuccess)
                {
                    enmType = comAttachment.GetType();
                    fSuccess = comAttachment.isOk();
                }
                /* Get attachment medium for further activities: */
                CMedium comMedium;
                if (fSuccess)
                {
                    comMedium = comAttachment.GetMedium();
                    fSuccess = comAttachment.isOk();
                }

                /* Show error message if necessary: */
                if (!fSuccess)
                    notifyOperationProgressError(UIErrorString::formatErrorInfo(comAttachment));
                else
                {
                    /* Pass hard-drives only: */
                    if (enmType != KDeviceType_HardDisk)
                        continue;

                    /* Get medium id for further activities: */
                    QUuid uMediumId;
                    if (fSuccess)
                    {
                        uMediumId = comMedium.GetId();
                        fSuccess = comMedium.isOk();
                    }

                    /* Create encryption update progress: */
                    CProgress comProgress;
                    if (fSuccess)
                    {
                        /* Cipher attribute changed? */
                        QString strNewCipher;
                        if (newGeneralData.m_fEncryptionCipherChanged)
                        {
                            strNewCipher = newGeneralData.m_fEncryptionEnabled ?
                                           m_encryptionCiphers.at(newGeneralData.m_iEncryptionCipherIndex) : QString();
                        }

                        /* Password attribute changed? */
                        QString strNewPassword;
                        QString strNewPasswordId;
                        if (newGeneralData.m_fEncryptionPasswordChanged)
                        {
                            strNewPassword = newGeneralData.m_fEncryptionEnabled ?
                                             newGeneralData.m_strEncryptionPassword : QString();
                            strNewPasswordId = newGeneralData.m_fEncryptionEnabled ?
                                               strMachineName : QString();
                        }

                        /* Get the maps of encrypted media and their passwords: */
                        const EncryptedMediumMap &encryptedMedium = newGeneralData.m_encryptedMedia;
                        const EncryptionPasswordMap &encryptionPasswords = newGeneralData.m_encryptionPasswords;

                        /* Check if old password exists/provided: */
                        const QString strOldPasswordId = encryptedMedium.key(uMediumId);
                        const QString strOldPassword = encryptionPasswords.value(strOldPasswordId);

                        /* Create encryption progress: */
                        comProgress = comMedium.ChangeEncryption(strOldPassword,
                                                                 strNewCipher,
                                                                 strNewPassword,
                                                                 strNewPasswordId);
                        fSuccess = comMedium.isOk();
                    }

                    /* Create encryption update progress object: */
                    QPointer<UIProgressObject> pObject;
                    if (fSuccess)
                    {
                        pObject = new UIProgressObject(comProgress);
                        if (pObject)
                        {
                            connect(pObject.data(), &UIProgressObject::sigProgressChange,
                                    this, &UIMachineSettingsGeneral::sigOperationProgressChange,
                                    Qt::QueuedConnection);
                            connect(pObject.data(), &UIProgressObject::sigProgressError,
                                    this, &UIMachineSettingsGeneral::sigOperationProgressError,
                                    Qt::BlockingQueuedConnection);
                            pObject->exec();
                            if (pObject)
                                delete pObject;
                            else
                            {
                                // Premature application shutdown,
                                // exit immediately:
                                return true;
                            }
                        }
                    }

                    /* Show error message if necessary: */
                    if (!fSuccess)
                        notifyOperationProgressError(UIErrorString::formatErrorInfo(comMedium));
                }
            }
        }
    }
    /* Return result: */
    return fSuccess;
}
