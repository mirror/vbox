/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIImportApplianceWzd class declaration
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIImportApplianceWzd_h__
#define __UIImportApplianceWzd_h__

/* Global includes */
#include <QPointer>

/* Local includes */
#include "QIDialog.h"
#include "QIWizard.h"

/* Generated includes */
#include "UIImportApplianceWzdPage1.gen.h"
#include "UIImportApplianceWzdPage2.gen.h"

/* Global forwards */
class QDialogButtonBox;

typedef QPointer<UIApplianceImportEditorWidget> ImportAppliancePointer;
Q_DECLARE_METATYPE(ImportAppliancePointer);

class UIImportLicenseViewer : public QIDialog
{
    Q_OBJECT;

public:

    UIImportLicenseViewer(QWidget *pParent);

    void setContents(const QString &strName, const QString &strText);

protected:

    void retranslateUi();

private slots:

    void sltPrint();
    void sltSave();

private:

    QLabel *m_pCaption;
    QTextEdit *m_pLicenseText;
    QDialogButtonBox *m_pButtonBox;
    QPushButton *m_pPrintButton;
    QPushButton *m_pSaveButton;
    QString m_strName;
    QString m_strText;
};

class UIImportApplianceWzd : public QIWizard
{
    Q_OBJECT;

public:

    UIImportApplianceWzd(const QString &strFile = "", QWidget *pParent = 0);

    bool isValid() const;

protected:

    void retranslateUi();

private slots:

    void sltCurrentIdChanged(int iId);
};

class UIImportApplianceWzdPage1 : public QIWizardPage, public Ui::UIImportApplianceWzdPage1
{
    Q_OBJECT;

public:

    UIImportApplianceWzdPage1();

protected:

    void retranslateUi();

    void initializePage();

    bool isComplete() const;
    bool validatePage();
};

class UIImportApplianceWzdPage2 : public QIWizardPage, public Ui::UIImportApplianceWzdPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget WRITE setApplianceWidget);

public:

    UIImportApplianceWzdPage2();

protected:

    void retranslateUi();

    bool validatePage();
    bool import();

private:

    ImportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }
    void setApplianceWidget(const ImportAppliancePointer &pApplianceWidget) { m_pApplianceWidget = pApplianceWidget; }
    ImportAppliancePointer m_pApplianceWidget;
};

#endif /* __UIImportApplianceWzd_h__ */

