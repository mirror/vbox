/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewVMWzd class implementation
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

/* Global includes */
#include <QDir>

/* Local includes */
#include "UIIconPool.h"
#include "UINewHDWizard.h"
#include "UINewVMWzd.h"
#include "QIFileDialog.h"
#include "UIMessageCenter.h"
#include "UIMachineSettingsStorage.h"
#include "VBoxDefs.h"

/* Using declarations: */
using namespace VBoxGlobalDefs;

/* Globals */
struct osTypePattern
{
    QRegExp pattern;
    const char *pcstId;
};

/* Defines some patterns to guess the right OS type. Should be in sync with
 * VirtualBox-settings-common.xsd in Main. The list is sorted by priority. The
 * first matching string found, will be used. */
static const osTypePattern gs_OSTypePattern[] =
{
    { QRegExp("DOS", Qt::CaseInsensitive), "DOS" },

    /* Windows */
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

    /* Solaris */
    { QRegExp("So.*11", Qt::CaseInsensitive), "Solaris11_64" },
    { QRegExp("((Op.*So)|(os20[01][0-9])|(So.*10)|(India)|(Neva)).*64", Qt::CaseInsensitive), "OpenSolaris_64" },
    { QRegExp("(Op.*So)|(os20[01][0-9])|(So.*10)|(India)|(Neva)", Qt::CaseInsensitive), "OpenSolaris" },
    { QRegExp("So.*64", Qt::CaseInsensitive), "Solaris_64" },
    { QRegExp("So", Qt::CaseInsensitive), "Solaris" },

    /* OS/2 */
    { QRegExp("OS[/|!-]{,1}2.*W.*4.?5", Qt::CaseInsensitive), "OS2Warp45" },
    { QRegExp("OS[/|!-]{,1}2.*W.*4", Qt::CaseInsensitive), "OS2Warp4" },
    { QRegExp("OS[/|!-]{,1}2.*W", Qt::CaseInsensitive), "OS2Warp3" },
    { QRegExp("(OS[/|!-]{,1}2.*e)|(eCS.*)", Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp("OS[/|!-]{,1}2", Qt::CaseInsensitive), "OS2" },

    /* Code names for Linux distributions */
    { QRegExp("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)).*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("(edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)|(maverick)|(natty)|(oneiric)", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("((sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(sid)).*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("(sarge)|(etch)|(lenny)|(squeeze)|(wheezy)|(sid)", Qt::CaseInsensitive), "Debian" },
    { QRegExp("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)).*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("(moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)|(laughlin)|(lovelock)", Qt::CaseInsensitive), "Fedora" },

    /* Regular names of Linux distributions */
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

    /* Other */
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

UINewVMWzd::UINewVMWzd(QWidget *pParent) : QIWizard(pParent)
{
    /* Create & add pages */
    addPage(new UINewVMWzdPage1);
    addPage(new UINewVMWzdPage2);
    addPage(new UINewVMWzdPage3);
    addPage(new UINewVMWzdPage4);
    addPage(new UINewVMWzdPage5);

    /* Initial translate */
    retranslateUi();

    /* Initial translate all pages */
    retranslateAllPages();

#ifndef Q_WS_MAC
    /* Assign watermark */
    assignWatermark(":/vmw_new_welcome.png");
#else /* Q_WS_MAC */
    /* Assign background image */
    assignBackground(":/vmw_new_welcome_bg.png");
#endif /* Q_WS_MAC */

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();
}

const CMachine UINewVMWzd::machine() const
{
    /* Use 'machine' field value from page 5 */
    return field("machine").value<CMachine>();
}

void UINewVMWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("Create New Virtual Machine"));

    setButtonText(QWizard::FinishButton, tr("Create"));
}

UINewVMWzdPage1::UINewVMWzdPage1()
{
    /* Decorate page */
    Ui::UINewVMWzdPage1::setupUi(this);
}

void UINewVMWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the New Virtual Machine Wizard!"));

    m_pPage1Text1->setText(tr("<p>This wizard will guide you through the steps that are necessary "
                              "to create a new virtual machine for VirtualBox.</p><p>%1</p>")
                           .arg(standardHelpText()));
}

void UINewVMWzdPage1::initializePage()
{
    /* Fill and translate */
    retranslateUi();
}

UINewVMWzdPage2::UINewVMWzdPage2()
{
    /* Decorate page */
    Ui::UINewVMWzdPage2::setupUi(this);

    /* Register 'name' & 'type' fields */
    registerField("name*", m_pNameEditor);
    registerField("type*", m_pTypeSelector, "type", SIGNAL(osTypeChanged()));
    registerField("machineFolder", this, "machineFolder");

    connect(m_pNameEditor, SIGNAL(textChanged(const QString&)),
            this, SLOT(sltNameChanged(const QString&)));
    connect(m_pTypeSelector, SIGNAL(osTypeChanged()),
            this, SLOT(sltOsTypeChanged()));

    /* Setup contents */
    m_pTypeSelector->activateLayout();
}

void UINewVMWzdPage2::sltNameChanged(const QString &strNewText)
{
    /* Search for a matching OS type based on the string the user typed
     * already. */
    for (size_t i=0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
        if (strNewText.contains(gs_OSTypePattern[i].pattern))
        {
            m_pTypeSelector->blockSignals(true);
            m_pTypeSelector->setType(vboxGlobal().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            m_pTypeSelector->blockSignals(false);
            break;
        }
}

void UINewVMWzdPage2::sltOsTypeChanged()
{
    /* If the user manually edited the OS type, we didn't want our automatic OS
     * type guessing anymore. So simply disconnect the text edit signal. */
    disconnect(m_pNameEditor, SIGNAL(textChanged(const QString&)),
               this, SLOT(sltNameChanged(const QString&)));
}

void UINewVMWzdPage2::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage2::retranslateUi(this);

    /* Wizard page 2 title */
    setTitle(tr("VM Name and OS Type"));
}

void UINewVMWzdPage2::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* 'Name' field should have focus initially */
    m_pNameEditor->setFocus();
}

void UINewVMWzdPage2::cleanupPage()
{
    cleanupMachineFolder();
}

bool UINewVMWzdPage2::validatePage()
{
    return createMachineFolder();
}

bool UINewVMWzdPage2::createMachineFolder()
{
    /* Cleanup old folder if present: */
    bool fMachineFolderDeleted = cleanupMachineFolder();
    if (!fMachineFolderDeleted)
    {
        msgCenter().warnAboutCannotCreateMachineFolder(this, m_strMachineFolder);
        return false;
    }

    /* Get VBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Get default machines directory: */
    QString strDefaultMachinesFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
    /* Compose machine filename name: */
    QString strMachineFilename = vbox.ComposeMachineFilename(field("name").toString(), strDefaultMachinesFolder);
    QFileInfo fileInfo(strMachineFilename);
    /* Get machine directory: */
    QString strMachineFolder = fileInfo.absolutePath();

    /* Try to create this machine directory (and it's predecessors): */
    bool fMachineFolderCreated = QDir().mkpath(strMachineFolder);
    if (!fMachineFolderCreated)
    {
        msgCenter().warnAboutCannotCreateMachineFolder(this, strMachineFolder);
        return false;
    }

    /* Initialize machine dir value: */
    m_strMachineFolder = strMachineFolder;
    return true;
}

bool UINewVMWzdPage2::cleanupMachineFolder()
{
    /* Return if machine folder was NOT set: */
    if (m_strMachineFolder.isEmpty())
        return true;
    /* Try to cleanup this machine directory (and it's predecessors): */
    bool fMachineFolderRemoved = QDir().rmpath(m_strMachineFolder);
    /* Reset machine dir value: */
    if (fMachineFolderRemoved)
        m_strMachineFolder = QString();
    /* Return cleanup result: */
    return fMachineFolderRemoved;
}

QString UINewVMWzdPage2::machineFolder() const
{
    return m_strMachineFolder;
}

void UINewVMWzdPage2::setMachineFolder(const QString &strMachineFolder)
{
    m_strMachineFolder = strMachineFolder;
}

UINewVMWzdPage3::UINewVMWzdPage3()
{
    /* Decorate page */
    Ui::UINewVMWzdPage3::setupUi(this);

    /* Register 'ram' field */
    registerField("ram*", m_pRamSlider, "value", SIGNAL(valueChanged(int)));

    /* Setup contents */
    m_pRamEditor->setFixedWidthByText("88888");
    m_pRamEditor->setAlignment(Qt::AlignRight);
    m_pRamEditor->setValidator(new QIntValidator(m_pRamSlider->minRAM(), m_pRamSlider->maxRAM(), this));

    /* Setup page connections */
    connect(m_pRamSlider, SIGNAL(valueChanged(int)), this, SLOT(ramSliderValueChanged(int)));
    connect(m_pRamEditor, SIGNAL(textChanged(const QString &)), this, SLOT(ramEditorTextChanged(const QString &)));

    /* Initialise page connections */
    ramSliderValueChanged(m_pRamSlider->value());
}

void UINewVMWzdPage3::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage3::retranslateUi(this);

    /* Wizard page 3 title */
    setTitle(tr("Memory"));

    /* Translate recommended 'ram' field value */
    QString strRecommendedRAM = field("type").value<CGuestOSType>().isNull() ? QString() :
                                QString::number(field("type").value<CGuestOSType>().GetRecommendedRAM());
    m_pPage3Text2->setText(tr("The recommended base memory size is <b>%1</b> MB.").arg(strRecommendedRAM));

    /* Translate minimum & maximum 'ram' field values */
    m_pRamMin->setText(QString("%1 %2").arg(m_pRamSlider->minRAM()).arg(VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes")));
    m_pRamMax->setText(QString("%1 %2").arg(m_pRamSlider->maxRAM()).arg(VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes")));
}

void UINewVMWzdPage3::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Assign recommended 'ram' field value */
    CGuestOSType type = field("type").value<CGuestOSType>();
    ramSliderValueChanged(type.GetRecommendedRAM());

    /* 'Ram' field should have focus initially */
    m_pRamSlider->setFocus();
}

bool UINewVMWzdPage3::isComplete() const
{
    /* Check what 'ram' field value feats the bounds */
    return field("ram").toInt() >= qMax(1, (int)m_pRamSlider->minRAM()) &&
           field("ram").toInt() <= (int)m_pRamSlider->maxRAM();
}

void UINewVMWzdPage3::ramSliderValueChanged(int iValue)
{
    /* Update 'ram' field editor connected to slider */
    m_pRamEditor->setText(QString::number(iValue));
}

void UINewVMWzdPage3::ramEditorTextChanged(const QString &strText)
{
    /* Update 'ram' field slider connected to editor */
    m_pRamSlider->setValue(strText.toInt());
}

UINewVMWzdPage4::UINewVMWzdPage4()
{
    /* Decorate page */
    Ui::UINewVMWzdPage4::setupUi(this);

    /* Register CMedium class */
    qRegisterMetaType<CMedium>();

    /* Register all related 'hardDisk*' fields */
    registerField("hardDisk", this, "hardDisk");
    registerField("hardDiskId", this, "hardDiskId");
    registerField("hardDiskName", this, "hardDiskName");
    registerField("hardDiskLocation", this, "hardDiskLocation");

    /* Insert shifting spacer */
    QGridLayout *pLayout = qobject_cast<QGridLayout*>(m_pBootHDCnt->layout());
    Assert(pLayout);
    QStyleOptionButton options;
    options.initFrom(m_pDiskCreate);
    int wid = m_pDiskCreate->style()->subElementRect(QStyle::SE_RadioButtonIndicator, &options, m_pDiskCreate).width() +
              m_pDiskCreate->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, &options, m_pDiskCreate) -
              pLayout->spacing() - 1;
    QSpacerItem *spacer = new QSpacerItem(wid, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    pLayout->addItem(spacer, 2, 0);

    /* Initialise medium-combo-box */
    m_pDiskSelector->setType(VBoxDefs::MediumType_HardDisk);
    m_pDiskSelector->repopulate();

    /* Setup medium-manager button */
    m_pVMMButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png",
                                              ":/select_file_dis_16px.png"));

    /* Setup page connections */
    connect(m_pBootHDCnt, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskCreate, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskPresent, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pVMMButton, SIGNAL(clicked()), this, SLOT(getWithFileOpenDialog()));

    /* Initialise page connections */
    hardDiskSourceChanged();
}

void UINewVMWzdPage4::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage4::retranslateUi(this);

    /* Wizard page 4 title */
    setTitle(tr("Virtual Hard Disk"));

    /* Translate recommended 'hdd' field value */
    QString strRecommendedHDD = field("type").value<CGuestOSType>().isNull() ? QString() :
                                VBoxGlobal::formatSize(field("type").value<CGuestOSType>().GetRecommendedHDD());
    m_pPage4Text2->setText (tr ("The recommended size of the start-up disk is <b>%1</b>.").arg (strRecommendedHDD));
}

void UINewVMWzdPage4::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Prepare initial choice */
    m_pBootHDCnt->setChecked(true);
    m_pDiskSelector->setCurrentIndex(0);
    m_pDiskCreate->setChecked(true);

    /* 'Create new hard-disk' should have focus initially */
    m_pDiskCreate->setFocus();
}

void UINewVMWzdPage4::cleanupPage()
{
    /* Clean medium if present */
    ensureNewHardDiskDeleted();
    /* Clean fields of that page */
    QIWizardPage::cleanupPage();
}

bool UINewVMWzdPage4::isComplete() const
{
    /* Check what 'hardDisk' field value feats the rules */
    return !m_pBootHDCnt->isChecked() ||
           !m_pDiskPresent->isChecked() ||
           !vboxGlobal().findMedium(m_pDiskSelector->id()).isNull();
}

bool UINewVMWzdPage4::validatePage()
{
    /* Ensure unused hard-disk is deleted */
    if (!m_pBootHDCnt->isChecked() || m_pDiskCreate->isChecked() || (!m_HardDisk.isNull() && m_strHardDiskId != m_HardDisk.GetId()))
        ensureNewHardDiskDeleted();

    /* Ask user about disk-less machine */
    if (!m_pBootHDCnt->isChecked() && !msgCenter().confirmHardDisklessMachine(this))
        return false;

    /* Show the New Hard Disk wizard */
    if (m_pBootHDCnt->isChecked() && m_pDiskCreate->isChecked() && !getWithNewHardDiskWizard())
        return false;

    return true;
}

void UINewVMWzdPage4::ensureNewHardDiskDeleted()
{
    if (m_HardDisk.isNull())
        return;

    QString id = m_HardDisk.GetId();

    bool success = false;

    CProgress progress = m_HardDisk.DeleteStorage();
    if (m_HardDisk.isOk())
    {
        msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_delete_90px.png", this, true);
        if (progress.isOk() && progress.GetResultCode() == S_OK)
            success = true;
    }

    if (success)
        vboxGlobal().removeMedium(VBoxDefs::MediumType_HardDisk, id);
    else
        msgCenter().cannotDeleteHardDiskStorage(this, m_HardDisk, progress);

    m_HardDisk.detach();
}

void UINewVMWzdPage4::hardDiskSourceChanged()
{
    m_pDiskCreate->setEnabled(m_pBootHDCnt->isChecked());
    m_pDiskPresent->setEnabled(m_pBootHDCnt->isChecked());
    m_pDiskSelector->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());
    m_pVMMButton->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());

    if (m_pBootHDCnt->isChecked() && m_pDiskPresent->isChecked())
    {
        m_strHardDiskId = m_pDiskSelector->id();
        m_strHardDiskName = m_pDiskSelector->currentText();
        m_strHardDiskLocation = m_pDiskSelector->location();
    }
    else
    {
        m_strHardDiskId.clear();
        m_strHardDiskName.clear();
        m_strHardDiskLocation.clear();
    }

    emit completeChanged();
}

