/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageExpert class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardNewCloudVMPageBasic1.h"
#include "UIWizardNewCloudVMPageBasic2.h"

/* Forward declarations: */
class QGroupBox;

/** UIWizardPage extension for UIWizardNewCloudVMPage1 and UIWizardNewCloudVMPage2. */
class UIWizardNewCloudVMPageExpert : public UIWizardPage,
                                     public UIWizardNewCloudVMPage1,
                                     public UIWizardNewCloudVMPage2
{
    Q_OBJECT;
    Q_PROPERTY(CCloudClient client READ client);
    Q_PROPERTY(CVirtualSystemDescription vsd READ vsd);
    Q_PROPERTY(CVirtualSystemDescriptionForm vsdForm READ vsdForm);

public:

    /** Constructs expert page. */
    UIWizardNewCloudVMPageExpert();

protected:

    /** Allows to access 'field()' from base part. */
    virtual QVariant fieldImp(const QString &strFieldName) const /* override */ { return UIWizardPage::field(strFieldName); }

    /** Handle any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles change in destination combo-box. */
    void sltHandleDestinationChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();
    /** Handles account tool-button click. */
    void sltHandleAccountButtonClick();

    /** Handles change in instance list. */
    void sltHandleInstanceListChange();

private:

    /** Holds the destination container instance. */
    QGroupBox *m_pCntDestination;
    /** Holds the settings container instance. */
    QGroupBox *m_pSettingsCnt;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h */
