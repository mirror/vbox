/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewVMWzd class implementation
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* Local includes */
#include "UINewVMWzd.h"
#include "UINewHDWzd.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxMediaManagerDlg.h"
#include "VBoxVMSettingsHD.h"

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
    { QRegExp("Wi.*3", Qt::CaseInsensitive), "Windows31" },
    { QRegExp("Wi.*98", Qt::CaseInsensitive), "Windows98" },
    { QRegExp("Wi.*95", Qt::CaseInsensitive), "Windows95" },
    { QRegExp("Wi.*Me", Qt::CaseInsensitive), "WindowsMe" },
    { QRegExp("(Wi.*NT)|(NT)", Qt::CaseInsensitive), "WindowsNT4" },
    { QRegExp("((Wi.*X)|(XP)).*64", Qt::CaseInsensitive), "WindowsXP_64" },
    { QRegExp("(Wi.*X)|(XP)", Qt::CaseInsensitive), "WindowsXP" },
    { QRegExp("((Wi.*2003)|(W2K3).*64", Qt::CaseInsensitive), "Windows2003_64" },
    { QRegExp("(Wi.*2003)|(W2K3)", Qt::CaseInsensitive), "Windows2003" },
    { QRegExp("((Wi.*V)|(Vista)).*64", Qt::CaseInsensitive), "WindowsVista_64" },
    { QRegExp("(Wi.*V)|(Vista)", Qt::CaseInsensitive), "WindowsVista" },
    { QRegExp("((Wi.*2008)|(W2K8)).*64", Qt::CaseInsensitive), "Windows2008_64" },
    { QRegExp("(Wi.*2008)|(W2K8)", Qt::CaseInsensitive), "Windows2008" },
    { QRegExp("(Wi.*2)|(W2K)", Qt::CaseInsensitive), "Windows2000" },
    { QRegExp("Wi.*7.*64", Qt::CaseInsensitive), "Windows7_64" },
    { QRegExp("Wi.*7", Qt::CaseInsensitive), "Windows7" },
    { QRegExp("Wi", Qt::CaseInsensitive), "WindowsXP" },

    /* Solaris */
    { QRegExp("((Op.*So)|(os20[01][0-9])).*64", Qt::CaseInsensitive), "OpenSolaris_64" },
    { QRegExp("(Op.*So)|(os20[01][0-9])", Qt::CaseInsensitive), "OpenSolaris" },
    { QRegExp("So.*64", Qt::CaseInsensitive), "Solaris_64" },
    { QRegExp("So", Qt::CaseInsensitive), "Solaris" },

    /* OS/2 */
    { QRegExp("OS[/|!-]{,1}2.*W.*4.?5", Qt::CaseInsensitive), "OS2Warp45" },
    { QRegExp("OS[/|!-]{,1}2.*W.*4", Qt::CaseInsensitive), "OS2Warp4" },
    { QRegExp("OS[/|!-]{,1}2.*W", Qt::CaseInsensitive), "OS2Warp3" },
    { QRegExp("(OS[/|!-]{,1}2.*e)|(eCS.*)", Qt::CaseInsensitive), "OS2eCS" },
    { QRegExp("OS[/|!-]{,1}2", Qt::CaseInsensitive), "OS2" },

    /* Code names for Linux distributions */
    { QRegExp("((edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)).*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("(edgy)|(feisty)|(gutsy)|(hardy)|(intrepid)|(jaunty)|(karmic)|(lucid)", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("((sarge)|(etch)|(lenny)|(squeeze)|(sid)).*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("(sarge)|(etch)|(lenny)|(squeeze)|(sid)", Qt::CaseInsensitive), "Debian" },
    { QRegExp("((moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)).*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("(moonshine)|(werewolf)|(sulphur)|(cambridge)|(leonidas)|(constantine)|(goddard)", Qt::CaseInsensitive), "Fedora" },

    /* Regular names of Linux distributions */
    { QRegExp("Arc.*64", Qt::CaseInsensitive), "ArchLinux_64" },
    { QRegExp("Arc", Qt::CaseInsensitive), "ArchLinux" },
    { QRegExp("De.*64", Qt::CaseInsensitive), "Debian_64" },
    { QRegExp("De", Qt::CaseInsensitive), "Debian" },
    { QRegExp("(SU)|(No)", Qt::CaseInsensitive), "OpenSUSE" },
    { QRegExp("((SU)|(No)).*64", Qt::CaseInsensitive), "OpenSUSE_64" },
    { QRegExp("Fe.*64", Qt::CaseInsensitive), "Fedora_64" },
    { QRegExp("Fe", Qt::CaseInsensitive), "Fedora" },
    { QRegExp("((Ge)|(Sab)).*64", Qt::CaseInsensitive), "Gentoo_64" },
    { QRegExp("(Ge)|(Sab)", Qt::CaseInsensitive), "Gentoo" },
    { QRegExp("Man.*64", Qt::CaseInsensitive), "Mandriva_64" },
    { QRegExp("Man", Qt::CaseInsensitive), "Mandriva" },
    { QRegExp("((Red)|(rhel)).*64", Qt::CaseInsensitive), "RedHat_64" },
    { QRegExp("(Red)|(rhel)", Qt::CaseInsensitive), "RedHat" },
    { QRegExp("Tur", Qt::CaseInsensitive), "Turbolinux" },
    { QRegExp("Ub.*64", Qt::CaseInsensitive), "Ubuntu_64" },
    { QRegExp("Ub", Qt::CaseInsensitive), "Ubuntu" },
    { QRegExp("Xa.*64", Qt::CaseInsensitive), "Xandros_64" },
    { QRegExp("Xa", Qt::CaseInsensitive), "Xandros" },
    { QRegExp("Or.*64", Qt::CaseInsensitive), "Oracle_64" },
    { QRegExp("Or", Qt::CaseInsensitive), "Oracle" },
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
    { QRegExp("Mac.*64", Qt::CaseInsensitive), "MacOS_64" },
    { QRegExp("Mac", Qt::CaseInsensitive), "MacOS" },
    { QRegExp("Net", Qt::CaseInsensitive), "Netware" },
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

    /* Translate */
    retranslateUi();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_new_welcome_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark */
    assignWatermark(":/vmw_new_welcome.png");
#endif /* Q_WS_MAC */
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
}

UINewVMWzdPage1::UINewVMWzdPage1()
{
    /* Decorate page */
    Ui::UINewVMWzdPage1::setupUi(this);

    /* Translate */
    retranslateUi();
}

void UINewVMWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the New Virtual Machine Wizard!"));
}

UINewVMWzdPage2::UINewVMWzdPage2()
{
    /* Decorate page */
    Ui::UINewVMWzdPage2::setupUi(this);

    /* Register 'name' & 'type' fields */
    registerField("name*", m_pNameEditor);
    registerField("type*", m_pTypeSelector, "type", SIGNAL(osTypeChanged()));

    connect(m_pNameEditor, SIGNAL(textChanged(const QString&)),
            this, SLOT(sltNameChanged(const QString&)));

    /* Setup contents */
    m_pTypeSelector->activateLayout();

    /* Translate */
    retranslateUi();
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
    /* Translate */
    retranslateUi();

    /* 'Name' field should have focus initially */
    m_pNameEditor->setFocus();
}

void UINewVMWzdPage2::sltNameChanged(const QString &strNewText)
{
    /* Search for a matching OS type based on the string the user typed
     * already. */
    /** @todo Perhaps we shouldn't do this if the user has manually selected
     *        anything in any of the the combo boxes. */
    for (size_t i=0; i < RT_ELEMENTS(gs_OSTypePattern); ++i)
        if (strNewText.contains(gs_OSTypePattern[i].pattern))
        {
            m_pTypeSelector->setType(vboxGlobal().vmGuestOSType(gs_OSTypePattern[i].pcstId));
            break;
        }
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

    /* Translate */
    retranslateUi();
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
    m_pRamMin->setText(QString("%1 %2").arg(m_pRamSlider->minRAM()).arg(tr("MB", "megabytes")));
    m_pRamMax->setText(QString("%1 %2").arg(m_pRamSlider->maxRAM()).arg(tr("MB", "megabytes")));
}

void UINewVMWzdPage3::initializePage()
{
    /* Translate */
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
    m_pVMMButton->setIcon(VBoxGlobal::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));

    /* Setup page connections */
    connect(m_pBootHDCnt, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskCreate, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskPresent, SIGNAL(toggled(bool)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pDiskSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(hardDiskSourceChanged()));
    connect(m_pVMMButton, SIGNAL(clicked()), this, SLOT(getWithMediaManager()));

    /* Initialise page connections */
    hardDiskSourceChanged();

    /* Translate */
    retranslateUi();
}

void UINewVMWzdPage4::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewVMWzdPage4::retranslateUi(this);

    /* Wizard page 4 title */
    setTitle(tr("Virtual Hard Disk"));

    /* Translate recommended 'hdd' field value */
    QString strRecommendedHDD = field("type").value<CGuestOSType>().isNull() ? QString() :
                                QString::number(field("type").value<CGuestOSType>().GetRecommendedHDD());
    m_pPage4Text2->setText (tr ("The recommended size of the boot hard disk is <b>%1</b> MB.").arg (strRecommendedHDD));
}

