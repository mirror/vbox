/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic1 class declaration
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

#ifndef __UIWizardFirstRunPageBasic1_h__
#define __UIWizardFirstRunPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 1st page of the First Run wizard: */
class UIWizardFirstRunPageBasic1 : public UIWizardPage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardFirstRunPageBasic1(bool fBootHardDiskWasSet);

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Variables: */
    bool m_fBootHardDiskWasSet;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif // __UIWizardFirstRunPageBasic1_h__

