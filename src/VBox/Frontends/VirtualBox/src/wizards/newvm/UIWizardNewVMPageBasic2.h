/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic2 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QIRichTextLabel;
class UIFilePathSelector;
class UIUserNamePasswordEditor;
struct UIUnattendedInstallData;


class UIWizardNewVMPage2 : public UIWizardPageBase
{
public:

    UIWizardNewVMPage2();

protected:

    void markWidgets() const;
    void retranslateWidgets();

    QString ISOFilePath() const;
    bool isUnattendedEnabled() const;
    const QString &detectedOSTypeId() const;
    bool determineOSType(const QString &strISOPath);
    bool isISOFileSelectorComplete() const;
    void setTypeByISODetectedOSType(const QString &strDetectedOSType);
    /** Return false if ISO path is not empty but points to an missing or unreadable file. */
    bool checkISOFile() const;

    /** @name Widgets
      * @{ */
        mutable UIFilePathSelector *m_pISOFilePathSelector;
        QIRichTextLabel *m_pUnattendedLabel;
    /** @} */

    QString m_strDetectedOSTypeId;

private:

    friend class UIWizardNewVM;
};

class UIWizardNewVMPageBasic2 : public UIWizardPage, public UIWizardNewVMPage2
{

    Q_OBJECT;
    Q_PROPERTY(QString ISOFilePath READ ISOFilePath);
    Q_PROPERTY(bool isUnattendedEnabled READ isUnattendedEnabled);
    Q_PROPERTY(QString detectedOSTypeId READ detectedOSTypeId);

public:

    UIWizardNewVMPageBasic2();
    virtual int nextId() const /* override */;

protected:

    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Don't reset the user entered values in case of "back" button press. */
    virtual void cleanupPage() /* override */;
    virtual bool isComplete() const; /* override */

private slots:

    void sltISOPathChanged(const QString &strPath);

private:

    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h */