void UINewVMWzdPage4::getWithFileOpenDialog()
{
    /* Get opened vboxMedium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(VBoxDefs::MediumType_HardDisk, this);
    if (!strMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pDiskSelector->setCurrentItem(strMediumId);
        /* Update hard disk source: */
        hardDiskSourceChanged();
        /* Focus on hard disk combo: */
        m_pDiskSelector->setFocus();
    }
}

bool UINewVMWzdPage4::getWithNewHardDiskWizard()
{
    UINewHDWizard dlg(this, field("name").toString(), field("machineFolder").toString(), field("type").value<CGuestOSType>().GetRecommendedHDD());

    if (dlg.exec() == QDialog::Accepted)
    {
        m_HardDisk = dlg.hardDisk();
        m_pDiskSelector->setCurrentItem(m_HardDisk.GetId());
        m_pDiskPresent->click();
        return true;
    }

    return false;
}

CMedium UINewVMWzdPage4::hardDisk() const
{
    return m_HardDisk;
}

void UINewVMWzdPage4::setHardDisk(const CMedium &hardDisk)
{
    m_HardDisk = hardDisk;
}

QString UINewVMWzdPage4::hardDiskId() const
{
    return m_strHardDiskId;
}

void UINewVMWzdPage4::setHardDiskId(const QString &strHardDiskId)
{
    m_strHardDiskId = strHardDiskId;
}

