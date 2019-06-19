/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic2 class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIFormEditorWidget.h"
#include "UIWizardImportAppDefs.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QLabel;
class QStackedLayout;
class QIRichTextLabel;

/** UIWizardPageBase extension for 2nd page of the Import Appliance wizard. */
class UIWizardNewCloudVMPage2 : public UIWizardPageBase
{
protected:

    /** Constructs 2nd page base. */
    UIWizardNewCloudVMPage2();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Refreshes form properties table. */
    void refreshFormPropertiesTable();

    /** Returns appliance widget instance. */
    ImportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /** Holds the settings container layout instance. */
    QStackedLayout *m_pSettingsCntLayout;

    /** Holds the appliance widget instance. */
    ImportAppliancePointer     m_pApplianceWidget;
    /** Holds the Form Editor widget instance. */
    UIFormEditorWidgetPointer  m_pFormEditor;
};

/** UIWizardPage extension for 2nd page of the Import Appliance wizard, extends UIWizardNewCloudVMPage2 as well. */
class UIWizardNewCloudVMPageBasic2 : public UIWizardPage, public UIWizardNewCloudVMPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs 2nd basic page.
      * @param  strFileName  Brings appliance file name. */
    UIWizardNewCloudVMPageBasic2(const QString &strFileName);

protected:

    /** Allows to access 'field()' from base part. */
    virtual QVariant fieldImp(const QString &strFieldName) const /* override */ { return UIWizardPage::field(strFieldName); }

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

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;

    /** Holds the signature/certificate info label instance. */
    QLabel *m_pCertLabel;

    /** Holds the certificate text template type. */
    enum {
        kCertText_Uninitialized = 0, kCertText_Unsigned,
        kCertText_IssuedTrusted,     kCertText_IssuedExpired,     kCertText_IssuedUnverified,
        kCertText_SelfSignedTrusted, kCertText_SelfSignedExpired, kCertText_SelfSignedUnverified
    } m_enmCertText;

    /** Holds the "signed by" information. */
    QString  m_strSignedBy;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic2_h */
