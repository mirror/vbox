/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIFirstRunWzd class implementation
 */

/*
 * Copyright (C) 2008-2010 Sun Microsystems, Inc.
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
#include "UIFirstRunWzd.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxMediaManagerDlg.h"
#include "VBoxVMSettingsHD.h"

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

    /* Translate */
    retranslateUi();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_first_run_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark */
    assignWatermark(":/vmw_new_welcome.png");
#endif /* Q_WS_MAC */
}

void UIFirstRunWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("First Run Wizard"));
}

UIFirstRunWzdPage1::UIFirstRunWzdPage1()
{
    /* Decorate page */
    Ui::UIFirstRunWzdPage1::setupUi(this);

    /* Translate */
    retranslateUi();
}

void UIFirstRunWzdPage1::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    CMediumAttachment hda = machine.GetMediumAttachment(VBoxVMSettingsHD::tr("IDE Controller"), 0, 0);
    m_pPage1Text1Var1->setHidden(hda.isNull());
    m_pPage1Text1Var2->setHidden(!hda.isNull());
}

void UIFirstRunWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIFirstRunWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the First Run Wizard!"));
}

UIFirstRunWzdPage2::UIFirstRunWzdPage2()
{
    /* Decorate page */
    Ui::UIFirstRunWzdPage2::setupUi(this);

    /* Register KStorageBus class */
    qRegisterMetaType<KStorageBus>();

    /* Register 'type', 'description', 'source', 'id' fields! */
    registerField("bus", this, "bus");
    registerField("description", this, "description");
    registerField("source", this, "source");
    registerField("id", this, "id");

    /* Setup contents */
    m_pSelectMediaButton->setIcon(VBoxGlobal::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));

    /* Setup connections */
    connect (m_pTypeCD, SIGNAL(clicked()), this, SLOT(sltMediumChanged()));
    connect (m_pTypeFD, SIGNAL(clicked()), this, SLOT(sltMediumChanged()));
    connect (m_pMediaSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(sltMediumChanged()));
    connect (m_pSelectMediaButton, SIGNAL(clicked()), this, SLOT(sltOpenVirtualMediaManager()));

    /* Translate */
    retranslateUi();
}

void UIFirstRunWzdPage2::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    CMediumAttachment hda = machine.GetMediumAttachment(VBoxVMSettingsHD::tr("IDE Controller"), 0, 0);
    m_pPage2Text1Var1->setHidden(hda.isNull());
    m_pPage2Text1Var2->setHidden(!hda.isNull());
    m_pPage2Text2Var1->setHidden(hda.isNull());
    m_pPage2Text2Var2->setHidden(!hda.isNull());

    /* Assign selector machine */
    m_pMediaSelector->setMachineId(machine.GetId());
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
    /* Translate */
    retranslateUi();

    /* Initial choice */
    m_pTypeCD->click();
    m_pMediaSelector->setCurrentIndex(0);

    /* CD button should initially have focus */
    m_pTypeCD->isChecked();
}

bool UIFirstRunWzdPage2::isComplete() const
{
    return !vboxGlobal().findMedium(field("id").toString()).isNull();
}

void UIFirstRunWzdPage2::sltMediumChanged()
{
    /* Update data */
    if (m_pTypeCD->isChecked())
    {
        if (m_pMediaSelector->type() != VBoxDefs::MediumType_DVD)
        {
            m_pMediaSelector->setType(VBoxDefs::MediumType_DVD);
            m_pMediaSelector->repopulate();
        }
        m_Bus = KStorageBus_IDE;
        m_strDescription = VBoxGlobal::removeAccelMark(m_pTypeCD->text());
        m_strSource = m_pMediaSelector->currentText();
        m_strId = m_pMediaSelector->id();
    }
    else if (m_pTypeFD->isChecked())
    {
        if (m_pMediaSelector->type() != VBoxDefs::MediumType_Floppy)
        {
            m_pMediaSelector->setType(VBoxDefs::MediumType_Floppy);
            m_pMediaSelector->repopulate();
        }
        m_Bus = KStorageBus_Floppy;
        m_strDescription = VBoxGlobal::removeAccelMark(m_pTypeFD->text());
        m_strSource = m_pMediaSelector->currentText();
        m_strId = m_pMediaSelector->id();
    }
    else
    {
        m_Bus = KStorageBus_Null;
        m_strDescription.clear();
        m_strSource.clear();
        m_strId.clear();
    }

    emit completeChanged();
}

void UIFirstRunWzdPage2::sltOpenVirtualMediaManager()
{
    /* Create & open VMM */
    VBoxMediaManagerDlg dlg(this);
    dlg.setup(m_pMediaSelector->type(), true /* aDoSelect */);
    if (dlg.exec() == QDialog::Accepted)
        m_pMediaSelector->setCurrentItem(dlg.selectedId());
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
    m_pSummaryText->viewport()->setAutoFillBackground (false);
    /* Make the summary field read-only */
    m_pSummaryText->setReadOnly (true);

    /* Translate */
    retranslateUi();
}

void UIFirstRunWzdPage3::init()
{
    /* Current machine */
    CMachine machine = field("machine").value<CMachine>();
    AssertMsg(!machine.isNull(), ("Field 'machine' must be set!\n"));

    /* Hide unnecessary text labels */
    CMediumAttachment hda = machine.GetMediumAttachment(VBoxVMSettingsHD::tr("IDE Controller"), 0, 0);
    m_pPage3Text1Var1->setHidden(hda.isNull());
    m_pPage3Text1Var2->setHidden(!hda.isNull());
    m_pPage3Text2Var1->setHidden(hda.isNull());
    m_pPage3Text2Var2->setHidden(!hda.isNull());
}

void UIFirstRunWzdPage3::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIFirstRunWzdPage3::retranslateUi(this);

    /* Wizard page 3 title */
    setTitle(tr("Summary"));

    /* Compose common summary */
    QString summary;

    QString description = field("description").toString();
    QString source = field("source").toString();

    summary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td><nobr>%2</nobr></td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td><nobr>%4</nobr></td></tr>"
    )
    .arg (tr("Type", "summary"), description)
    .arg (tr("Source", "summary"), source)
    ;
    /* Feat summary to 3 lines */
    setSummaryFieldLinesNumber(m_pSummaryText, 2);

    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + summary + "</table>");
}

void UIFirstRunWzdPage3::initializePage()
{
    /* Translate */
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
    return insertDevice();
}

bool UIFirstRunWzdPage3::insertDevice()
{
    /* Composing default controller name */
    KStorageBus bus = field("bus").value<KStorageBus>();
    QString mediumId = field("id").toString();
    LONG port = bus == KStorageBus_IDE ? 1 : 0;
    LONG device = 0;
    QString name;
    /* Search for the first controller of the given type */
    QVector<CStorageController> controllers = m_Machine.GetStorageControllers();
    foreach (CStorageController controller, controllers)
    {
        if (controller.GetBus() == bus)
        {
            name = controller.GetName();
            break;
        }
    }
    Assert (!name.isEmpty());

    /* Mount medium to the predefined port/device */
    m_Machine.MountMedium(name, port, device, mediumId, false /* force */);
    if (m_Machine.isOk())
        return true;
    else
    {
        vboxProblem().cannotRemountMedium(this, m_Machine, vboxGlobal().findMedium(mediumId),
                                          true /* mount? */, false /* retry? */);
        return false;
    }
}
