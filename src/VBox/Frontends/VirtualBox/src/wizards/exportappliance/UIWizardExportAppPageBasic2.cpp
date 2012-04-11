/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageBasic2 class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QVBoxLayout>
#include <QRadioButton>

/* Local includes: */
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportApp.h"
#include "QIRichTextLabel.h"

UIWizardExportAppPageBasic2::UIWizardExportAppPageBasic2()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pTypeLocalFilesystem = new QRadioButton(this);
        m_pTypeSunCloud = new QRadioButton(this);
        m_pTypeSimpleStorageSystem = new QRadioButton(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pTypeLocalFilesystem);
    pMainLayout->addWidget(m_pTypeSunCloud);
    pMainLayout->addWidget(m_pTypeSimpleStorageSystem);
    pMainLayout->addStretch();

    /* Select default storage type: */
#if 0
    /* Load storage-type from GUI extra data: */
    bool ok;
    StorageType storageType = StorageType(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_Export_StorageType).toInt(&ok));
    if (ok)
        setStorageType(storageType);
#else
    /* Just select first of types: */
    setStorageType(Filesystem);
#endif

    /* Setup connections: */
    connect(m_pTypeLocalFilesystem, SIGNAL(clicked()), this, SIGNAL(completeChanged()));
    connect(m_pTypeSunCloud, SIGNAL(clicked()), this, SIGNAL(completeChanged()));
    connect(m_pTypeSimpleStorageSystem, SIGNAL(clicked()), this, SIGNAL(completeChanged()));

    /* Register StorageType class: */
    qRegisterMetaType<StorageType>();
    /* Register 'storageType' field! */
    registerField("storageType", this, "storageType");
}

void UIWizardExportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Appliance Export Settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("Please specify the target for the OVF export. "
                                            "You can choose between a local file system export, "
                                            "uploading the OVF to the Sun Cloud service "
                                            "or an S3 storage server."));
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&Local Filesystem "));
    m_pTypeSunCloud->setText(UIWizardExportApp::tr("Sun &Cloud"));
    m_pTypeSimpleStorageSystem->setText(UIWizardExportApp::tr("&Simple Storage System (S3)"));
}

void UIWizardExportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

StorageType UIWizardExportAppPageBasic2::storageType() const
{
    if (m_pTypeSunCloud->isChecked())
        return SunCloud;
    else if (m_pTypeSimpleStorageSystem->isChecked())
        return S3;
    return Filesystem;
}

void UIWizardExportAppPageBasic2::setStorageType(StorageType storageType)
{
    switch (storageType)
    {
        case Filesystem:
            m_pTypeLocalFilesystem->setChecked(true);
            m_pTypeLocalFilesystem->setFocus();
            break;
        case SunCloud:
            m_pTypeSunCloud->setChecked(true);
            m_pTypeSunCloud->setFocus();
            break;
        case S3:
            m_pTypeSimpleStorageSystem->setChecked(true);
            m_pTypeSimpleStorageSystem->setFocus();
            break;
    }
}

