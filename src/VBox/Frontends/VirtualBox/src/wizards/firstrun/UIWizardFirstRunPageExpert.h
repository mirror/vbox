/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageExpert class declaration
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardFirstRunPageExpert_h__
#define __UIWizardFirstRunPageExpert_h__

/* Local includes: */
#include "UIWizardFirstRunPageBasic1.h"
#include "UIWizardFirstRunPageBasic2.h"
#include "UIWizardFirstRunPageBasic3.h"

/* Expert page of the First Run wizard: */
class UIWizardFirstRunPageExpert : public UIWizardPage,
                                   public UIWizardFirstRunPage1,
                                   public UIWizardFirstRunPage2,
                                   public UIWizardFirstRunPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString id READ id WRITE setId);

public:

    /* Constructor: */
    UIWizardFirstRunPageExpert(const QString &strMachineId, bool fBootHardDiskWasSet);

protected:

    /* Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }

private slots:

    /* Open with file-open dialog: */
    void sltOpenMediumWithFileOpenDialog();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();
};

#endif // __UIWizardFirstRunPageExpert_h__