void UINewVMWzdPage4::initializePage()
{
    /* Translate */
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
    if (!m_pBootHDCnt->isChecked() && !vboxProblem().confirmHardDisklessMachine(this))
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
        vboxProblem().showModalProgressDialog(progress, windowTitle(), this);
        if (progress.isOk() && progress.GetResultCode() == S_OK)
            success = true;
    }

    if (success)
        vboxGlobal().removeMedium(VBoxDefs::MediumType_HardDisk, id);
    else
        vboxProblem().cannotDeleteHardDiskStorage(this, m_HardDisk, progress);

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

void UINewVMWzdPage4::getWithMediaManager()
{
    VBoxMediaManagerDlg dlg(this);
    dlg.setup(VBoxDefs::MediumType_HardDisk, true);

    if (dlg.exec() == QDialog::Accepted)
    {
        QString newId = dlg.selectedId();
        if (m_pDiskSelector->id() != newId)
            m_pDiskSelector->setCurrentItem(newId);
    }

    m_pDiskSelector->setFocus();
}

bool UINewVMWzdPage4::getWithNewHardDiskWizard()
{
    UINewHDWzd dlg(this);
    dlg.setRecommendedName(field("name").toString());
    dlg.setRecommendedSize(field("type").value<CGuestOSType>().GetRecommendedHDD());

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
{
    /* Decorate page */
    Ui::UINewVMWzdPage5::setupUi(this);

    /* Register CMachine class */
    qRegisterMetaType<CMachine>();

    /* Register 'machine' field */
    registerField("machine", this, "machine");

    /* Disable the background painting of the summary widget */
    m_pSummaryText->viewport()->setAutoFillBackground (false);
    /* Make the summary field read-only */
    m_pSummaryText->setReadOnly (true);

    /* Translate */
    retranslateUi();
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
    .arg(tr("Base Memory", "summary"), ram, tr("MB", "megabytes"))
    ;
    /* Feat summary to 3 lines */
    setSummaryFieldLinesNumber(m_pSummaryText, 3);

    /* Add hard-disk info */
    if (!field("hardDiskId").toString().isNull())
    {
        summary += QString(
            "<tr><td><nobr>%8: </nobr></td><td><nobr>%9</nobr></td></tr>")
            .arg(tr("Boot Hard Disk", "summary"), field("hardDiskName").toString());
        /* Extend summary to 4 lines */
        setSummaryFieldLinesNumber(m_pSummaryText, 4);
    }

    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + summary + "</table>");
}

