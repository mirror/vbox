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
#include "UIWizardCloneVMPageBasic1.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardCloneVMPageExpert::UIWizardCloneVMPageExpert(const QString &strOriginalName, const QString &strDefaultPath,
                                                     bool /*fAdditionalInfo*/, bool fShowChildsOption, const QString &strGroup)
    : m_pMainLayout(0)
    , m_pNamePathGroupBox(0)
    , m_pCloneTypeGroupBox(0)
    , m_pCloneModeGroupBox(0)
    , m_pAdditionalOptionsGroupBox(0)
    , m_strGroup(strGroup)
{
    prepare(strOriginalName, strDefaultPath, fShowChildsOption);
}

void UIWizardCloneVMPageExpert::prepare(const QString &strOriginalName, const QString &strDefaultPath, bool fShowChildsOption)
{
    m_pMainLayout = new QGridLayout(this);
    AssertReturnVoid(m_pMainLayout);
    m_pNamePathGroupBox = new UICloneVMNamePathEditor(strOriginalName, strDefaultPath);
    if (m_pNamePathGroupBox)
    {
        m_pMainLayout->addWidget(m_pNamePathGroupBox, 0, 0, 3, 2);
        connect(m_pNamePathGroupBox, &UICloneVMNamePathEditor::sigCloneNameChanged,
                this, &UIWizardCloneVMPageExpert::sltCloneNameChanged);
        connect(m_pNamePathGroupBox, &UICloneVMNamePathEditor::sigClonePathChanged,
                this, &UIWizardCloneVMPageExpert::sltClonePathChanged);
    }

    m_pCloneTypeGroupBox = new UICloneVMCloneTypeGroupBox;
    if (m_pCloneTypeGroupBox)
        m_pMainLayout->addWidget(m_pCloneTypeGroupBox, 3, 0, 2, 1);

    m_pCloneModeGroupBox = new UICloneVMCloneModeGroupBox(fShowChildsOption);
    if (m_pCloneModeGroupBox)
        m_pMainLayout->addWidget(m_pCloneModeGroupBox, 3, 1, 2, 1);

    m_pAdditionalOptionsGroupBox = new UICloneVMAdditionalOptionsEditor;
    if (m_pAdditionalOptionsGroupBox)
    {
        m_pMainLayout->addWidget(m_pAdditionalOptionsGroupBox, 5, 0, 2, 2);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigMACAddressClonePolicyChanged,
                this, &UIWizardCloneVMPageExpert::sltMACAddressClonePolicyChanged);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigKeepDiskNamesToggled,
                this, &UIWizardCloneVMPageExpert::sltKeepDiskNamesToggled);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigKeepHardwareUUIDsToggled,
                this, &UIWizardCloneVMPageExpert::sltKeepHardwareUUIDsToggled);
    }

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
    if (m_pAdditionalOptionsGroupBox)
        m_pAdditionalOptionsGroupBox->setTitle(UIWizardCloneVM::tr("Additional options"));
}

void UIWizardCloneVMPageExpert::initializePage()
{
    if (m_pNamePathGroupBox)
    {
        m_pNamePathGroupBox->setFocus();
        cloneVMWizardPropertySet(CloneName, m_pNamePathGroupBox->cloneName());
        cloneVMWizardPropertySet(CloneFilePath,
                                 UIWizardCloneVMNamePage::composeCloneFilePath(m_pNamePathGroupBox->cloneName(), m_strGroup, m_pNamePathGroupBox->clonePath()));
    }
    if (m_pAdditionalOptionsGroupBox)
    {
        cloneVMWizardPropertySet(MacAddressPolicy, m_pAdditionalOptionsGroupBox->macAddressClonePolicy());
        cloneVMWizardPropertySet(KeepDiskNames, m_pAdditionalOptionsGroupBox->keepDiskNames());
        cloneVMWizardPropertySet(KeepHardwareUUIDs, m_pAdditionalOptionsGroupBox->keepHardwareUUIDs());
    }
    if (m_pCloneTypeGroupBox)
        cloneVMWizardPropertySet(LinkedClone, !m_pCloneTypeGroupBox->isFullClone());
    if (m_pCloneModeGroupBox)
        cloneVMWizardPropertySet(CloneMode, m_pCloneModeGroupBox->cloneMode());

    retranslateUi();
}

bool UIWizardCloneVMPageExpert::isComplete() const
{
    return m_pNamePathGroupBox && m_pNamePathGroupBox->isComplete(m_strGroup);
}

bool UIWizardCloneVMPageExpert::validatePage()
{
    return qobject_cast<UIWizardCloneVM*>(wizard())->cloneVM();;
}

void UIWizardCloneVMPageExpert::sltCloneNameChanged(const QString &strCloneName)
{
    AssertReturnVoid(m_pNamePathGroupBox);
    cloneVMWizardPropertySet(CloneName, strCloneName);
    cloneVMWizardPropertySet(CloneFilePath,
                             UIWizardCloneVMNamePage::composeCloneFilePath(strCloneName, m_strGroup, m_pNamePathGroupBox->clonePath()));
}

void UIWizardCloneVMPageExpert::sltClonePathChanged(const QString &strClonePath)
{
    AssertReturnVoid(m_pNamePathGroupBox);
    cloneVMWizardPropertySet(CloneFilePath,
                             UIWizardCloneVMNamePage::composeCloneFilePath(m_pNamePathGroupBox->cloneName(), m_strGroup, strClonePath));
}

void UIWizardCloneVMPageExpert::sltMACAddressClonePolicyChanged(MACAddressClonePolicy enmMACAddressClonePolicy)
{
    cloneVMWizardPropertySet(MacAddressPolicy, enmMACAddressClonePolicy);
}

void UIWizardCloneVMPageExpert::sltKeepDiskNamesToggled(bool fKeepDiskNames)
{
    cloneVMWizardPropertySet(KeepDiskNames, fKeepDiskNames);
}

void UIWizardCloneVMPageExpert::sltKeepHardwareUUIDsToggled(bool fKeepHardwareUUIDs)
{
    cloneVMWizardPropertySet(KeepHardwareUUIDs, fKeepHardwareUUIDs);
}
