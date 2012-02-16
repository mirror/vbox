/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewVMWzd class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINewVMWzd_h__
#define __UINewVMWzd_h__


/* Local includes */
#include "QIWizard.h"
#include "COMDefs.h"
#include "QIWithRetranslateUI.h"

/* Generated includes */
#include "UINewVMWzdPage1.gen.h"
#include "UINewVMWzdPage2.gen.h"
#include "UINewVMWzdPage3.gen.h"
#include "UINewVMWzdPage4.gen.h"
#include "UINewVMWzdPage5.gen.h"

class UINewVMWzd : public QIWizard
{
    Q_OBJECT;

public:

    UINewVMWzd(QWidget *pParent);

    const CMachine machine() const;

protected:

    void retranslateUi();
};

class UINewVMWzdPage1 : public QIWizardPage, public Ui::UINewVMWzdPage1
{
    Q_OBJECT;

public:

    UINewVMWzdPage1();

protected:

    void retranslateUi();

    void initializePage();

private:

    QString m_strTableTemplate;
};

class UINewVMWzdPage2 : public QIWizardPage, public Ui::UINewVMWzdPage2
{
    Q_OBJECT;
    Q_PROPERTY(QString machineFolder READ machineFolder WRITE setMachineFolder);

public:

    UINewVMWzdPage2();

protected slots:

    void sltNameChanged(const QString &strNewText);
    void sltOsTypeChanged();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    bool validatePage();

private:

    bool createMachineFolder();
    bool cleanupMachineFolder();

    QString machineFolder() const;
    void setMachineFolder(const QString &strMachineFolder);
    QString m_strMachineFolder;
};

class UINewVMWzdPage3 : public QIWizardPage, public Ui::UINewVMWzdPage3
{
    Q_OBJECT;

public:

    UINewVMWzdPage3();

protected:

    void retranslateUi();

    void initializePage();

    bool isComplete() const;

private slots:

    void ramSliderValueChanged(int iValue);
    void ramEditorTextChanged(const QString &strText);
};

class UINewVMWzdPage4 : public QIWizardPage, public Ui::UINewVMWzdPage4
{
    Q_OBJECT;
    Q_PROPERTY(CMedium hardDisk READ hardDisk WRITE setHardDisk);
    Q_PROPERTY(QString hardDiskId READ hardDiskId WRITE setHardDiskId);
    Q_PROPERTY(QString hardDiskName READ hardDiskName WRITE setHardDiskName);
    Q_PROPERTY(QString hardDiskLocation READ hardDiskLocation WRITE setHardDiskLocation);

public:

    UINewVMWzdPage4();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    bool isComplete() const;
    bool validatePage();

private slots:

    void ensureNewHardDiskDeleted();
    void hardDiskSourceChanged();
    void getWithFileOpenDialog();

private:

    bool getWithNewHardDiskWizard();

    CMedium hardDisk() const;
    void setHardDisk(const CMedium &hardDisk);
    CMedium m_HardDisk;

    QString hardDiskId() const;
    void setHardDiskId(const QString &strHardDiskId);
    QString m_strHardDiskId;

    QString hardDiskName() const;
    void setHardDiskName(const QString &strHardDiskName);
    QString m_strHardDiskName;

    QString hardDiskLocation() const;
    void setHardDiskLocation(const QString &strHardDiskLocation);
    QString m_strHardDiskLocation;
};

class UINewVMWzdPage5 : public QIWizardPage, public Ui::UINewVMWzdPage5
{
    Q_OBJECT;
    Q_PROPERTY(CMachine machine READ machine WRITE setMachine);

public:

    UINewVMWzdPage5();

protected:

    void retranslateUi();

    void initializePage();

    bool validatePage();

private:

    bool constructMachine();

    CMachine machine() const;
    void setMachine(const CMachine &machine);
    CMachine m_Machine;

    QString getNextControllerName(KStorageBus type);
    int m_iIDECount;
    int m_iSATACount;
    int m_iSCSICount;
    int m_iFloppyCount;
    int m_iSASCount;
};

#endif // __UINewVMWzd_h__

