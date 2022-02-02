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
    { QRegExp( "(Wi.*11)|(W11)",                  Qt::CaseInsensitive), "Windows11_64" },
    { QRegExp(  "Wi.*3.*1",                       Qt::CaseInsensitive), "Windows31" },
    /* Set Windows 10 as default for "Windows". */
    { QRegExp(  "Wi.*64",                         Qt::CaseInsensitive), "Windows10_64" },
    { QRegExp(  "Wi.*32",                         Qt::CaseInsensitive), "Windows10" },
    /* ReactOS wants to be considered as Windows 2003 */
    { QRegExp(  "Reac.*",                         Qt::CaseInsensitive), "Windows2003" },

    /* Solaris: */
    { QRegExp("Sol.*11",                                                  Qt::CaseInsensitive), "Solaris11_64" },
    { QRegExp("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*64", Qt::CaseInsensitive), "OpenSolaris_64" },
    { QRegExp("((Op.*Sol)|(os20[01][0-9])|(Sol.*10)|(India)|(Neva)).*32", Qt::CaseInsensitive), "OpenSolaris" },
    { QRegExp("Sol.*64",                                                  Qt::CaseInsensitive), "Solaris_64" },
    { QRegExp("Sol.*32",                                                  Qt::CaseInsensitive), "Solaris" },

    /* OS/2: */
    { QRegExp("OS[/|!-]{,1}2.*W.*4.?5", Qt::CaseInsensitive), "OS2Warp45" },
    { QRegExp("OS[/|!-]{,1}2.*W.*4",    Qt::CaseInsensitive), "OS2Warp4" },
    { QRegExp("OS[/|!-]{,1}2.*W",       Qt::CaseInsensitive), "OS2Warp3" },
    { QRegExp("OS[/|!-]{,1}2.*e",       Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp("OS[/|!-]{,1}2.*Ar.*",    Qt::CaseInsensitive), "OS2ArcaOS" },
    { QRegExp("OS[/|!-]{,1}2",          Qt::CaseInsensitive), "OS2" },
    { QRegExp("(eComS.*|eCS.*)",        Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp("Arca.*",                 Qt::CaseInsensitive), "OS2ArcaOS" },

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
    { QRegExp("((Ub)|(Mint)).*64",                 Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("((Ub)|(Mint)).*32",                 Qt::CaseInsensitive), "Ubuntu" },
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
        UINotificationMessage::cannotRemoveMachineFolder(strMachineFolder, pWizard->notificationCenter());
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
        bool fMachineFolderRemoved = QDir().rmpath(strCreatedFolder);
        /* Reset machine folder value: */
        if (fMachineFolderRemoved)
            pWizard->setCreatedMachineFolder(QString());
        /* Return cleanup result: */
        return fMachineFolderRemoved;
    }
    return true;
}

void UIWizardNewVMNameOSTypeCommon::detectOSAndImagesFromISO(const QString &strISOPath, UIWizardNewVM *pWizard)
{
    if (!pWizard)
        return;

    QFileInfo isoFileInfo(strISOPath);
    if (!isoFileInfo.exists())
    {
        pWizard->setDetectedOSTypeId(QString());
        return;
    }

    CUnattended comUnatteded = uiCommon().virtualBox().CreateUnattendedInstaller();
    comUnatteded.SetIsoPath(strISOPath);
    comUnatteded.DetectIsoOS();
    pWizard->setDetectedOSTypeId(comUnatteded.GetDetectedOSTypeId());

    const QVector<ULONG> &indices = comUnatteded.GetDetectedImageIndices();
    QVector<ulong> qIndices;
    for (int i = 0; i < indices.size(); ++i)
        qIndices << indices[i];
    pWizard->setDetectedWindowsImageNamesAndIndices(comUnatteded.GetDetectedImageNames(), qIndices);
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
        m_pSkipUnattendedCheckBox->setToolTip(UIWizardNewVM::tr("<p>Disables the unattended install and just mount the ISO.</p>"));
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
            m_pNameAndSystemEditor->setFocus();
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

    if (m_pNameAndSystemEditor)
        m_pNameAndSystemEditor->setFocus();
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
    UIWizardNewVMNameOSTypeCommon::detectOSAndImagesFromISO(strPath, pWizard);

    if (UIWizardNewVMNameOSTypeCommon::guessOSTypeDetectedOSTypeString(m_pNameAndSystemEditor, pWizard->detectedOSTypeId()))
        m_userModifiedParameters << "GuestOSTypeFromISO";
    else /* Remove GuestOSTypeFromISO fromthe set if it is there: */
        m_userModifiedParameters.remove("GuestOSTypeFromISO");

    pWizard->setISOFilePath(strPath);

    /* Update the global recent ISO path: */
    QFileInfo fileInfo(strPath);
    if (fileInfo.exists() && fileInfo.isReadable())
        uiCommon().updateRecentlyUsedMediumListAndFolder(UIMediumDeviceType_DVD, strPath);
    setSkipCheckBoxEnable();
    emit completeChanged();
}

void UIWizardNewVMNameOSTypePage::sltGuestOSFamilyChanged(const QString &strGuestOSFamilyId)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setGuestOSFamilyId(strGuestOSFamilyId);
}

void UIWizardNewVMNameOSTypePage::sltSkipUnattendedInstallChanged(bool fSkip)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    m_userModifiedParameters << "SkipUnattendedInstall";
    wizardWindow<UIWizardNewVM>()->setSkipUnattendedInstall(fSkip);
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
    if (!isOSTypeDetectionOK())
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

bool UIWizardNewVMNameOSTypePage::isOSTypeDetectionOK() const
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);
    return pWizard->isOSTypeDetectionOK();
}
