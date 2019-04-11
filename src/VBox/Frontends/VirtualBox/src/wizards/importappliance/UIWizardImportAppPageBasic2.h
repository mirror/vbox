/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic2 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardImportAppDefs.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QLabel;
class QIRichTextLabel;

/** UIWizardPageBase extension for 2nd page of the Import Appliance wizard. */
class UIWizardImportAppPage2 : public UIWizardPageBase
{
protected:

    /** Constructs 2nd page base. */
    UIWizardImportAppPage2();

    /** Returns appliance widget instance. */
    ImportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }

    /** Holds the appliance widget instance. */
    ImportAppliancePointer m_pApplianceWidget;
};

/** UIWizardPage extension for 2nd page of the Import Appliance wizard, extends UIWizardImportAppPage2 as well. */
class UIWizardImportAppPageBasic2 : public UIWizardPage, public UIWizardImportAppPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs 2nd basic page.
      * @param  strFileName  Brings appliance file name. */
    UIWizardImportAppPageBasic2(const QString &strFileName);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;
    /** Performs page cleanup. */
    virtual void cleanupPage() /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

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
    QString m_strSignedBy;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic2_h */
