/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic4 class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardNewVMPageBasic4_h__
#define __UIWizardNewVMPageBasic4_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class QIRichTextLabel;
class QGroupBox;
class QRadioButton;
class VBoxMediaComboBox;
class QIToolButton;

/* 4th page of the New Virtual Machine wizard: */
class UIWizardNewVMPageBasic4 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(CMedium virtualDisk READ virtualDisk WRITE setVirtualDisk);
    Q_PROPERTY(QString virtualDiskId READ virtualDiskId WRITE setVirtualDiskId);
    Q_PROPERTY(QString virtualDiskName READ virtualDiskName WRITE setVirtualDiskName);
    Q_PROPERTY(QString virtualDiskLocation READ virtualDiskLocation WRITE setVirtualDiskLocation);

public:

    /* Constructor: */
    UIWizardNewVMPageBasic4();

private slots:

    /* Handlers: */
    void ensureNewVirtualDiskDeleted();
    void virtualDiskSourceChanged();
    void getWithFileOpenDialog();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void cleanupPage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Helpers: */
    bool getWithNewVirtualDiskWizard();

    /* Stuff for 'virtualDisk' field: */
    CMedium virtualDisk() const;
    void setVirtualDisk(const CMedium &virtualDisk);
    CMedium m_virtualDisk;

    /* Stuff for 'virtualDiskId' field: */
    QString virtualDiskId() const;
    void setVirtualDiskId(const QString &strVirtualDiskId);
    QString m_strVirtualDiskId;

    /* Stuff for 'virtualDiskName' field: */
    QString virtualDiskName() const;
    void setVirtualDiskName(const QString &strVirtualDiskName);
    QString m_strVirtualDiskName;

    /* Stuff for 'virtualDiskLocation' field: */
    QString virtualDiskLocation() const;
    void setVirtualDiskLocation(const QString &strVirtualDiskLocation);
    QString m_strVirtualDiskLocation;

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pLabel2;
    QGroupBox *m_pBootHDCnt;
    QRadioButton *m_pDiskCreate;
    QRadioButton *m_pDiskPresent;
    VBoxMediaComboBox *m_pDiskSelector;
    QIToolButton *m_pVMMButton;
};

#endif // __UIWizardNewVMPageBasic4_h__

