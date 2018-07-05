/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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

/* Qt includes: */
# include <QVBoxLayout>
# include <QGroupBox>
# include <QRadioButton>

/* GUI includes: */
# include "UIWizardExportAppPageBasic2.h"
# include "UIWizardExportApp.h"
# include "QIRichTextLabel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage2::UIWizardExportAppPage2()
{
}

void UIWizardExportAppPage2::chooseDefaultStorageType()
{
    /* Choose Filesystem by default: */
    setStorageType(Filesystem);
}

StorageType UIWizardExportAppPage2::storageType() const
{
    /* Check SunCloud and S3: */
    if (m_pTypeSunCloud->isChecked())
        return SunCloud;
    else if (m_pTypeSimpleStorageSystem->isChecked())
        return S3;

    /* Return Filesystem by default: */
    return Filesystem;
}

void UIWizardExportAppPage2::setStorageType(StorageType storageType)
{
    /* Check and focus the requested type: */
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


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic2 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic2::UIWizardExportAppPageBasic2()
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel(this);

        /* Create storage type container: */
        m_pTypeCnt = new QGroupBox(this);
        {
            /* Create storage type container layout: */
            QVBoxLayout *pTypeCntLayout = new QVBoxLayout(m_pTypeCnt);
            {
                /* Create Local Filesystem radio-button: */
                m_pTypeLocalFilesystem = new QRadioButton(m_pTypeCnt);
                /* Create SunCloud radio-button: */
                m_pTypeSunCloud = new QRadioButton(m_pTypeCnt);
                /* Create Simple Storage System radio-button: */
                m_pTypeSimpleStorageSystem = new QRadioButton(m_pTypeCnt);

                /* Add into layout: */
                pTypeCntLayout->addWidget(m_pTypeLocalFilesystem);
                pTypeCntLayout->addWidget(m_pTypeSunCloud);
                pTypeCntLayout->addWidget(m_pTypeSimpleStorageSystem);
            }
        }

        /* Add into layout: */
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pTypeCnt);
        pMainLayout->addStretch();

        /* Choose default storage type: */
        chooseDefaultStorageType();
    }

    /* Setup connections: */
    connect(m_pTypeLocalFilesystem, &QRadioButton::clicked,     this, &UIWizardExportAppPageBasic2::completeChanged);
    connect(m_pTypeSunCloud, &QRadioButton::clicked,            this, &UIWizardExportAppPageBasic2::completeChanged);
    connect(m_pTypeSimpleStorageSystem, &QRadioButton::clicked, this, &UIWizardExportAppPageBasic2::completeChanged);

    /* Register classes: */
    qRegisterMetaType<StorageType>();

    /* Register fields: */
    registerField("storageType", this, "storageType");
}

void UIWizardExportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Appliance settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("Please choose where to create the virtual appliance. "
                                            "You can create it on your own computer, "
                                            "on the Sun Cloud service or on an S3 storage server."));
    m_pTypeCnt->setTitle(UIWizardExportApp::tr("Create on"));
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&This computer"));
    m_pTypeSunCloud->setText(UIWizardExportApp::tr("Sun &Cloud"));
    m_pTypeSimpleStorageSystem->setText(UIWizardExportApp::tr("&Simple Storage System (S3)"));
}

void UIWizardExportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}
