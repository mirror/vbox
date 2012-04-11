/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVMPageBasic1 class declaration
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardCloneVMPageBasic1_h__
#define __UIWizardCloneVMPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class QLineEdit;
class QCheckBox;

/* 1st page of the Clone Virtual Machine wizard: */
class UIWizardCloneVMPageBasic1 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(QString cloneName READ cloneName WRITE setCloneName);
    Q_PROPERTY(bool reinitMACs READ isReinitMACsChecked);

public:

    /* Constructor: */
    UIWizardCloneVMPageBasic1(const QString &strOriginalName);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Stuff for 'cloneName' field: */
    QString cloneName() const;
    void setCloneName(const QString &strName);

    /* Stuff for 'reinitMACs' field: */
    bool isReinitMACsChecked() const;

    /* Variables: */
    QString m_strOriginalName;

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pLabel2;
    QLineEdit *m_pNameEditor;
    QCheckBox *m_pReinitMACsCheckBox;
};

#endif // __UIWizardCloneVMPageBasic1_h__

