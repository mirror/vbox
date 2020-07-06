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
class QToolBox;
class QIRichTextLabel;
class UIFilePathSelector;
class UIUserNamePasswordEditor;
struct UIUnattendedInstallData;


class UIWizardNewVMPage2 : public UIWizardPageBase
{
public:

    UIWizardNewVMPage2();

    /** @name Property getters/setters
      * @{ */
        QString userName() const;
        void setUserName(const QString &strName);
        QString password() const;
        void setPassword(const QString &strPassword);
        QString hostname() const;
        void setHostname(const QString &strHostName);
        bool installGuestAdditions() const;
        void setInstallGuestAdditions(bool fInstallGA);
        QString guestAdditionsISOPath() const;
        void setGuestAdditionsISOPath(const QString &strISOPath);
        QString productKey() const;
    /** @} */

protected:
    enum
    {
        ToolBoxItems_UserNameHostname,
        ToolBoxItems_GAInstall,
        ToolBoxItems_ProductKey
    };

    QWidget *createUserNameHostNameWidgets();
    QWidget *createGAInstallWidgets();
    QWidget *createProductKeyWidgets();

    bool checkGAISOFile() const;

    /** @name Widgets
      * @{ */
        QToolBox *m_pToolBox;
        UIUserNamePasswordEditor *m_pUserNamePasswordEditor;
        QLineEdit *m_pHostnameLineEdit;
        QLabel  *m_pHostnameLabel;
        /** Guest additions iso selection widgets. */
        QCheckBox *m_pInstallGACheckBox;
        QLabel  *m_pISOPathLabel;
        UIFilePathSelector *m_pISOFilePathSelector;
        /** Product key stuff. */
        QLineEdit *m_pProductKeyLineEdit;
        QLabel  *m_pProductKeyLabel;
    /** @} */

};

class UIWizardNewVMPageBasic2 : public UIWizardPage, public UIWizardNewVMPage2
{
    Q_OBJECT;
    Q_PROPERTY(QString userName READ userName WRITE setUserName);
    Q_PROPERTY(QString password READ password WRITE setPassword);
    Q_PROPERTY(QString hostname READ hostname WRITE setHostname);
    Q_PROPERTY(bool installGuestAdditions READ installGuestAdditions WRITE setInstallGuestAdditions);
    Q_PROPERTY(QString guestAdditionsISOPath READ guestAdditionsISOPath WRITE setGuestAdditionsISOPath);
    Q_PROPERTY(QString productKey READ productKey);


public:

    UIWizardNewVMPageBasic2();

protected:

    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Don't reset the user entered values in case of "back" button press. */
    virtual void cleanupPage() /* override */;

private slots:

    void sltInstallGACheckBoxToggle(bool fChecked);
    void sltGAISOPathChanged(const QString &strPath);

private:

    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    bool isComplete() const;
    /** Returns true if we show the widgets for guest os product key. */
    bool isProductKeyWidgetVisible() const;

    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h */
