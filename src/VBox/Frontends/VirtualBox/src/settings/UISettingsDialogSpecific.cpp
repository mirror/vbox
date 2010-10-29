/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDialogSpecific class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QStackedWidget>

/* Local includes */
#include "UISettingsDialogSpecific.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"

#include "VBoxGLSettingsGeneral.h"
#include "VBoxGLSettingsInput.h"
#include "VBoxGLSettingsUpdate.h"
#include "VBoxGLSettingsLanguage.h"
#include "VBoxGLSettingsNetwork.h"

#include "VBoxVMSettingsGeneral.h"
#include "VBoxVMSettingsSystem.h"
#include "VBoxVMSettingsDisplay.h"
#include "VBoxVMSettingsHD.h"
#include "VBoxVMSettingsAudio.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsSerial.h"
#include "VBoxVMSettingsParallel.h"
#include "VBoxVMSettingsUSB.h"
#include "VBoxVMSettingsSF.h"

#if 0 /* Global USB filters are DISABLED now: */
# define ENABLE_GLOBAL_USB
#endif /* Global USB filters are DISABLED now: */

UIGLSettingsDlg::UIGLSettingsDlg(QWidget *pParent)
    : UISettingsDialog(pParent)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/global_settings_16px.png"));
#endif /* !Q_WS_MAC */

    /* Creating settings pages: */
    for (int i = GLSettingsPage_General; i < GLSettingsPage_MAX; ++i)
    {
        if (isAvailable(i))
        {
            switch (i)
            {
                /* General page: */
                case GLSettingsPage_General:
                {
                    UISettingsPage *pSettingsPage = new VBoxGLSettingsGeneral;
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            i, "#general", pSettingsPage);
                    break;
                }
                /* Input page: */
                case GLSettingsPage_Input:
                {
                    UISettingsPage *pSettingsPage = new VBoxGLSettingsInput;
                    addItem(":/hostkey_32px.png", ":/hostkey_disabled_32px.png",
                            ":/hostkey_16px.png", ":/hostkey_disabled_16px.png",
                            i, "#input", pSettingsPage);
                    break;
                }
                /* Update page: */
                case GLSettingsPage_Update:
                {
                    UISettingsPage *pSettingsPage = new VBoxGLSettingsUpdate;
                    addItem(":/refresh_32px.png", ":/refresh_disabled_32px.png",
                            ":/refresh_16px.png", ":/refresh_disabled_16px.png",
                            i, "#update", pSettingsPage);
                    break;
                }
                /* Language page: */
                case GLSettingsPage_Language:
                {
                    UISettingsPage *pSettingsPage = new VBoxGLSettingsLanguage;
                    addItem(":/site_32px.png", ":/site_disabled_32px.png",
                            ":/site_16px.png", ":/site_disabled_16px.png",
                            i, "#language", pSettingsPage);
                    break;
                }
                /* USB page: */
                case GLSettingsPage_USB:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsUSB(VBoxVMSettingsUSB::HostType);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            i, "#usb", pSettingsPage);
                    break;
                }
                /* Network page: */
                case GLSettingsPage_Network:
                {
                    UISettingsPage *pSettingsPage = new VBoxGLSettingsNetwork;
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            i, "#language", pSettingsPage);
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Choose first item by default: */
    m_pSelector->selectById(0);
}

void UIGLSettingsDlg::getFrom()
{
    /* Get properties and settings: */
    CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings settings = vboxGlobal().settings();
    /* Iterate over the settings pages: */
    QList<UISettingsPage*> pages = m_pSelector->settingPages();
    for (int i = 0; i < pages.size(); ++i)
    {
        /* For every page => load the settings: */
        UISettingsPage *pPage = pages[i];
        if (pPage->isEnabled())
            pPage->getFrom(properties, settings);
    }
}

void UIGLSettingsDlg::putBackTo()
{
    /* Get properties and settings: */
    CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings oldSettings = vboxGlobal().settings();
    VBoxGlobalSettings newSettings = oldSettings;
    /* Iterate over the settings pages: */
    QList<UISettingsPage*> pages = m_pSelector->settingPages();
    for (int i = 0; i < pages.size(); ++i)
    {
        /* For every page => save the settings: */
        UISettingsPage *pPage = pages[i];
        if (pPage->isEnabled())
            pPage->putBackTo(properties, newSettings);
    }
    /* If properties are not OK => show the error: */
    if (!properties.isOk())
        vboxProblem().cannotSetSystemProperties(properties);
    /* Else save the new settings if they were changed: */
    else if (!(newSettings == oldSettings))
        vboxGlobal().setSettings(newSettings);
}

