/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMSummaryPage class implementation.
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
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMediumSelector.h"
#include "UIMediumSizeEditor.h"
#include "UIMessageCenter.h"
#include "UICommon.h"
#include "UIWizardNewVMSummaryPage.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "COMEnums.h"
#include "CGuestOSType.h"
#include "CSystemProperties.h"


UIWizardNewVMSummaryPage::UIWizardNewVMSummaryPage()
    : m_pLabel(0)
{
    prepare();
}


void UIWizardNewVMSummaryPage::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);

    pMainLayout->addStretch();

    createConnections();
}

void UIWizardNewVMSummaryPage::createConnections()
{
}

void UIWizardNewVMSummaryPage::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Summary"));
}

void UIWizardNewVMSummaryPage::initializePage()
{
    retranslateUi();

}

bool UIWizardNewVMSummaryPage::isComplete() const
{

    return true;
}

bool UIWizardNewVMSummaryPage::validatePage()
{
    bool fResult = true;
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);

    /* Make sure user really intents to creae a vm with no hard drive: */
    if (pWizard->diskSource() == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        if (!pWizard->emptyDiskRecommended())
        {
            if (!msgCenter().confirmHardDisklessMachine(this))
                return false;
        }
    }
    else if (pWizard->diskSource() == SelectedDiskSource_New)
    {
        /* Check if the path we will be using for hard drive creation exists: */
        const QString &strMediumPath = pWizard->mediumPath();
        fResult = !QFileInfo(strMediumPath).exists();
        if (!fResult)
        {
            msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
            return fResult;
        }
        /* Check FAT size limitation of the host hard drive: */
        fResult = UIDiskEditorGroupBox::checkFATSizeLimitation(pWizard->mediumVariant(),
                                                                strMediumPath,
                                                                pWizard->mediumSize());
        if (!fResult)
        {
            msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
            return fResult;
        }

        /* Try to create the hard drive:*/
        fResult = pWizard->createVirtualDisk();
        /*Don't show any error message here since UIWizardNewVM::createVirtualDisk already does so: */
        if (!fResult)
            return fResult;
    }

    fResult = pWizard->createVM();
    /* Try to delete the hard disk: */
    if (!fResult)
        pWizard->deleteVirtualDisk();

    return fResult;
}