QString UINewVMWzdPage4::hardDiskName() const
{
    return m_strHardDiskName;
}

void UINewVMWzdPage4::setHardDiskName(const QString &strHardDiskName)
{
    m_strHardDiskName = strHardDiskName;
}

QString UINewVMWzdPage4::hardDiskLocation() const
{
    return m_strHardDiskLocation;
}

void UINewVMWzdPage4::setHardDiskLocation(const QString &strHardDiskLocation)
{
    m_strHardDiskLocation = strHardDiskLocation;
}

UINewVMWzdPage5::UINewVMWzdPage5()
    : m_iIDECount(0)
    , m_iSATACount(0)
    , m_iSCSICount(0)
    , m_iFloppyCount(0)
    , m_iSASCount(0)
{
    /* Decorate page */
    Ui::UINewVMWzdPage5::setupUi(this);

    /* Register CMachine class */
    qRegisterMetaType<CMachine>();

    /* Register 'machine' field */
    registerField("machine", this, "machine");
}

void UINewVMWzdPage5::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage5::retranslateUi(this);

    /* Wizard page 5 title */
    setTitle(tr("Summary"));

    /* Compose common summary */
    QString summary;

    QString name = field("name").toString();
    QString type = field("type").value<CGuestOSType>().isNull() ? QString() :
                   field("type").value<CGuestOSType>().GetDescription();
    QString ram  = QString::number(field("ram").toInt());

    summary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td>%2</td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td>%4</td></tr>"
        "<tr><td><nobr>%5: </nobr></td><td>%6 %7</td></tr>"
    )
    .arg(tr("Name", "summary"), name)
    .arg(tr("OS Type", "summary"), type)
    .arg(tr("Base Memory", "summary"), ram, VBoxGlobal::tr("MB", "size suffix MBytes=1024KBytes"))
    ;

    /* Add hard-disk info */
    if (!field("hardDiskId").toString().isNull())
    {
        summary += QString(
            "<tr><td><nobr>%8: </nobr></td><td><nobr>%9</nobr></td></tr>")
            .arg(tr("Start-up Disk", "summary"), field("hardDiskName").toString());
    }

    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + summary + "</table>");

    m_pPage5Text2->setText(tr("<p>If the above is correct press the <b>%1</b> button. Once "
                              "you press it, a new virtual machine will be created. </p><p>Note "
                              "that you can alter these and all other setting of the created "
                              "virtual machine at any time using the <b>Settings</b> dialog "
                              "accessible through the menu of the main window.</p>")
                           .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::FinishButton)))));
}

