/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicNameOSStype class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <QDir>
#include <QRegularExpression>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UINameAndSystemEditor.h"
#include "UINotificationCenter.h"
#include "UIWizardNewVMNameOSTypePage.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CUnattended.h"

/* Defines some patterns to guess the right OS type. Should be in sync with
 * VirtualBox-settings-common.xsd in Main. The list is sorted by priority. The
 * first matching string found, will be used. */
struct osTypePattern
{
    QRegularExpression pattern;
    const char *pcstId;
};

static const osTypePattern gs_OSTypePattern[] =
{
    /* DOS: */
    { QRegularExpression("DOS", QRegularExpression::CaseInsensitiveOption), "DOS" },

    /* Windows: */
    { QRegularExpression(  "Wi.*98",                         QRegularExpression::CaseInsensitiveOption), "Windows98" },
    { QRegularExpression(  "Wi.*95",                         QRegularExpression::CaseInsensitiveOption), "Windows95" },
    { QRegularExpression(  "Wi.*Me",                         QRegularExpression::CaseInsensitiveOption), "WindowsMe" },
    { QRegularExpression( "(Wi.*NT)|(NT[-._v]*4)",           QRegularExpression::CaseInsensitiveOption), "WindowsNT4" },
    { QRegularExpression( "NT[-._v]*3[.,]*[51x]",            QRegularExpression::CaseInsensitiveOption), "WindowsNT3x" },
    /* Note: Do not automatically set WindowsXP_64 on 64-bit hosts, as Windows XP 64-bit
     *       is extremely rare -- most users never heard of it even. So always default to 32-bit. */
    { QRegularExpression("((Wi.*XP)|(XP)).*",                QRegularExpression::CaseInsensitiveOption), "WindowsXP" },
    { QRegularExpression("((Wi.*2003)|(W2K3)|(Win2K3)).*64", QRegularExpression::CaseInsensitiveOption), "Windows2003_64" },
    { QRegularExpression("((Wi.*2003)|(W2K3)|(Win2K3)).*32", QRegularExpression::CaseInsensitiveOption), "Windows2003" },
    { QRegularExpression("((Wi.*Vis)|(Vista)).*64",          QRegularExpression::CaseInsensitiveOption), "WindowsVista_64" },
    { QRegularExpression("((Wi.*Vis)|(Vista)).*32",          QRegularExpression::CaseInsensitiveOption), "WindowsVista" },
    { QRegularExpression( "(Wi.*2016)|(W2K16)|(Win2K16)",    QRegularExpression::CaseInsensitiveOption), "Windows2016_64" },
    { QRegularExpression( "(Wi.*2012)|(W2K12)|(Win2K12)",    QRegularExpression::CaseInsensitiveOption), "Windows2012_64" },
    { QRegularExpression("((Wi.*2008)|(W2K8)|(Win2k8)).*64", QRegularExpression::CaseInsensitiveOption), "Windows2008_64" },
    { QRegularExpression("((Wi.*2008)|(W2K8)|(Win2K8)).*32", QRegularExpression::CaseInsensitiveOption), "Windows2008" },
    { QRegularExpression( "(Wi.*2000)|(W2K)|(Win2K)",        QRegularExpression::CaseInsensitiveOption), "Windows2000" },
    { QRegularExpression( "(Wi.*7.*64)|(W7.*64)",            QRegularExpression::CaseInsensitiveOption), "Windows7_64" },
    { QRegularExpression( "(Wi.*7.*32)|(W7.*32)",            QRegularExpression::CaseInsensitiveOption), "Windows7" },
    { QRegularExpression( "(Wi.*8.*1.*64)|(W8.*64)",         QRegularExpression::CaseInsensitiveOption), "Windows81_64" },
    { QRegularExpression( "(Wi.*8.*1.*32)|(W8.*32)",         QRegularExpression::CaseInsensitiveOption), "Windows81" },
    { QRegularExpression( "(Wi.*8.*64)|(W8.*64)",            QRegularExpression::CaseInsensitiveOption), "Windows8_64" },
    { QRegularExpression( "(Wi.*8.*32)|(W8.*32)",            QRegularExpression::CaseInsensitiveOption), "Windows8" },
    { QRegularExpression( "(Wi.*10.*64)|(W10.*64)",          QRegularExpression::CaseInsensitiveOption), "Windows10_64" },
    { QRegularExpression( "(Wi.*10.*32)|(W10.*32)",          QRegularExpression::CaseInsensitiveOption), "Windows10" },
    { QRegularExpression( "(Wi.*11)|(W11)",                  QRegularExpression::CaseInsensitiveOption), "Windows11_64" },
    { QRegularExpression(  "Wi.*3.*1",                       QRegularExpression::CaseInsensitiveOption), "Windows31" },
    /* Set Windows 10 as default for "Windows". */
    { QRegularExpression(  "Wi.*64",                         QRegularExpression::CaseInsensitiveOption), "Windows10_64" },
    { QRegularExpression(  "Wi.*32",                         QRegularExpression::CaseInsensitiveOption), "Windows10" },
    /* ReactOS wants to be considered as Windows 2003 */
    { QRegularExpression(  "Reac.*",                         QRegularExpression::CaseInsensitiveOption), "Windows2003" },

    /* Solaris: */
    { QRegularExpression("Sol.*11",                                                  QRegularExpression::CaseInsensitiveOption), "Solaris11_64" },
    { QRegularExpression("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*64", QRegularExpression::CaseInsensitiveOption), "OpenSolaris_64" },
    { QRegularExpression("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*32", QRegularExpression::CaseInsensitiveOption), "OpenSolaris" },
    { QRegularExpression("Sol.*64",                                                  QRegularExpression::CaseInsensitiveOption), "Solaris_64" },
    { QRegularExpression("Sol.*32",                                                  QRegularExpression::CaseInsensitiveOption), "Solaris" },

    /* OS/2: */
    { QRegularExpression("OS[/|!-]{,1}2.*W.*4.?5", QRegularExpression::CaseInsensitiveOption), "OS2Warp45" },
    { QRegularExpression("OS[/|!-]{,1}2.*W.*4",    QRegularExpression::CaseInsensitiveOption), "OS2Warp4" },
    { QRegularExpression("OS[/|!-]{,1}2.*W",       QRegularExpression::CaseInsensitiveOption), "OS2Warp3" },
    { QRegularExpression("OS[/|!-]{,1}2.*e",       QRegularExpression::CaseInsensitiveOption), "OS2eCS" },
    { QRegularExpression("OS[/|!-]{,1}2.*Ar.*",    QRegularExpression::CaseInsensitiveOption), "OS2ArcaOS" },
    { QRegularExpression("OS[/|!-]{,1}2",          QRegularExpression::CaseInsensitiveOption), "OS2" },
    { QRegularExpression("(eComS.*|eCS.*)",        QRegularExpression::CaseInsensitiveOption), "OS2eCS" },
    { QRegularExpression("Arca.*",                 QRegularExpression::CaseInsensitiveOption), "OS2ArcaOS" },

    /* Other: Must come before Ubuntu/Maverick and before Linux??? */
    { QRegularExpression("QN", QRegularExpression::CaseInsensitiveOption), "QNX" },

    /* Mac OS X: Must come before Ubuntu/Maverick and before Linux: */
    { QRegularExpression("((mac.*10[.,]{0,1}4)|(os.*x.*10[.,]{0,1}4)|(mac.*ti)|(os.*x.*ti)|(Tig)).64",     QRegularExpression::CaseInsensitiveOption), "MacOS_64" },
    { QRegularExpression("((mac.*10[.,]{0,1}4)|(os.*x.*10[.,]{0,1}4)|(mac.*ti)|(os.*x.*ti)|(Tig)).32",     QRegularExpression::CaseInsensitiveOption), "MacOS" },
    { QRegularExpression("((mac.*10[.,]{0,1}5)|(os.*x.*10[.,]{0,1}5)|(mac.*leo)|(os.*x.*leo)|(Leop)).*64", QRegularExpression::CaseInsensitiveOption), "MacOS_64" },
    { QRegularExpression("((mac.*10[.,]{0,1}5)|(os.*x.*10[.,]{0,1}5)|(mac.*leo)|(os.*x.*leo)|(Leop)).*32", QRegularExpression::CaseInsensitiveOption), "MacOS" },
    { QRegularExpression("((mac.*10[.,]{0,1}6)|(os.*x.*10[.,]{0,1}6)|(mac.*SL)|(os.*x.*SL)|(Snow L)).*64", QRegularExpression::CaseInsensitiveOption), "MacOS106_64" },
    { QRegularExpression("((mac.*10[.,]{0,1}6)|(os.*x.*10[.,]{0,1}6)|(mac.*SL)|(os.*x.*SL)|(Snow L)).*32", QRegularExpression::CaseInsensitiveOption), "MacOS106" },
    { QRegularExpression( "(mac.*10[.,]{0,1}7)|(os.*x.*10[.,]{0,1}7)|(mac.*ML)|(os.*x.*ML)|(Mount)",       QRegularExpression::CaseInsensitiveOption), "MacOS108_64" },
    { QRegularExpression( "(mac.*10[.,]{0,1}8)|(os.*x.*10[.,]{0,1}8)|(Lion)",                              QRegularExpression::CaseInsensitiveOption), "MacOS107_64" },
    { QRegularExpression( "(mac.*10[.,]{0,1}9)|(os.*x.*10[.,]{0,1}9)|(mac.*mav)|(os.*x.*mav)|(Mavericks)", QRegularExpression::CaseInsensitiveOption), "MacOS109_64" },
    { QRegularExpression( "(mac.*yos)|(os.*x.*yos)|(Yosemite)",                                            QRegularExpression::CaseInsensitiveOption), "MacOS1010_64" },
    { QRegularExpression( "(mac.*cap)|(os.*x.*capit)|(Capitan)",                                           QRegularExpression::CaseInsensitiveOption), "MacOS1011_64" },
    { QRegularExpression( "(mac.*hig)|(os.*x.*high.*sierr)|(High Sierra)",                                 QRegularExpression::CaseInsensitiveOption), "MacOS1013_64" },
    { QRegularExpression( "(mac.*sie)|(os.*x.*sierr)|(Sierra)",                                            QRegularExpression::CaseInsensitiveOption), "MacOS1012_64" },
    { QRegularExpression("((Mac)|(Tig)|(Leop)|(Yose)|(os[ ]*x)).*64",                                      QRegularExpression::CaseInsensitiveOption), "MacOS_64" },
    { QRegularExpression("((Mac)|(Tig)|(Leop)|(Yose)|(os[ ]*x)).*32",                                      QRegularExpression::CaseInsensitiveOption), "MacOS" },

    /* Code names for Linux distributions: */
    { QRegularExpression("((bianca)|(cassandra)|(celena)|(daryna)|(elyssa)|(felicia)|(gloria)|(helena)|(isadora)|(julia)|(katya)|(lisa)|(maya)|(nadia)|(olivia)|(petra)|(qiana)|(rebecca)|(rafaela)|(rosa)).*64", QRegularExpression::CaseInsensitiveOption), "Ubuntu_64" },
    { QRegularExpression("((bianca)|(cassandra)|(celena)|(daryna)|(elyssa)|(felicia)|(gloria)|(helena)|(isadora)|(julia)|(katya)|(lisa)|(maya)|(nadia)|(olivia)|(petra)|(qiana)|(rebecca)|(rafaela)|(rosa)).*32", QRegularExpression::CaseInsensitiveOption), "Ubuntu" },
    { QRegularExpression("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)|(quantal)|(raring)|(saucy)|(trusty)|(utopic)|(vivid)|(wily)|(xenial)|(yakkety)|(zesty)).*64", QRegularExpression::CaseInsensitiveOption), "Ubuntu_64" },
    { QRegularExpression("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)|(precise)|(quantal)|(raring)|(saucy)|(trusty)|(utopic)|(vivid)|(wily)|(xenial)|(yakkety)|(zesty)).*32", QRegularExpression::CaseInsensitiveOption), "Ubuntu" },
    { QRegularExpression("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(jessie)|(stretch)|(buster)|(sid)).*64", QRegularExpression::CaseInsensitiveOption), "Debian_64" },
    { QRegularExpression("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(jessie)|(stretch)|(buster)|(sid)).*32", QRegularExpression::CaseInsensitiveOption), "Debian" },
    { QRegularExpression("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)|(beefy)|(spherical)).*64", QRegularExpression::CaseInsensitiveOption), "Fedora_64" },
    { QRegularExpression("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)|(verne)|(beefy)|(spherical)).*32", QRegularExpression::CaseInsensitiveOption), "Fedora" },

    /* Regular names of Linux distributions: */
    { QRegularExpression("Arc.*64",                           QRegularExpression::CaseInsensitiveOption), "ArchLinux_64" },
    { QRegularExpression("Arc.*32",                           QRegularExpression::CaseInsensitiveOption), "ArchLinux" },
    { QRegularExpression("Deb.*64",                           QRegularExpression::CaseInsensitiveOption), "Debian_64" },
    { QRegularExpression("Deb.*32",                           QRegularExpression::CaseInsensitiveOption), "Debian" },
    { QRegularExpression("((SU)|(Nov)|(SLE)).*64",            QRegularExpression::CaseInsensitiveOption), "OpenSUSE_64" },
    { QRegularExpression("((SU)|(Nov)|(SLE)).*32",            QRegularExpression::CaseInsensitiveOption), "OpenSUSE" },
    { QRegularExpression("Fe.*64",                            QRegularExpression::CaseInsensitiveOption), "Fedora_64" },
    { QRegularExpression("Fe.*32",                            QRegularExpression::CaseInsensitiveOption), "Fedora" },
    { QRegularExpression("((Gen)|(Sab)).*64",                 QRegularExpression::CaseInsensitiveOption), "Gentoo_64" },
    { QRegularExpression("((Gen)|(Sab)).*32",                 QRegularExpression::CaseInsensitiveOption), "Gentoo" },
    { QRegularExpression("((Man)|(Mag)).*64",                 QRegularExpression::CaseInsensitiveOption), "Mandriva_64" },
    { QRegularExpression("((Man)|(Mag)).*32",                 QRegularExpression::CaseInsensitiveOption), "Mandriva" },
    { QRegularExpression("((Red)|(rhel)|(cen)).*64",          QRegularExpression::CaseInsensitiveOption), "RedHat_64" },
    { QRegularExpression("((Red)|(rhel)|(cen)).*32",          QRegularExpression::CaseInsensitiveOption), "RedHat" },
    { QRegularExpression("Tur.*64",                           QRegularExpression::CaseInsensitiveOption), "Turbolinux_64" },
    { QRegularExpression("Tur.*32",                           QRegularExpression::CaseInsensitiveOption), "Turbolinux" },
    { QRegularExpression("((Ub)|(Mint)).*64",                 QRegularExpression::CaseInsensitiveOption), "Ubuntu_64" },
    { QRegularExpression("((Ub)|(Mint)).*32",                 QRegularExpression::CaseInsensitiveOption), "Ubuntu" },
    { QRegularExpression("Xa.*64",                            QRegularExpression::CaseInsensitiveOption), "Xandros_64" },
    { QRegularExpression("Xa.*32",                            QRegularExpression::CaseInsensitiveOption), "Xandros" },
    { QRegularExpression("((Or)|(oel)|(ol)).*64",             QRegularExpression::CaseInsensitiveOption), "Oracle_64" },
    { QRegularExpression("((Or)|(oel)|(ol)).*32",             QRegularExpression::CaseInsensitiveOption), "Oracle" },
    { QRegularExpression("Knoppix",                           QRegularExpression::CaseInsensitiveOption), "Linux26" },
    { QRegularExpression("Dsl",                               QRegularExpression::CaseInsensitiveOption), "Linux24" },
    { QRegularExpression("((Lin)|(lnx)).*2.?2",               QRegularExpression::CaseInsensitiveOption), "Linux22" },
    { QRegularExpression("((Lin)|(lnx)).*2.?4.*64",           QRegularExpression::CaseInsensitiveOption), "Linux24_64" },
    { QRegularExpression("((Lin)|(lnx)).*2.?4.*32",           QRegularExpression::CaseInsensitiveOption), "Linux24" },
    { QRegularExpression("((((Lin)|(lnx)).*2.?6)|(LFS)).*64", QRegularExpression::CaseInsensitiveOption), "Linux26_64" },
    { QRegularExpression("((((Lin)|(lnx)).*2.?6)|(LFS)).*32", QRegularExpression::CaseInsensitiveOption), "Linux26" },
    { QRegularExpression("((Lin)|(lnx)).*64",                 QRegularExpression::CaseInsensitiveOption), "Linux26_64" },
    { QRegularExpression("((Lin)|(lnx)).*32",                 QRegularExpression::CaseInsensitiveOption), "Linux26" },

    /* Other: */
    { QRegularExpression("L4",                   QRegularExpression::CaseInsensitiveOption), "L4" },
    { QRegularExpression("((Fr.*B)|(fbsd)).*64", QRegularExpression::CaseInsensitiveOption), "FreeBSD_64" },
    { QRegularExpression("((Fr.*B)|(fbsd)).*32", QRegularExpression::CaseInsensitiveOption), "FreeBSD" },
    { QRegularExpression("Op.*B.*64",            QRegularExpression::CaseInsensitiveOption), "OpenBSD_64" },
    { QRegularExpression("Op.*B.*32",            QRegularExpression::CaseInsensitiveOption), "OpenBSD" },
    { QRegularExpression("Ne.*B.*64",            QRegularExpression::CaseInsensitiveOption), "NetBSD_64" },
    { QRegularExpression("Ne.*B.*32",            QRegularExpression::CaseInsensitiveOption), "NetBSD" },
    { QRegularExpression("Net",                  QRegularExpression::CaseInsensitiveOption), "Netware" },
    { QRegularExpression("Rocki",                QRegularExpression::CaseInsensitiveOption), "JRockitVE" },
    { QRegularExpression("bs[23]{0,1}-",         QRegularExpression::CaseInsensitiveOption), "VBoxBS_64" }, /* bootsector tests */
    { QRegularExpression("Ot",                   QRegularExpression::CaseInsensitiveOption), "Other" },
};

