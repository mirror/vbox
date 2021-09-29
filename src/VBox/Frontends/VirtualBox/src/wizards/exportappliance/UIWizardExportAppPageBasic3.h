/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CAppliance.h"

/* Forward declarations: */
class QStackedWidget;
class QIRichTextLabel;
class UIApplianceExportEditorWidget;
class UIFormEditorWidget;
class UIWizardExportApp;

/** Namespace for 3rd basic page of the Export Appliance wizard. */
namespace UIWizardExportAppPage3
{
    /** Refresh stacked widget. */
    void refreshStackedWidget(QStackedWidget *pStackedWidget,
                              bool fIsFormatCloudOne);

    /** Refreshes appliance settings widget. */
    void refreshApplianceSettingsWidget(UIApplianceExportEditorWidget *pApplianceWidget,
                                        const QList<QUuid> &machineIDs,
                                        const QString &strUri,
                                        bool fIsFormatCloudOne);
    /** Refreshes form properties table. */
    void refreshFormPropertiesTable(UIFormEditorWidget *pFormEditor,
                                    const CVirtualSystemDescriptionForm &comVsdForm,
                                    bool fIsFormatCloudOne);
}

/** UINativeWizardPage extension for 3rd basic page of the Export Appliance wizard,
  * based on UIWizardAddCloudVMPage3 namespace functions. */
class UIWizardExportAppPageBasic3 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Returns wizard this page belongs to. */
    UIWizardExportApp *wizard() const;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;

    /** Holds the settings widget 2 instance. */
    QStackedWidget *m_pSettingsWidget2;

    /** Holds the appliance widget reference. */
    UIApplianceExportEditorWidget *m_pApplianceWidget;
    /** Holds the Form Editor widget instance. */
    UIFormEditorWidget            *m_pFormEditor;

    /** Holds whether cloud exporting is at launching stage. */
    bool  m_fLaunching;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic3_h */
