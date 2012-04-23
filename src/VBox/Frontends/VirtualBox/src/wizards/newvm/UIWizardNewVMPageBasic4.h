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

/* Global includes: */
#include <QVariant>

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class QGroupBox;
class QRadioButton;
class VBoxMediaComboBox;
class QIToolButton;
class QIRichTextLabel;

/* 4th page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPage4 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPage4();

    /* Handlers: */
    void updateVirtualDiskSource();
    void getWithFileOpenDialog();
    bool getWithNewVirtualDiskWizard();

    /* Stuff for 'virtualDisk' field: */
    CMedium virtualDisk() const { return m_virtualDisk; }
    void setVirtualDisk(const CMedium &virtualDisk) { m_virtualDisk = virtualDisk; }

    /* Stuff for 'virtualDiskId' field: */
    QString virtualDiskId() const { return m_strVirtualDiskId; }
    void setVirtualDiskId(const QString &strVirtualDiskId) { m_strVirtualDiskId = strVirtualDiskId; }

    /* Stuff for 'virtualDiskName' field: */
    QString virtualDiskName() const { return m_strVirtualDiskName; }
    void setVirtualDiskName(const QString &strVirtualDiskName) { m_strVirtualDiskName = strVirtualDiskName; }

    /* Stuff for 'virtualDiskLocation' field: */
    QString virtualDiskLocation() const { return m_strVirtualDiskLocation; }
    void setVirtualDiskLocation(const QString &strVirtualDiskLocation) { m_strVirtualDiskLocation = strVirtualDiskLocation; }

    /* Helpers: */
    void ensureNewVirtualDiskDeleted();

    /* Variables: */
    CMedium m_virtualDisk;
    QString m_strVirtualDiskId;
    QString m_strVirtualDiskName;
    QString m_strVirtualDiskLocation;

    /* Widgets: */
    QGroupBox *m_pDiskCnt;
    QRadioButton *m_pDiskCreate;
    QRadioButton *m_pDiskPresent;
    VBoxMediaComboBox *m_pDiskSelector;
    QIToolButton *m_pVMMButton;
};

/* 4th page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasic4 : public UIWizardPage, public UIWizardNewVMPage4
{
    Q_OBJECT;
    Q_PROPERTY(CMedium virtualDisk READ virtualDisk WRITE setVirtualDisk);
    Q_PROPERTY(QString virtualDiskId READ virtualDiskId WRITE setVirtualDiskId);
    Q_PROPERTY(QString virtualDiskName READ virtualDiskName WRITE setVirtualDiskName);
    Q_PROPERTY(QString virtualDiskLocation READ virtualDiskLocation WRITE setVirtualDiskLocation);

public:

    /* Constructor: */
    UIWizardNewVMPageBasic4();

protected:

    /* Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }
    /* Wrapper to access 'wizard-field' from base part: */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /* Handlers: */
    void sltVirtualDiskSourceChanged();
    void sltGetWithFileOpenDialog();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void cleanupPage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel1;
    QIRichTextLabel *m_pLabel2;
};

#endif // __UIWizardNewVMPageBasic4_h__

