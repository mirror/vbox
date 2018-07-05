/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic4 class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWizardExportAppPageBasic4_h___
#define ___UIWizardExportAppPageBasic4_h___

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"
#include "UIWizardExportAppDefs.h"

/* Forward declarations: */
class UIWizardExportApp;
class QIRichTextLabel;


/** UIWizardPageBase extension for 4th page of the Export Appliance wizard. */
class UIWizardExportAppPage4 : public UIWizardPageBase
{
protected:

    /** Constructs 4th page base. */
    UIWizardExportAppPage4();

    /** Refreshes appliance settings widget. */
    void refreshApplianceSettingsWidget();

    /** Returns the appliance widget reference. */
    ExportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /** Holds the appliance widget reference. */
    UIApplianceExportEditorWidget *m_pApplianceWidget;
};


/** UIWizardPage extension for 4th page of the Export Appliance wizard, extends UIWizardExportAppPage4 as well. */
class UIWizardExportAppPageBasic4 : public UIWizardPage, public UIWizardExportAppPage4
{
    Q_OBJECT;
    Q_PROPERTY(ExportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs 4th basic page. */
    UIWizardExportAppPageBasic4();

protected:

    /** Allows access wizard from base part. */
    UIWizard* wizardImp() { return UIWizardPage::wizard(); }
    /** Allows access page from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Performs page initialization. */
    void initializePage();

    /** Performs page validation. */
    bool validatePage();

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic4_h___ */
