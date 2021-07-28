/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVM class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h
#define FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"

/** Add Cloud VM wizard. */
class UIWizardAddCloudVM : public UINativeWizard
{
    Q_OBJECT;

public:

    /** Constructs Add Cloud VM wizard passing @a pParent & @a enmMode to the base-class.
      * @param  strFullGroupName  Brings full group name (/provider/profile) to add VM to. */
    UIWizardAddCloudVM(QWidget *pParent,
                       const QString &strFullGroupName = QString(),
                       WizardMode enmMode = WizardMode_Auto);

    /** Returns full group name. */
    QString fullGroupName() const { return m_strFullGroupName; }

    /** Defines @a strProviderShortName. */
    void setShortProviderName(const QString &strProviderShortName) { m_strProviderShortName = strProviderShortName; }
    /** Returns short provider name. */
    QString shortProviderName() const { return m_strProviderShortName; }

    /** Defines @a strProfileName. */
    void setProfileName(const QString &strProfileName) { m_strProfileName = strProfileName; }
    /** Returns profile name. */
    QString profileName() const { return m_strProfileName; }

    /** Defines @a instanceIds. */
    void setInstanceIds(const QStringList &instanceIds) { m_instanceIds = instanceIds; }
    /** Returns instance IDs. */
    QStringList instanceIds() const { return m_instanceIds; }

    /** Defines Cloud @a comClient object wrapper. */
    void setClient(const CCloudClient &comClient) { m_comClient = comClient; }
    /** Returns Cloud Client object wrapper. */
    CCloudClient client() const { return m_comClient; }

    /** Adds cloud VMs. */
    bool addCloudVMs();

protected:

    /** Populates pages. */
    virtual void populatePages() /* override final */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override final */;

private:

    /** Holds the full group name (/provider/profile) to add VM to. */
    QString       m_strFullGroupName;
    /** Holds the short provider name. */
    QString       m_strProviderShortName;
    /** Holds the profile name. */
    QString       m_strProfileName;
    /** Holds the instance ids. */
    QStringList   m_instanceIds;
    /** Holds the Cloud Client object wrapper. */
    CCloudClient  m_comClient;
};

/** Safe pointer to add cloud vm wizard. */
typedef QPointer<UIWizardAddCloudVM> UISafePointerWizardAddCloudVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVM_h */
