/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicInstallSetup class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicInstallSetup_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicInstallSetup_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QLineEdit;
class QSpinBox;
class QIRichTextLabel;

class UIUserNamePasswordEditor : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

public:

    UIUserNamePasswordEditor(QWidget *pParent = 0);

protected:

    void retranslateUi();

private:

    void prepare();
    void addField(QLabel *&pLabel, QLineEdit *&pLineEdit, QGridLayout *pLayout, bool fIsPasswordField = false);

    QLineEdit *m_pUserNameField;
    QLineEdit *m_pPasswordField;
    QLineEdit *m_pPasswordRepeatField;

    QLabel *m_pUserNameFieldLabel;
    QLabel *m_pPasswordFieldLabel;
    QLabel *m_pPasswordRepeatFieldLabel;

};

/* 2nd page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPageInstallSetup : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPageInstallSetup();


    /* Widgets: */
    UIUserNamePasswordEditor *m_pUserNamePasswordEditor;
};

/* 2nd page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasicInstallSetup : public UIWizardPage, public UIWizardNewVMPageInstallSetup
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizardNewVMPageBasicInstallSetup();

private slots:

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicInstallSetup_h */
