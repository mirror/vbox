/* $Id: VirtualBoxImpl.h 41951 2009-01-22 21:23:10Z bird $ */

/** @file
 *
 * VirtualBox COM class implementation, extra bits
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_VIRTUALBOXIMPLEXTRA
#define ____H_VIRTUALBOXIMPLEXTRA

class SettingsTreeHelper : public settings::XmlTreeBackend::InputResolver
                         , public settings::XmlTreeBackend::AutoConverter
{
public:

    // InputResolver interface
    xml::Input *resolveEntity (const char *aURI, const char *aID);

    // AutoConverter interface
    bool needsConversion (const settings::Key &aRoot, char **aOldVersion) const;
    const char *templateUri() const;
};

#endif // ____H_VIRTUALBOXIMPL EXTRA
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
