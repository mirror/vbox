/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVM class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVM_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"
#include "UIWizardCloneVMEditors.h"

/* COM includes: */
#include "CMachine.h"
#include "CSnapshot.h"

/** Clone Virtual Machine wizard: */
class UIWizardCloneVM : public UINativeWizard
{
    Q_OBJECT;

public:

    UIWizardCloneVM(QWidget *pParent, const CMachine &machine,
                    const QString &strGroup, CSnapshot snapshot = CSnapshot(), const QString &strHelpHashtag = QString());
    void setCloneModePageVisible(bool fIsFullClone);
    bool isCloneModePageVisible() const;
    /** Clone VM stuff. */
    bool cloneVM();
    bool machineHasSnapshot() const;

    /** @name Parameter setter/getters
      * @{ */
        void setCloneName(const QString &strCloneName);
        const QString &cloneName() const;

        void setCloneFilePath(const QString &strCloneFilePath);
        const QString &cloneFilePath() const;

        MACAddressClonePolicy macAddressClonePolicy() const;
        void setMacAddressPolicy(MACAddressClonePolicy enmMACAddressClonePolicy);

        bool keepDiskNames() const;
        void setKeepDiskNames(bool fKeepDiskNames);

        bool keepHardwareUUIDs() const;
        void setKeepHardwareUUIDs(bool fKeepHardwareUUIDs);

        bool linkedClone() const;
        void setLinkedClone(bool fLinkedClone);

        KCloneMode cloneMode() const;
        void setCloneMode(KCloneMode enmCloneMode);
    /** @} */

protected:

    virtual void populatePages() /* final override */;

private:

    void retranslateUi();
    void prepare();

    CMachine  m_machine;
    CSnapshot m_snapshot;
    QString   m_strGroup;
    int m_iCloneModePageIndex;

    /** @name Parameters needed during machine cloning
      * @{ */
        QString m_strCloneName;
        QString m_strCloneFilePath;
        MACAddressClonePolicy m_enmMACAddressClonePolicy;
        bool m_fKeepDiskNames;
        bool m_fKeepHardwareUUIDs;
        bool m_fLinkedClone;
        KCloneMode m_enmCloneMode;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVM_h */
