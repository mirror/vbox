/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic2 class declaration
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardFirstRunPageBasic2_h__
#define __UIWizardFirstRunPageBasic2_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class CMachine;
class QGroupBox;
class VBoxMediaComboBox;
class QIToolButton;
class QIRichTextLabel;

/* 2nd page of the First Run wizard (base part): */
class UIWizardFirstRunPage2 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardFirstRunPage2(bool fBootHardDiskWasSet);

    /* Handlers: */
    void onOpenMediumWithFileOpenDialog();

    /* Stuff for 'id' field: */
    QString id() const;
    void setId(const QString &strId);

    /* Variables: */
    bool m_fBootHardDiskWasSet;

    /* Widgets: */
    QGroupBox *m_pSourceCnt;
    VBoxMediaComboBox *m_pMediaSelector;
    QIToolButton *m_pSelectMediaButton;
};

/* 2nd page of the First Run wizard (basic extension): */
class UIWizardFirstRunPageBasic2 : public UIWizardPage, public UIWizardFirstRunPage2
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source);
    Q_PROPERTY(QString id READ id WRITE setId);

public:

    /* Constructor: */
    UIWizardFirstRunPageBasic2(const QString &strMachineId, bool fBootHardDiskWasSet);

protected:

    /* Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }

private slots:

    /* Open with file-open dialog: */
    void sltOpenMediumWithFileOpenDialog();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Validation stuff: */
    bool validatePage();

    /* Stuff for 'source' field: */
    QString source() const;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif // __UIWizardFirstRunPageBasic2_h__

