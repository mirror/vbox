/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h
#define FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CAppliance.h"
#include "CCloudClient.h"
#include "CVirtualSystemDescription.h"
#include "CVirtualSystemDescriptionForm.h"

/** MAC address export policies. */
enum MACAddressExportPolicy
{
    MACAddressExportPolicy_KeepAllMACs,
    MACAddressExportPolicy_StripAllNonNATMACs,
    MACAddressExportPolicy_StripAllMACs,
    MACAddressExportPolicy_MAX
};
Q_DECLARE_METATYPE(MACAddressExportPolicy);

/** Cloud export option modes. */
enum CloudExportMode
{
    CloudExportMode_Invalid,
    CloudExportMode_AskThenExport,
    CloudExportMode_ExportThenAsk,
    CloudExportMode_DoNotAsk
};
Q_DECLARE_METATYPE(CloudExportMode);

/** Export Appliance wizard. */
class UIWizardExportApp : public UINativeWizard
{
    Q_OBJECT;

public:

    /** Constructs Export Appliance wizard passing @a pParent to the base-class.
      * @param  predefinedMachineNames  Brings the predefined list of machine names.
      * @param  fFastTraverToExportOCI  Brings whether wizard should start with OCI target. */
    UIWizardExportApp(QWidget *pParent,
                      const QStringList &predefinedMachineNames = QStringList(),
                      bool fFastTraverToExportOCI = false);

    /** @name Common fields.
      * @{ */
        /** Returns a list of machine names. */
        QStringList machineNames() const;
        /** Defines a list of machine @a names. */
        void setMachineNames(const QStringList &names);
        /** Returns a list of machine IDs. */
        QList<QUuid> machineIDs() const;
        /** Defines a list of machine @a ids. */
        void setMachineIDs(const QList<QUuid> &ids);

        /** Returns format. */
        QString format() const;
        /** Defines @a strFormat. */
        void setFormat(const QString &strFormat);

        /** Returns whether format is cloud one. */
        bool isFormatCloudOne() const;
        /** Defines whether format is @a fCloudOne. */
        void setFormatCloudOne(bool fCloudOne);
    /** @} */

    /** @name Local export fields.
      * @{ */
        /** Returns path. */
        QString path() const;
        /** Defines @a strPath. */
        void setPath(const QString &strPath);

        /** Returns MAC address export policy. */
        MACAddressExportPolicy macAddressExportPolicy() const;
        /** Defines MAC address export @a enmPolicy. */
        void setMACAddressExportPolicy(MACAddressExportPolicy enmPolicy);

        /** Returns whether manifest is selected. */
        bool isManifestSelected() const;
        /** Defines whether manifest is @a fSelected. */
        void setManifestSelected(bool fSelected);

        /** Returns whether include ISOs is selected. */
        bool isIncludeISOsSelected() const;
        /** Defines whether include ISOs is @a fSelected. */
        void setIncludeISOsSelected(bool fSelected);

        /** Returns local appliance object. */
        CAppliance localAppliance();
        /** Defines local @a comAppliance object. */
        void setLocalAppliance(const CAppliance &comAppliance);
    /** @} */

    /** @name Cloud export fields.
      * @{ */
        /** Returns profile name. */
        QString profileName() const;
        /** Defines profile @a strName. */
        void setProfileName(const QString &strName);

        /** Returns cloud appliance object. */
        CAppliance cloudAppliance();
        /** Defines cloud @a comAppliance object. */
        void setCloudAppliance(const CAppliance &comAppliance);

        /** Returns cloud client object. */
        CCloudClient cloudClient();
        /** Defines cloud @a comClient object. */
        void setCloudClient(const CCloudClient &comClient);

        /** Returns virtual system description object. */
        CVirtualSystemDescription vsd();
        /** Defines virtual system @a comDescription object. */
        void setVsd(const CVirtualSystemDescription &comDescription);

        /** Returns virtual system description export form object. */
        CVirtualSystemDescriptionForm vsdExportForm();
        /** Defines virtual system description export @a comForm object. */
        void setVsdExportForm(const CVirtualSystemDescriptionForm &comForm);

        /** Returns cloud export mode. */
        CloudExportMode cloudExportMode() const;
        /** Defines cloud export @a enmMode. */
        void setCloudExportMode(const CloudExportMode &enmMode);
    /** @} */

    /** @name Auxiliary stuff.
      * @{ */
        /** Goes forward. Required for fast travel to next page. */
        void goForward();

        /** Composes universal resource identifier.
          * @param  fWithFile  Brings whether uri should include file name as well. */
        QString uri(bool fWithFile = true) const;

        /** Exports Appliance. */
        bool exportAppliance();
    /** @} */

protected:

    /** @name Virtual stuff.
      * @{ */
        /** Populates pages. */
        virtual void populatePages() /* override final */;

        /** Handles translation event. */
        virtual void retranslateUi() /* override final */;
    /** @} */

private:

    /** @name Auxiliary stuff.
      * @{ */
        /** Exports VMs enumerated in @a comAppliance. */
        bool exportVMs(CAppliance &comAppliance);
    /** @} */

    /** @name Arguments.
      * @{ */
        /** Holds the predefined list of machine names. */
        QStringList  m_predefinedMachineNames;
        /** Holds whether we should fast travel to page 2. */
        bool         m_fFastTraverToExportOCI;
    /** @} */

    /** @name Common fields.
      * @{ */
        /** Holds the list of machine names. */
        QStringList   m_machineNames;
        /** Holds the list of machine IDs. */
        QList<QUuid>  m_machineIDs;

        /** Holds the format. */
        QString  m_strFormat;
        /** Holds whether format is cloud one. */
        bool     m_fFormatCloudOne;
    /** @} */

    /** @name Local export fields.
      * @{ */
        /** Holds the path. */
        QString                 m_strPath;
        /** Holds the MAC address export policy. */
        MACAddressExportPolicy  m_enmMACAddressExportPolicy;
        /** Holds whether manifest is selected. */
        bool                    m_fManifestSelected;
        /** Holds whether ISOs are included. */
        bool                    m_fIncludeISOsSelected;
        /** Holds local appliance object. */
        CAppliance              m_comLocalAppliance;
    /** @} */

    /** @name Cloud export fields.
      * @{ */
        /** Holds profile name. */
        QString                        m_strProfileName;
        /** Holds cloud appliance object. */
        CAppliance                     m_comCloudAppliance;
        /** Returns cloud client object. */
        CCloudClient                   m_comCloudClient;
        /** Returns virtual system description object. */
        CVirtualSystemDescription      m_comVsd;
        /** Returns virtual system description export form object. */
        CVirtualSystemDescriptionForm  m_comVsdExportForm;
        /** Returns cloud export mode. */
        CloudExportMode                m_enmCloudExportMode;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h */
