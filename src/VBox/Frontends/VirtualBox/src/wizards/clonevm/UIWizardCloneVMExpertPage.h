/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMExpertPage class declaration.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMExpertPage_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMExpertPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UINativeWizardPage.h"
#include "UIWizardCloneVMEditors.h"

/* Forward declarations: */
class UICloneVMAdditionalOptionsEditor;
class UICloneVMCloneModeGroupBox;
class UICloneVMCloneTypeGroupBox;
class UICloneVMNamePathEditor;

/** Expert page of the Clone Virtual Machine wizard. */
class UIWizardCloneVMExpertPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIWizardCloneVMExpertPage(const QString &strOriginalName, const QString &strDefaultPath,
                              bool fAdditionalInfo, bool fShowChildsOption, const QString &strGroup);

private slots:

    void sltCloneNameChanged(const QString &strCloneName);
    void sltClonePathChanged(const QString &strClonePath);
    void sltMACAddressClonePolicyChanged(MACAddressClonePolicy enmMACAddressClonePolicy);
    void sltKeepDiskNamesToggled(bool fKeepDiskNames);
    void sltKeepHardwareUUIDsToggled(bool fKeepHardwareUUIDs);
    void sltCloneTypeChanged(bool fIsFullClone);

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void initializePage();
    void prepare(const QString &strOriginalName, const QString &strDefaultPath, bool fShowChildsOption);

    /** Validation stuff. */
    bool isComplete() const;
    bool validatePage();
    void setCloneModeGroupBoxEnabled();

    UICloneVMNamePathEditor *m_pNamePathGroupBox;
    UICloneVMCloneTypeGroupBox *m_pCloneTypeGroupBox;
    UICloneVMCloneModeGroupBox *m_pCloneModeGroupBox;
    UICloneVMAdditionalOptionsEditor *m_pAdditionalOptionsGroupBox;
    QString m_strGroup;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMExpertPage_h */
