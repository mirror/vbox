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

    /* Assign watermark */
    assignWatermark(":/vmw_new_welcome.png");
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
    QString ideCtrName = VBoxVMSettingsHD::tr("IDE Controller");
    QString floppyCtrName = VBoxVMSettingsHD::tr("Floppy Controller");
    KStorageBus ideBus = KStorageBus_IDE;
    KStorageBus floppyBus = KStorageBus_Floppy;
    m_Machine.AddStorageController(ideCtrName, ideBus);
    m_Machine.AddStorageController(floppyCtrName, floppyBus);

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
                m.AttachDevice(ideCtrName, 0, 0, KDeviceType_HardDisk, field("hardDiskId").toString());
                if (!m.isOk())
                    vboxProblem().cannotAttachDevice(this, m, VBoxDefs::MediumType_HardDisk,
                                                     field("hardDiskLocation").toString(), ideBus, 0, 0);
            }

            /* Attach empty CD/DVD ROM Device */
            m.AttachDevice(ideCtrName, 1, 0, KDeviceType_DVD, QString(""));
            if (!m.isOk())
                vboxProblem().cannotAttachDevice(this, m, VBoxDefs::MediumType_DVD, QString(), ideBus, 1, 0);

            /* Attach empty Floppy Device */
            m.AttachDevice(floppyCtrName, 0, 0, KDeviceType_Floppy, QString(""));
            if (!m.isOk())
                vboxProblem().cannotAttachDevice(this, m, VBoxDefs::MediumType_Floppy, QString(), floppyBus, 0, 0);

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