bool UIWizardNewVMNameOSTypeCommon::guessOSTypeFromName(UINameAndSystemEditor *pNameAndSystemEditor, QString strNewName)
{
    AssertReturn(pNameAndSystemEditor, false);
    /* Append default architecture bit-count (64/32) if not already in the name: */
    if (!strNewName.contains("32") && !strNewName.contains("64"))
    {
        /** @todo cache this result, no need to re-query it for each keystroke... */
        CHost host = uiCommon().host();
        bool fSupportsHWVirtEx = host.GetProcessorFeature(KProcessorFeature_HWVirtEx);
        bool fSupportsLongMode = host.GetProcessorFeature(KProcessorFeature_LongMode);
        strNewName += ARCH_BITS == 64 && fSupportsHWVirtEx && fSupportsLongMode ? "64" : "32";
    }

    /* Search for a matching OS type based on the string the user typed already. */
    for (size_t i = 0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
    {
        if (strNewName.contains(gs_OSTypePattern[i].pattern))
        {
            pNameAndSystemEditor->setType(uiCommon().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            return true;
        }
    }
    return false;
}

bool UIWizardNewVMNameOSTypeCommon::guessOSTypeDetectedOSTypeString(UINameAndSystemEditor *pNameAndSystemEditor, QString strDetectedOSType)
{
    AssertReturn(pNameAndSystemEditor, false);
    if (strDetectedOSType.isEmpty())
    {
        pNameAndSystemEditor->setType(uiCommon().vmGuestOSType("Other"));
        /* Return false to allow OS type guessing from name. See caller code: */
        return false;
    }
    /* Append 32 as bit-count if the name has no 64 and 32 in the name since API returns a type name with no arch bit count for 32-bit OSs: */
    if (!strDetectedOSType.contains("32") && !strDetectedOSType.contains("64"))
        strDetectedOSType += "32";

    /* Search for a matching OS type based on the string the user typed already. */
    for (size_t i = 0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
    {
        if (strDetectedOSType.contains(gs_OSTypePattern[i].pattern))
        {

            pNameAndSystemEditor->setType(uiCommon().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            return true;
        }
    }
    return false;
}

void UIWizardNewVMNameOSTypeCommon::composeMachineFilePath(UINameAndSystemEditor *pNameAndSystemEditor,
                                                           UIWizardNewVM *pWizard)
{
    if (!pNameAndSystemEditor || !pWizard)
        return;
    if (pNameAndSystemEditor->name().isEmpty() || pNameAndSystemEditor->path().isEmpty())
        return;
    /* Get VBox: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* Compose machine filename: */
    pWizard->setMachineFilePath(vbox.ComposeMachineFilename(pNameAndSystemEditor->name(),
                                                            pWizard->machineGroup(),
                                                            QString(),
                                                            pNameAndSystemEditor->path()));
    /* Compose machine folder/basename: */
    const QFileInfo fileInfo(pWizard->machineFilePath());
    pWizard->setMachineFolder(fileInfo.absolutePath());
    pWizard->setMachineBaseName(pNameAndSystemEditor->name());
}

bool UIWizardNewVMNameOSTypeCommon::createMachineFolder(UINameAndSystemEditor *pNameAndSystemEditor,
                                                        UIWizardNewVM *pWizard)
{
    if (!pNameAndSystemEditor || !pWizard)
        return false;
    const QString &strMachineFolder = pWizard->machineFolder();
    const QString &strCreatedFolder = pWizard->createdMachineFolder();

    /* Cleanup previosly created folder if any: */
    if (!cleanupMachineFolder(pWizard))
    {
        UINotificationMessage::cannotRemoveMachineFolder(strCreatedFolder, pWizard->notificationCenter());
        return false;
    }

    /* Check if the folder already exists and check if it has been created by this wizard */
    if (QDir(strMachineFolder).exists())
    {
        /* Looks like we have already created this folder for this run of the wizard. Just return */
        if (strCreatedFolder == strMachineFolder)
            return true;
        /* The folder is there but not because of this wizard. Avoid overwriting a existing machine's folder */
        else
        {
            UINotificationMessage::cannotOverwriteMachineFolder(strMachineFolder, pWizard->notificationCenter());
            return false;
        }
    }

    /* Try to create new folder (and it's predecessors): */
    bool fMachineFolderCreated = QDir().mkpath(strMachineFolder);
    if (!fMachineFolderCreated)
    {
        UINotificationMessage::cannotCreateMachineFolder(strMachineFolder, pWizard->notificationCenter());
        return false;
    }
    pWizard->setCreatedMachineFolder(strMachineFolder);
    return true;
}

bool UIWizardNewVMNameOSTypeCommon::cleanupMachineFolder(UIWizardNewVM *pWizard, bool fWizardCancel /* = false */)
{
    if (!pWizard)
        return false;
    const QString &strMachineFolder = pWizard->machineFolder();
    const QString &strCreatedFolder = pWizard->createdMachineFolder();
    /* Make sure folder was previosly created: */
    if (strCreatedFolder.isEmpty())
        return true;
    /* Clean this folder if the machine folder has been changed by the user or we are cancelling the wizard: */
    if (strCreatedFolder != strMachineFolder || fWizardCancel)
    {
        /* Try to cleanup folder (and it's predecessors): */
        bool fMachineFolderRemoved = QDir(strCreatedFolder).removeRecursively();
        /* Reset machine folder value: */
        if (fMachineFolderRemoved)
            pWizard->setCreatedMachineFolder(QString());
        /* Return cleanup result: */
        return fMachineFolderRemoved;
    }
    return true;
}

bool UIWizardNewVMNameOSTypeCommon::checkISOFile(UINameAndSystemEditor *pNameAndSystemEditor)
{
    if (!pNameAndSystemEditor)
        return false;
    const QString &strPath = pNameAndSystemEditor->ISOImagePath();
    if (strPath.isNull() || strPath.isEmpty())
        return true;
    QFileInfo fileInfo(strPath);
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return false;
    return true;
}

UIWizardNewVMNameOSTypePage::UIWizardNewVMNameOSTypePage()
    : m_pNameAndSystemLayout(0)
    , m_pNameAndSystemEditor(0)
    , m_pSkipUnattendedCheckBox(0)
    , m_pNameOSTypeLabel(0)
{
    prepare();
}

void UIWizardNewVMNameOSTypePage::prepare()
{
    QVBoxLayout *pPageLayout = new QVBoxLayout(this);
    if (pPageLayout)
    {
        m_pNameOSTypeLabel = new QIRichTextLabel(this);
        if (m_pNameOSTypeLabel)
            pPageLayout->addWidget(m_pNameOSTypeLabel);

        /* Prepare Name and OS Type editor: */
        pPageLayout->addWidget(createNameOSTypeWidgets());

        pPageLayout->addStretch();
    }

    createConnections();
}

void UIWizardNewVMNameOSTypePage::createConnections()
{
    if (m_pNameAndSystemEditor)
    {
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigNameChanged, this, &UIWizardNewVMNameOSTypePage::sltNameChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigPathChanged, this, &UIWizardNewVMNameOSTypePage::sltPathChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOsTypeChanged, this, &UIWizardNewVMNameOSTypePage::sltOsTypeChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigImageChanged, this, &UIWizardNewVMNameOSTypePage::sltISOPathChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOSFamilyChanged, this, &UIWizardNewVMNameOSTypePage::sltGuestOSFamilyChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigEditionChanged, this, &UIWizardNewVMNameOSTypePage::sltSelectedEditionChanged);
    }
    if (m_pSkipUnattendedCheckBox)
        connect(m_pSkipUnattendedCheckBox, &QCheckBox::toggled, this, &UIWizardNewVMNameOSTypePage::sltSkipUnattendedInstallChanged);
}

bool UIWizardNewVMNameOSTypePage::isComplete() const
{
    markWidgets();
    if (m_pNameAndSystemEditor->name().isEmpty())
        return false;
    return UIWizardNewVMNameOSTypeCommon::checkISOFile(m_pNameAndSystemEditor);
}

void UIWizardNewVMNameOSTypePage::sltNameChanged(const QString &strNewName)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    if (!m_userModifiedParameters.contains("GuestOSTypeFromISO"))
    {
        m_pNameAndSystemEditor->blockSignals(true);
        if (UIWizardNewVMNameOSTypeCommon::guessOSTypeFromName(m_pNameAndSystemEditor, strNewName))
        {
            wizardWindow<UIWizardNewVM>()->setGuestOSType(m_pNameAndSystemEditor->type());
            m_userModifiedParameters << "GuestOSTypeFromName";
        }
        m_pNameAndSystemEditor->blockSignals(false);
    }
    UIWizardNewVMNameOSTypeCommon::composeMachineFilePath(m_pNameAndSystemEditor, wizardWindow<UIWizardNewVM>());
    emit completeChanged();
}

void UIWizardNewVMNameOSTypePage::sltPathChanged(const QString &strNewPath)
{
    Q_UNUSED(strNewPath);
    UIWizardNewVMNameOSTypeCommon::composeMachineFilePath(m_pNameAndSystemEditor, wizardWindow<UIWizardNewVM>());
}

void UIWizardNewVMNameOSTypePage::sltOsTypeChanged()
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    //m_userModifiedParameters << "GuestOSType";
    if (m_pNameAndSystemEditor)
        wizardWindow<UIWizardNewVM>()->setGuestOSType(m_pNameAndSystemEditor->type());
}

void UIWizardNewVMNameOSTypePage::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Virtual machine Name and Operating System"));

    if (m_pNameOSTypeLabel)
        m_pNameOSTypeLabel->setText(UIWizardNewVM::tr("Please choose a descriptive name and destination folder for the new "
                                                      "virtual machine. The name you choose will be used throughout VirtualBox "
                                                      "to identify this machine."));

    if (m_pSkipUnattendedCheckBox)
    {
        m_pSkipUnattendedCheckBox->setText(UIWizardNewVM::tr("&Skip Unattended Installation"));
        m_pSkipUnattendedCheckBox->setToolTip(UIWizardNewVM::tr("When checked, the unattended install is disabled and the selected ISO "
                                                                "is mounted on the vm."));
    }

    if (m_pNameAndSystemLayout && m_pNameAndSystemEditor)
        m_pNameAndSystemLayout->setColumnMinimumWidth(0, m_pNameAndSystemEditor->firstColumnWidth());
}

