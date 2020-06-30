/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicGAInstall class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicGAInstall_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicGAInstall_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class QLineEdit;
class QIRichTextLabel;
class UIFilePathSelector;

class UIWizardNewVMPageGAInstall : public UIWizardPageBase
{
public:

    UIWizardNewVMPageGAInstall();

    /** @name Property getters
      * @{ */
        QString productKey() const;
        bool installGuestAdditions() const;
        void setInstallGuestAdditions(bool fInstallGA);
        QString guestAdditionsISOPath() const;
        void setGuestAdditionsISOPath(const QString &strISOPath);
    /** @} */

protected:

    bool checkISOFile() const;

    /** @name Widgets
      * @{ */
        QCheckBox *m_pInstallGACheckBox;
        QLabel  *m_pISOPathLabel;
        UIFilePathSelector *m_pISOFilePathSelector;
    /** @} */

};

class UIWizardNewVMPageBasicGAInstall : public UIWizardPage, public UIWizardNewVMPageGAInstall
{
    Q_OBJECT;
    Q_PROPERTY(bool installGuestAdditions READ installGuestAdditions WRITE setInstallGuestAdditions);
    Q_PROPERTY(QString guestAdditionsISOPath READ guestAdditionsISOPath WRITE setGuestAdditionsISOPath);

public:

    UIWizardNewVMPageBasicGAInstall();
    virtual int nextId() const /* override */;

private slots:

    void sltInstallGACheckBoxToggle(bool fChecked);
    void sltPathChanged(const QString &strPath);

private:

    void retranslateUi();
    void initializePage();
    bool isComplete() const;

    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicGAInstall_h */
