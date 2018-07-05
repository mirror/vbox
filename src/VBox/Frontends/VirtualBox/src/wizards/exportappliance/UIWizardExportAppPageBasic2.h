/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic2 class declaration.
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

#ifndef ___UIWizardExportAppPageBasic2_h___
#define ___UIWizardExportAppPageBasic2_h___

/* GUI includes: */
#include "UIWizardPage.h"
#include "UIWizardExportAppDefs.h"

/* Forward declarations: */
class QGroupBox;
class QRadioButton;
class QIRichTextLabel;


/** UIWizardPageBase extension for 2nd page of the Export Appliance wizard. */
class UIWizardExportAppPage2 : public UIWizardPageBase
{
protected:

    /** Constructs 2nd page base. */
    UIWizardExportAppPage2();

    /** Chooses default storage type. */
    void chooseDefaultStorageType();

    /** Returns current storage type. */
    StorageType storageType() const;
    /** Defines current @a storageType. */
    void setStorageType(StorageType storageType);

    /** Holds the storage type container instance. */
    QGroupBox *m_pTypeCnt;
    /** Holds the Local Filesystem radio-button. */
    QRadioButton *m_pTypeLocalFilesystem;
    /** Holds the Sun Cloud radio-button. */
    QRadioButton *m_pTypeSunCloud;
    /** Holds the Simple Storage System radio-button. */
    QRadioButton *m_pTypeSimpleStorageSystem;
};


/** UIWizardPage extension for 2nd page of the Export Appliance wizard, extends UIWizardExportAppPage2 as well. */
class UIWizardExportAppPageBasic2 : public UIWizardPage, public UIWizardExportAppPage2
{
    Q_OBJECT;
    Q_PROPERTY(StorageType storageType READ storageType WRITE setStorageType);

public:

    /** Constructs 2nd basic page. */
    UIWizardExportAppPageBasic2();

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Performs page initialization. */
    void initializePage();

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic2_h___ */
