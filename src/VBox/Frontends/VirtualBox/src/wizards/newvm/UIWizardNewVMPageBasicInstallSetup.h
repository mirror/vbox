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

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QLineEdit;
class QIRichTextLabel;
class UIUserNamePasswordEditor;
struct UIUnattendedInstallData;


class UIWizardNewVMPageInstallSetup : public UIWizardPageBase
{
public:

    UIWizardNewVMPageInstallSetup();

    /** @name Property getters
      * @{ */
        QString userName() const;
        QString password() const;
        QString hostname() const;
    /** @} */

protected:

    /* Widgets: */
    UIUserNamePasswordEditor *m_pUserNamePasswordEditor;
    QLineEdit *m_pHostnameLineEdit;
    QLabel  *m_pHostnameLabel;
};

class UIWizardNewVMPageBasicInstallSetup : public UIWizardPage, public UIWizardNewVMPageInstallSetup
{
    Q_OBJECT;
    Q_PROPERTY(QString userName READ userName);
    Q_PROPERTY(QString password READ password);
    Q_PROPERTY(QString hostname READ hostname);

public:

    /* Constructor: */
    UIWizardNewVMPageBasicInstallSetup();
    void setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData);
    virtual int nextId() const /* override */;

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
