/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIExportApplianceWzd class declaration
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

#ifndef __UIExportApplianceWzd_h__
#define __UIExportApplianceWzd_h__

/* Global includes */
#include <QPointer>

/* Local includes */
#include "QIWizard.h"

/* Generated includes */
#include "UIExportApplianceWzdPage1.gen.h"
#include "UIExportApplianceWzdPage2.gen.h"
#include "UIExportApplianceWzdPage3.gen.h"
#include "UIExportApplianceWzdPage4.gen.h"

/* Local forwards */
class CAppliance;

enum StorageType { Filesystem, SunCloud, S3 };
Q_DECLARE_METATYPE(StorageType);

typedef QPointer<VBoxExportApplianceWgt> ExportAppliancePointer;
Q_DECLARE_METATYPE(ExportAppliancePointer);

class UIExportApplianceWzd : public QIWizard
{
    Q_OBJECT;

public:

    UIExportApplianceWzd(QWidget *pParent, const QString &strSelectedVMName = QString());

protected:

    void retranslateUi();

private slots:

    void sltCurrentIdChanged(int iId);
};

class UIExportApplianceWzdPage1 : public QIWizardPage, public Ui::UIExportApplianceWzdPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString selectedVMName READ selectedVMName WRITE setSelectedVMName);
    Q_PROPERTY(QStringList machineNames READ machineNames WRITE setMachineNames);
    Q_PROPERTY(QStringList machineIDs READ machineIDs WRITE setMachineIDs);

public:

    UIExportApplianceWzdPage1();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    bool isComplete() const;

private slots:

    void sltSelectedVMChanged();

private:

    void populateVMSelectorItems();

    QString selectedVMName() const { return m_strSelectedVMName; }
    void setSelectedVMName(const QString &strSelectedVMName) { m_strSelectedVMName = strSelectedVMName; }
    QString m_strSelectedVMName;

    QStringList machineNames() const { return m_MachineNames; }
    void setMachineNames(const QStringList &machineNames) { m_MachineNames = machineNames; }
    QStringList m_MachineNames;

    QStringList machineIDs() const { return m_MachineIDs; }
    void setMachineIDs(const QStringList &machineIDs) { m_MachineIDs = machineIDs; }
    QStringList m_MachineIDs;
};

class UIExportApplianceWzdPage2 : public QIWizardPage, public Ui::UIExportApplianceWzdPage2
{
    Q_OBJECT;
    Q_PROPERTY(ExportAppliancePointer applianceWidget READ applianceWidget WRITE setApplianceWidget);

public:

    UIExportApplianceWzdPage2();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    int nextId() const;

private:

    bool prepareSettingsWidget();

    ExportAppliancePointer applianceWidget() const { return m_pApplianceWidget; }
    void setApplianceWidget(const ExportAppliancePointer &pApplianceWidget) { m_pApplianceWidget = pApplianceWidget; }
    ExportAppliancePointer m_pApplianceWidget;
};

class UIExportApplianceWzdPage3 : public QIWizardPage, public Ui::UIExportApplianceWzdPage3
{
    Q_OBJECT;
    Q_PROPERTY(StorageType storageType READ storageType WRITE setStorageType);

public:

    UIExportApplianceWzdPage3();

protected:

    void retranslateUi();

    void initializePage();

private slots:

    void sltStorageTypeChanged();

private:

    StorageType storageType() const { return m_StorageType; }
    void setStorageType(StorageType storageType) { m_StorageType = storageType; }
    StorageType m_StorageType;
};

class UIExportApplianceWzdPage4 : public QIWizardPage, public Ui::UIExportApplianceWzdPage4
{
    Q_OBJECT;

public:

    UIExportApplianceWzdPage4();

protected:

    void retranslateUi();

    void initializePage();

    bool isComplete() const;
    bool validatePage();

private:

    bool exportAppliance();
    bool exportVMs(CAppliance &appliance);
    QString uri() const;

    QString m_strDefaultApplianceName;
};

#endif /* __UIExportApplianceWzd_h__ */

