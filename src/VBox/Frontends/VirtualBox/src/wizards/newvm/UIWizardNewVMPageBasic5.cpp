/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic5 class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QMetaType>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UICommon.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic5.h"

/* COM includes: */
#include "CGuestOSType.h"
#include "CSystemProperties.h"

UIWizardNewVMPageBasic5::UIWizardNewVMPageBasic5()
{
    prepare();
    qRegisterMetaType<CMedium>();
    registerField("mediumFormat", this, "mediumFormat");
    registerField("mediumVariant" /* KMediumVariant */, this, "mediumVariant");
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");

    /* We do not have any UI elements for HDD format selection since we default to VDI in case of guided wizard mode: */
    bool fFoundVDI = false;
    CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
    foreach (const CMediumFormat &format, formats)
    {
        if (format.GetName() == "VDI")
        {
            m_mediumFormat = format;
            fFoundVDI = true;
        }
    }
    if (!fFoundVDI)
        AssertMsgFailed(("No medium format corresponding to VDI could be found!"));

    m_strDefaultExtension =  defaultExtension(m_mediumFormat);

    /* Since the medium format is static we can decide widget visibility here: */
    setWidgetVisibility(m_mediumFormat);
}

CMediumFormat UIWizardNewVMPageBasic5::mediumFormat() const
{
    return m_mediumFormat;
}

void UIWizardNewVMPageBasic5::prepare()
{


    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->addWidget(createMediumVariantWidgets(true));
    pMainLayout->addStretch();

    // connect(m_pVariantButtonGroup,static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
    //         this, &UIWizardNewVDPageBasic2::completeChanged);
    // connect(m_pSplitBox, &QCheckBox::stateChanged,
    //         this, &UIWizardNewVDPageBasic2::completeChanged);

    // QVBoxLayout *pMainLayout = new QVBoxLayout(this);


    // m_pLabel = new QIRichTextLabel(this);
    // pMainLayout->addWidget(m_pLabel);

    // pMainLayout->addStretch();
    // createConnections();
}

void UIWizardNewVMPageBasic5::createConnections()
{
}

void UIWizardNewVMPageBasic5::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Storage on physical hard disk"));
    UIWizardNewVDPage2::retranslateWidgets();


    //retranslateWidgets();
}

void UIWizardNewVMPageBasic5::initializePage()
{
    /* We set the medium name and path according to machine name/path and do let user change these in the guided mode: */
    QString strDefaultName = fieldImp("machineBaseName").toString();
    m_strDefaultName = strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName;
    m_strDefaultPath = fieldImp("machineFolder").toString();
    // fieldImp("type").value<CGuestOSType>().GetRecommendedHDD()

    retranslateUi();
}

void UIWizardNewVMPageBasic5::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic5::isComplete() const
{
    return true;
}