void UIGLSettingsDlg::retranslateUi()
{
    /* Set dialog's name: */
    setWindowTitle(title());

    /* General page: */
    m_pSelector->setItemText(GLSettingsPage_General, tr("General"));

    /* Input page: */
    m_pSelector->setItemText(GLSettingsPage_Input, tr("Input"));

    /* Update page: */
    m_pSelector->setItemText(GLSettingsPage_Update, tr("Update"));

    /* Language page: */
    m_pSelector->setItemText(GLSettingsPage_Language, tr("Language"));

    /* USB page: */
    m_pSelector->setItemText(GLSettingsPage_USB, tr("USB"));

    /* Network page: */
    m_pSelector->setItemText(GLSettingsPage_Network, tr("Network"));

    /* Translate the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();
}

QString UIGLSettingsDlg::title() const
{
    return tr("VirtualBox - %1").arg(titleExtension());
}

bool UIGLSettingsDlg::isAvailable(int id)
{
    /* Show the host error message for particular group if present.
     * We don't use the generic cannotLoadGlobalConfig()
     * call here because we want this message to be suppressible: */
    switch (id)
    {
        case GLSettingsPage_USB:
        {
#ifdef ENABLE_GLOBAL_USB
            /* Get the host object: */
            CHost host = vboxGlobal().virtualBox().GetHost();
            /* Show the host error message if any: */
            if (!host.isReallyOk())
                vboxProblem().cannotAccessUSB(host);
            /* Check if USB is implemented: */
            CHostUSBDeviceFilterVector filters = host.GetUSBDeviceFilters();
            Q_UNUSED(filters);
            if (host.lastRC() == E_NOTIMPL)
                return false;
#else
            return false;
#endif
            break;
        }
        case GLSettingsPage_Network:
        {
#ifndef VBOX_WITH_NETFLT
            return false;
#endif /* !VBOX_WITH_NETFLT */
            break;
        }
        default:
            break;
    }
    return true;
}

UIVMSettingsDlg::UIVMSettingsDlg(QWidget *pParent,
                                 const CMachine &machine,
                                 const QString &strCategory,
                                 const QString &strControl)
    : UISettingsDialog(pParent)
    , m_machine(machine)
    , m_fAllowResetFirstRunFlag(false)
    , m_fResetFirstRunFlag(false)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/settings_16px.png"));
