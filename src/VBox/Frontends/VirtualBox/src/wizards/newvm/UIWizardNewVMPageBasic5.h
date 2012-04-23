/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic5 class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardNewVMPageBasic5_h__
#define __UIWizardNewVMPageBasic5_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 5th page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPage5 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPage5();
};

/* 5th page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasic5 : public UIWizardPage, public UIWizardNewVMPage5
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardNewVMPageBasic5();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pSummary;
    QIRichTextLabel *m_pLabel2;
};

#endif // __UIWizardNewVMPageBasic5_h__

