/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVM class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h
#define FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"

/** Add Cloud VM wizard. */
class UIWizardAddCloudVM : public UIWizard
{
    Q_OBJECT;

public:

    /** Basic page IDs. */
    enum
    {
        Page1
    };

    /** Expert page IDs. */
    enum
    {
        PageExpert
    };

    /** Constructs Add Cloud VM wizard passing @a pParent & @a enmMode to the base-class.
      * @param  comClient  Brings the Cloud Client object wrapper to work with. */
    UIWizardAddCloudVM(QWidget *pParent,
                       const CCloudClient &comClient = CCloudClient(),
                       WizardMode enmMode = WizardMode_Auto);

    /** Prepares all. */
    virtual void prepare() /* override */;

    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient) { m_comClient = comClient; }
    /** Returns Cloud Client object. */
    CCloudClient client() const { return m_comClient; }

    /** Adds cloud VMs. */
    bool addCloudVMs();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Holds the Cloud Client object wrapper. */
    CCloudClient  m_comClient;
};

/** Safe pointer to add cloud vm wizard. */
typedef QPointer<UIWizardAddCloudVM> UISafePointerWizardAddCloudVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h */
