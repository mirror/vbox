<?xml version="1.0"?>

<!--
    xml2tag.xsl:
        XSLT stylesheet that extracts just the tags of an XML document.

    Copyright (C) 2018-2020 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet
  version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:output method="text"/>

    <xsl:strip-space elements="*"/>

    <xsl:template match="*">
        <xsl:value-of select="name()"/>
        <xsl:text>&#10;</xsl:text>
        <xsl:apply-templates select="*"/>
    </xsl:template>

</xsl:stylesheet>
