/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIApplianceImportEditorWidget_h__
#define __UIApplianceImportEditorWidget_h__

/* GUI includes: */
#include "UIApplianceEditorWidget.h"

/* Forward declarations: */
class UIFilePathSelector;
class QIRichTextLabel;
class QComboBox;
class QGridLayout;

/** MAC address policies. */
enum MACAddressImportPolicy
{
    MACAddressImportPolicy_KeepAllMACs,
    MACAddressImportPolicy_KeepNATMACs,
    MACAddressImportPolicy_StripAllMACs,
    MACAddressImportPolicy_MAX
};

class UIApplianceImportEditorWidget: public UIApplianceEditorWidget
{
    Q_OBJECT;

public:
    UIApplianceImportEditorWidget(QWidget *pParent);

    bool setFile(const QString &strFile);
    void prepareImport();
    bool import();

    QList<QPair<QString, QString> > licenseAgreements() const;

protected:
    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    void    sltHandlePathChanged(const QString &newPath);

private:

    void    prepareWidgets();
    /** Populates MAC address policies. */
    void    populateMACAddressImportPolicies();
    void    setMACAddressImportPolicy(MACAddressImportPolicy enmMACAddressImportPolicy);
    void    sltHandleMACAddressImportPolicyComboChange();
    void    updateMACAddressImportPolicyComboToolTip();

    QIRichTextLabel    *m_pPathSelectorLabel;
    UIFilePathSelector *m_pPathSelector;
    /** Holds the checkbox that controls 'import HDs as VDI' behaviour. */
    QCheckBox          *m_pImportHDsAsVDI;
    QLabel             *m_pMACComboBoxLabel;
    QComboBox          *m_pMACComboBox;
    QGridLayout        *m_pOptionsLayout;
    QLabel             *m_pAdditionalOptionsLabel;
};

#endif /* __UIApplianceImportEditorWidget_h__ */