#endif /* Q_WS_MAC */

    /* Allow to reset first-run flag just when medium enumeration was finished: */
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltAllowResetFirstRunFlag()));

    /* Creating settings pages: */
    for (int i = VMSettingsPage_General; i < VMSettingsPage_MAX; ++i)
    {
        if (isAvailable(i))
        {
            switch (i)
            {
                /* General page: */
                case VMSettingsPage_General:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsGeneral;
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            i, "#general", pSettingsPage);
                    break;
                }
                /* System page: */
                case VMSettingsPage_System:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsSystem;
                    connect(pSettingsPage, SIGNAL(tableChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/chipset_32px.png", ":/chipset_disabled_32px.png",
                            ":/chipset_16px.png", ":/chipset_disabled_16px.png",
                            i, "#system", pSettingsPage);
                    break;
                }
                /* Display page: */
                case VMSettingsPage_Display:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsDisplay;
                    addItem(":/vrdp_32px.png", ":/vrdp_disabled_32px.png",
                            ":/vrdp_16px.png", ":/vrdp_disabled_16px.png",
                            i, "#display", pSettingsPage);
                    break;
                }
                /* Storage page: */
                case VMSettingsPage_Storage:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsHD;
                    connect(pSettingsPage, SIGNAL(storageChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/hd_32px.png", ":/hd_disabled_32px.png",
                            ":/attachment_16px.png", ":/attachment_disabled_16px.png",
                            i, "#storage", pSettingsPage);
                    break;
                }
                /* Audio page: */
                case VMSettingsPage_Audio:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsAudio;
                    addItem(":/sound_32px.png", ":/sound_disabled_32px.png",
                            ":/sound_16px.png", ":/sound_disabled_16px.png",
                            i, "#audio", pSettingsPage);
                    break;
                }
                /* Network page: */
                case VMSettingsPage_Network:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsNetworkPage;
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            i, "#network", pSettingsPage);
                    break;
                }
                /* Ports page: */
                case VMSettingsPage_Ports:
                {
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            i, "#ports");
                    break;
                }
                /* Serial page: */
                case VMSettingsPage_Serial:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsSerialPage;
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            i, "#serialPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Parallel page: */
                case VMSettingsPage_Parallel:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsParallelPage;
                    addItem(":/parallel_port_32px.png", ":/parallel_port_disabled_32px.png",
                            ":/parallel_port_16px.png", ":/parallel_port_disabled_16px.png",
                            i, "#parallelPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* USB page: */
                case VMSettingsPage_USB:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsUSB(VBoxVMSettingsUSB::MachineType);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            i, "#usb", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Shared Folders page: */
                case VMSettingsPage_SF:
                {
                    UISettingsPage *pSettingsPage = new VBoxVMSettingsSF(MachineType);
                    addItem(":/shared_folder_32px.png", ":/shared_folder_disabled_32px.png",
                            ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png",
                            i, "#sfolders", pSettingsPage);
                    break;
                }
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Setup Settings Dialog: */
    if (!strCategory.isNull())
    {
        m_pSelector->selectByLink(strCategory);
        /* Search for a widget with the given name: */
        if (!strControl.isNull())
        {
            if (QWidget *pWidget = m_pStack->currentWidget()->findChild<QWidget*>(strControl))
            {
                QList<QWidget*> parents;
                QWidget *pParentWidget = pWidget;
                while ((pParentWidget = pParentWidget->parentWidget()) != 0)
                {
                    if (QTabWidget *pTabWidget = qobject_cast<QTabWidget*>(pParentWidget))
                    {
                        /* The tab contents widget is two steps down
                         * (QTabWidget -> QStackedWidget -> QWidget): */
                        QWidget *pTabPage = parents[parents.count() - 1];
                        if (pTabPage)
                            pTabPage = parents[parents.count() - 2];
                        if (pTabPage)
                            pTabWidget->setCurrentWidget(pTabPage);
                    }
                    parents.append(pParentWidget);
                }
                pWidget->setFocus();
            }
        }
    }
    /* First item as default: */
    else
        m_pSelector->selectById(0);
}

void UIVMSettingsDlg::getFrom()
{
    /* Iterate over the settings pages: */
    QList<UISettingsPage*> pages = m_pSelector->settingPages();
    for (int i = 0; i < pages.size(); ++i)
    {
        /* For every page => load the settings: */
        UISettingsPage *pPage = pages[i];
        if (pPage->isEnabled())
            pPage->getFrom(m_machine);
    }
    /* Finally set the reset First Run Wizard flag to "false" to make sure
     * user will see this dialog if he hasn't change the boot-order
     * and/or mounted images configuration: */
    m_fResetFirstRunFlag = false;
}

void UIVMSettingsDlg::putBackTo()
{
    /* Iterate over the settings pages: */
    QList<UISettingsPage*> pages = m_pSelector->settingPages();
    for (int i = 0; i < pages.size(); ++i)
    {
        /* For every page => save the settings: */
        UISettingsPage *pPage = pages[i];
        if (pPage->isEnabled())
            pPage->putBackTo();
    }

    /* Guest OS type & VT-x/AMD-V option correlation auto-fix: */
    VBoxVMSettingsGeneral *pGeneralPage =
        qobject_cast<VBoxVMSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
    VBoxVMSettingsSystem *pSystemPage =
        qobject_cast<VBoxVMSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
    if (pGeneralPage && pSystemPage &&
        pGeneralPage->is64BitOSTypeSelected() && !pSystemPage->isHWVirtExEnabled())
        m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_Enabled, true);

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Disable 2D Video Acceleration for non-Windows guests: */
    if (pGeneralPage && !pGeneralPage->isWindowsOSTypeSelected())
    {
        VBoxVMSettingsDisplay *pDisplayPage =
            qobject_cast<VBoxVMSettingsDisplay*>(m_pSelector->idToPage(VMSettingsPage_Display));
        if (pDisplayPage && pDisplayPage->isAcceleration2DVideoSelected())
            m_machine.SetAccelerate2DVideoEnabled(false);
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */

#ifndef VBOX_OSE
    /* Enable OHCI controller if HID is enabled: */
    if (pSystemPage && pSystemPage->isHIDEnabled())
    {
        CUSBController controller = m_machine.GetUSBController();
        if (!controller.isNull())
            controller.SetEnabled(true);
    }
#endif /* !VBOX_OSE */

    /* Clear the "GUI_FirstRun" extra data key in case if
     * the boot order or disk configuration were changed: */
    if (m_fResetFirstRunFlag)
        m_machine.SetExtraData(VBoxDefs::GUI_FirstRun, QString::null);
}

