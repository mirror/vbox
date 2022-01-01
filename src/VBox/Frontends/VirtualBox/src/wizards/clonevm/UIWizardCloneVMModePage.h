/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMModePage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMModePage_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMModePage_h
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
class QIRichTextLabel;
class UICloneVMCloneModeGroupBox;

/** 3rd page of the Clone Virtual Machine wizard (basic extension). */
class UIWizardCloneVMModePage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIWizardCloneVMModePage(bool fShowChildsOption);

private slots:

    void sltCloneModeChanged(KCloneMode enmCloneMode);

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void initializePage();
    void prepare();

    /** Validation stuff. */
    bool validatePage();

    /** Widgets. */
    QIRichTextLabel *m_pLabel;
    UICloneVMCloneModeGroupBox *m_pCloneModeGroupBox;

    bool m_fShowChildsOption;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMModePage_h */