void UINewVMWzdPage5::initializePage()
{
    /* Translate */
    retranslateUi();

    /* Summary should have focus initially */
    m_pSummaryText->setFocus();
}

bool UINewVMWzdPage5::validatePage()
{
    /* Try to construct machine */
    return constructMachine();
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
        m_Machine = vbox.CreateMachine(field("name").toString(), typeId, QString::null, QString::null);
        if (!vbox.isOk())
        {
            vboxProblem().cannotCreateMachine(vbox, this);
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
                                (ULONG) (VBoxGlobal::requiredVideoMemory(&m_Machine) / _1M)));

    /* Enabling audio by default */
    m_Machine.GetAudioAdapter().SetEnabled(true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2) */
    CUSBController usbController = m_Machine.GetUSBController();
    if (!usbController.isNull())
    {
        usbController.SetEnabled(true);
        usbController.SetEnabledEhci(true);
    }

    /* Create default storage controllers */
    QString ctrName = VBoxVMSettingsHD::tr("Storage Controller");
    KStorageBus ideBus = KStorageBus_IDE;

    // Add storage controller
    m_Machine.AddStorageController(ctrName, ideBus);

    // Set storage controller type
    CStorageController ctr = m_Machine.GetStorageControllerByName(ctrName);
    KStorageControllerType storageControllerType = type.GetRecommendedStorageController();
    ctr.SetControllerType(storageControllerType);

    // Turn on PAE, if recommended
    m_Machine.SetCpuProperty(KCpuPropertyType_PAE, type.GetRecommendedPae());

    // Set recommended firmware type
    KFirmwareType fwType = type.GetRecommendedFirmware();
    m_Machine.SetFirmwareType(fwType);

    /* Register the VM prior to attaching hard disks */
    vbox.RegisterMachine(m_Machine);
    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateMachine(vbox, m_Machine, this);
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

            /* Boot hard disk (IDE Primary Master) */
            if (!field("hardDiskId").toString().isNull())
            {
                m.AttachDevice(ctrName, 0, 0, KDeviceType_HardDisk, field("hardDiskId").toString());
                if (!m.isOk())
                    vboxProblem().cannotAttachDevice(this, m, VBoxDefs::MediumType_HardDisk,
                                                     field("hardDiskLocation").toString(), ideBus, 0, 0);
            }

            /* Attach empty CD/DVD ROM Device */
            m.AttachDevice(ctrName, 1, 0, KDeviceType_DVD, QString(""));
            if (!m.isOk())
                vboxProblem().cannotAttachDevice(this, m, VBoxDefs::MediumType_DVD, QString(), ideBus, 1, 0);

            if (m.isOk())
            {
                m.SaveSettings();
                if (m.isOk())
                    success = true;
                else
                    vboxProblem().cannotSaveMachineSettings(m, this);
            }

            session.Close();
        }
        if (!success)
        {
            /* Unregister on failure */
            vbox.UnregisterMachine(machineId);
            if (vbox.isOk())
                m_Machine.DeleteSettings();
            return false;
        }
    }

    /* Ensure we don't try to delete a newly created hard disk on success */
    if (!field("hardDisk").value<CMedium>().isNull())
        field("hardDisk").value<CMedium>().detach();

    return true;
}

CMachine UINewVMWzdPage5::machine() const
{
    return m_Machine;
}

void UINewVMWzdPage5::setMachine(const CMachine &machine)
{
    m_Machine = machine;
}
