/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicUnattended class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicUnattended_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicUnattended_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class UINameAndSystemEditor;
class QIRichTextLabel;
class QCheckBox;
class QLabel;
class UIFilePathSelector;

class UIWizardNewVMPageUnattended : public UIWizardPageBase
{
protected:

    enum ISOError
    {
        ISOError_NoError,
        ISOError_NoFileName,
        ISOError_DontExists,
        ISOError_NotReadable,
        ISOError_Max
    };

    UIWizardNewVMPageUnattended();
    bool determineOSType(const QString &strISOPath);
    virtual void updateStatusLabel();

    /** @name Property getters
      * @{ */
        QString ISOFilePath() const;
        bool isUnattendedEnabled() const;
        bool startHeadless() const;
        const QString &detectedOSTypeId() const;
        const QString &detectedOSVersion() const;
        const QString &detectedOSFlavor() const;
        const QString &detectedOSLanguages() const;
        const QString &detectedOSHints() const;
    /** @} */

    QCheckBox *m_pUnattendedCheckBox;
    QCheckBox *m_pStartHeadlessCheckBox;
    QLabel *m_pISOSelectorLabel;
    QLabel *m_pStatusLabel;
    UIFilePathSelector *m_pISOFilePathSelector;
    ISOError m_ISOError;

private:

    QString m_strDetectedOSTypeId;
    QString m_strDetectedOSVersion;
    QString m_strDetectedOSFlavor;
    QString m_strDetectedOSLanguages;
    QString m_strDetectedOSHints;

    friend class UIWizardNewVM;
};

/* 1st page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasicUnattended : public UIWizardPage, public UIWizardNewVMPageUnattended
{
    Q_OBJECT;
    Q_PROPERTY(QString ISOFilePath READ ISOFilePath);
    Q_PROPERTY(bool isUnattendedEnabled READ isUnattendedEnabled);
    Q_PROPERTY(bool startHeadless READ startHeadless);
    Q_PROPERTY(QString detectedOSTypeId READ detectedOSTypeId);
    Q_PROPERTY(QString detectedOSVersion READ detectedOSVersion);
    Q_PROPERTY(QString detectedOSFlavor READ detectedOSFlavor);
    Q_PROPERTY(QString detectedOSLanguages READ detectedOSLanguages);
    Q_PROPERTY(QString detectedOSHints READ detectedOSHints);

signals:

    void sigDetectedOSTypeChanged();

public:

    UIWizardNewVMPageBasicUnattended();
    virtual bool isComplete() const; /* override */

protected:

    UIWizardPage* thisImp() { return this; }
    virtual void updateStatusLabel();

private slots:

    void sltUnattendedCheckBoxToggle(bool fEnabled);
    void sltPathChanged(const QString &strPath);

private:

    /** Returns false if user selects unattended install and does not provide a valid ISO file path. Else returns true. */
    bool checkISOFile() const;
    void retranslateUi();
    void initializePage();
    void cleanupPage();
    virtual bool validatePage() /* override */;

    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicUnattended_h */