void UIVMSettingsDlg::retranslateUi()
{
    /* Set dialog's name: */
    setWindowTitle(title());

    /* We have to make sure that the Serial & Network subpages are retranslated
     * before they are revalidated. Cause: They do string comparing within
     * vboxGlobal which is retranslated at that point already. */
    QEvent event(QEvent::LanguageChange);
    QWidget *pPage = 0;

    /* General page: */
    m_pSelector->setItemText(VMSettingsPage_General, tr("General"));

    /* System page: */
    m_pSelector->setItemText(VMSettingsPage_System, tr("System"));

    /* Display page: */
    m_pSelector->setItemText(VMSettingsPage_Display, tr("Display"));

    /* Storage page: */
    m_pSelector->setItemText(VMSettingsPage_Storage, tr("Storage"));

    /* Audio page: */
    m_pSelector->setItemText(VMSettingsPage_Audio, tr("Audio"));

    /* Network page: */
    m_pSelector->setItemText(VMSettingsPage_Network, tr("Network"));
    if ((pPage = m_pSelector->idToPage(VMSettingsPage_Network)))
        qApp->sendEvent(pPage, &event);

    /* Ports page: */
    m_pSelector->setItemText(VMSettingsPage_Ports, tr("Ports"));

    /* Serial page: */
    m_pSelector->setItemText(VMSettingsPage_Serial, tr("Serial Ports"));
    if ((pPage = m_pSelector->idToPage(VMSettingsPage_Serial)))
        qApp->sendEvent(pPage, &event);

    /* Parallel page: */
    m_pSelector->setItemText(VMSettingsPage_Parallel, tr("Parallel Ports"));
    if ((pPage = m_pSelector->idToPage(VMSettingsPage_Parallel)))
        qApp->sendEvent(pPage, &event);

    /* USB page: */
    m_pSelector->setItemText(VMSettingsPage_USB, tr("USB"));

    /* SFolders page: */
    m_pSelector->setItemText(VMSettingsPage_SF, tr("Shared Folders"));

    /* Translate the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();

    /* Revalidate all pages to retranslate the warning messages also: */
    QList<QIWidgetValidator*> validators = findChildren<QIWidgetValidator*>();
    for (int i = 0; i < validators.size(); ++i)
    {
        QIWidgetValidator *pValidator = validators[i];
        if (!pValidator->isValid())
            sltRevalidate(pValidator);
    }
}

QString UIVMSettingsDlg::title() const
{
    QString strDialogTitle;
    if (!m_machine.isNull())
        strDialogTitle = tr("%1 - %2").arg(m_machine.GetName()).arg(titleExtension());
    return strDialogTitle;
}

bool UIVMSettingsDlg::recorrelate(QWidget *pPage, QString &strWarning)
{
    /* This method performs correlation option check
     * between different pages of VM Settings dialog: */

    if (pPage == m_pSelector->idToPage(VMSettingsPage_General) ||
        pPage == m_pSelector->idToPage(VMSettingsPage_System))
    {
        /* Get General & System pages: */
        VBoxVMSettingsGeneral *pGeneralPage =
            qobject_cast<VBoxVMSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
        VBoxVMSettingsSystem *pSystemPage =
            qobject_cast<VBoxVMSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));

        /* Guest OS type & VT-x/AMD-V option correlation test: */
        if (pGeneralPage && pSystemPage &&
            pGeneralPage->is64BitOSTypeSelected() && !pSystemPage->isHWVirtExEnabled())
        {
            strWarning = tr(
                "you have selected a 64-bit guest OS type for this VM. As such guests "
                "require hardware virtualization (VT-x/AMD-V), this feature will be enabled "
                "automatically.");
            return true;
        }
    }

