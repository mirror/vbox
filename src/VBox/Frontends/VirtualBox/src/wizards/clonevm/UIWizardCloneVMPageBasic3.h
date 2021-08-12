/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageBasic3 class declaration.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic3_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic3_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QRadioButton;
class QIRichTextLabel;
class UICloneVMCloneModeGroupBox;

// /* 3rd page of the Clone Virtual Machine wizard (base part): */
// class UIWizardCloneVMPage3 : public UIWizardPageBase
// {
// protected:

//     /* Constructor: */
//     UIWizardCloneVMPage3(bool fShowChildsOption);

//     /* Stuff for 'cloneMode' field: */
//     KCloneMode cloneMode() const;
//     void setCloneMode(KCloneMode cloneMode);

//     /* Variables: */

//     /* Widgets: */
//     QRadioButton *m_pMachineRadio;
//     QRadioButton *m_pMachineAndChildsRadio;
//     QRadioButton *m_pAllRadio;
// };

/* 3rd page of the Clone Virtual Machine wizard (basic extension): */
class UIWizardCloneVMPageBasic3 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardCloneVMPageBasic3(bool fShowChildsOption);

private slots:

    void sltCloneModeChanged(KCloneMode enmCloneMode);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void prepare();

    /* Validation stuff: */
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    UICloneVMCloneModeGroupBox *m_pCloneModeGroupBox;

    bool m_fShowChildsOption;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic3_h */
