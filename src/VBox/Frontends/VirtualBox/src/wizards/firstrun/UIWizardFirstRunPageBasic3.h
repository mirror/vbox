/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic3 class declaration
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

#ifndef __UIWizardFirstRunPageBasic3_h__
#define __UIWizardFirstRunPageBasic3_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 3rd page of the First Run wizard (base part): */
class UIWizardFirstRunPage3 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardFirstRunPage3(bool fBootHardDiskWasSet);

    /* Variables: */
    bool m_fBootHardDiskWasSet;
};

/* 3rd page of the First Run wizard (basic extension): */
class UIWizardFirstRunPageBasic3 : public UIWizardPage, public UIWizardFirstRunPage3
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardFirstRunPageBasic3(bool fBootHardDiskWasSet);

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pSummaryText;
    QIRichTextLabel *m_pLabel2;
};

#endif // __UIWizardFirstRunPageBasic3_h__

