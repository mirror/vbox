/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageExpert class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardAddCloudVMPageBasic1.h"

/* Forward declarations: */
class QGroupBox;

/** UIWizardPage extension for UIWizardAddCloudVMPage1. */
class UIWizardAddCloudVMPageExpert : public UIWizardPage,
                                     public UIWizardAddCloudVMPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source);
    Q_PROPERTY(QString profileName READ profileName);
    Q_PROPERTY(QStringList instanceIds READ instanceIds);

public:

    /** Constructs expert page. */
    UIWizardAddCloudVMPageExpert();

protected:

    /** Allows access wizard from base part. */
    virtual UIWizard *wizardImp() const /* override */ { return UIWizardPage::wizard(); }

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

    /** Handles change in source combo-box. */
    void sltHandleSourceChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();

    /** Handles account tool-button click. */
    void sltHandleAccountButtonClick();

private:

    /** Holds the source container instance. */
    QGroupBox *m_pCntSource;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h */
