/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class declaration.
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

#ifndef ___UIWizardExportAppPageBasic3_h___
#define ___UIWizardExportAppPageBasic3_h___

/* Qt includes: */
#include <QList>
#include <QVariant>

/* GUI includes: */
#include "UIWizardExportAppDefs.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QJsonDocument;
class QJsonValue;
class QIRichTextLabel;
class UIWizardExportApp;


/** UIWizardPageBase extension for 3rd page of the Export Appliance wizard. */
class UIWizardExportAppPage3 : public UIWizardPageBase
{
protected:

    /** Constructs 3rd page base. */
    UIWizardExportAppPage3();

    /** Populates cloud client parameters. */
    void populateCloudClientParameters();

    /** Parses JSON @a document. */
    static QList<AbstractVSDParameter> parseJsonDocument(const QJsonDocument &document);
    /** Parses JSON bool @a field. */
    static bool parseJsonFieldBool(const QString &strFieldName, const QJsonValue &field);
    /** Parses JSON double @a field. */
    static double parseJsonFieldDouble(const QString &strFieldName, const QJsonValue &field);
    /** Parses JSON string @a field. */
    static QString parseJsonFieldString(const QString &strFieldName, const QJsonValue &field);
    /** Parses JSON array @a field. */
    static QStringList parseJsonFieldArray(const QString &strFieldName, const QJsonValue &field);

    /** Refreshes appliance settings widget. */
    void refreshApplianceSettingsWidget();

    /** Returns the appliance widget reference. */
    ExportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /** Holds the cloud client parameters. */
    QList<AbstractVSDParameter> m_listCloudClientParameters;

    /** Holds the appliance widget reference. */
    UIApplianceExportEditorWidget *m_pApplianceWidget;
};


/** UIWizardPage extension for 3rd page of the Export Appliance wizard, extends UIWizardExportAppPage3 as well. */
class UIWizardExportAppPageBasic3 : public UIWizardPage, public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(ExportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Allows access wizard from base part. */
    UIWizard* wizardImp() { return UIWizardPage::wizard(); }
    /** Allows access page from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic3_h___ */
