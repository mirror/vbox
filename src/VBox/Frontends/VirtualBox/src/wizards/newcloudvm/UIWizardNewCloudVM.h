/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVM class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizard.h"

/** Import Appliance wizard. */
class UIWizardNewCloudVM : public UIWizard
{
    Q_OBJECT;

public:

    /** Basic page IDs. */
    enum
    {
        Page1,
        Page2
    };

    /** Expert page IDs. */
    enum
    {
        PageExpert
    };

    /** Constructs export appliance wizard passing @a pParent to the base-class.
      * @param  strFileName  Brings appliance file name. */
    UIWizardNewCloudVM(QWidget *pParent, bool fImportFromOCIByDefault, const QString &strFileName);

    /** Prepares all. */
    virtual void prepare() /* override */;

    /** Returns whether appliance is valid. */
    bool isValid() const;

    /** Imports appliance. */
    bool importAppliance();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

protected slots:

    /** Handles change for page with @a iId. */
    virtual void sltCurrentIdChanged(int iId) /* override */;
    /** Handles custom button 2 click  for page with @a iId. */
    virtual void sltCustomButtonClicked(int iId) /* override */;

private:

    /** Holds whether default source should be Import from OCI. */
    bool     m_fImportFromOCIByDefault;
    /** Handles the appliance file name. */
    QString  m_strFileName;
};

/** Safe pointer to appliance wizard. */
typedef QPointer<UIWizardNewCloudVM> UISafePointerWizardImportApp;

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h */
