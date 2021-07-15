/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMHardwarePageBasic class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>
#include <QVariant>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QRadioButton;
class QCheckBox;
class QIRichTextLabel;
class UIBaseMemoryEditor;
class UINewVMHardwareContainer;
class UIVirtualCPUEditor;
class UIWizardNewVM;

namespace UIWizardNewVMHardwarePage
{
}
//        int baseMemory() const;
//        int VCPUCount() const;
//        bool EFIEnabled() const;


class UIWizardNewVMHardwarePageBasic : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMHardwarePageBasic();

protected:


private slots:

    void sltMemorySizeChanged(int iValue);
    void sltCPUCountChanged(int iCount);
    void sltEFIEnabledChanged(bool fEnabled);

private:

    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    void cleanupPage();
    bool isComplete() const;

    /** @name Widgets
     * @{ */
       QIRichTextLabel    *m_pLabel;
       UINewVMHardwareContainer *m_pHardwareWidgetContainer;
    /** @} */
    /** This set is used to decide if we have to set wizard's parameters
      * some default values or not. When user modifies a value through a widget we
      * no longer touch user set value during page initilization. see initializePage. */
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h */
