/* $Id$ */

/** @file
 *
 * VirtualBox XML/Settings API utility declarations
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

/** VirtualBox XML settings namespace */
#define VBOX_XML_NAMESPACE      "http://www.innotek.de/VirtualBox-settings"

/** VirtualBox XML settings version number substring ("x.y")  */
#define VBOX_XML_VERSION        "1.3.pre"

/** VirtualBox XML settings version platform substring */
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

/** VirtualBox XML settings full version string ("x.y-platform") */
#define VBOX_XML_VERSION_FULL   VBOX_XML_VERSION "-" VBOX_XML_PLATFORM

/** VirtualBox XML settings platform-specific (main) schema file */
#define VBOX_XML_SCHEMA         "VirtualBox-settings-" VBOX_XML_PLATFORM ".xsd"
/** VirtualBox XML settings common schema include file */
#define VBOX_XML_SCHEMA_COMMON  "VirtualBox-settings-common.xsd"
/** VirtualBox XML settings root element schema include file */
#define VBOX_XML_SCHEMA_ROOT    "VirtualBox-settings-root.xsd"

/** VirtualBox XML settings converter file */
#define VBOX_XML_SETTINGS_CONVERTER "SettingsConverter.xsl"

#endif /* ____H_VIRTUALBOXXMLUTIL */