void UINewVMWzdPage5::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Summary should have focus initially */
    m_pSummaryText->setFocus();
}

bool UINewVMWzdPage5::validatePage()
{
    startProcessing();
    /* Try to construct machine */
    bool fResult = constructMachine();
    endProcessing();
    return fResult;
}

bool UINewVMWzdPage5::constructMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* OS type */
    CGuestOSType type = field("type").value<CGuestOSType>();
    AssertMsg(!type.isNull(), ("GuestOSType must return non-null type"));
    QString typeId = type.GetId();

    /* Create a machine with the default settings file location */
    if (m_Machine.isNull())
    {
        m_Machine = vbox.CreateMachine(QString::null,       // auto-compose filename
                                       field("name").toString(),
                                       typeId,
                                       QString::null,       // machine ID
                                       false);              // forceOverwrite
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateMachine(vbox, this);
            return false;
        }

        /* The FirstRun wizard is to be shown only when we don't attach any hard disk or attach a new (empty) one.
         * Selecting an existing hard disk will cancel the wizard. */
        if (field("hardDiskId").toString().isNull() || !field("hardDisk").value<CMedium>().isNull())
            m_Machine.SetExtraData(VBoxDefs::GUI_FirstRun, "yes");
    }

    /* RAM size */
    m_Machine.SetMemorySize(field("ram").toInt());

    /* VRAM size - select maximum between recommended and minimum for fullscreen */
    m_Machine.SetVRAMSize (qMax (type.GetRecommendedVRAM(),
                                (ULONG) (VBoxGlobal::requiredVideoMemory(typeId) / _1M)));

    /* Selecting recommended chipset type */
    m_Machine.SetChipsetType(type.GetRecommendedChipset());

    /* Selecting recommended Audio Controller */
    m_Machine.GetAudioAdapter().SetAudioController(type.GetRecommendedAudioController());
    /* Enabling audio by default */
    m_Machine.GetAudioAdapter().SetEnabled(true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2) */
    CUSBController usbController = m_Machine.GetUSBController();
    if (   !usbController.isNull()
        && type.GetRecommendedUsb()
        && usbController.GetProxyAvailable())
    {
        usbController.SetEnabled(true);

        /*
         * USB 2.0 is only available if the proper ExtPack is installed.
         *
         * Note. Configuring EHCI here and providing messages about
         * the missing extpack isn't exactly clean, but it is a
         * necessary evil to patch over legacy compatability issues
         * introduced by the new distribution model.
         */
        CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
        if (manager.IsExtPackUsable(UI_ExtPackName))
            usbController.SetEnabledEhci(true);
    }

    /* Create a floppy controller if recommended */
    QString ctrFloppyName = getNextControllerName(KStorageBus_Floppy);
    if (type.GetRecommendedFloppy()) {
        m_Machine.AddStorageController(ctrFloppyName, KStorageBus_Floppy);
        CStorageController flpCtr = m_Machine.GetStorageControllerByName(ctrFloppyName);
        flpCtr.SetControllerType(KStorageControllerType_I82078);
    }

    /* Create recommended DVD storage controller */
    KStorageBus ctrDvdBus = type.GetRecommendedDvdStorageBus();
    QString ctrDvdName = getNextControllerName(ctrDvdBus);
    m_Machine.AddStorageController(ctrDvdName, ctrDvdBus);

    /* Set recommended DVD storage controller type */
    CStorageController dvdCtr = m_Machine.GetStorageControllerByName(ctrDvdName);
    KStorageControllerType dvdStorageControllerType = type.GetRecommendedDvdStorageController();
    dvdCtr.SetControllerType(dvdStorageControllerType);

    /* Create recommended HD storage controller if it's not the same as the DVD controller */
    KStorageBus ctrHdBus = type.GetRecommendedHdStorageBus();
    KStorageControllerType hdStorageControllerType = type.GetRecommendedHdStorageController();
    CStorageController hdCtr;
    QString ctrHdName;
    if (ctrHdBus != ctrDvdBus || hdStorageControllerType != dvdStorageControllerType)
    {
        ctrHdName = getNextControllerName(ctrHdBus);
        m_Machine.AddStorageController(ctrHdName, ctrHdBus);
        hdCtr = m_Machine.GetStorageControllerByName(ctrHdName);
        hdCtr.SetControllerType(hdStorageControllerType);

        /* Set the port count to 1 if SATA is used. */
        if (hdStorageControllerType == KStorageControllerType_IntelAhci)
            hdCtr.SetPortCount(1);
    }
    else
    {
        /* The HD controller is the same as DVD */
        hdCtr = dvdCtr;
        ctrHdName = ctrDvdName;
    }

    /* Turn on PAE, if recommended */
    m_Machine.SetCPUProperty(KCPUPropertyType_PAE, type.GetRecommendedPae());

    /* Set recommended firmware type */
    KFirmwareType fwType = type.GetRecommendedFirmware();
    m_Machine.SetFirmwareType(fwType);

    /* Set recommended human interface device types */
    if (type.GetRecommendedUsbHid())
    {
        m_Machine.SetKeyboardHidType(KKeyboardHidType_USBKeyboard);
        m_Machine.SetPointingHidType(KPointingHidType_USBMouse);
        if (!usbController.isNull())
            usbController.SetEnabled(true);
    }

    if (type.GetRecommendedUsbTablet())
    {
        m_Machine.SetPointingHidType(KPointingHidType_USBTablet);
        if (!usbController.isNull())
            usbController.SetEnabled(true);
    }

    /* Set HPET flag */
    m_Machine.SetHpetEnabled(type.GetRecommendedHpet());

    /* Set UTC flags */
    m_Machine.SetRTCUseUTC(type.GetRecommendedRtcUseUtc());

    /* Set graphic bits. */
    if (type.GetRecommended2DVideoAcceleration())
        m_Machine.SetAccelerate2DVideoEnabled(type.GetRecommended2DVideoAcceleration());

    if (type.GetRecommended3DAcceleration())
        m_Machine.SetAccelerate3DEnabled(type.GetRecommended3DAcceleration());

    /* Register the VM prior to attaching hard disks */
    vbox.RegisterMachine(m_Machine);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMachine(vbox, m_Machine, this);
        return false;
    }

    /* Attach default devices */
    {
        bool success = false;
        QString machineId = m_Machine.GetId();
        CSession session = vboxGlobal().openSession(machineId);
        if (!session.isNull())
        {
            CMachine m = session.GetMachine();

            QString strId = field("hardDiskId").toString();
            /* Boot hard disk */
            if (!strId.isNull())
            {
                VBoxMedium vmedium = vboxGlobal().findMedium(strId);
                CMedium medium = vmedium.medium();              // @todo r=dj can this be cached somewhere?
                m.AttachDevice(ctrHdName, 0, 0, KDeviceType_HardDisk, medium);
                if (!m.isOk())
                    msgCenter().cannotAttachDevice(m, VBoxDefs::MediumType_HardDisk, field("hardDiskLocation").toString(),
                                                     StorageSlot(ctrHdBus, 0, 0), this);
            }

            /* Attach empty CD/DVD ROM Device */
            m.AttachDevice(ctrDvdName, 1, 0, KDeviceType_DVD, CMedium());
            if (!m.isOk())
                msgCenter().cannotAttachDevice(m, VBoxDefs::MediumType_DVD, QString(), StorageSlot(ctrDvdBus, 1, 0), this);


            /* Attach an empty floppy drive if recommended */
            if (type.GetRecommendedFloppy()) {
                m.AttachDevice(ctrFloppyName, 0, 0, KDeviceType_Floppy, CMedium());
                if (!m.isOk())
                    msgCenter().cannotAttachDevice(m, VBoxDefs::MediumType_Floppy, QString(),
                                                   StorageSlot(KStorageBus_Floppy, 0, 0), this);
            }

            if (m.isOk())
            {
                m.SaveSettings();
                if (m.isOk())
                    success = true;
                else
                    msgCenter().cannotSaveMachineSettings(m, this);
            }

            session.UnlockMachine();
        }
        if (!success)
        {
            /* Unregister on failure */
            QVector<CMedium> aMedia = m_Machine.Unregister(KCleanupMode_UnregisterOnly);   //  @todo replace with DetachAllReturnHardDisksOnly once a progress dialog is in place below
            if (vbox.isOk())
            {
                CProgress progress = m_Machine.Delete(aMedia);
                progress.WaitForCompletion(-1);         // @todo do this nicely with a progress dialog, this can delete lots of files
            }
            return false;
        }
    }

    /* Ensure we don't try to delete a newly created hard disk on success */
    if (!field("hardDisk").value<CMedium>().isNull())
        field("hardDisk").value<CMedium>().detach();

    return true;
}

