/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsGeneral class implementation.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QLineEdit>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UIMachineSettingsGeneral.h"
# include "UIMessageCenter.h"
# include "UIConverter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineSettingsGeneral::UIMachineSettingsGeneral()
    : m_fHWVirtExEnabled(false)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsGeneral::setupUi (this);

    /* Setup validators */
    m_pNameAndSystemEditor->nameEditor()->setValidator(new QRegExpValidator(QRegExp(".+"), this));

    /* Shared Clipboard mode */
    mCbClipboard->addItem (""); /* KClipboardMode_Disabled */
    mCbClipboard->addItem (""); /* KClipboardMode_HostToGuest */
    mCbClipboard->addItem (""); /* KClipboardMode_GuestToHost */
    mCbClipboard->addItem (""); /* KClipboardMode_Bidirectional */

    /* Drag'n'drop mode */
    mCbDragAndDrop->addItem (""); /* KDnDMode_Disabled */
    mCbDragAndDrop->addItem (""); /* KDnDMode_HostToGuest */
    mCbDragAndDrop->addItem (""); /* KDnDMode_GuestToHost */
    mCbDragAndDrop->addItem (""); /* KDnDMode_Bidirectional */

#ifdef Q_WS_MAC
    mTeDescription->setMinimumHeight (150);
#endif /* Q_WS_MAC */

    /* Prepare validation: */
    prepareValidation();

    /* Applying language settings */
    retranslateUi();
}

