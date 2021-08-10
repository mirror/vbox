/* $Id$ */
/** @file
 * VBox Qt GUI - UIUserNamePasswordEditor class implementation.
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
// #include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
// #include <QDir>
// #include <QFileInfo>
#include <QLabel>
// #include <QRadioButton>
#include <QGridLayout>

/* GUI includes: */
#include "QILineEdit.h"
// #include "QIToolButton.h"
// #include "QIRichTextLabel.h"
#include "UICommon.h"
// #include "UIConverter.h"
#include "UIFilePathSelector.h"
// #include "UIHostnameDomainNameEditor.h"
// #include "UIIconPool.h"
// #include "UIMediumSizeEditor.h"
// #include "UIUserNamePasswordEditor.h"
// #include "UIWizardDiskEditors.h"
// #include "UIWizardNewVM.h"
// #include "UIWizardNewVMDiskPageBasic.h"
#include "UIWizardCloneVMEditors.h"

/* Other VBox includes: */
#include "iprt/assert.h"
// #include "iprt/fs.h"
#include "COMEnums.h"
#include "CSystemProperties.h"


/*********************************************************************************************************************************
*   UICloneVMNamePathEditor implementation.                                                                                      *
*********************************************************************************************************************************/

UICloneVMNamePathEditor::UICloneVMNamePathEditor(const QString &strOriginalName, const QString &strDefaultPath, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pContainerLayout(0)
    , m_pNameLineEdit(0)
    , m_pPathSelector(0)
    , m_pNameLabel(0)
    , m_pPathLabel(0)
    , m_strOriginalName(strOriginalName)
    , m_strDefaultPath(strDefaultPath)
{
    prepare();
}

void UICloneVMNamePathEditor::setFirstColumnWidth(int iWidth)
{
    if (m_pContainerLayout)
        m_pContainerLayout->setColumnMinimumWidth(0, iWidth);
}

int UICloneVMNamePathEditor::firstColumnWidth() const
{
    int iMaxWidth = 0;
    if (m_pNameLabel)
        iMaxWidth = qMax(iMaxWidth, m_pNameLabel->minimumSizeHint().width());
    if (m_pPathLabel)
        iMaxWidth = qMax(iMaxWidth, m_pPathLabel->minimumSizeHint().width());
    return iMaxWidth;
}

void UICloneVMNamePathEditor::prepare()
{
    m_pContainerLayout = new QGridLayout(this);
    m_pContainerLayout->setContentsMargins(0, 0, 0, 0);

    m_pNameLabel = new QLabel;
    if (m_pNameLabel)
    {
        m_pNameLabel->setAlignment(Qt::AlignRight);
        m_pNameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_pContainerLayout->addWidget(m_pNameLabel, 0, 0, 1, 1);
    }

    m_pNameLineEdit = new QILineEdit();
    if (m_pNameLineEdit)
    {
        m_pContainerLayout->addWidget(m_pNameLineEdit, 0, 1, 1, 1);
        m_pNameLineEdit->setText(tr("%1 Clone").arg(m_strOriginalName));
    }

    m_pPathLabel = new QLabel(this);
    if (m_pPathLabel)
    {
        m_pPathLabel->setAlignment(Qt::AlignRight);
        m_pPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_pContainerLayout->addWidget(m_pPathLabel, 1, 0, 1, 1);
    }

    m_pPathSelector = new UIFilePathSelector(this);
    if (m_pPathSelector)
    {
        m_pContainerLayout->addWidget(m_pPathSelector, 1, 1, 1, 1);
        m_pPathSelector->setPath(m_strDefaultPath);
    }

    retranslateUi();
}

void UICloneVMNamePathEditor::retranslateUi()
{
    if (m_pNameLabel)
        m_pNameLabel->setText(tr("Name:"));

    if (m_pPathLabel)
        m_pPathLabel->setText(tr("Path:"));
}



/*********************************************************************************************************************************
*   UICloneVMAdditionalOptionsEditor implementation.                                                                             *
*********************************************************************************************************************************/


UICloneVMAdditionalOptionsEditor::UICloneVMAdditionalOptionsEditor(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pContainerLayout(0)
    , m_pMACComboBoxLabel(0)
    , m_pMACComboBox(0)
    , m_pAdditionalOptionsLabel(0)
    , m_pKeepDiskNamesCheckBox(0)
    , m_pKeepHWUUIDsCheckBox(0)
{
    prepare();
}

void UICloneVMAdditionalOptionsEditor::setFirstColumnWidth(int iWidth)
{
    if (m_pContainerLayout)
        m_pContainerLayout->setColumnMinimumWidth(0, iWidth);
}

int UICloneVMAdditionalOptionsEditor::firstColumnWidth() const
{
    int iMaxWidth = 0;
    if (m_pMACComboBoxLabel)
        iMaxWidth = qMax(iMaxWidth, m_pMACComboBoxLabel->minimumSizeHint().width());
    if (m_pAdditionalOptionsLabel)
        iMaxWidth = qMax(iMaxWidth, m_pAdditionalOptionsLabel->minimumSizeHint().width());
    return iMaxWidth;
}

MACAddressClonePolicy UICloneVMAdditionalOptionsEditor::macAddressClonePolicy() const
{
    return m_pMACComboBox->currentData().value<MACAddressClonePolicy>();
}

void UICloneVMAdditionalOptionsEditor::setMACAddressClonePolicy(MACAddressClonePolicy enmMACAddressClonePolicy)
{
    const int iIndex = m_pMACComboBox->findData(enmMACAddressClonePolicy);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pMACComboBox->setCurrentIndex(iIndex);
}

void UICloneVMAdditionalOptionsEditor::prepare()
{
    m_pContainerLayout = new QGridLayout(this);
    m_pContainerLayout->setContentsMargins(0, 0, 0, 0);

    m_pMACComboBoxLabel = new QLabel;
    if (m_pMACComboBoxLabel)
    {
        m_pMACComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
        m_pMACComboBoxLabel->setBuddy(m_pMACComboBox);
        m_pContainerLayout->addWidget(m_pMACComboBoxLabel, 2, 0, 1, 1);
    }

    m_pMACComboBox = new QComboBox;
    if (m_pMACComboBox)
        m_pContainerLayout->addWidget(m_pMACComboBox, 2, 1, 1, 1);
    populateMACAddressClonePolicies();


    /* Load currently supported clone options: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KCloneOptions> supportedOptions = comProperties.GetSupportedCloneOptions();
    /* Check whether we support additional clone options at all: */
    int iVerticalPosition = 3;
    const bool fSupportedKeepDiskNames = supportedOptions.contains(KCloneOptions_KeepDiskNames);
    const bool fSupportedKeepHWUUIDs = supportedOptions.contains(KCloneOptions_KeepHwUUIDs);
    if (fSupportedKeepDiskNames || fSupportedKeepHWUUIDs)
    {
        m_pAdditionalOptionsLabel = new QLabel;
        if (m_pAdditionalOptionsLabel)
        {
            m_pAdditionalOptionsLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
            m_pContainerLayout->addWidget(m_pAdditionalOptionsLabel, iVerticalPosition, 0, 1, 1);
        }
    }
    if (fSupportedKeepDiskNames)
    {
        m_pKeepDiskNamesCheckBox = new QCheckBox;
        if (m_pKeepDiskNamesCheckBox)
            m_pContainerLayout->addWidget(m_pKeepDiskNamesCheckBox, iVerticalPosition++, 1, 1, 1);
    }
    if (fSupportedKeepHWUUIDs)
    {
        m_pKeepHWUUIDsCheckBox = new QCheckBox;
        if (m_pKeepHWUUIDsCheckBox)
            m_pContainerLayout->addWidget(m_pKeepHWUUIDsCheckBox, iVerticalPosition++, 1, 1, 1);
    }


    retranslateUi();
}

void UICloneVMAdditionalOptionsEditor::retranslateUi()
{
    m_pMACComboBoxLabel->setText(tr("MAC Address &Policy:"));
    for (int i = 0; i < m_pMACComboBox->count(); ++i)
    {
        const MACAddressClonePolicy enmPolicy = m_pMACComboBox->itemData(i).value<MACAddressClonePolicy>();
        switch (enmPolicy)
        {
            case MACAddressClonePolicy_KeepAllMACs:
            {
                m_pMACComboBox->setItemText(i, tr("Include all network adapter MAC addresses"));
                m_pMACComboBox->setItemData(i, tr("Include all network adapter MAC addresses during cloning."), Qt::ToolTipRole);
                break;
            }
            case MACAddressClonePolicy_KeepNATMACs:
            {
                m_pMACComboBox->setItemText(i, tr("Include only NAT network adapter MAC addresses"));
                m_pMACComboBox->setItemData(i, tr("Include only NAT network adapter MAC addresses during cloning."), Qt::ToolTipRole);
                break;
            }
            case MACAddressClonePolicy_StripAllMACs:
            {
                m_pMACComboBox->setItemText(i, tr("Generate new MAC addresses for all network adapters"));
                m_pMACComboBox->setItemData(i, tr("Generate new MAC addresses for all network adapters during cloning."), Qt::ToolTipRole);
                break;
            }
            default:
                break;
        }
    }

    if (m_pAdditionalOptionsLabel)
        m_pAdditionalOptionsLabel->setText(tr("Additional Options:"));
    if (m_pKeepDiskNamesCheckBox)
    {
        m_pKeepDiskNamesCheckBox->setToolTip(tr("Don't change the disk names during cloning."));
        m_pKeepDiskNamesCheckBox->setText(tr("Keep &Disk Names"));
    }
    if (m_pKeepHWUUIDsCheckBox)
    {
        m_pKeepHWUUIDsCheckBox->setToolTip(tr("Don't change hardware UUIDs during cloning."));
        m_pKeepHWUUIDsCheckBox->setText(tr("Keep &Hardware UUIDs"));
    }

}

void UICloneVMAdditionalOptionsEditor::populateMACAddressClonePolicies()
{
    AssertReturnVoid(m_pMACComboBox && m_pMACComboBox->count() == 0);

    /* Map known clone options to known MAC address export policies: */
    QMap<KCloneOptions, MACAddressClonePolicy> knownOptions;
    knownOptions[KCloneOptions_KeepAllMACs] = MACAddressClonePolicy_KeepAllMACs;
    knownOptions[KCloneOptions_KeepNATMACs] = MACAddressClonePolicy_KeepNATMACs;

    /* Load currently supported clone options: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KCloneOptions> supportedOptions = comProperties.GetSupportedCloneOptions();

    /* Check which of supported options/policies are known: */
    QList<MACAddressClonePolicy> supportedPolicies;
    foreach (const KCloneOptions &enmOption, supportedOptions)
        if (knownOptions.contains(enmOption))
            supportedPolicies << knownOptions.value(enmOption);

    /* Add supported policies first: */
    foreach (const MACAddressClonePolicy &enmPolicy, supportedPolicies)
        m_pMACComboBox->addItem(QString(), QVariant::fromValue(enmPolicy));

    /* Add hardcoded policy finally: */
    m_pMACComboBox->addItem(QString(), QVariant::fromValue(MACAddressClonePolicy_StripAllMACs));

    /* Set default: */
    if (supportedPolicies.contains(MACAddressClonePolicy_KeepNATMACs))
        setMACAddressClonePolicy(MACAddressClonePolicy_KeepNATMACs);
    else
        setMACAddressClonePolicy(MACAddressClonePolicy_StripAllMACs);
}