QString UINewVMWzdPage5::getNextControllerName(KStorageBus type)
{
    QString strControllerName;
    switch (type)
    {
        case KStorageBus_IDE:
        {
            strControllerName = UIMachineSettingsStorage::tr("IDE Controller");
            ++m_iIDECount;
            if (m_iIDECount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iIDECount);
            break;
        }
        case KStorageBus_SATA:
        {
            strControllerName = UIMachineSettingsStorage::tr("SATA Controller");
            ++m_iSATACount;
            if (m_iSATACount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSATACount);
            break;
        }
        case KStorageBus_SCSI:
        {
            strControllerName = UIMachineSettingsStorage::tr("SCSI Controller");
            ++m_iSCSICount;
            if (m_iSCSICount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSCSICount);
            break;
        }
        case KStorageBus_Floppy:
        {
            strControllerName = UIMachineSettingsStorage::tr("Floppy Controller");
            ++m_iFloppyCount;
            if (m_iFloppyCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iFloppyCount);
            break;
        }
        case KStorageBus_SAS:
        {
            strControllerName = UIMachineSettingsStorage::tr("SAS Controller");
            ++m_iSASCount;
            if (m_iSASCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSASCount);
            break;
        }
        default:
            break;
    }
    return strControllerName;
}

CMachine UINewVMWzdPage5::machine() const
{
    return m_Machine;
}

void UINewVMWzdPage5::setMachine(const CMachine &machine)
{
    m_Machine = machine;
}
