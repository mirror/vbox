/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardImportAppPageBasic2 class declaration
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

#ifndef __UIWizardImportAppPageBasic2_h__
#define __UIWizardImportAppPageBasic2_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "UIWizardImportAppDefs.h"

/* Forward declarations: */
class QDialogButtonBox;
class QIRichTextLabel;

/* 2nd page of the Import Appliance wizard (base part): */
class UIWizardImportAppPage2 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardImportAppPage2();

    /* Stuff for 'applianceWidget' field: */
    ImportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /* Widgets: */
    ImportAppliancePointer m_pApplianceWidget;
};

/* 2nd page of the Import Appliance wizard (basic extension): */
class UIWizardImportAppPageBasic2 : public UIWizardPage, public UIWizardImportAppPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget);

public:

    /* Constructor: */
    UIWizardImportAppPageBasic2(const QString &strFileName);

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void cleanupPage();

    /* Validation stuff: */
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif /* __UIWizardImportAppPageBasic2_h__ */

