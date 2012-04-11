/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic2 class implementation
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
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVM.h"
#include "UIMessageCenter.h"
#include "QIRichTextLabel.h"
#include "VBoxOSTypeSelectorWidget.h"

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

UIWizardNewVMPageBasic2::UIWizardNewVMPageBasic2()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pNameEditorCnt = new QGroupBox(this);
            m_pNameEditorCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pNameEditorLayout = new QHBoxLayout(m_pNameEditorCnt);
                m_pNameEditor = new QLineEdit(m_pNameEditorCnt);
            pNameEditorLayout->addWidget(m_pNameEditor);
        m_pTypeSelectorCnt = new QGroupBox(this);
            m_pTypeSelectorCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pTypeSelectorLayout = new QHBoxLayout(m_pTypeSelectorCnt);
                m_pTypeSelector = new VBoxOSTypeSelectorWidget(m_pTypeSelectorCnt);
                    m_pTypeSelector->activateLayout();
            pTypeSelectorLayout->addWidget(m_pTypeSelector);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pNameEditorCnt);
    pMainLayout->addWidget(m_pTypeSelectorCnt);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pNameEditor, SIGNAL(textChanged(const QString&)), this, SLOT(sltNameChanged(const QString&)));
    connect(m_pTypeSelector, SIGNAL(osTypeChanged()), this, SLOT(sltOsTypeChanged()));

    /* Register fields: */
    registerField("name*", m_pNameEditor);
    registerField("type", m_pTypeSelector, "type", SIGNAL(osTypeChanged()));
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
}

void UIWizardNewVMPageBasic2::sltNameChanged(const QString &strNewName)
{
    /* Search for a matching OS type based on the string the user typed already. */
    for (size_t i=0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
        if (strNewName.contains(gs_OSTypePattern[i].pattern))
        {
            m_pTypeSelector->blockSignals(true);
            m_pTypeSelector->setType(vboxGlobal().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            m_pTypeSelector->blockSignals(false);
            break;
        }
}

void UIWizardNewVMPageBasic2::sltOsTypeChanged()
{
    /* If the user manually edited the OS type, we didn't want our automatic OS type guessing anymore.
     * So simply disconnect the text-edit signal. */
    disconnect(m_pNameEditor, SIGNAL(textChanged(const QString&)), this, SLOT(sltNameChanged(const QString&)));
}

void UIWizardNewVMPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("VM Name and OS Type"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVM::tr("<p>Enter a name for the new virtual machine and select the type of the guest operating system "
                                        "you plan to install onto the virtual machine.</p><p>The name of the virtual machine usually "
                                        "indicates its software and hardware configuration. It will be used by all VirtualBox components "
                                        "to identify your virtual machine.</p>"));
    m_pNameEditorCnt->setTitle(UIWizardNewVM::tr("N&ame"));
    m_pTypeSelectorCnt->setTitle(UIWizardNewVM::tr("OS &Type"));
}

void UIWizardNewVMPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* 'Name' field should have focus initially: */
    m_pNameEditor->setFocus();
}

void UIWizardNewVMPageBasic2::cleanupPage()
{
    /* Cleanup: */
    cleanupMachineFolder();
    /* Call for base-class: */
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic2::validatePage()
{
    return createMachineFolder();
}

bool UIWizardNewVMPageBasic2::machineFolderCreated()
{
    return !m_strMachineFolder.isEmpty();
}

bool UIWizardNewVMPageBasic2::createMachineFolder()
{
    /* Cleanup previosly created folder if any: */
    if (machineFolderCreated() && !cleanupMachineFolder())
    {
        msgCenter().warnAboutCannotRemoveMachineFolder(this, m_strMachineFolder);
        return false;
    }

    /* Get VBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Get default machines directory: */
    QString strDefaultMachinesFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
    /* Compose machine filename: */
    QString strMachineFilename = vbox.ComposeMachineFilename(m_pNameEditor->text(), strDefaultMachinesFolder);
    /* Compose machine folder/basename: */
    QFileInfo fileInfo(strMachineFilename);
    QString strMachineFolder = fileInfo.absolutePath();
    QString strMachineBaseName = fileInfo.completeBaseName();

    /* Make sure that folder doesn't exists: */
    if (QDir(strMachineFolder).exists())
    {
        msgCenter().warnAboutCannotRewriteMachineFolder(this, strMachineFolder);
        return false;
    }

    /* Try to create new folder (and it's predecessors): */
    bool fMachineFolderCreated = QDir().mkpath(strMachineFolder);
    if (!fMachineFolderCreated)
    {
        msgCenter().warnAboutCannotCreateMachineFolder(this, strMachineFolder);
        return false;
    }

    /* Initialize fields: */
    m_strMachineFolder = strMachineFolder;
    m_strMachineBaseName = strMachineBaseName;
    return true;
}

bool UIWizardNewVMPageBasic2::cleanupMachineFolder()
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

QString UIWizardNewVMPageBasic2::machineFolder() const
{
    return m_strMachineFolder;
}

void UIWizardNewVMPageBasic2::setMachineFolder(const QString &strMachineFolder)
{
    m_strMachineFolder = strMachineFolder;
}

QString UIWizardNewVMPageBasic2::machineBaseName() const
{
    return m_strMachineBaseName;
}

void UIWizardNewVMPageBasic2::setMachineBaseName(const QString &strMachineBaseName)
{
    m_strMachineBaseName = strMachineBaseName;
}

