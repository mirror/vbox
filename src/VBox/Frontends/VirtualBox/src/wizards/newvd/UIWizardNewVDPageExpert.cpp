/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageExpert class implementation.
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
#include <QDir>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QRegExpValidator>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>

/* GUI includes: */
#include "UIConverter.h"
#include "UIWizardNewVDPageExpert.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "QILineEdit.h"
#include "UIMediumSizeEditor.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardNewVDPageExpert::UIWizardNewVDPageExpert(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
    : UIWizardNewVDPage3(strDefaultName, strDefaultPath)
    , m_pFormatGroupBox(0)
    , m_pVariantGroupBox(0)
    , m_pLocationGroupBox(0)
    , m_pSizeGroupBox(0)
{
    /* Get default extension for new virtual-disk: */
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        m_pLocationGroupBox = new QGroupBox(this);
        {
            m_pLocationGroupBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pLocationGroupBoxLayout = new QHBoxLayout(m_pLocationGroupBox);
            {
                m_pLocationEditor = new QLineEdit(m_pLocationGroupBox);
                m_pLocationOpenButton = new QIToolButton(m_pLocationGroupBox);
                {
                    m_pLocationOpenButton->setAutoRaise(true);
                    m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
                }
                pLocationGroupBoxLayout->addWidget(m_pLocationEditor);
                pLocationGroupBoxLayout->addWidget(m_pLocationOpenButton);
            }
        }
        m_pSizeGroupBox = new QGroupBox(this);
        {
            m_pSizeGroupBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pSizeGroupBoxLayout = new QVBoxLayout(m_pSizeGroupBox);
            {
                m_pSizeEditor = new UIMediumSizeEditor;
                {
                    pSizeGroupBoxLayout->addWidget(m_pSizeEditor);
                }
            }
        }
        m_pFormatGroupBox = new QGroupBox(this);
        {
            m_pFormatGroupBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pFormatGroupBoxLayout = new QVBoxLayout(m_pFormatGroupBox);
            pFormatGroupBoxLayout->addWidget(createFormatButtonGroup(true));
        }
        m_pVariantGroupBox = new QGroupBox(this);
        {
            m_pVariantGroupBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pVariantGroupBoxLayout = new QVBoxLayout(m_pVariantGroupBox);
            {
                m_pFixedCheckBox = new QCheckBox;
                m_pSplitBox = new QCheckBox(m_pVariantGroupBox);
                pVariantGroupBoxLayout->addWidget(m_pFixedCheckBox);
                pVariantGroupBoxLayout->addWidget(m_pSplitBox);
            }
        }
        pMainLayout->addWidget(m_pLocationGroupBox, 0, 0, 1, 2);
        pMainLayout->addWidget(m_pSizeGroupBox, 1, 0, 1, 2);
        pMainLayout->addWidget(m_pFormatGroupBox, 2, 0, Qt::AlignTop);
        pMainLayout->addWidget(m_pVariantGroupBox, 2, 1, Qt::AlignTop);
        setMediumSize(uDefaultSize);
        sltMediumFormatChanged();
    }

    /* Setup connections: */
    connect(m_pFormatButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, &UIWizardNewVDPageExpert::sltMediumFormatChanged);
    connect(m_pFixedCheckBox, &QAbstractButton::toggled,
            this, &UIWizardNewVDPageExpert::completeChanged);
    connect(m_pSplitBox, &QCheckBox::stateChanged,
            this, &UIWizardNewVDPageExpert::completeChanged);
    connect(m_pLocationEditor, &QLineEdit::textChanged,
            this, &UIWizardNewVDPageExpert::completeChanged);
    connect(m_pLocationOpenButton, &QIToolButton::clicked,
            this, &UIWizardNewVDPageExpert::sltSelectLocationButtonClicked);
    connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
            this, &UIWizardNewVDPageExpert::completeChanged);

    /* Register classes: */
    qRegisterMetaType<CMediumFormat>();
    /* Register fields: */
    registerField("mediumFormat", this, "mediumFormat");
    registerField("mediumVariant", this, "mediumVariant");
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");
}

void UIWizardNewVDPageExpert::sltMediumFormatChanged()
{
    CMediumFormat comMediumFormat = mediumFormat();
    if (comMediumFormat.isNull())
    {
        AssertMsgFailed(("No medium format set!"));
        return;
    }
    updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    updateLocationEditorAfterFormatChange(comMediumFormat, m_formatExtensions);

    /* Broadcast complete-change: */
    completeChanged();
}

void UIWizardNewVDPageExpert::sltSelectLocationButtonClicked()
{
    /* Call to base-class: */
    onSelectLocationButtonClicked();
}

void UIWizardNewVDPageExpert::retranslateUi()
{
    UIWizardNewVDPage1::retranslateWidgets();
    UIWizardNewVDPage2::retranslateWidgets();
    UIWizardNewVDPage3::retranslateWidgets();
    /* Translate widgets: */
    if (m_pLocationGroupBox)
        m_pLocationGroupBox->setTitle(UIWizardNewVD::tr("Hard disk file &location"));
    if (m_pSizeGroupBox)
        m_pSizeGroupBox->setTitle(UIWizardNewVD::tr("Hard disk file &size"));
    if (m_pFormatGroupBox)
        m_pFormatGroupBox->setTitle(UIWizardNewVD::tr("Hard disk file &type"));
    if (m_pFormatButtonGroup)
    {
        QList<QAbstractButton*> buttons = m_pFormatButtonGroup->buttons();
        for (int i = 0; i < buttons.size(); ++i)
        {
            QAbstractButton *pButton = buttons[i];
            UIMediumFormat enmFormat = gpConverter->fromInternalString<UIMediumFormat>(m_formatNames[m_pFormatButtonGroup->id(pButton)]);
            pButton->setText(gpConverter->toString(enmFormat));
        }
    }
    if (m_pVariantGroupBox)
        m_pVariantGroupBox->setTitle(UIWizardNewVD::tr("Storage on physical hard disk"));
    if (m_pSplitBox)
        m_pSplitBox->setText(UIWizardNewVD::tr("&Split into files of less than 2GB"));
}

void UIWizardNewVDPageExpert::initializePage()
{
    /* Get default extension for new virtual-disk: */
    m_strDefaultExtension = defaultExtension(field("mediumFormat").value<CMediumFormat>());
    /* Set default name as text for location editor: */
    if (m_pLocationEditor)
        m_pLocationEditor->setText(absoluteFilePath(m_strDefaultName, m_strDefaultPath, m_strDefaultExtension));


    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewVDPageExpert::isComplete() const
{
    /* Make sure medium format/variant is correct,
     * current name is not empty and current size feats the bounds: */
    return !mediumFormat().isNull() &&
           mediumVariant() != (qulonglong)KMediumVariant_Max &&
           !m_pLocationEditor->text().trimmed().isEmpty() &&
           mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
}

bool UIWizardNewVDPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Make sure such file doesn't exist already: */
    const QString strMediumPath(mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return fResult;
    }

    /* Make sure we are passing FAT size limitation: */
    fResult = UIWizardNewVDPage3::checkFATSizeLimitation(fieldImp("mediumVariant").toULongLong(),
                                                         fieldImp("mediumPath").toString(),
                                                         fieldImp("mediumSize").toULongLong());
    if (!fResult)
    {
        msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
        return fResult;
    }

    /* Lock finish button: */
    startProcessing();
    /* Try to create virtual-disk: */
    fResult = qobject_cast<UIWizardNewVD*>(wizard())->createVirtualDisk();
    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}
