/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageExpert class implementation.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Global includes: */
# include <QButtonGroup>
# include <QCheckBox>
# include <QGridLayout>
# include <QGroupBox>
# include <QLabel>
# include <QRadioButton>

/* Local includes: */
# include "QILineEdit.h"
# include "UIFilePathSelector.h"
# include "UIWizardCloneVMPageExpert.h"
# include "UIWizardCloneVM.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardCloneVMPageExpert::UIWizardCloneVMPageExpert(const QString &strOriginalName, const QString &strDefaultPath,
                                                     bool fAdditionalInfo, bool fShowChildsOption, const QString &strGroup)
    : UIWizardCloneVMPage1(strOriginalName, strDefaultPath, strGroup)
    , UIWizardCloneVMPage2(fAdditionalInfo)
    , UIWizardCloneVMPage3(fShowChildsOption)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        m_pNameCnt = new QGroupBox(this);
        {
            QGridLayout *pNameCntLayout = new QGridLayout(m_pNameCnt);
            {
                pNameCntLayout->setContentsMargins(0, 0, 0, 0);

                m_pNameLabel = new QLabel;
                if (m_pNameLabel)
                {
                    m_pNameLabel->setAlignment(Qt::AlignRight);
                    m_pNameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                    pNameCntLayout->addWidget(m_pNameLabel, 0, 0, 1, 1);

                }
                m_pNameLineEdit = new QILineEdit(m_pNameCnt);
                if (m_pNameLineEdit)
                {
                    m_pNameLineEdit->setText(UIWizardCloneVM::tr("%1 Clone").arg(m_strOriginalName));
                    pNameCntLayout->addWidget(m_pNameLineEdit, 0, 1, 1, 1);
                }

                m_pPathLabel = new QLabel(this);
                if (m_pPathLabel)
                {
                    m_pPathLabel->setAlignment(Qt::AlignRight);
                    m_pPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                    pNameCntLayout->addWidget(m_pPathLabel, 1, 0, 1, 1);
                }

                m_pPathSelector = new UIFilePathSelector(this);
                if (m_pPathSelector)
                {
                    pNameCntLayout->addWidget(m_pPathSelector, 1, 1, 1, 1);
                    m_pPathSelector->setPath(m_strDefaultPath);
                }
            }
        }
        m_pCloneTypeCnt = new QGroupBox(this);
        {
            m_pButtonGroup = new QButtonGroup(m_pCloneTypeCnt);
            {
                QVBoxLayout *pCloneTypeCntLayout = new QVBoxLayout(m_pCloneTypeCnt);
                {
                    m_pFullCloneRadio = new QRadioButton(m_pCloneTypeCnt);
                    {
                        m_pFullCloneRadio->setChecked(true);
                    }
                    m_pLinkedCloneRadio = new QRadioButton(m_pCloneTypeCnt);
                    pCloneTypeCntLayout->addWidget(m_pFullCloneRadio);
                    pCloneTypeCntLayout->addWidget(m_pLinkedCloneRadio);
                }
                m_pButtonGroup->addButton(m_pFullCloneRadio);
                m_pButtonGroup->addButton(m_pLinkedCloneRadio);
            }
        }
        m_pCloneModeCnt = new QGroupBox(this);
        {
            QVBoxLayout *pCloneModeCntLayout = new QVBoxLayout(m_pCloneModeCnt);
            {
                m_pMachineRadio = new QRadioButton(m_pCloneModeCnt);
                {
                    m_pMachineRadio->setChecked(true);
                }
                m_pMachineAndChildsRadio = new QRadioButton(m_pCloneModeCnt);
                {
                    if (!m_fShowChildsOption)
                       m_pMachineAndChildsRadio->hide();
                }
                m_pAllRadio = new QRadioButton(m_pCloneModeCnt);
                pCloneModeCntLayout->addWidget(m_pMachineRadio);
                pCloneModeCntLayout->addWidget(m_pMachineAndChildsRadio);
                pCloneModeCntLayout->addWidget(m_pAllRadio);
            }
        }
        m_pCloneOptionsCnt = new QGroupBox(this);
        if (m_pCloneOptionsCnt)
        {
            m_pCloneOptionsLayout = new QGridLayout(m_pCloneOptionsCnt);
            //m_pCloneOptionsLayout->setContentsMargins(0, 0, 0, 0);
            /* Create MAC policy combo-box: */
            m_pMACComboBox = new QComboBox;
            if (m_pMACComboBox)
            {
                /* Add into layout: */
                m_pCloneOptionsLayout->addWidget(m_pMACComboBox, 0, 1, 1, 1);
            }

            /* Create format combo-box label: */
            m_pMACComboBoxLabel = new QLabel;
            if (m_pMACComboBoxLabel)
            {
                m_pMACComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                m_pMACComboBoxLabel->setBuddy(m_pMACComboBox);
                /* Add into layout: */
                m_pCloneOptionsLayout->addWidget(m_pMACComboBoxLabel, 0, 0, 1, 1);
            }
        }

        pMainLayout->addWidget(m_pNameCnt, 0, 0, 1, 2);
        pMainLayout->addWidget(m_pCloneTypeCnt, 1, 0, Qt::AlignTop);
        pMainLayout->addWidget(m_pCloneModeCnt, 1, 1, Qt::AlignTop);
        pMainLayout->addWidget(m_pCloneOptionsCnt, 2, 0, 1, 2, Qt::AlignTop);
        pMainLayout->setRowStretch(3, 1);
    }

    /* Setup connections: */
    connect(m_pNameLineEdit, &QILineEdit::textChanged,
            this, &UIWizardCloneVMPageExpert::completeChanged);
    connect(m_pPathSelector, &UIFilePathSelector::pathChanged,
            this, &UIWizardCloneVMPageExpert::completeChanged);
    connect(m_pNameLineEdit, &QILineEdit::textChanged,
            this, &UIWizardCloneVMPageExpert::sltNameChanged);
    connect(m_pPathSelector, &UIFilePathSelector::pathChanged,
            this, &UIWizardCloneVMPageExpert::sltPathChanged);
    connect(m_pButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*, bool)>(&QButtonGroup::buttonToggled),
            this, &UIWizardCloneVMPageExpert::sltButtonToggled);

    /* Register classes: */
    qRegisterMetaType<KCloneMode>();

    /* Populate MAC address policies: */
    populateMACAddressClonePolicies();

    /* Register fields: */
    registerField("cloneName", this, "cloneName");
    registerField("cloneFilePath", this, "cloneFilePath");
    registerField("linkedClone", this, "linkedClone");
    registerField("cloneMode", this, "cloneMode");
    registerField("macAddressClonePolicy", this, "macAddressClonePolicy");
    composeCloneFilePath();
}

