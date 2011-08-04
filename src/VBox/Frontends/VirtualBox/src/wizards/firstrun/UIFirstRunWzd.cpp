/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIFirstRunWzd class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "UIFirstRunWzd.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

UIFirstRunWzd::UIFirstRunWzd(QWidget *pParent, const CMachine &machine) : QIWizard(pParent)
{
    /* Create & add pages */
    UIFirstRunWzdPage1 *pPage1 = new UIFirstRunWzdPage1;
    UIFirstRunWzdPage2 *pPage2 = new UIFirstRunWzdPage2;
    UIFirstRunWzdPage3 *pPage3 = new UIFirstRunWzdPage3;

    addPage(pPage1);
    addPage(pPage2);
    addPage(pPage3);

    /* Set 'machine' field value for page 3 */
    setField("machine", QVariant::fromValue(machine));

    /* Init pages basing on machine set */
    pPage1->init();
    pPage2->init();
    pPage3->init();

    /* Initial translate */
    retranslateUi();

    /* Initial translate all pages */
    retranslateAllPages();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_first_run_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark */
    assignWatermark(":/vmw_first_run.png");
#endif /* Q_WS_MAC */
}

bool UIFirstRunWzd::isBootHardDiskAttached(const CMachine &machine)
{
    /* Result is 'false' initially: */
    bool fIsBootHardDiskAttached = false;
    /* Get 'vbox' global object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Determine machine 'OS type': */
    const CGuestOSType &osType = vbox.GetGuestOSType(machine.GetOSTypeId());
    /* Determine recommended controller's 'bus' & 'type': */
    KStorageBus hdCtrBus = osType.GetRecommendedHdStorageBus();
    KStorageControllerType hdCtrType = osType.GetRecommendedHdStorageController();
    /* Enumerate attachments vector: */
    const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
    for (int i = 0; i < attachments.size(); ++i)
    {
        /* Get current attachment: */
        const CMediumAttachment &attachment = attachments[i];
        /* Determine attachment's controller: */
        const CStorageController &controller = machine.GetStorageControllerByName(attachment.GetController());
        /* If controller's 'bus' & 'type' are recommended and attachment's 'type' is 'hard disk': */
        if (controller.GetBus() == hdCtrBus &&
            controller.GetControllerType() == hdCtrType &&
            attachment.GetType() == KDeviceType_HardDisk)
        {
            /* Set the result to 'true': */
            fIsBootHardDiskAttached = true;
            break;
        }
    }
    /* Return result: */
    return fIsBootHardDiskAttached;
}

void UIFirstRunWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("First Run Wizard"));

    setButtonText(QWizard::FinishButton, tr("Start"));
}

UIFirstRunWzdPage1::UIFirstRunWzdPage1()
{
    /* Decorate page */
    Ui::UIFirstRunWzdPage1::setupUi(this);
}

void UIFirstRunWzdPage1::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    bool fIsBootHDAttached = UIFirstRunWzd::isBootHardDiskAttached(machine);
    m_pPage1Text1Var1->setHidden(!fIsBootHDAttached);
    m_pPage1Text1Var2->setHidden(fIsBootHDAttached);
}

void UIFirstRunWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIFirstRunWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the First Run Wizard!"));

    m_pPage1Text1Var1->setText(tr("<p>You have started a newly created virtual machine for the "
                                  "first time. This wizard will help you to perform the steps "
                                  "necessary for installing an operating system of your choice "
                                  "onto this virtual machine.</p><p>%1</p>")
                               .arg(standardHelpText()));

    m_pPage1Text1Var2->setText(tr("<p>You have started a newly created virtual machine for the "
                                  "first time. This wizard will help you to perform the steps "
                                  "necessary for booting an operating system of your choice on "
                                  "the virtual machine.</p><p>Note that you will not be able to "
                                  "install an operating system into this virtual machine right "
                                  "now because you did not attach any hard disk to it. If this "
                                  "is not what you want, you can cancel the execution of this "
                                  "wizard, select <b>Settings</b> from the <b>Machine</b> menu "
                                  "of the main VirtualBox window to access the settings dialog "
                                  "of this machine and change the hard disk configuration.</p>"
                                  "<p>%1</p>")
                               .arg(standardHelpText()));
}

void UIFirstRunWzdPage1::initializePage()
{
    /* Fill and translate */
    retranslateUi();
}

UIFirstRunWzdPage2::UIFirstRunWzdPage2()
{
    /* Decorate page */
    Ui::UIFirstRunWzdPage2::setupUi(this);

    /* Register 'source' and 'id' fields! */
    registerField("source", this, "source");
    registerField("id", this, "id");

    /* Setup contents */
    m_pSelectMediaButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png",
                                                      ":/select_file_dis_16px.png"));

    /* Setup connections */
    connect(m_pMediaSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(sltMediumChanged()));
    connect(m_pSelectMediaButton, SIGNAL(clicked()), this, SLOT(sltOpenVirtualMediaManager()));
}

void UIFirstRunWzdPage2::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    bool fIsBootHDAttached = UIFirstRunWzd::isBootHardDiskAttached(machine);
    m_pPage2Text1Var1->setHidden(!fIsBootHDAttached);
    m_pPage2Text1Var2->setHidden(fIsBootHDAttached);

    /* Assign media selector attributes */
    m_pMediaSelector->setMachineId(machine.GetId());
    m_pMediaSelector->setType(VBoxDefs::MediumType_DVD);
    m_pMediaSelector->repopulate();
}

