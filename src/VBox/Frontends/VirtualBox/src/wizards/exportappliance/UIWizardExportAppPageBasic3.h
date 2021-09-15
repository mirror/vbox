/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic3_h
#define FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic3_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QList>
#include <QVariant>

/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UIFormEditorWidget.h"
#include "UIWizardPage.h"

/* COM includes: */
#include "CAppliance.h"

/* Forward declarations: */
class QStackedLayout;
class QIRichTextLabel;
class UIApplianceExportEditorWidget;
class UIWizardExportApp;


/** UIWizardPageBase extension for 3rd page of the Export Appliance wizard. */
class UIWizardExportAppPage3 : public UIWizardPageBase
{
protected:

    /** Constructs 3rd page base. */
    UIWizardExportAppPage3();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Refreshes appliance settings widget. */
    void refreshApplianceSettingsWidget();
    /** Refreshes form properties table. */
    void refreshFormPropertiesTable();

    /** Returns Local Appliance object. */
    CAppliance localAppliance() const;

    /** Holds the settings container layout instance. */
    QStackedLayout *m_pSettingsCntLayout;

    /** Holds the appliance widget reference. */
    UIApplianceExportEditorWidget *m_pApplianceWidget;
    /** Holds the Form Editor widget instance. */
    UIFormEditorWidgetPointer      m_pFormEditor;
};


/** UIWizardPage extension for 3rd page of the Export Appliance wizard, extends UIWizardExportAppPage3 as well. */
class UIWizardExportAppPageBasic3 : public UIWizardPage, public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(CAppliance localAppliance READ localAppliance);

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Allows access wizard from base part. */
    UIWizard *wizardImp() const { return UIWizardPage::wizard(); }
    /** Allows access page from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;
    /** Performs page cleanup. */
    virtual void cleanupPage() /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

    /** Updates page appearance. */
    virtual void updatePageAppearance() /* override */;

private slots:

    /** Handles custom button clicked.
      * @param  iId  Brings clicked button id. */
    void sltHandleCustomButtonClicked(int iId);

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic3_h */