CGuestOSType UIMachineSettingsGeneral::guestOSType() const
{
    return m_pNameAndSystemEditor->type();
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

bool UIMachineSettingsGeneral::is64BitOSTypeSelected() const
{
    return m_pNameAndSystemEditor->type().GetIs64Bit();
}

#ifdef VBOX_WITH_VIDEOHWACCEL
bool UIMachineSettingsGeneral::isWindowsOSTypeSelected() const
{
    return m_pNameAndSystemEditor->type().GetFamilyId() == "Windows";
}
#endif /* VBOX_WITH_VIDEOHWACCEL */

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare general data: */
    UIDataSettingsMachineGeneral generalData;

    /* Gather general data: */
    generalData.m_strName = m_machine.GetName();
    generalData.m_strGuestOsTypeId = m_machine.GetOSTypeId();
    generalData.m_strSnapshotsFolder = m_machine.GetSnapshotFolder();
    generalData.m_strSnapshotsHomeDir = QFileInfo(m_machine.GetSettingsFilePath()).absolutePath();
    generalData.m_clipboardMode = m_machine.GetClipboardMode();
    generalData.m_dndMode = m_machine.GetDnDMode();
    generalData.m_strDescription = m_machine.GetDescription();

    /* Cache general data: */
    m_cache.cacheInitialData(generalData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsGeneral::getFromCache()
{
    /* Get general data from cache: */
    const UIDataSettingsMachineGeneral &generalData = m_cache.base();

    /* Load general data to page: */
    m_pNameAndSystemEditor->setName(generalData.m_strName);
    m_pNameAndSystemEditor->setType(vboxGlobal().vmGuestOSType(generalData.m_strGuestOsTypeId));
    mPsSnapshot->setPath(generalData.m_strSnapshotsFolder);
    mPsSnapshot->setHomeDir(generalData.m_strSnapshotsHomeDir);
    mCbClipboard->setCurrentIndex(generalData.m_clipboardMode);
    mCbDragAndDrop->setCurrentIndex(generalData.m_dndMode);
    mTeDescription->setPlainText(generalData.m_strDescription);

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsGeneral::putToCache()
{
    /* Prepare general data: */
    UIDataSettingsMachineGeneral generalData = m_cache.base();

    /* Gather general data: */
    generalData.m_strName = m_pNameAndSystemEditor->name();
    generalData.m_strGuestOsTypeId = m_pNameAndSystemEditor->type().GetId();
    generalData.m_strSnapshotsFolder = mPsSnapshot->path();
    generalData.m_clipboardMode = (KClipboardMode)mCbClipboard->currentIndex();
    generalData.m_dndMode = (KDnDMode)mCbDragAndDrop->currentIndex();
    generalData.m_strDescription = mTeDescription->toPlainText().isEmpty() ?
                                   QString::null : mTeDescription->toPlainText();

    /* Cache general data: */
    m_cache.cacheCurrentData(generalData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if general data was changed: */
    if (m_cache.wasChanged())
    {
        /* Get general data from cache: */
        const UIDataSettingsMachineGeneral &generalData = m_cache.data();

        /* Store general data: */
        if (isMachineInValidMode())
        {
            /* Advanced tab: */
            m_machine.SetClipboardMode(generalData.m_clipboardMode);
            m_machine.SetDnDMode(generalData.m_dndMode);
            /* Description tab: */
            m_machine.SetDescription(generalData.m_strDescription);
        }
        if (isMachineOffline())
        {
            /* Basic tab: Must update long mode CPU feature bit when os type changes. */
            if (generalData.m_strGuestOsTypeId != m_cache.base().m_strGuestOsTypeId)
            {
                m_machine.SetOSTypeId(generalData.m_strGuestOsTypeId);

                CVirtualBox vbox = vboxGlobal().virtualBox();
                CGuestOSType newType = vbox.GetGuestOSType(generalData.m_strGuestOsTypeId);
                m_machine.SetCPUProperty(KCPUPropertyType_LongMode, newType.GetIs64Bit());
            }

            /* Advanced tab: */
            m_machine.SetSnapshotFolder(generalData.m_strSnapshotsFolder);
            /* Basic (again) tab: */
            /* VM name must be last as otherwise its VM rename magic can collide with other settings in the config,
             * especially with the snapshot folder: */
            m_machine.SetName(generalData.m_strName);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsGeneral::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;
    message.first = VBoxGlobal::removeAccelMark(mTwGeneral->tabText(0));

    /* VM name validation: */
    if (m_pNameAndSystemEditor->name().trimmed().isEmpty())
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

    /* Return result: */
    return fPass;
}

void UIMachineSettingsGeneral::setOrderAfter (QWidget *aWidget)
{
    /* Basic tab-order */
    setTabOrder (aWidget, mTwGeneral->focusProxy());
    setTabOrder (mTwGeneral->focusProxy(), m_pNameAndSystemEditor);

    /* Advanced tab-order */
    setTabOrder (m_pNameAndSystemEditor, mPsSnapshot);
    setTabOrder (mPsSnapshot, mCbClipboard);
    setTabOrder (mCbClipboard, mCbDragAndDrop);

    /* Description tab-order */
    setTabOrder (mCbDragAndDrop, mTeDescription);
}

void UIMachineSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsGeneral::retranslateUi (this);

    /* Path selector */
    mPsSnapshot->setWhatsThis (tr ("Holds the path where snapshots of this "
                                   "virtual machine will be stored. Be aware that "
                                   "snapshots can take quite a lot of disk "
                                   "space."));

    /* Shared Clipboard mode */
    mCbClipboard->setItemText (0, gpConverter->toString (KClipboardMode_Disabled));
    mCbClipboard->setItemText (1, gpConverter->toString (KClipboardMode_HostToGuest));
    mCbClipboard->setItemText (2, gpConverter->toString (KClipboardMode_GuestToHost));
    mCbClipboard->setItemText (3, gpConverter->toString (KClipboardMode_Bidirectional));

    /* Drag'n'drop mode */
    mCbDragAndDrop->setItemText (0, gpConverter->toString (KDnDMode_Disabled));
    mCbDragAndDrop->setItemText (1, gpConverter->toString (KDnDMode_HostToGuest));
    mCbDragAndDrop->setItemText (2, gpConverter->toString (KDnDMode_GuestToHost));
    mCbDragAndDrop->setItemText (3, gpConverter->toString (KDnDMode_Bidirectional));
}

void UIMachineSettingsGeneral::prepareValidation()
{
    /* Prepare validation: */
    connect(m_pNameAndSystemEditor, SIGNAL(sigOsTypeChanged()), this, SLOT(revalidate()));
    connect(m_pNameAndSystemEditor, SIGNAL(sigNameChanged(const QString&)), this, SLOT(revalidate()));
}

void UIMachineSettingsGeneral::polishPage()
{
    /* Basic tab: */
    m_pNameAndSystemEditor->setEnabled(isMachineOffline());
    /* Advanced tab: */
    mLbSnapshot->setEnabled(isMachineOffline());
    mPsSnapshot->setEnabled(isMachineOffline());
    mLbClipboard->setEnabled(isMachineInValidMode());
    mCbClipboard->setEnabled(isMachineInValidMode());
    mLbDragAndDrop->setEnabled(isMachineInValidMode());
    mCbDragAndDrop->setEnabled(isMachineInValidMode());
}