void UIFirstRunWzdPage2::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIFirstRunWzdPage2::retranslateUi(this);

    /* Wizard page 2 title */
    setTitle(tr("Select Installation Media"));
}

void UIFirstRunWzdPage2::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Initial choice */
    m_pMediaSelector->setCurrentIndex(0);
    sltMediumChanged();

    /* Media selector should initially have focus */
    m_pMediaSelector->setFocus();
}

bool UIFirstRunWzdPage2::isComplete() const
{
    return !vboxGlobal().findMedium(field("id").toString()).isNull();
}

void UIFirstRunWzdPage2::sltMediumChanged()
{
    m_strSource = m_pMediaSelector->currentText();
    m_strId = m_pMediaSelector->id();
    emit completeChanged();
}

void UIFirstRunWzdPage2::sltOpenVirtualMediaManager()
{
    /* Get opened vboxMedium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(m_pMediaSelector->type(), this);
    /* Update medium-combo if necessary: */
    if (!strMediumId.isNull())
        m_pMediaSelector->setCurrentItem(strMediumId);
}

UIFirstRunWzdPage3::UIFirstRunWzdPage3()
{
    /* Decorate page */
    Ui::UIFirstRunWzdPage3::setupUi(this);

    /* Register CMachine class */
    qRegisterMetaType<CMachine>();

    /* Register 'machine' field */
    registerField("machine", this, "machine");

    /* Disable the background painting of the summary widget */
    m_pSummaryText->viewport()->setAutoFillBackground(false);
    /* Make the summary field read-only */
    m_pSummaryText->setReadOnly(true);
}

void UIFirstRunWzdPage3::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    bool fIsBootHDAttached = UIFirstRunWzd::isBootHardDiskAttached(machine);
    m_pPage3Text1Var1->setHidden(!fIsBootHDAttached);
    m_pPage3Text1Var2->setHidden(fIsBootHDAttached);
    m_pPage3Text2Var1->setHidden(!fIsBootHDAttached);
    m_pPage3Text2Var2->setHidden(fIsBootHDAttached);
}

void UIFirstRunWzdPage3::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIFirstRunWzdPage3::retranslateUi(this);

    /* Wizard page 3 title */
    setTitle(tr("Summary"));

    /* Compose common summary */
    QString summary;

    QString description = tr("CD/DVD-ROM Device");
    QString source = field("source").toString();

    summary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td><nobr>%2</nobr></td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td><nobr>%4</nobr></td></tr>"
    )
    .arg(tr("Type", "summary"), description)
    .arg(tr("Source", "summary"), source)
    ;
    /* Feat summary to 3 lines */
    setSummaryFieldLinesNumber(m_pSummaryText, 2);

    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + summary + "</table>");
}

void UIFirstRunWzdPage3::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Summary should initially have focus */
    m_pSummaryText->setFocus();
}

void UIFirstRunWzdPage3::cleanupPage()
{
    /* Do not call superclass method! */
}

bool UIFirstRunWzdPage3::validatePage()
{
    startProcessing();
    bool fResult = insertDevice();
    endProcessing();
    return fResult;
}

bool UIFirstRunWzdPage3::insertDevice()
{
    /* Get 'vbox' global object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Determine machine 'OS type': */
    const CGuestOSType &osType = vbox.GetGuestOSType(m_Machine.GetOSTypeId());
    /* Determine recommended controller's 'bus' & 'type': */
    KStorageBus dvdCtrBus = osType.GetRecommendedDvdStorageBus();
    KStorageControllerType dvdCtrType = osType.GetRecommendedDvdStorageController();
    /* Declare null 'dvd' attachment: */
    CMediumAttachment cda;
    /* Enumerate attachments vector: */
    const CMediumAttachmentVector &attachments = m_Machine.GetMediumAttachments();
    for (int i = 0; i < attachments.size(); ++i)
    {
        /* Get current attachment: */
        const CMediumAttachment &attachment = attachments[i];
        /* Determine attachment's controller: */
        const CStorageController &controller = m_Machine.GetStorageControllerByName(attachment.GetController());
        /* If controller's 'bus' & 'type' are recommended and attachment's 'type' is 'dvd': */
        if (controller.GetBus() == dvdCtrBus &&
            controller.GetControllerType() == dvdCtrType &&
            attachment.GetType() == KDeviceType_DVD)
        {
            /* Remember attachment: */
            cda = attachment;
            break;
        }
    }
    AssertMsg(!cda.isNull(), ("Storage Controller is NOT properly configured!\n"));
    /* Get chosen 'dvd' medium to mount: */
    QString mediumId = field("id").toString();
    VBoxMedium vmedium = vboxGlobal().findMedium(mediumId);
    CMedium medium = vmedium.medium();              // @todo r=dj can this be cached somewhere?
    /* Mount medium to the predefined port/device: */
    m_Machine.MountMedium(cda.GetController(), cda.GetPort(), cda.GetDevice(), medium, false /* force */);
    if (m_Machine.isOk())
        return true;
    else
    {
        msgCenter().cannotRemountMedium(this, m_Machine, vmedium,
                                          true /* mount? */, false /* retry? */);
        return false;
    }
}

