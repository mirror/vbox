/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkUtils namespace implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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

/* Qt includes: */
# include <QStringList>

/* GUI includes: */
# include "UIHostNetworkUtils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


quint32 UIHostNetworkUtils::ipv4FromQStringToQuint32(const QString &strAddress)
{
    quint32 uAddress = 0;
    foreach (const QString &strPart, strAddress.split('.'))
    {
        uAddress = uAddress << 8;
        bool fOk = false;
        uint uPart = strPart.toUInt(&fOk);
        if (fOk)
            uAddress += uPart;
    }
    return uAddress;
}

QString UIHostNetworkUtils::ipv4FromQuint32ToQString(quint32 uAddress)
{
    QStringList address;
    while (uAddress)
    {
        uint uPart = uAddress & 0xFF;
        address.prepend(QString::number(uPart));
        uAddress = uAddress >> 8;
    }
    return address.join('.');
}

