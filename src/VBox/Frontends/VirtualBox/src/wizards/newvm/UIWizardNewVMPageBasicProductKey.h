/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicProductKey class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicProductKey_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicProductKey_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QLabel;
class QLineEdit;
class QIRichTextLabel;

class UIWizardNewVMPageProductKey : public UIWizardPageBase
{
public:

    UIWizardNewVMPageProductKey();

    /** @name Property getters
      * @{ */
    QString productKey() const;
    /** @} */

protected:

    /* Widgets: */
    QLineEdit *m_pProductKeyLineEdit;
    QLabel  *m_pHostnameLabel;
};

/* 2nd page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasicProductKey : public UIWizardPage, public UIWizardNewVMPageProductKey
{
    Q_OBJECT;
    Q_PROPERTY(QString productKey READ productKey);

public:

    /* Constructor: */
    UIWizardNewVMPageBasicProductKey();

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

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicProductKey_h */
