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
#include "UIMediumSizeEditor.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic5.h"

/* COM includes: */
#include "CGuestOSType.h"
#include "CSystemProperties.h"

UIWizardNewVMPageBasic5::UIWizardNewVMPageBasic5()
    : m_fUserSetSize(false)
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

    retranslateUi();
}

CMediumFormat UIWizardNewVMPageBasic5::mediumFormat() const
{
    return m_mediumFormat;
}

QString UIWizardNewVMPageBasic5::mediumPath() const
{
    return absoluteFilePath(toFileName(m_strDefaultName, m_strDefaultExtension), m_strDefaultPath);
}

void UIWizardNewVMPageBasic5::prepare()
{


    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->addWidget(createMediumVariantWidgets(true));

    m_pSizeLabel = new QIRichTextLabel;
    m_pSizeEditor = new UIMediumSizeEditor;

    pMainLayout->addWidget(m_pSizeLabel);
    pMainLayout->addWidget(m_pSizeEditor);
    pMainLayout->addStretch();

    createConnections();
}

void UIWizardNewVMPageBasic5::createConnections()
{
    connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged, this, &UIWizardNewVDPageBasic3::completeChanged);
    connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged, this, &UIWizardNewVMPageBasic5::sltHandleSizeEditorChange);
}

void UIWizardNewVMPageBasic5::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Storage on physical hard disk"));
    UIWizardNewVDPage2::retranslateWidgets();

    if (m_pSizeLabel)
        m_pSizeLabel->setText(UIWizardNewVM::tr("Select the size of the virtual hard disk in megabytes. "
                                                "This size is the limit on the amount of file data "
                                                "that a virtual machine will be able to store on the hard disk."));
}

void UIWizardNewVMPageBasic5::initializePage()
{
    /* We set the medium name and path according to machine name/path and do let user change these in the guided mode: */
    QString strDefaultName = fieldImp("machineBaseName").toString();
    m_strDefaultName = strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName;
    m_strDefaultPath = fieldImp("machineFolder").toString();
    if (m_pSizeEditor && !m_fUserSetSize)
    {
        m_pSizeEditor->blockSignals(true);
        setMediumSize(fieldImp("type").value<CGuestOSType>().GetRecommendedHDD());
        m_pSizeEditor->blockSignals(false);
    }
}

void UIWizardNewVMPageBasic5::cleanupPage()
{
    /* do not reset fields: */
}

bool UIWizardNewVMPageBasic5::isComplete() const
{
    return mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
}


void UIWizardNewVMPageBasic5::sltHandleSizeEditorChange()
{
    m_fUserSetSize = true;
}
