/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic1 class implementation
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

/* Global includes: */
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>

/* Local includes: */
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVM.h"
#include "UIMessageCenter.h"
#include "UINameAndSystemEditor.h"
#include "QIRichTextLabel.h"

/* Defines some patterns to guess the right OS type. Should be in sync with
 * VirtualBox-settings-common.xsd in Main. The list is sorted by priority. The
 * first matching string found, will be used. */
struct osTypePattern
{
    QRegExp pattern;
    const char *pcstId;
};

static const osTypePattern gs_OSTypePattern[] =
{
    /* DOS: */
    { QRegExp("DOS", Qt::CaseInsensitive), "DOS" },

    /* Windows: */
    { QRegExp("Wi.*98", Qt::CaseInsensitive), "Windows98" },
    { QRegExp("Wi.*95", Qt::CaseInsensitive), "Windows95" },
    { QRegExp("Wi.*Me", Qt::CaseInsensitive), "WindowsMe" },
    { QRegExp("(Wi.*NT)|(NT4)", Qt::CaseInsensitive), "WindowsNT4" },
    { QRegExp("((Wi.*XP)|(\\bXP\\b)).*64", Qt::CaseInsensitive), "WindowsXP_64" },
    { QRegExp("(Wi.*XP)|(\\bXP\\b)", Qt::CaseInsensitive), "WindowsXP" },
    { QRegExp("((Wi.*2003)|(W2K3)).*64", Qt::CaseInsensitive), "Windows2003_64" },
    { QRegExp("(Wi.*2003)|(W2K3)", Qt::CaseInsensitive), "Windows2003" },
    { QRegExp("((Wi.*V)|(Vista)).*64", Qt::CaseInsensitive), "WindowsVista_64" },
    { QRegExp("(Wi.*V)|(Vista)", Qt::CaseInsensitive), "WindowsVista" },
    { QRegExp("((Wi.*2008)|(W2K8)).*64", Qt::CaseInsensitive), "Windows2008_64" },
    { QRegExp("(Wi.*2008)|(W2K8)", Qt::CaseInsensitive), "Windows2008" },
    { QRegExp("(Wi.*2000)|(W2K)", Qt::CaseInsensitive), "Windows2000" },
    { QRegExp("(Wi.*7.*64)|(W7.*64)", Qt::CaseInsensitive), "Windows7_64" },
    { QRegExp("(Wi.*7)|(W7)", Qt::CaseInsensitive), "Windows7" },
    { QRegExp("(Wi.*8.*64)|(W8.*64)", Qt::CaseInsensitive), "Windows8_64" },
    { QRegExp("(Wi.*8)|(W8)", Qt::CaseInsensitive), "Windows8" },
    { QRegExp("Wi.*3", Qt::CaseInsensitive), "Windows31" },
    { QRegExp("Wi", Qt::CaseInsensitive), "WindowsXP" },

    /* Solaris: */
    { QRegExp("So.*11", Qt::CaseInsensitive), "Solaris11_64" },
    { QRegExp("((Op.*So)|(os20[01][0-9])|(So.*10)|(India)|(Neva)).*64", Qt::CaseInsensitive), "OpenSolaris_64" },
    { QRegExp("(Op.*So)|(os20[01][0-9])|(So.*10)|(India)|(Neva)", Qt::CaseInsensitive), "OpenSolaris" },
    { QRegExp("So.*64", Qt::CaseInsensitive), "Solaris_64" },
    { QRegExp("So", Qt::CaseInsensitive), "Solaris" },

    /* OS/2: */
    { QRegExp("OS[/|!-]{,1}2.*W.*4.?5", Qt::CaseInsensitive), "OS2Warp45" },
    { QRegExp("OS[/|!-]{,1}2.*W.*4", Qt::CaseInsensitive), "OS2Warp4" },
    { QRegExp("OS[/|!-]{,1}2.*W", Qt::CaseInsensitive), "OS2Warp3" },
    { QRegExp("(OS[/|!-]{,1}2.*e)|(eCS.*)", Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp("OS[/|!-]{,1}2", Qt::CaseInsensitive), "OS2" },

    /* Code names for Linux distributions: */
    { QRegExp("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)).*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("(edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(sid)).*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("(sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(sid)", Qt::CaseInsensitive), "Debian" },
    { QRegExp("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)).*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("(moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)", Qt::CaseInsensitive), "Fedora" },

    /* Regular names of Linux distributions: */
    { QRegExp("Arc.*64", Qt::CaseInsensitive), "ArchLinux_64" },
    { QRegExp("Arc", Qt::CaseInsensitive), "ArchLinux" },
    { QRegExp("Deb.*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("Deb", Qt::CaseInsensitive), "Debian" },
    { QRegExp("((SU)|(Nov)|(SLE)).*64", Qt::CaseInsensitive), "OpenSUSE_64" },
    { QRegExp("(SU)|(Nov)|(SLE)", Qt::CaseInsensitive), "OpenSUSE" },
    { QRegExp("Fe.*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("Fe", Qt::CaseInsensitive), "Fedora" },
    { QRegExp("((Gen)|(Sab)).*64", Qt::CaseInsensitive), "Gentoo_64" },
    { QRegExp("(Gen)|(Sab)", Qt::CaseInsensitive), "Gentoo" },
    { QRegExp("((Man)|(Mag)).*64", Qt::CaseInsensitive), "Mandriva_64" },
    { QRegExp("((Man)|(Mag))", Qt::CaseInsensitive), "Mandriva" },
    { QRegExp("((Red)|(rhel)|(cen)).*64", Qt::CaseInsensitive), "RedHat_64" },
    { QRegExp("(Red)|(rhel)|(cen)", Qt::CaseInsensitive), "RedHat" },
    { QRegExp("Tur.*64", Qt::CaseInsensitive), "Turbolinux_64" },
    { QRegExp("Tur", Qt::CaseInsensitive), "Turbolinux" },
    { QRegExp("Ub.*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("Ub", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("Xa.*64", Qt::CaseInsensitive), "Xandros_64" },
    { QRegExp("Xa", Qt::CaseInsensitive), "Xandros" },
    { QRegExp("((Or)|(oel)).*64", Qt::CaseInsensitive), "Oracle_64" },
    { QRegExp("(Or)|(oel)", Qt::CaseInsensitive), "Oracle" },
    { QRegExp("Knoppix", Qt::CaseInsensitive), "Linux26" },
    { QRegExp("Dsl", Qt::CaseInsensitive), "Linux24" },
    { QRegExp("((Li)|(lnx)).*2.?2", Qt::CaseInsensitive), "Linux22" },
    { QRegExp("((Li)|(lnx)).*2.?4.*64", Qt::CaseInsensitive), "Linux24_64" },
    { QRegExp("((Li)|(lnx)).*2.?4", Qt::CaseInsensitive), "Linux24" },
    { QRegExp("((((Li)|(lnx)).*2.?6)|(LFS)).*64", Qt::CaseInsensitive), "Linux26_64" },
    { QRegExp("(((Li)|(lnx)).*2.?6)|(LFS)", Qt::CaseInsensitive), "Linux26" },
    { QRegExp("((Li)|(lnx)).*64", Qt::CaseInsensitive), "Linux26_64" },
    { QRegExp("(Li)|(lnx)", Qt::CaseInsensitive), "Linux26" },

    /* Other: */
    { QRegExp("L4", Qt::CaseInsensitive), "L4" },
    { QRegExp("((Fr.*B)|(fbsd)).*64", Qt::CaseInsensitive), "FreeBSD_64" },
    { QRegExp("(Fr.*B)|(fbsd)", Qt::CaseInsensitive), "FreeBSD" },
    { QRegExp("Op.*B.*64", Qt::CaseInsensitive), "OpenBSD_64" },
    { QRegExp("Op.*B", Qt::CaseInsensitive), "OpenBSD" },
    { QRegExp("Ne.*B.*64", Qt::CaseInsensitive), "NetBSD_64" },
    { QRegExp("Ne.*B", Qt::CaseInsensitive), "NetBSD" },
    { QRegExp("QN", Qt::CaseInsensitive), "QNX" },
    { QRegExp("((Mac)|(Tig)|(Leop)|(osx)).*64", Qt::CaseInsensitive), "MacOS_64" },
    { QRegExp("(Mac)|(Tig)|(Leop)|(osx)", Qt::CaseInsensitive), "MacOS" },
    { QRegExp("Net", Qt::CaseInsensitive), "Netware" },
    { QRegExp("Rocki", Qt::CaseInsensitive), "JRockitVE" },
    { QRegExp("Ot", Qt::CaseInsensitive), "Other" },
};

UIWizardNewVMPage1::UIWizardNewVMPage1()
{
}

void UIWizardNewVMPage1::onNameChanged(const QString &strNewName)
{
    /* Search for a matching OS type based on the string the user typed already. */
    for (size_t i = 0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
        if (strNewName.contains(gs_OSTypePattern[i].pattern))
        {
            m_pNameAndSystemEditor->blockSignals(true);
            m_pNameAndSystemEditor->setType(vboxGlobal().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            m_pNameAndSystemEditor->blockSignals(false);
            break;
        }
}

void UIWizardNewVMPage1::onOsTypeChanged()
{
    /* If the user manually edited the OS type, we didn't want our automatic OS type guessing anymore.
     * So simply disconnect the text-edit signal. */
    m_pNameAndSystemEditor->disconnect(SIGNAL(sigNameChanged(const QString &)), thisImp(), SLOT(sltNameChanged(const QString &)));
}

bool UIWizardNewVMPage1::machineFolderCreated()
{
    return !m_strMachineFolder.isEmpty();
}

bool UIWizardNewVMPage1::createMachineFolder()
{
    /* Cleanup previosly created folder if any: */
    if (machineFolderCreated() && !cleanupMachineFolder())
    {
        msgCenter().warnAboutCannotRemoveMachineFolder(thisImp(), m_strMachineFolder);
        return false;
    }

    /* Get VBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Get default machines directory: */
    QString strDefaultMachinesFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
    /* Compose machine filename: */
    QString strMachineFilename = vbox.ComposeMachineFilename(m_pNameAndSystemEditor->name(), strDefaultMachinesFolder);
    /* Compose machine folder/basename: */
    QFileInfo fileInfo(strMachineFilename);
    QString strMachineFolder = fileInfo.absolutePath();
    QString strMachineBaseName = fileInfo.completeBaseName();

    /* Make sure that folder doesn't exists: */
    if (QDir(strMachineFolder).exists())
    {
        msgCenter().warnAboutCannotRewriteMachineFolder(thisImp(), strMachineFolder);
        return false;
    }

    /* Try to create new folder (and it's predecessors): */
    bool fMachineFolderCreated = QDir().mkpath(strMachineFolder);
    if (!fMachineFolderCreated)
    {
        msgCenter().warnAboutCannotCreateMachineFolder(thisImp(), strMachineFolder);
        return false;
    }

    /* Initialize fields: */
    m_strMachineFolder = strMachineFolder;
    m_strMachineBaseName = strMachineBaseName;
    return true;
}

bool UIWizardNewVMPage1::cleanupMachineFolder()
{
    /* Make sure folder was previosly created: */
    if (m_strMachineFolder.isEmpty())
        return false;
    /* Try to cleanup folder (and it's predecessors): */
    bool fMachineFolderRemoved = QDir().rmpath(m_strMachineFolder);
    /* Reset machine folder value: */
    if (fMachineFolderRemoved)
        m_strMachineFolder = QString();
    /* Return cleanup result: */
    return fMachineFolderRemoved;
}

UIWizardNewVMPageBasic1::UIWizardNewVMPageBasic1()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pNameAndSystemCnt = new QGroupBox(this);
        {
            m_pNameAndSystemCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pNameAndSystemLayout = new QHBoxLayout(m_pNameAndSystemCnt);
            {
                m_pNameAndSystemEditor = new UINameAndSystemEditor(m_pNameAndSystemCnt);
                pNameAndSystemLayout->addWidget(m_pNameAndSystemEditor);
            }
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pNameAndSystemCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pNameAndSystemEditor, SIGNAL(sigNameChanged(const QString &)), this, SLOT(sltNameChanged(const QString &)));
    connect(m_pNameAndSystemEditor, SIGNAL(sigOsTypeChanged()), this, SLOT(sltOsTypeChanged()));

    /* Register fields: */
    registerField("name*", m_pNameAndSystemEditor, "name", SIGNAL(sigNameChanged(const QString &)));
    registerField("type", m_pNameAndSystemEditor, "type", SIGNAL(sigOsTypeChanged()));
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
}

void UIWizardNewVMPageBasic1::sltNameChanged(const QString &strNewName)
{
    /* Call to base-class: */
    onNameChanged(strNewName);
}

void UIWizardNewVMPageBasic1::sltOsTypeChanged()
{
    /* Call to base-class: */
    onOsTypeChanged();
}

void UIWizardNewVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Name and operating system"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVM::tr("Please choose a descriptive name for the new virtual machine "
                                        "and select the type of operating system you intend to install on it. "
                                        "The name you choose will be used throughout VirtualBox "
                                        "to identify this machine."));
    m_pNameAndSystemCnt->setTitle(UIWizardNewVM::tr("Name and operating system"));
}

void UIWizardNewVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

void UIWizardNewVMPageBasic1::cleanupPage()
{
    /* Cleanup: */
    cleanupMachineFolder();
    /* Call to base-class: */
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic1::validatePage()
{
    /* Try to create machine folder: */
    return createMachineFolder();
}

