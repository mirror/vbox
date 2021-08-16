/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageExpert class implementation.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

/* GUI includes: */
#include "QILineEdit.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIWizardCloneVMPageExpert.h"
#include "UIWizardCloneVM.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardCloneVMPageExpert::UIWizardCloneVMPageExpert(const QString &strOriginalName, const QString &strDefaultPath,
                                                     bool /*fAdditionalInfo*/, bool fShowChildsOption, const QString &/*strGroup*/)
    : m_pMainLayout(0)
    , m_pNamePathGroupBox(0)
    , m_pCloneTypeGroupBox(0)
    , m_pCloneModeGroupBox(0)
    , m_pAdditionalOptionsroupBox(0)
{
    prepare(strOriginalName, strDefaultPath, fShowChildsOption);
}

void UIWizardCloneVMPageExpert::prepare(const QString &strOriginalName, const QString &strDefaultPath, bool fShowChildsOption)
{
    m_pMainLayout = new QGridLayout(this);
    AssertReturnVoid(m_pMainLayout);
    m_pNamePathGroupBox = new UICloneVMNamePathEditor(strOriginalName, strDefaultPath);
    if (m_pNamePathGroupBox)
        m_pMainLayout->addWidget(m_pNamePathGroupBox, 0, 0, 3, 2);

    m_pCloneTypeGroupBox = new UICloneVMCloneTypeGroupBox;
    if (m_pCloneTypeGroupBox)
        m_pMainLayout->addWidget(m_pCloneTypeGroupBox, 3, 0, 2, 1);

    m_pCloneModeGroupBox = new UICloneVMCloneModeGroupBox(fShowChildsOption);
    if (m_pCloneModeGroupBox)
        m_pMainLayout->addWidget(m_pCloneModeGroupBox, 3, 1, 2, 1);

    m_pAdditionalOptionsroupBox = new UICloneVMAdditionalOptionsEditor;
    if (m_pAdditionalOptionsroupBox)
        m_pMainLayout->addWidget(m_pAdditionalOptionsroupBox, 5, 0, 2, 2);
    retranslateUi();
}

void UIWizardCloneVMPageExpert::retranslateUi()
{
    /* Translate widgets: */
    if (m_pNamePathGroupBox)
        m_pNamePathGroupBox->setTitle(UIWizardCloneVM::tr("New machine &name and path"));
    if (m_pCloneTypeGroupBox)
        m_pCloneTypeGroupBox->setTitle(UIWizardCloneVM::tr("Clone type"));
    if (m_pCloneModeGroupBox)
        m_pCloneModeGroupBox->setTitle(UIWizardCloneVM::tr("Snapshots"));
    if (m_pAdditionalOptionsroupBox)
        m_pAdditionalOptionsroupBox->setTitle(UIWizardCloneVM::tr("Additional options"));
}

void UIWizardCloneVMPageExpert::initializePage()
{
    retranslateUi();
}

bool UIWizardCloneVMPageExpert::isComplete() const
{
    // if (!m_pPathSelector)
    //     return false;

    // QString path = m_pPathSelector->path();
    // if (path.isEmpty())
    //     return false;
    // /* Make sure VM name feat the rules: */
    // QString strName = m_pNameLineEdit->text().trimmed();
    // return !strName.isEmpty() && strName != m_strOriginalName;
    return true;
}

bool UIWizardCloneVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    // /* Lock finish button: */
    // startProcessing();

    // /* Trying to clone VM: */
    // if (fResult)
    //     fResult = qobject_cast<UIWizardCloneVM*>(wizard())->cloneVM();

    // /* Unlock finish button: */
    // endProcessing();

    /* Return result: */
    return fResult;
}

//void UIWizardCloneVMPageExpert::sltNameChanged()
//{
    //composeCloneFilePath();
//}

//void UIWizardCloneVMPageExpert::sltPathChanged()
//{
    //composeCloneFilePath();
//}
