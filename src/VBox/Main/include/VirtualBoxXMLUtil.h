/** @file
 *
 * VirtualBox XML utility class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIRTUALBOXXMLUTIL
#define ____H_VIRTUALBOXXMLUTIL

// for BSTR referred from VBox/cfgldr.h
#include "VBox/com/defs.h"

#include <VBox/cdefs.h>
#include <VBox/cfgldr.h>
#include <iprt/file.h>

/** VirtualBox XML settings namespace */
#define VBOX_XML_NAMESPACE      "http://www.innotek.de/VirtualBox-settings"

#define VBOX_XML_VERSION "1.2"

/** VirtualBox XML settings version string */
#if defined (RT_OS_DARWIN)
#   define VBOX_XML_PLATFORM     "macosx"
#elif defined (RT_OS_FREEBSD)
#   define VBOX_XML_PLATFORM     "freebsd"
#elif defined (RT_OS_LINUX)
#   define VBOX_XML_PLATFORM     "linux"
#elif defined (RT_OS_NETBSD)
#   define VBOX_XML_PLATFORM     "netbsd"
#elif defined (RT_OS_OPENBSD)
#   define VBOX_XML_PLATFORM     "openbsd"
#elif defined (RT_OS_OS2)
#   define VBOX_XML_PLATFORM     "os2"
#elif defined (RT_OS_SOLARIS)
#   define VBOX_XML_PLATFORM     "solaris"
#elif defined (RT_OS_WINDOWS)
#   define VBOX_XML_PLATFORM     "windows"
#else
#   error Unsupported platform!
#endif

/** VirtualBox XML common settings version string */
#define VBOX_XML_PLATFORM_COMMON  "common"

/** VirtualBox XML settings schema file */
#define VBOX_XML_SCHEMA         "VirtualBox-settings-" VBOX_XML_PLATFORM ".xsd"
#define VBOX_XML_SCHEMA_COMMON  "VirtualBox-settings-" VBOX_XML_PLATFORM_COMMON ".xsd"

class VirtualBoxXMLUtil
{
protected:

    static DECLCALLBACK(int) cfgLdrEntityResolver (const char *pcszPublicId,
                                                   const char *pcszSystemId,
                                                   const char *pcszBaseURI,
                                                   PCFGLDRENTITY pEntity);

    /** VirtualBox XML settings "namespace schema" pair */
    static const char *XmlSchemaNS;
};


#endif // ____H_VIRTUALBOXXMLUTIL

