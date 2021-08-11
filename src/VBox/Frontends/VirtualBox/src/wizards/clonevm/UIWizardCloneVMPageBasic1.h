/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* Local includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class UICloneVMAdditionalOptionsEditor;
class UICloneVMNamePathEditor;
class QIRichTextLabel;

namespace UIWizardCloneVMNamePage
{
    QString composeCloneFilePath(const QString &strCloneName, const QString &strGroup, const QString &strFolderPath);


}

class UIWizardCloneVMPageBasic1 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardCloneVMPageBasic1(const QString &strOriginalName, const QString &strDefaultPath, const QString &strGroup);

private slots:

    void sltCloneNameChanged(const QString &strCloneName);
    void sltClonePathChanged(const QString &strClonePath);
    /** Handles change in MAC address policy combo-box. */
    void sltHandleMACAddressClonePolicyComboChange();

private:

    void retranslateUi();
    void initializePage();
    void prepare(const QString &strDefaultClonePath);
    /* Validation stuff: */
    bool isComplete() const;

    QIRichTextLabel *m_pMainLabel;
    // QGridLayout     *m_pContainerLayout;

    UICloneVMNamePathEditor *m_pNamePathEditor;
    UICloneVMAdditionalOptionsEditor *m_pAdditionalOptionsEditor;
    QString      m_strOriginalName;
    QString      m_strGroup;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageBasic1_h */
