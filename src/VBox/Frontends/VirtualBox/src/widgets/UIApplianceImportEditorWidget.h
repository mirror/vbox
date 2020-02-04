/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIApplianceEditorWidget.h"

/* Forward declarations: */
class UIFilePathSelector;
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
Q_DECLARE_METATYPE(MACAddressImportPolicy);

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

    QLabel             *m_pPathSelectorLabel;
    UIFilePathSelector *m_pPathSelector;
    /** Holds the checkbox that controls 'import HDs as VDI' behaviour. */
    QCheckBox          *m_pImportHDsAsVDI;
    QLabel             *m_pMACComboBoxLabel;
    QComboBox          *m_pMACComboBox;
    QGridLayout        *m_pOptionsLayout;
    QLabel             *m_pAdditionalOptionsLabel;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h */