void UIWizardNewVMNameOSTypePage::initializePage()
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);

    retranslateUi();

    /* Initialize this page's widgets etc: */
    {
        if (m_pNameAndSystemEditor)
        {
            m_pNameAndSystemEditor->setFocus();
            m_pNameAndSystemEditor->setEditionSelectorEnabled(false);
        }
        setSkipCheckBoxEnable();
    }

    /* Initialize some of the wizard's parameters: */
    {
        if (m_pNameAndSystemEditor)
        {
            pWizard->setGuestOSFamilyId(m_pNameAndSystemEditor->familyId());
            pWizard->setGuestOSType(m_pNameAndSystemEditor->type());
            /* Vm name, folder, file path etc. will be initilized by composeMachineFilePath: */
        }
    }
}

bool UIWizardNewVMNameOSTypePage::validatePage()
{
    /* Try to create machine folder: */
    return UIWizardNewVMNameOSTypeCommon::createMachineFolder(m_pNameAndSystemEditor, wizardWindow<UIWizardNewVM>());
}

void UIWizardNewVMNameOSTypePage::sltISOPathChanged(const QString &strPath)
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(this->wizard());
    AssertReturnVoid(pWizard);

    pWizard->setISOFilePath(strPath);

    if (UIWizardNewVMNameOSTypeCommon::guessOSTypeDetectedOSTypeString(m_pNameAndSystemEditor, pWizard->detectedOSTypeId()))
        m_userModifiedParameters << "GuestOSTypeFromISO";
    else /* Remove GuestOSTypeFromISO fromthe set if it is there: */
        m_userModifiedParameters.remove("GuestOSTypeFromISO");

    /* Update the global recent ISO path: */
    QFileInfo fileInfo(strPath);
    if (fileInfo.exists() && fileInfo.isReadable())
        uiCommon().updateRecentlyUsedMediumListAndFolder(UIMediumDeviceType_DVD, strPath);

    /* Populate the editions selector: */
    if (m_pNameAndSystemEditor)
         m_pNameAndSystemEditor->setEditionNameAndIndices(pWizard->detectedWindowsImageNames(),
                                                         pWizard->detectedWindowsImageIndices());

    setSkipCheckBoxEnable();
    setEditionSelectorEnabled();
    emit completeChanged();
}

