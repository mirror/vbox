/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic1 class declaration
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

#ifndef __UIWizardNewVMPageBasic1_h__
#define __UIWizardNewVMPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 1st page of the New Virtual Machine wizard: */
class UIWizardNewVMPageBasic1 : public UIWizardPage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardNewVMPageBasic1();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Helpers: */
    static QString tableTemplate();

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pLabel2;
};

#endif // __UIWizardNewVMPageBasic1_h__