void UIWizardCloneVMPageExpert::sltButtonToggled(QAbstractButton *pButton, bool fChecked)
{
    if (pButton == m_pLinkedCloneRadio && fChecked)
    {
        m_pCloneModeCnt->setEnabled(false);
        m_pMachineRadio->setChecked(true);
    }
    else
    {
        m_pCloneModeCnt->setEnabled(true);
    }
}

void UIWizardCloneVMPageExpert::retranslateUi()
{
    /* Translate widgets: */
    m_pNameCnt->setTitle(UIWizardCloneVM::tr("New machine &name and path"));
    m_pCloneTypeCnt->setTitle(UIWizardCloneVM::tr("Clone type"));
    m_pFullCloneRadio->setText(UIWizardCloneVM::tr("&Full Clone"));
    m_pLinkedCloneRadio->setText(UIWizardCloneVM::tr("&Linked Clone"));
    m_pCloneModeCnt->setTitle(UIWizardCloneVM::tr("Snapshots"));
    m_pMachineRadio->setText(UIWizardCloneVM::tr("Current &machine state"));
    m_pMachineAndChildsRadio->setText(UIWizardCloneVM::tr("Current &snapshot tree branch"));
    m_pAllRadio->setText(UIWizardCloneVM::tr("&Everything"));
    m_pNameLabel->setText(UIWizardCloneVM::tr("Name:"));
    m_pPathLabel->setText(UIWizardCloneVM::tr("Path:"));

    m_pCloneOptionsCnt->setTitle(UIWizardCloneVM::tr("Additional &options"));

    /* Translate MAC address policy combo-box: */
    m_pMACComboBoxLabel->setText(UIWizardCloneVM::tr("MAC Address &Policy:"));
    m_pMACComboBox->setItemText(MACAddressClonePolicy_KeepAllMACs,
                                UIWizardCloneVM::tr("Include all network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressClonePolicy_KeepNATMACs,
                                UIWizardCloneVM::tr("Include only NAT network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressClonePolicy_StripAllMACs,
                                UIWizardCloneVM::tr("Generate new MAC addresses for all network adapters"));
    m_pMACComboBox->setItemData(MACAddressClonePolicy_KeepAllMACs,
                                UIWizardCloneVM::tr("Include all network adapter MAC addresses in exported "
                                                      "during cloning."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressClonePolicy_KeepNATMACs,
                                UIWizardCloneVM::tr("Include only NAT network adapter MAC addresses "
                                                      "during cloning."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressClonePolicy_StripAllMACs,
                                UIWizardCloneVM::tr("Generate new MAC addresses for all network adapters "
                                                      "during cloning."), Qt::ToolTipRole);

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pMACComboBoxLabel;

    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pCloneOptionsLayout->setColumnMinimumWidth(0, iMaxWidth);
}

void UIWizardCloneVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVMPageExpert::isComplete() const
{
    if (!m_pPathSelector)
        return false;

    QString path = m_pPathSelector->path();
    if (path.isEmpty())
        return false;
    /* Make sure VM name feat the rules: */
    QString strName = m_pNameLineEdit->text().trimmed();
    return !strName.isEmpty() && strName != m_strOriginalName;
}

bool UIWizardCloneVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Trying to clone VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardCloneVM*>(wizard())->cloneVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardCloneVMPageExpert::sltNameChanged()
{
    composeCloneFilePath();
}

void UIWizardCloneVMPageExpert::sltPathChanged()
{
    composeCloneFilePath();
}
