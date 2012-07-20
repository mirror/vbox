/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsGeneral class implementation
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

/* Qt includes: */
#include <QDir>
#include <QLineEdit>

/* GUI includes: */
#include "UIMachineSettingsGeneral.h"
#include "UIMessageCenter.h"
#include "QIWidgetValidator.h"
#include "UIConverter.h"

UIMachineSettingsGeneral::UIMachineSettingsGeneral()
    : mValidator(0)
    , m_fHWVirtExEnabled(false)
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
    mCbDragAndDrop->addItem (""); /* KDragAndDropMode_Disabled */
    mCbDragAndDrop->addItem (""); /* KDragAndDropMode_HostToGuest */
    mCbDragAndDrop->addItem (""); /* KDragAndDropMode_GuestToHost */
    mCbDragAndDrop->addItem (""); /* KDragAndDropMode_Bidirectional */

#ifdef Q_WS_MAC
    mTeDescription->setMinimumHeight (150);
#endif /* Q_WS_MAC */

    /* Applying language settings */
    retranslateUi();
}

CGuestOSType UIMachineSettingsGeneral::guestOSType() const
{
    return m_pNameAndSystemEditor->type();
}

void UIMachineSettingsGeneral::setHWVirtExEnabled(bool fEnabled)
{
    m_fHWVirtExEnabled = fEnabled;
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

/* Load data to cashe from corresponding external object(s),
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
    QString strSaveMountedAtRuntime = m_machine.GetExtraData(GUI_SaveMountedAtRuntime);
    generalData.m_fSaveMountedAtRuntime = strSaveMountedAtRuntime != "no";
    QString strShowMiniToolBar = m_machine.GetExtraData(GUI_ShowMiniToolBar);
    generalData.m_fShowMiniToolBar = strShowMiniToolBar != "no";
    QString strMiniToolBarAlignment = m_machine.GetExtraData(GUI_MiniToolBarAlignment);
    generalData.m_fMiniToolBarAtTop = strMiniToolBarAlignment == "top";
    generalData.m_strSnapshotsFolder = m_machine.GetSnapshotFolder();
    generalData.m_strSnapshotsHomeDir = QFileInfo(m_machine.GetSettingsFilePath()).absolutePath();
    generalData.m_clipboardMode = m_machine.GetClipboardMode();
    generalData.m_dragAndDropMode = m_machine.GetDragAndDropMode();
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
    mCbSaveMounted->setChecked(generalData.m_fSaveMountedAtRuntime);
    mCbShowToolBar->setChecked(generalData.m_fShowMiniToolBar);
    mCbToolBarAlignment->setChecked(generalData.m_fMiniToolBarAtTop);
    mPsSnapshot->setPath(generalData.m_strSnapshotsFolder);
    mPsSnapshot->setHomeDir(generalData.m_strSnapshotsHomeDir);
    mCbClipboard->setCurrentIndex(generalData.m_clipboardMode);
    mCbDragAndDrop->setCurrentIndex(generalData.m_dragAndDropMode);
    mTeDescription->setPlainText(generalData.m_strDescription);

    /* Polish page finally: */
    polishPage();

    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
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
    generalData.m_fSaveMountedAtRuntime = mCbSaveMounted->isChecked();
    generalData.m_fShowMiniToolBar = mCbShowToolBar->isChecked();
    generalData.m_fMiniToolBarAtTop = mCbToolBarAlignment->isChecked();
    generalData.m_strSnapshotsFolder = mPsSnapshot->path();
    generalData.m_clipboardMode = (KClipboardMode)mCbClipboard->currentIndex();
    generalData.m_dragAndDropMode = (KDragAndDropMode)mCbDragAndDrop->currentIndex();
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
            m_machine.SetDragAndDropMode(generalData.m_dragAndDropMode);
            m_machine.SetExtraData(GUI_SaveMountedAtRuntime, generalData.m_fSaveMountedAtRuntime ? "yes" : "no");
            m_machine.SetExtraData(GUI_ShowMiniToolBar, generalData.m_fShowMiniToolBar ? "yes" : "no");
            m_machine.SetExtraData(GUI_MiniToolBarAlignment, generalData.m_fMiniToolBarAtTop ? "top" : "bottom");
            /* Description tab: */
            m_machine.SetDescription(generalData.m_strDescription);
        }
        if (isMachineOffline())
        {
            /* Basic tab: */
            m_machine.SetOSTypeId(generalData.m_strGuestOsTypeId);
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

void UIMachineSettingsGeneral::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (m_pNameAndSystemEditor, SIGNAL (sigOsTypeChanged()), mValidator, SLOT (revalidate()));
}

bool UIMachineSettingsGeneral::revalidate(QString &strWarning, QString& /* strTitle */)
{
    if (is64BitOSTypeSelected() && !m_fHWVirtExEnabled)
        strWarning = tr("you have selected a 64-bit guest OS type for this VM. As such guests "
                        "require hardware virtualization (VT-x/AMD-V), this feature will be enabled "
                        "automatically.");
    return true;
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
    setTabOrder (mCbDragAndDrop, mCbSaveMounted);
    setTabOrder (mCbSaveMounted, mCbShowToolBar);
    setTabOrder (mCbShowToolBar, mCbToolBarAlignment);

    /* Description tab-order */
    setTabOrder (mCbToolBarAlignment, mTeDescription);
}

void UIMachineSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsGeneral::retranslateUi (this);

    /* Path selector */
    mPsSnapshot->setWhatsThis (tr ("Displays the path where snapshots of this "
                                   "virtual machine will be stored. Be aware that "
                                   "snapshots can take quite a lot of disk "
                                   "space."));

    /* Shared Clipboard mode */
    mCbClipboard->setItemText (0, gpConverter->toString (KClipboardMode_Disabled));
    mCbClipboard->setItemText (1, gpConverter->toString (KClipboardMode_HostToGuest));
    mCbClipboard->setItemText (2, gpConverter->toString (KClipboardMode_GuestToHost));
    mCbClipboard->setItemText (3, gpConverter->toString (KClipboardMode_Bidirectional));

    /* Drag'n'drop mode */
    mCbDragAndDrop->setItemText (0, gpConverter->toString (KDragAndDropMode_Disabled));
    mCbDragAndDrop->setItemText (1, gpConverter->toString (KDragAndDropMode_HostToGuest));
    mCbDragAndDrop->setItemText (2, gpConverter->toString (KDragAndDropMode_GuestToHost));
    mCbDragAndDrop->setItemText (3, gpConverter->toString (KDragAndDropMode_Bidirectional));
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
    mLbMedia->setEnabled(isMachineInValidMode());
    mCbSaveMounted->setEnabled(isMachineInValidMode());
    mLbToolBar->setEnabled(isMachineInValidMode());
    mCbShowToolBar->setEnabled(isMachineInValidMode());
    mCbToolBarAlignment->setEnabled(isMachineInValidMode() && mCbShowToolBar->isChecked());
}

