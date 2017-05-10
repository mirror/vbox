/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkUtils namespace declaration.
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

#ifndef ___UIHostNetworkUtils_h___
#define ___UIHostNetworkUtils_h___

/* Qt includes: */
#include <QString>


/** Host Network Manager: Host network utilities. */
namespace UIHostNetworkUtils
{
    /** Converts IPv4 address from QString to quint32. */
    quint32 ipv4FromQStringToQuint32(const QString &strAddress);
    /** Converts IPv4 address from quint32 to QString. */
    QString ipv4FromQuint32ToQString(quint32 uAddress);
}

/* Using this namespace where included: */
using namespace UIHostNetworkUtils;

#endif /* !___UIHostNetworkUtils_h___ */

