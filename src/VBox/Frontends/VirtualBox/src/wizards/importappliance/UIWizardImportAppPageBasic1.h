/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic1 class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardImportAppDefs.h"
#include "UIWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudProfile.h"
#include "CCloudProvider.h"
#include "CCloudProviderManager.h"

/* Forward declarations: */
class QLabel;
class QStackedLayout;
class QIComboBox;
class QIRichTextLabel;
class UIEmptyFilePathSelector;

/** Source combo data fields. */
enum
{
    SourceData_ID              = Qt::UserRole + 1,
    SourceData_Name            = Qt::UserRole + 2,
    SourceData_ShortName       = Qt::UserRole + 3,
    SourceData_IsItCloudFormat = Qt::UserRole + 4
};

/** UIWizardPageBase extension for 1st page of the Import Appliance wizard. */
class UIWizardImportAppPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardImportAppPage1(bool fImportFromOCIByDefault);

    /** Populates sources. */
    void populateSources();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Updates source combo tool-tips. */
    void updateSourceComboToolTip();

    /** Defines @a strSource. */
    void setSource(const QString &strSource);
    /** Returns source. */
    QString source() const;
    /** Returns whether source under certain @a iIndex is cloud one. */
    bool isSourceCloudOne(int iIndex = -1) const;

    /** Returns source ID. */
    QUuid sourceId() const;

    /** Holds whether default source should be Import from OCI. */
    bool  m_fImportFromOCIByDefault;

    /** Holds the Cloud Provider Manager reference. */
    CCloudProviderManager  m_comCloudProviderManager;
    /** Holds the Cloud Provider object reference. */
    CCloudProvider         m_comCloudProvider;
    /** Holds the Cloud Profile object reference. */
    CCloudProfile          m_comCloudProfile;

    /** Holds the source layout instance. */
    QGridLayout *m_pSourceLayout;
    /** Holds the source type label instance. */
    QLabel      *m_pSourceLabel;
    /** Holds the source type combo-box instance. */
    QIComboBox  *m_pSourceComboBox;

    /** Holds the stacked layout instance. */
    QStackedLayout *m_pStackedLayout;

    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;

    /** Holds the cloud container layout instance. */
    QGridLayout *m_pCloudContainerLayout;
};

/** UIWizardPage extension for 1st page of the Import Appliance wizard, extends UIWizardImportAppPage1 as well. */
class UIWizardImportAppPageBasic1 : public UIWizardPage, public UIWizardImportAppPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source WRITE setSource);
    Q_PROPERTY(bool isSourceCloudOne READ isSourceCloudOne);

public:

    /** Constructs 1st basic page. */
    UIWizardImportAppPageBasic1(bool fImportFromOCIByDefault);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles import source change. */
    void sltHandleSourceChange();

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h */
