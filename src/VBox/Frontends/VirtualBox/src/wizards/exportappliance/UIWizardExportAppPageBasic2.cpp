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
# include <QGroupBox>
# include <QRadioButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppPageBasic2.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage2::UIWizardExportAppPage2()
{
}

void UIWizardExportAppPage2::chooseDefaultStorageType()
{
    /* Choose LocalFilesystem by default: */
    setStorageType(LocalFilesystem);
}

StorageType UIWizardExportAppPage2::storageType() const
{
    /* Check Cloud Service Provider: */
    if (m_pTypeCloudServiceProvider->isChecked())
        return CloudProvider;

    /* Return LocalFilesystem by default: */
    return LocalFilesystem;
}

void UIWizardExportAppPage2::setStorageType(StorageType enmStorageType)
{
    /* Check and focus the requested type: */
    switch (enmStorageType)
    {
        case LocalFilesystem:
            m_pTypeLocalFilesystem->setChecked(true);
            m_pTypeLocalFilesystem->setFocus();
            break;
        case CloudProvider:
            m_pTypeCloudServiceProvider->setChecked(true);
            m_pTypeCloudServiceProvider->setFocus();
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
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel;
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create Local LocalFilesystem radio-button: */
        m_pTypeLocalFilesystem = new QRadioButton;
        if (m_pTypeLocalFilesystem)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pTypeLocalFilesystem);
        }
        /* Create CloudProvider radio-button: */
        m_pTypeCloudServiceProvider = new QRadioButton;
        if (m_pTypeCloudServiceProvider)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pTypeCloudServiceProvider);
        }

        /* Add vertical stretch: */
        pMainLayout->addStretch();
    }

    /* Choose default storage type: */
    chooseDefaultStorageType();

    /* Setup connections: */
    connect(m_pTypeLocalFilesystem, &QRadioButton::clicked,      this, &UIWizardExportAppPageBasic2::completeChanged);
    connect(m_pTypeCloudServiceProvider, &QRadioButton::clicked, this, &UIWizardExportAppPageBasic2::completeChanged);

    /* Register classes: */
    qRegisterMetaType<StorageType>();

    /* Register fields: */
    registerField("storageType", this, "storageType");
}

void UIWizardExportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Create on"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("Please choose where to create the virtual appliance. "
                                            "You can create it on your own computer "
                                            "or on one of cloud servers you have registered."));
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&This computer"));
    m_pTypeCloudServiceProvider->setText(UIWizardExportApp::tr("&Cloud Service Provider"));
}

void UIWizardExportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}
