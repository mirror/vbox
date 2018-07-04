/* $Id$ */
/** @file
 * VBox Qt GUI - UIMedium related implementations.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIMediumDefs.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CMediumFormat.h"
# include "CSystemProperties.h"

/* COM includes: */
# include "CMediumFormat.h"
# include "CSystemProperties.h"
# include "CVirtualBox.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* Convert global medium type (KDeviceType) to local (UIMediumType): */
UIMediumType UIMediumDefs::mediumTypeToLocal(KDeviceType globalType)
{
    switch (globalType)
    {
        case KDeviceType_HardDisk:
            return UIMediumType_HardDisk;
        case KDeviceType_DVD:
            return UIMediumType_DVD;
        case KDeviceType_Floppy:
            return UIMediumType_Floppy;
        default:
            break;
    }
    return UIMediumType_Invalid;
}

/* Convert local medium type (UIMediumType) to global (KDeviceType): */
KDeviceType UIMediumDefs::mediumTypeToGlobal(UIMediumType localType)
{
    switch (localType)
    {
        case UIMediumType_HardDisk:
            return KDeviceType_HardDisk;
        case UIMediumType_DVD:
            return KDeviceType_DVD;
        case UIMediumType_Floppy:
            return KDeviceType_Floppy;
        default:
            break;
    }
    return KDeviceType_Null;
}

QList<QPair<QString, QString> > UIMediumDefs::MediumBackends(const CVirtualBox &comVBox, KDeviceType enmType)
{
    /* Prepare a list of pairs with the form <tt>{"Backend Name", "*.suffix1 .suffix2 ..."}</tt>. */
    const CSystemProperties comSystemProperties = comVBox.GetSystemProperties();
    QVector<CMediumFormat> mediumFormats = comSystemProperties.GetMediumFormats();
    QList<QPair<QString, QString> > backendPropList;
    for (int i = 0; i < mediumFormats.size(); ++i)
    {
        /* Acquire file extensions & device types: */
        QVector<QString> fileExtensions;
        QVector<KDeviceType> deviceTypes;
        mediumFormats[i].DescribeFileExtensions(fileExtensions, deviceTypes);

        /* Compose filters list: */
        QStringList filters;
        for (int iExtensionIndex = 0; iExtensionIndex < fileExtensions.size(); ++iExtensionIndex)
            if (deviceTypes[iExtensionIndex] == enmType)
                filters << QString("*.%1").arg(fileExtensions[iExtensionIndex]);
        /* Create a pair out of the backend description and all suffix's. */
        if (!filters.isEmpty())
            backendPropList << QPair<QString, QString>(mediumFormats[i].GetName(), filters.join(" "));
    }
    return backendPropList;
}

QList<QPair<QString, QString> > UIMediumDefs::HDDBackends(const CVirtualBox &comVBox)
{
    return MediumBackends(comVBox, KDeviceType_HardDisk);
}

QList<QPair<QString, QString> > UIMediumDefs::DVDBackends(const CVirtualBox &comVBox)
{
    return MediumBackends(comVBox, KDeviceType_DVD);
}

QList<QPair<QString, QString> > UIMediumDefs::FloppyBackends(const CVirtualBox &comVBox)
{
    return MediumBackends(comVBox, KDeviceType_Floppy);
}

QString UIMediumDefs::getPreferredExtensionForMedium(KDeviceType enmDeviceType)
{
    CSystemProperties comSystemProperties = vboxGlobal().virtualBox().GetSystemProperties();
    QVector<CMediumFormat> mediumFormats = comSystemProperties.GetMediumFormats();
    for (int i = 0; i < mediumFormats.size(); ++i)
    {
        /* File extensions */
        QVector <QString> fileExtensions;
        QVector <KDeviceType> deviceTypes;

        mediumFormats[i].DescribeFileExtensions(fileExtensions, deviceTypes);
        if (fileExtensions.size() != deviceTypes.size())
            continue;
        for (int a = 0; a < fileExtensions.size(); ++a)
        {
            if (deviceTypes[a] == enmDeviceType)
                return fileExtensions[a];
        }
    }
    return QString();
}

QVector<CMediumFormat> UIMediumDefs::getFormatsForDeviceType(KDeviceType enmDeviceType)
{
    CSystemProperties comSystemProperties = vboxGlobal().virtualBox().GetSystemProperties();
    QVector<CMediumFormat> mediumFormats = comSystemProperties.GetMediumFormats();
    QVector<CMediumFormat> formatList;
    for (int i = 0; i < mediumFormats.size(); ++i)
    {
        /* File extensions */
        QVector <QString> fileExtensions;
        QVector <KDeviceType> deviceTypes;

        mediumFormats[i].DescribeFileExtensions(fileExtensions, deviceTypes);
        if (deviceTypes.contains(enmDeviceType))
            formatList.push_back(mediumFormats[i]);
    }
    return formatList;
}
