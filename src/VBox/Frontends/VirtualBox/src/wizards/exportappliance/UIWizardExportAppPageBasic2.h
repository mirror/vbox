/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageBasic2 class declaration
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

#ifndef __UIWizardExportAppPageBasic2_h__
#define __UIWizardExportAppPageBasic2_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "UIWizardExportAppDefs.h"

/* Forward declarations: */
class QIRichTextLabel;
class QRadioButton;

/* 2nd page of the Export Appliance wizard: */
class UIWizardExportAppPageBasic2 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(StorageType storageType READ storageType WRITE setStorageType);

public:

    /* Constructor: */
    UIWizardExportAppPageBasic2();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Stuff for 'storageType' field: */
    StorageType storageType() const;
    void setStorageType(StorageType storageType);

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QRadioButton *m_pTypeLocalFilesystem;
    QRadioButton *m_pTypeSunCloud;
    QRadioButton *m_pTypeSimpleStorageSystem;
};

#endif /* __UIWizardExportAppPageBasic2_h__ */