void UIWizardNewVMNameOSTypePage::sltGuestOSFamilyChanged(const QString &strGuestOSFamilyId)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setGuestOSFamilyId(strGuestOSFamilyId);
}

void UIWizardNewVMNameOSTypePage::sltSelectedEditionChanged(ulong uEditionIndex)
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);
    pWizard->setSelectedWindowImageIndex(uEditionIndex);
    /* Update the OS type since IUnattended updates the detected OS type after edition (image index) changes: */
    UIWizardNewVMNameOSTypeCommon::guessOSTypeDetectedOSTypeString(m_pNameAndSystemEditor, pWizard->detectedOSTypeId());
}

void UIWizardNewVMNameOSTypePage::sltSkipUnattendedInstallChanged(bool fSkip)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    m_userModifiedParameters << "SkipUnattendedInstall";
    wizardWindow<UIWizardNewVM>()->setSkipUnattendedInstall(fSkip);
    setEditionSelectorEnabled();
}

QWidget *UIWizardNewVMNameOSTypePage::createNameOSTypeWidgets()
{
    /* Prepare container widget: */
    QWidget *pContainerWidget = new QWidget;
    if (pContainerWidget)
    {
        /* Prepare layout: */
        m_pNameAndSystemLayout = new QGridLayout(pContainerWidget);
        if (m_pNameAndSystemLayout)
        {
            m_pNameAndSystemLayout->setContentsMargins(0, 0, 0, 0);

            /* Prepare Name and OS Type editor: */
            m_pNameAndSystemEditor = new UINameAndSystemEditor(0,
                                                               true /* fChooseName? */,
                                                               true /* fChoosePath? */,
                                                               true /* fChooseImage? */,
                                                               true /* fChooseEdition? */,
                                                               true /* fChooseType? */);
            if (m_pNameAndSystemEditor)
                m_pNameAndSystemLayout->addWidget(m_pNameAndSystemEditor, 0, 0, 1, 2);

            /* Prepare Skip Unattended checkbox: */
            m_pSkipUnattendedCheckBox = new QCheckBox;
            if (m_pSkipUnattendedCheckBox)
                m_pNameAndSystemLayout->addWidget(m_pSkipUnattendedCheckBox, 1, 1);
        }
    }

    /* Return container widget: */
    return pContainerWidget;
}

