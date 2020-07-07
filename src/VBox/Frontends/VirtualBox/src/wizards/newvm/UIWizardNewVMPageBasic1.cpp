/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic1 class implementation.
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIMessageCenter.h"
#include "UINameAndSystemEditor.h"
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CSystemProperties.h"
#include "CUnattended.h"

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
    { QRegExp(  "Wi.*98",                         Qt::CaseInsensitive), "Windows98" },
    { QRegExp(  "Wi.*95",                         Qt::CaseInsensitive), "Windows95" },
    { QRegExp(  "Wi.*Me",                         Qt::CaseInsensitive), "WindowsMe" },
    { QRegExp( "(Wi.*NT)|(NT[-._v]*4)",           Qt::CaseInsensitive), "WindowsNT4" },
    { QRegExp( "NT[-._v]*3[.,]*[51x]",            Qt::CaseInsensitive), "WindowsNT3x" },
    /* Note: Do not automatically set WindowsXP_64 on 64-bit hosts, as Windows XP 64-bit
     *       is extremely rare -- most users never heard of it even. So always default to 32-bit. */
    { QRegExp("((Wi.*XP)|(XP)).*",                Qt::CaseInsensitive), "WindowsXP" },
    { QRegExp("((Wi.*2003)|(W2K3)|(Win2K3)).*64", Qt::CaseInsensitive), "Windows2003_64" },
    { QRegExp("((Wi.*2003)|(W2K3)|(Win2K3)).*32", Qt::CaseInsensitive), "Windows2003" },
    { QRegExp("((Wi.*Vis)|(Vista)).*64",          Qt::CaseInsensitive), "WindowsVista_64" },
    { QRegExp("((Wi.*Vis)|(Vista)).*32",          Qt::CaseInsensitive), "WindowsVista" },
    { QRegExp( "(Wi.*2016)|(W2K16)|(Win2K16)",    Qt::CaseInsensitive), "Windows2016_64" },
    { QRegExp( "(Wi.*2012)|(W2K12)|(Win2K12)",    Qt::CaseInsensitive), "Windows2012_64" },
    { QRegExp("((Wi.*2008)|(W2K8)|(Win2k8)).*64", Qt::CaseInsensitive), "Windows2008_64" },
    { QRegExp("((Wi.*2008)|(W2K8)|(Win2K8)).*32", Qt::CaseInsensitive), "Windows2008" },
    { QRegExp( "(Wi.*2000)|(W2K)|(Win2K)",        Qt::CaseInsensitive), "Windows2000" },
    { QRegExp( "(Wi.*7.*64)|(W7.*64)",            Qt::CaseInsensitive), "Windows7_64" },
    { QRegExp( "(Wi.*7.*32)|(W7.*32)",            Qt::CaseInsensitive), "Windows7" },
    { QRegExp( "(Wi.*8.*1.*64)|(W8.*64)",         Qt::CaseInsensitive), "Windows81_64" },
    { QRegExp( "(Wi.*8.*1.*32)|(W8.*32)",         Qt::CaseInsensitive), "Windows81" },
    { QRegExp( "(Wi.*8.*64)|(W8.*64)",            Qt::CaseInsensitive), "Windows8_64" },
    { QRegExp( "(Wi.*8.*32)|(W8.*32)",            Qt::CaseInsensitive), "Windows8" },
    { QRegExp( "(Wi.*10.*64)|(W10.*64)",          Qt::CaseInsensitive), "Windows10_64" },
    { QRegExp( "(Wi.*10.*32)|(W10.*32)",          Qt::CaseInsensitive), "Windows10" },
    { QRegExp(  "Wi.*3.*1",                       Qt::CaseInsensitive), "Windows31" },
    /* Set Windows 7 as default for "Windows". */
    { QRegExp(  "Wi.*64",                         Qt::CaseInsensitive), "Windows7_64" },
    { QRegExp(  "Wi.*32",                         Qt::CaseInsensitive), "Windows7" },
    /* ReactOS wants to be considered as Windows 2003 */
    { QRegExp(  "Reac.*",                         Qt::CaseInsensitive), "Windows2003" },

    /* Solaris: */
    { QRegExp("Sol.*11",                                                  Qt::CaseInsensitive), "Solaris11_64" },
    { QRegExp("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*64", Qt::CaseInsensitive), "OpenSolaris_64" },
    { QRegExp("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*32", Qt::CaseInsensitive), "OpenSolaris" },
    { QRegExp("Sol.*64",                                                  Qt::CaseInsensitive), "Solaris_64" },
    { QRegExp("Sol.*32",                                                  Qt::CaseInsensitive), "Solaris" },

    /* OS/2: */
    { QRegExp( "OS[/|!-]{,1}2.*W.*4.?5",    Qt::CaseInsensitive), "OS2Warp45" },
    { QRegExp( "OS[/|!-]{,1}2.*W.*4",       Qt::CaseInsensitive), "OS2Warp4" },
    { QRegExp( "OS[/|!-]{,1}2.*W",          Qt::CaseInsensitive), "OS2Warp3" },
    { QRegExp("(OS[/|!-]{,1}2.*e)|(eCS.*)", Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp( "OS[/|!-]{,1}2",             Qt::CaseInsensitive), "OS2" },
    { QRegExp( "eComS.*",                   Qt::CaseInsensitive), "OS2eCS" },

    /* Other: Must come before Ubuntu/Maverick and before Linux??? */
    { QRegExp("QN", Qt::CaseInsensitive), "QNX" },

    /* Mac OS X: Must come before Ubuntu/Maverick and before Linux: */
    { QRegExp("((mac.*10[.,]{0,1}4)|(os.*x.*10[.,]{0,1}4)|(mac.*ti)|(os.*x.*ti)|(Tig)).64",     Qt::CaseInsensitive), "MacOS_64" },
    { QRegExp("((mac.*10[.,]{0,1}4)|(os.*x.*10[.,]{0,1}4)|(mac.*ti)|(os.*x.*ti)|(Tig)).32",     Qt::CaseInsensitive), "MacOS" },
    { QRegExp("((mac.*10[.,]{0,1}5)|(os.*x.*10[.,]{0,1}5)|(mac.*leo)|(os.*x.*leo)|(Leop)).*64", Qt::CaseInsensitive), "MacOS_64" },
    { QRegExp("((mac.*10[.,]{0,1}5)|(os.*x.*10[.,]{0,1}5)|(mac.*leo)|(os.*x.*leo)|(Leop)).*32", Qt::CaseInsensitive), "MacOS" },
    { QRegExp("((mac.*10[.,]{0,1}6)|(os.*x.*10[.,]{0,1}6)|(mac.*SL)|(os.*x.*SL)|(Snow L)).*64", Qt::CaseInsensitive), "MacOS106_64" },
    { QRegExp("((mac.*10[.,]{0,1}6)|(os.*x.*10[.,]{0,1}6)|(mac.*SL)|(os.*x.*SL)|(Snow L)).*32", Qt::CaseInsensitive), "MacOS106" },
    { QRegExp( "(mac.*10[.,]{0,1}7)|(os.*x.*10[.,]{0,1}7)|(mac.*ML)|(os.*x.*ML)|(Mount)",       Qt::CaseInsensitive), "MacOS108_64" },
    { QRegExp( "(mac.*10[.,]{0,1}8)|(os.*x.*10[.,]{0,1}8)|(Lion)",                              Qt::CaseInsensitive), "MacOS107_64" },
    { QRegExp( "(mac.*10[.,]{0,1}9)|(os.*x.*10[.,]{0,1}9)|(mac.*mav)|(os.*x.*mav)|(Mavericks)", Qt::CaseInsensitive), "MacOS109_64" },
    { QRegExp( "(mac.*yos)|(os.*x.*yos)|(Yosemite)",                                            Qt::CaseInsensitive), "MacOS1010_64" },
    { QRegExp( "(mac.*cap)|(os.*x.*capit)|(Capitan)",                                           Qt::CaseInsensitive), "MacOS1011_64" },
    { QRegExp( "(mac.*hig)|(os.*x.*high.*sierr)|(High Sierra)",                                 Qt::CaseInsensitive), "MacOS1013_64" },
    { QRegExp( "(mac.*sie)|(os.*x.*sierr)|(Sierra)",                                            Qt::CaseInsensitive), "MacOS1012_64" },
    { QRegExp("((Mac)|(Tig)|(Leop)|(Yose)|(os[ ]*x)).*64",                                      Qt::CaseInsensitive), "MacOS_64" },
    { QRegExp("((Mac)|(Tig)|(Leop)|(Yose)|(os[ ]*x)).*32",                                      Qt::CaseInsensitive), "MacOS" },

    /* Code names for Linux distributions: */
    { QRegExp("((bianca)|(cassandra)|(celena)|(daryna)|(elyssa)|(felicia)|(gloria)|(helena)|(isadora)|(julia)|(katya)|(lisa)|(maya)|(nadia)|(olivia)|(petra)|(qiana)|(rebecca)|(rafaela)|(rosa)).*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("((bianca)|(cassandra)|(celena)|(daryna)|(elyssa)|(felicia)|(gloria)|(helena)|(isadora)|(julia)|(katya)|(lisa)|(maya)|(nadia)|(olivia)|(petra)|(qiana)|(rebecca)|(rafaela)|(rosa)).*32", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)|(quantal)|(raring)|(saucy)|(trusty)|(utopic)|(vivid)|(wily)|(xenial)|(yakkety)|(zesty)).*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)|(quantal)|(raring)|(saucy)|(trusty)|(utopic)|(vivid)|(wily)|(xenial)|(yakkety)|(zesty)).*32", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(jessie)|(stretch)|(buster)|(sid)).*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(jessie)|(stretch)|(buster)|(sid)).*32", Qt::CaseInsensitive), "Debian" },
    { QRegExp("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)|(beefy)|(spherical)).*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)|(beefy)|(spherical)).*32", Qt::CaseInsensitive), "Fedora" },

    /* Regular names of Linux distributions: */
    { QRegExp("Arc.*64",                           Qt::CaseInsensitive), "ArchLinux_64" },
    { QRegExp("Arc.*32",                           Qt::CaseInsensitive), "ArchLinux" },
    { QRegExp("Deb.*64",                           Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("Deb.*32",                           Qt::CaseInsensitive), "Debian" },
    { QRegExp("((SU)|(Nov)|(SLE)).*64",            Qt::CaseInsensitive), "OpenSUSE_64" },
    { QRegExp("((SU)|(Nov)|(SLE)).*32",            Qt::CaseInsensitive), "OpenSUSE" },
    { QRegExp("Fe.*64",                            Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("Fe.*32",                            Qt::CaseInsensitive), "Fedora" },
    { QRegExp("((Gen)|(Sab)).*64",                 Qt::CaseInsensitive), "Gentoo_64" },
    { QRegExp("((Gen)|(Sab)).*32",                 Qt::CaseInsensitive), "Gentoo" },
    { QRegExp("((Man)|(Mag)).*64",                 Qt::CaseInsensitive), "Mandriva_64" },
    { QRegExp("((Man)|(Mag)).*32",                 Qt::CaseInsensitive), "Mandriva" },
    { QRegExp("((Red)|(rhel)|(cen)).*64",          Qt::CaseInsensitive), "RedHat_64" },
    { QRegExp("((Red)|(rhel)|(cen)).*32",          Qt::CaseInsensitive), "RedHat" },
    { QRegExp("Tur.*64",                           Qt::CaseInsensitive), "Turbolinux_64" },
    { QRegExp("Tur.*32",                           Qt::CaseInsensitive), "Turbolinux" },
    { QRegExp("(Ub)|(Min).*64",                    Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("(Ub)|(Min).*32",                    Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("Xa.*64",                            Qt::CaseInsensitive), "Xandros_64" },
    { QRegExp("Xa.*32",                            Qt::CaseInsensitive), "Xandros" },
    { QRegExp("((Or)|(oel)|(ol)).*64",             Qt::CaseInsensitive), "Oracle_64" },
    { QRegExp("((Or)|(oel)|(ol)).*32",             Qt::CaseInsensitive), "Oracle" },
    { QRegExp("Knoppix",                           Qt::CaseInsensitive), "Linux26" },
    { QRegExp("Dsl",                               Qt::CaseInsensitive), "Linux24" },
    { QRegExp("((Lin)|(lnx)).*2.?2",               Qt::CaseInsensitive), "Linux22" },
    { QRegExp("((Lin)|(lnx)).*2.?4.*64",           Qt::CaseInsensitive), "Linux24_64" },
    { QRegExp("((Lin)|(lnx)).*2.?4.*32",           Qt::CaseInsensitive), "Linux24" },
    { QRegExp("((((Lin)|(lnx)).*2.?6)|(LFS)).*64", Qt::CaseInsensitive), "Linux26_64" },
    { QRegExp("((((Lin)|(lnx)).*2.?6)|(LFS)).*32", Qt::CaseInsensitive), "Linux26" },
    { QRegExp("((Lin)|(lnx)).*64",                 Qt::CaseInsensitive), "Linux26_64" },
    { QRegExp("((Lin)|(lnx)).*32",                 Qt::CaseInsensitive), "Linux26" },

    /* Other: */
    { QRegExp("L4",                   Qt::CaseInsensitive), "L4" },
    { QRegExp("((Fr.*B)|(fbsd)).*64", Qt::CaseInsensitive), "FreeBSD_64" },
    { QRegExp("((Fr.*B)|(fbsd)).*32", Qt::CaseInsensitive), "FreeBSD" },
    { QRegExp("Op.*B.*64",            Qt::CaseInsensitive), "OpenBSD_64" },
    { QRegExp("Op.*B.*32",            Qt::CaseInsensitive), "OpenBSD" },
    { QRegExp("Ne.*B.*64",            Qt::CaseInsensitive), "NetBSD_64" },
    { QRegExp("Ne.*B.*32",            Qt::CaseInsensitive), "NetBSD" },
    { QRegExp("Net",                  Qt::CaseInsensitive), "Netware" },
    { QRegExp("Rocki",                Qt::CaseInsensitive), "JRockitVE" },
    { QRegExp("bs[23]{0,1}-",         Qt::CaseInsensitive), "VBoxBS_64" }, /* bootsector tests */
    { QRegExp("Ot",                   Qt::CaseInsensitive), "Other" },
};

UIWizardNewVMPage1::UIWizardNewVMPage1(const QString &strGroup)
    : m_pButtonSimple(0)
    , m_pButtonUnattended(0)
    , m_pISOSelectorLabel(0)
    , m_pISOFilePathSelector(0)
    , m_pStartHeadlessLabel(0)
    , m_pStartHeadlessCheckBox(0)
    , m_pNameAndSystemEditor(0)
    , m_pUnattendedLabel(0)
    , m_pNameOSTypeLabel(0)
    , m_strGroup(strGroup)
{
    CHost host = uiCommon().host();
    m_fSupportsHWVirtEx = host.GetProcessorFeature(KProcessorFeature_HWVirtEx);
    m_fSupportsLongMode = host.GetProcessorFeature(KProcessorFeature_LongMode);
}

void UIWizardNewVMPage1::onNameChanged(QString strNewName)
{
    /* Do not forget about achitecture bits, if not yet specified: */
    if (!strNewName.contains("32") && !strNewName.contains("64"))
        strNewName += ARCH_BITS == 64 && m_fSupportsHWVirtEx && m_fSupportsLongMode ? "64" : "32";

    /* Search for a matching OS type based on the string the user typed already. */
    for (size_t i = 0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
        if (strNewName.contains(gs_OSTypePattern[i].pattern))
        {
            if (m_pNameAndSystemEditor)
            {
                m_pNameAndSystemEditor->blockSignals(true);
                m_pNameAndSystemEditor->setType(uiCommon().vmGuestOSType(gs_OSTypePattern[i].pcstId));
                m_pNameAndSystemEditor->blockSignals(false);
            }
            break;
        }
}

void UIWizardNewVMPage1::onOsTypeChanged()
{
    /* If the user manually edited the OS type, we didn't want our automatic OS type guessing anymore.
     * So simply disconnect the text-edit signal. */
    if (m_pNameAndSystemEditor)
        m_pNameAndSystemEditor->disconnect(SIGNAL(sigNameChanged(const QString &)), thisImp(), SLOT(sltNameChanged(const QString &)));
}

bool UIWizardNewVMPage1::determineOSType(const QString &strISOPath)
{
    QFileInfo isoFileInfo(strISOPath);
    if (!isoFileInfo.exists())
    {
        m_strDetectedOSTypeId.clear();
        // m_strDetectedOSVersion.clear();
        // m_strDetectedOSFlavor.clear();
        // m_strDetectedOSLanguages.clear();
        // m_strDetectedOSHints.clear();
        // updateStatusLabel();
        return false;
    }

    CUnattended comUnatteded = uiCommon().virtualBox().CreateUnattendedInstaller();
    comUnatteded.SetIsoPath(strISOPath);
    comUnatteded.DetectIsoOS();

    m_strDetectedOSTypeId = comUnatteded.GetDetectedOSTypeId();
    // m_strDetectedOSVersion = comUnatteded.GetDetectedOSVersion();
    // m_strDetectedOSFlavor = comUnatteded.GetDetectedOSFlavor();
    // m_strDetectedOSLanguages = comUnatteded.GetDetectedOSLanguages();
    // m_strDetectedOSHints = comUnatteded.GetDetectedOSHints();

    //updateStatusLabel();
    return true;
}

void UIWizardNewVMPage1::composeMachineFilePath()
{
    if (!m_pNameAndSystemEditor)
        return;
    if (m_pNameAndSystemEditor->name().isEmpty() || m_pNameAndSystemEditor->path().isEmpty())
        return;
    /* Get VBox: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* Compose machine filename: */
    m_strMachineFilePath = vbox.ComposeMachineFilename(m_pNameAndSystemEditor->name(),
                                                       m_strGroup,
                                                       QString(),
                                                       m_pNameAndSystemEditor->path());
    /* Compose machine folder/basename: */
    const QFileInfo fileInfo(m_strMachineFilePath);
    m_strMachineFolder = fileInfo.absolutePath();
    m_strMachineBaseName = fileInfo.completeBaseName();
}

bool UIWizardNewVMPage1::checkISOFile() const
{
    if (m_pButtonUnattended && m_pButtonUnattended->isChecked())
    {
        QString strISOFilePath = m_pISOFilePathSelector ? m_pISOFilePathSelector->path() : QString();
        if (!QFileInfo(strISOFilePath).exists())
            return false;
    }
    return true;
}

void UIWizardNewVMPage1::createNameOSTypeWidgets(QGridLayout *pGridLayout, bool fCreateLabels /* = true */)
{
    if (pGridLayout)
    {
        if (fCreateLabels)
        {
            m_pUnattendedLabel = new QIRichTextLabel;
            if (m_pUnattendedLabel)
                pGridLayout->addWidget(m_pUnattendedLabel, 0, 0, 1, 3);
        }

        m_pButtonGroup = new QButtonGroup;
        if (m_pButtonGroup)
        {
            m_pButtonSimple = new QRadioButton;
            if (m_pButtonSimple)
            {
                m_pButtonSimple->setChecked(true);
                pGridLayout->addWidget(m_pButtonSimple, 1, 0, 1, 3);
            }
            m_pButtonUnattended = new QRadioButton;
            if (m_pButtonUnattended)
            {
                QStyleOptionButton options;
                options.initFrom(m_pButtonUnattended);
                const int iWidth = m_pButtonUnattended->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth,
                                                                             &options, m_pButtonUnattended);
                pGridLayout->setColumnMinimumWidth(0, iWidth);
                pGridLayout->addWidget(m_pButtonUnattended, 2, 0, 1, 3);
            }

            m_pButtonGroup->addButton(m_pButtonSimple);
            m_pButtonGroup->addButton(m_pButtonUnattended);
        }

        m_pISOSelectorLabel = new QLabel;
        if (m_pISOSelectorLabel)
        {
            m_pISOSelectorLabel->setAlignment(Qt::AlignRight);
            m_pISOSelectorLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            m_pISOSelectorLabel->setEnabled(false);
            pGridLayout->addWidget(m_pISOSelectorLabel, 3, 1);
        }
        m_pISOFilePathSelector = new UIFilePathSelector;
        if (m_pISOFilePathSelector)
        {
            m_pISOFilePathSelector->setResetEnabled(false);
            m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
            m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
            m_pISOFilePathSelector->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pISOFilePathSelector->setEnabled(false);
            pGridLayout->addWidget(m_pISOFilePathSelector, 3, 2);
        }

        m_pStartHeadlessLabel = new QLabel;
        if (m_pStartHeadlessLabel)
        {
            m_pStartHeadlessLabel->setAlignment(Qt::AlignRight);
            m_pStartHeadlessLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            m_pStartHeadlessLabel->setEnabled(false);
            pGridLayout->addWidget(m_pStartHeadlessLabel, 4, 1);
        }
        m_pStartHeadlessCheckBox = new QCheckBox;
        if (m_pStartHeadlessCheckBox)
        {
            m_pStartHeadlessCheckBox->setEnabled(false);
            pGridLayout->addWidget(m_pStartHeadlessCheckBox, 4, 2);
        }

        if (fCreateLabels)
        {
            m_pNameOSTypeLabel = new QIRichTextLabel;
            if (m_pNameOSTypeLabel)
                pGridLayout->addWidget(m_pNameOSTypeLabel, 5, 0, 1, 3);
        }

        m_pNameAndSystemEditor = new UINameAndSystemEditor(0, true, true, true);
        if (m_pNameAndSystemEditor)
        {
            pGridLayout->addWidget(m_pNameAndSystemEditor, 6, 1, 1, 2);
        }
    }
}

bool UIWizardNewVMPage1::createMachineFolder()
{
    if (!m_pNameAndSystemEditor)
        return false;
    /* Cleanup previosly created folder if any: */
    if (!cleanupMachineFolder())
    {
        msgCenter().cannotRemoveMachineFolder(m_strMachineFolder, thisImp());
        return false;
    }

    composeMachineFilePath();

    /* Check if the folder already exists and check if it has been created by this wizard */
    if (QDir(m_strMachineFolder).exists())
    {
        /* Looks like we have already created this folder for this run of the wizard. Just return */
        if (m_strCreatedFolder == m_strMachineFolder)
            return true;
        /* The folder is there but not because of this wizard. Avoid overwriting a existing machine's folder */
        else
        {
            msgCenter().cannotRewriteMachineFolder(m_strMachineFolder, thisImp());
            return false;
        }
    }

    /* Try to create new folder (and it's predecessors): */
    bool fMachineFolderCreated = QDir().mkpath(m_strMachineFolder);
    if (!fMachineFolderCreated)
    {
        msgCenter().cannotCreateMachineFolder(m_strMachineFolder, thisImp());
        return false;
    }
    m_strCreatedFolder = m_strMachineFolder;
    return true;
}

bool UIWizardNewVMPage1::cleanupMachineFolder(bool fWizardCancel /* = false */)
{
    /* Make sure folder was previosly created: */
    if (m_strCreatedFolder.isEmpty())
        return true;
    /* Clean this folder if the machine folder has been changed by the user or we are cancelling the wizard: */
    if (m_strCreatedFolder != m_strMachineFolder || fWizardCancel)
    {
        /* Try to cleanup folder (and it's predecessors): */
        bool fMachineFolderRemoved = QDir().rmpath(m_strCreatedFolder);
        /* Reset machine folder value: */
        if (fMachineFolderRemoved)
            m_strCreatedFolder = QString();
        /* Return cleanup result: */
        return fMachineFolderRemoved;
    }
    return true;
}

QString UIWizardNewVMPage1::machineFilePath() const
{
    return m_strMachineFilePath;
}

void UIWizardNewVMPage1::setMachineFilePath(const QString &strMachineFilePath)
{
    m_strMachineFilePath = strMachineFilePath;
}

QString UIWizardNewVMPage1::machineFolder() const
{
    return m_strMachineFolder;
}

void UIWizardNewVMPage1::setMachineFolder(const QString &strMachineFolder)
{
    m_strMachineFolder = strMachineFolder;
}

QString UIWizardNewVMPage1::machineBaseName() const
{
    return m_strMachineBaseName;
}

void UIWizardNewVMPage1::setMachineBaseName(const QString &strMachineBaseName)
{
    m_strMachineBaseName = strMachineBaseName;
}

QString UIWizardNewVMPage1::guestOSFamiyId() const
{
    if (!m_pNameAndSystemEditor)
        return QString();
    return m_pNameAndSystemEditor->familyId();
}

QString UIWizardNewVMPage1::ISOFilePath() const
{
    if (!m_pISOFilePathSelector)
        return QString();
    return m_pISOFilePathSelector->path();
}

bool UIWizardNewVMPage1::isUnattendedEnabled() const
{
    if (!m_pButtonUnattended)
        return false;
    return m_pButtonUnattended->isChecked();
}

bool UIWizardNewVMPage1::startHeadless() const
{
    if (!m_pStartHeadlessCheckBox)
        return false;
    return m_pStartHeadlessCheckBox->isChecked();
}

const QString &UIWizardNewVMPage1::detectedOSTypeId() const
{
    return m_strDetectedOSTypeId;
}

UIWizardNewVMPageBasic1::UIWizardNewVMPageBasic1(const QString &strGroup)
    : UIWizardNewVMPage1(strGroup)
{
    prepare();
}

void UIWizardNewVMPageBasic1::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout(this);;
    createNameOSTypeWidgets(pMainLayout);
    createConnections();
    /* Register fields: */
    registerField("name*", m_pNameAndSystemEditor, "name", SIGNAL(sigNameChanged(const QString &)));
    registerField("type", m_pNameAndSystemEditor, "type", SIGNAL(sigOsTypeChanged()));
    registerField("machineFilePath", this, "machineFilePath");
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
    registerField("guestOSFamiyId", this, "guestOSFamiyId");
    registerField("ISOFilePath", this, "ISOFilePath");
    registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
    registerField("startHeadless", this, "startHeadless");
    registerField("detectedOSTypeId", this, "detectedOSTypeId");
}

void UIWizardNewVMPageBasic1::createConnections()
{
    connect(m_pButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, &UIWizardNewVMPageBasic1::sltUnattendedCheckBoxToggle);
    connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged, this, &UIWizardNewVMPageBasic1::sltISOPathChanged);
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigNameChanged, this, &UIWizardNewVMPageBasic1::sltNameChanged);
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigPathChanged, this, &UIWizardNewVMPageBasic1::sltPathChanged);
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOsTypeChanged, this, &UIWizardNewVMPageBasic1::sltOsTypeChanged);
}

