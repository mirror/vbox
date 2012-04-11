/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageBasic4 class declaration
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

#ifndef __UIWizardExportAppPageBasic4_h__
#define __UIWizardExportAppPageBasic4_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "UIWizardExportAppDefs.h"

/* Forward declarations: */
class QIRichTextLabel;

/* 4th page of the Export Appliance wizard: */
class UIWizardExportAppPageBasic4 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(ExportAppliancePointer applianceWidget READ applianceWidget);

public:

    /* Constructor: */
    UIWizardExportAppPageBasic4();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool validatePage();

    /* Stuff for 'applianceWidget' field: */
    ExportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    UIApplianceExportEditorWidget *m_pApplianceWidget;
};

#endif /* __UIWizardExportAppPageBasic4_h__ */

