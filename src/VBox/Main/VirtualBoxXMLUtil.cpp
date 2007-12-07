/** @file
 *
 * VirtualBox XML utility class implementation
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

#include "VirtualBoxXMLUtil.h"
#include "VirtualBoxXMLUtil_entities.h"
#include "VirtualBoxXMLUtil_common_entities.h"
#include "Logging.h"

#include <VBox/err.h>

#include <string.h>

/**
 *  Callback to resolve external entities when parsing and validating
 *  VirtualBox settings files (see FNCFGLDRENTITYRESOLVER).
 */
// static
DECLCALLBACK(int) VirtualBoxXMLUtil::cfgLdrEntityResolver (
    const char *pcszPublicId,
    const char *pcszSystemId,
    const char *pcszBaseURI,
    PCFGLDRENTITY pEntity)
{
    Assert (pEntity);
    if (!pEntity)
        return VERR_INVALID_POINTER;

#if 0
    LogFlow (("VirtualBoxXMLUtil::cfgLdrEntityResolver():\n"
              "  publicId='%s', systemId='%s', baseURI='%s'\n",
              pcszPublicId, pcszSystemId, pcszBaseURI));
#endif

    if (!strcmp (pcszSystemId, VBOX_XML_SCHEMA_COMMON))
    {
        pEntity->enmType = CFGLDRENTITYTYPE_MEMORY;
        pEntity->u.memory.puchBuf = (unsigned char*)g_abVirtualBox_settings_common_xsd;
        pEntity->u.memory.cbBuf   = g_cbVirtualBox_settings_common_xsd;
        pEntity->u.memory.bFree   = false;
        return VINF_SUCCESS;
    }
    else
    if (!strcmp (pcszSystemId, VBOX_XML_SCHEMA))
    {
        pEntity->enmType = CFGLDRENTITYTYPE_MEMORY;
        pEntity->u.memory.puchBuf = (unsigned char*)g_abVirtualBox_settings_xsd;
        pEntity->u.memory.cbBuf   = g_cbVirtualBox_settings_xsd;
        pEntity->u.memory.bFree   = false;
        return VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed (("Unexpected entity: '%s' - knows: '%s' and '%s'\n", pcszSystemId, VBOX_XML_SCHEMA_COMMON, VBOX_XML_SCHEMA));
        return VERR_GENERAL_FAILURE;
    }

    return VINF_SUCCESS;
}

// static
const char *VirtualBoxXMLUtil::XmlSchemaNS = VBOX_XML_NAMESPACE " " VBOX_XML_SCHEMA;