int UIWizardNewVMPageBasic1::nextId() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled())
        return UIWizardNewVM::Page3;
    return UIWizardNewVM::Page2;
}

void UIWizardNewVMPage1::setTypeByISODetectedOSType(const QString &strDetectedOSType)
{
    if (!strDetectedOSType.isEmpty())
        onNameChanged(strDetectedOSType);
}

bool UIWizardNewVMPageBasic1::isComplete() const
{
    if (m_pNameAndSystemEditor->name().isEmpty())
        return false;
    return checkISOFile();
}

void UIWizardNewVMPageBasic1::sltNameChanged(const QString &strNewName)
{
    onNameChanged(strNewName);
    composeMachineFilePath();
}

void UIWizardNewVMPageBasic1::sltPathChanged(const QString &strNewPath)
{
    Q_UNUSED(strNewPath);
    composeMachineFilePath();
}

void UIWizardNewVMPageBasic1::sltOsTypeChanged()
{
    /* Call to base-class: */
    onOsTypeChanged();
}

void UIWizardNewVMPageBasic1::sltISOPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    setTypeByISODetectedOSType(m_strDetectedOSTypeId);
    emit completeChanged();
}

void UIWizardNewVMPageBasic1::sltUnattendedCheckBoxToggle()
{
    const bool fEnabled = m_pButtonUnattended->isChecked();
    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    if (m_pStartHeadlessLabel)
        m_pStartHeadlessLabel->setEnabled(fEnabled);
    if (m_pStartHeadlessCheckBox)
        m_pStartHeadlessCheckBox->setEnabled(fEnabled);
    emit completeChanged();
}


void UIWizardNewVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Virtual machine name and operating system"));

    if (m_pUnattendedLabel)
        m_pUnattendedLabel->setText(UIWizardNewVM::tr("Please choose whether you want to start an unattended guest os install "
                                             "in which case you will have to select a valid installation medium. If not "
                                             "your virtual disk will have an empty virtual hard disk. "
                                             "Additionally you can choose to start the unattended install as a headless vm process."));

    if (m_pButtonSimple)
    {
        m_pButtonSimple->setText(UIWizardNewVM::tr("Leave Disk Empty"));
        m_pButtonSimple->setToolTip(UIWizardNewVM::tr("When checked, no guest OS will be installed after this wizard is closed"));
    }
    if (m_pButtonUnattended)
    {
        m_pButtonUnattended->setText(UIWizardNewVM::tr("Unattended Install"));
        m_pButtonUnattended->setToolTip(UIWizardNewVM::tr("When checked, an unattended guest OS will be initialized after this wizard is closed"));
    }

    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setText(UIWizardNewVM::tr("Image:"));

    if (m_pStartHeadlessLabel)
        m_pStartHeadlessLabel->setText(UIWizardNewVM::tr("Options:"));
    if (m_pStartHeadlessCheckBox)
    {
        m_pStartHeadlessCheckBox->setText(UIWizardNewVM::tr("Start VM Headless"));
        m_pStartHeadlessCheckBox->setToolTip(UIWizardNewVM::tr("When checked, the unattended install will start the virtual machine headless"));
    }

    if (m_pNameOSTypeLabel)
        m_pNameOSTypeLabel->setText(UIWizardNewVM::tr("Please choose a descriptive name and destination folder for the new virtual machine "
                                             "and select the type of operating system you intend to install on it. "
                                             "The name you choose will be used throughout VirtualBox "
                                             "to identify this machine."));

    if (   m_pNameAndSystemEditor
        && m_pISOSelectorLabel
        && m_pStartHeadlessLabel)
    {
        int iMinWidthHint = 0;
        iMinWidthHint = qMax(iMinWidthHint, m_pISOSelectorLabel->minimumSizeHint().width());
        iMinWidthHint = qMax(iMinWidthHint, m_pStartHeadlessLabel->minimumSizeHint().width());
        m_pNameAndSystemEditor->setMinimumLayoutIndent(iMinWidthHint);
    }
}

void UIWizardNewVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
    if (m_pNameAndSystemEditor)
        m_pNameAndSystemEditor->setFocus();
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
