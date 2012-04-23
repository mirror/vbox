/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic4 class declaration
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

#ifndef __UIWizardNewVDPageBasic4_h__
#define __UIWizardNewVDPageBasic4_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 4th page of the New Virtual Disk wizard (base part): */
class UIWizardNewVDPage4 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVDPage4();
};

/* 4th page of the New Virtual Disk wizard (basic extension): */
class UIWizardNewVDPageBasic4 : public UIWizardPage, public UIWizardNewVDPage4
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardNewVDPageBasic4();

private:

    /* Translation stuff: */
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

#endif // __UIWizardNewVDPageBasic4_h__

