/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic4 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QRadioButton;
class QIRichTextLabel;
class QIToolButton;
class UIMediaComboBox;

/** 3rd page of the New Virtual Machine wizard (base part). */
class UIWizardNewVMPage4 : public UIWizardPageBase
{

protected:

    /** Constructor. */
    UIWizardNewVMPage4();

    /** Handlers. */
    void updateVirtualDiskSource();
    void getWithFileOpenDialog();
    bool getWithNewVirtualDiskWizard();


    /** @name Property getters/setters
     * @{ */
       CMedium virtualDisk() const { return m_virtualDisk; }
       void setVirtualDisk(const CMedium &virtualDisk) { m_virtualDisk = virtualDisk; }
       QUuid virtualDiskId() const { return m_uVirtualDiskId; }
       void setVirtualDiskId(const QUuid &uVirtualDiskId) { m_uVirtualDiskId = uVirtualDiskId; }
       QString virtualDiskName() const { return m_strVirtualDiskName; }
       void setVirtualDiskName(const QString &strVirtualDiskName) { m_strVirtualDiskName = strVirtualDiskName; }
       QString virtualDiskLocation() const { return m_strVirtualDiskLocation; }
       void setVirtualDiskLocation(const QString &strVirtualDiskLocation) { m_strVirtualDiskLocation = strVirtualDiskLocation; }
    /** @} */

    QWidget *createDiskWidgets();

    /** Helpers. */
    void ensureNewVirtualDiskDeleted();
    void retranslateWidgets();

    /** Input. */
    bool m_fRecommendedNoDisk;

    /** @name Variables
     * @{ */
       CMedium m_virtualDisk;
       QUuid   m_uVirtualDiskId;
       QString m_strVirtualDiskName;
       QString m_strVirtualDiskLocation;
    /** @} */

    /** @name Widgets
     * @{ */
       QRadioButton *m_pDiskSkip;
       QRadioButton *m_pDiskCreate;
       QRadioButton *m_pDiskPresent;
       UIMediaComboBox *m_pDiskSelector;
       QIToolButton *m_pVMMButton;
    /** @} */

};

/** 3rd page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMPageBasic4 : public UIWizardPage, public UIWizardNewVMPage4
{
    Q_OBJECT;
    Q_PROPERTY(CMedium virtualDisk READ virtualDisk WRITE setVirtualDisk);
    Q_PROPERTY(QUuid virtualDiskId READ virtualDiskId WRITE setVirtualDiskId);
    Q_PROPERTY(QString virtualDiskName READ virtualDiskName WRITE setVirtualDiskName);
    Q_PROPERTY(QString virtualDiskLocation READ virtualDiskLocation WRITE setVirtualDiskLocation);

public:

    /** Constructor. */
    UIWizardNewVMPageBasic4();

protected:

    /** Wrapper to access 'wizard' from base part. */
    UIWizard *wizardImp() const { return wizard(); }
    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /** Handlers. */
    void sltVirtualDiskSourceChanged();
    void sltGetWithFileOpenDialog();

private:


    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    void cleanupPage();

    /** Validation stuff. */
    bool isComplete() const;
    virtual bool validatePage() /* override */;

    /** Widgets. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h */