#if defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_CRHGSMI)
    /* 2D Video Acceleration is available for Windows guests only: */
    if (pPage == m_pSelector->idToPage(VMSettingsPage_Display))
    {
        /* Get General & Display pages: */
        VBoxVMSettingsGeneral *pGeneralPage =
            qobject_cast<VBoxVMSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
        VBoxVMSettingsDisplay *pDisplayPage =
            qobject_cast<VBoxVMSettingsDisplay*>(m_pSelector->idToPage(VMSettingsPage_Display));
#ifdef VBOX_WITH_CRHGSMI
        if (pGeneralPage && pDisplayPage)
        {
            bool bWddmSupported = pGeneralPage->isWddmSupportedForOSType();
            pDisplayPage->setWddmMode(bWddmSupported);
            if (pDisplayPage->isAcceleration3DSelected() && bWddmSupported)
            {
                int vramMb = pDisplayPage->getVramSizeMB();
                int requiredVramMb = pDisplayPage->getMinVramSizeMBForWddm3D();
                if (vramMb < requiredVramMb)
                {
                    strWarning = tr(
                        "You have 3D Acceleration enabled for a operation system which uses the WDDM video driver. "
                        "For maximal performance set the guest VRAM to at least <b>%1</b>."
                        ).arg (vboxGlobal().formatSize (requiredVramMb * _1M, 0, VBoxDefs::FormatSize_RoundUp));
                    return true;
                }
            }
        }
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
        if (pGeneralPage && pDisplayPage &&
            pDisplayPage->isAcceleration2DVideoSelected() && !pGeneralPage->isWindowsOSTypeSelected())
        {
            strWarning = tr(
                "you have 2D Video Acceleration enabled. As 2D Video Acceleration "
                "is supported for Windows guests only, this feature will be disabled.");
            return true;
        }
#endif
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */

#ifndef VBOX_OSE
    if (pPage == m_pSelector->idToPage(VMSettingsPage_System) ||
        pPage == m_pSelector->idToPage(VMSettingsPage_USB))
    {
        /* Get System & USB pages: */
        VBoxVMSettingsSystem *pSystemPage =
            qobject_cast<VBoxVMSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
        VBoxVMSettingsUSB *pUsbPage =
            qobject_cast<VBoxVMSettingsUSB*>(m_pSelector->idToPage(VMSettingsPage_USB));
        if (pSystemPage && pUsbPage &&
            pSystemPage->isHIDEnabled() && !pUsbPage->isOHCIEnabled())
        {
            strWarning = tr(
                "you have enabled a USB HID (Human Interface Device). "
                "This will not work unless USB emulation is also enabled. "
                "This will be done automatically when you accept the VM Settings "
                "by pressing the OK button.");
            return true;
        }
    }
#endif /* !VBOX_OSE */

    return true;
}

void UIVMSettingsDlg::sltAllowResetFirstRunFlag()
{
    m_fAllowResetFirstRunFlag = true;
}

void UIVMSettingsDlg::sltResetFirstRunFlag()
{
    if (m_fAllowResetFirstRunFlag)
        m_fResetFirstRunFlag = true;
}

bool UIVMSettingsDlg::isAvailable(int id)
{
    if (m_machine.isNull())
        return false;

    /* Show the machine error message for particular group if present.
     * We don't use the generic cannotLoadMachineSettings()
     * call here because we want this message to be suppressible. */
    switch (id)
    {
        case VMSettingsPage_Serial:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            break;
        }
        case VMSettingsPage_Parallel:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            /* But for now this page is always disabled: */
            return false;
        }
        case VMSettingsPage_USB:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            /* Get the USB controller object: */
            CUSBController controller = m_machine.GetUSBController();
            /* Show the machine error message if any: */
            if (!m_machine.isReallyOk())
                vboxProblem().cannotAccessUSB(m_machine);
            /* Check if USB is implemented: */
            if (controller.isNull() || !controller.GetProxyAvailable())
                return false;
            break;
        }
        default:
            break;
    }
    return true;
}