void UIWizardNewVMNameOSTypePage::markWidgets() const
{
    if (m_pNameAndSystemEditor)
    {
        m_pNameAndSystemEditor->markNameEditor(m_pNameAndSystemEditor->name().isEmpty());
        m_pNameAndSystemEditor->markImageEditor(!UIWizardNewVMNameOSTypeCommon::checkISOFile(m_pNameAndSystemEditor),
                                                UIWizardNewVM::tr("Invalid file path or unreadable file"));
    }
}

void UIWizardNewVMNameOSTypePage::setSkipCheckBoxEnable()
{
    AssertReturnVoid(m_pSkipUnattendedCheckBox && m_pNameAndSystemEditor);
    const QString &strPath = m_pNameAndSystemEditor->ISOImagePath();
    if (strPath.isEmpty())
    {
        m_pSkipUnattendedCheckBox->setEnabled(false);
        return;
    }
    if (!isUnattendedInstallSupported())
    {
        m_pSkipUnattendedCheckBox->setEnabled(false);
        return;
    }

    m_pSkipUnattendedCheckBox->setEnabled(UIWizardNewVMNameOSTypeCommon::checkISOFile(m_pNameAndSystemEditor));
}

bool UIWizardNewVMNameOSTypePage::isUnattendedEnabled() const
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);
    return pWizard->isUnattendedEnabled();
}

bool UIWizardNewVMNameOSTypePage::isUnattendedInstallSupported() const
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);
    return pWizard->isUnattendedInstallSupported();
}


void  UIWizardNewVMNameOSTypePage::setEditionSelectorEnabled()
{
    if (!m_pNameAndSystemEditor || !m_pSkipUnattendedCheckBox)
        return;
    m_pNameAndSystemEditor->setEditionSelectorEnabled(   !m_pNameAndSystemEditor->isEditionsSelectorEmpty()
                                                      && !m_pSkipUnattendedCheckBox->isChecked());
}
