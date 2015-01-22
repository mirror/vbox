<?xml version="1.0"?>

<!--
    apiwrap-server-filelist.xsl:

        XSLT stylesheet that generate a makefile include with
        the lists of files that apiwrap-server.xsl produces
        from VirtualBox.xidl.

    Copyright (C) 2015 Oracle Corporation

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
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common"
    extension-element-prefixes="exsl">

<xsl:output method="text"/>

<xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_sNewLine">
    <xsl:choose>
        <xsl:when test="$KBUILD_HOST = 'win'">
            <xsl:value-of select="'&#13;&#10;'"/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="'&#10;'"/>
        </xsl:otherwise>
    </xsl:choose>
</xsl:variable>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  wildcard match, ignore everything which has no explicit match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="*" mode="filelist-even-sources"/>
<xsl:template match="*" mode="filelist-odd-sources"/>
<xsl:template match="*" mode="filelist-headers"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  interface match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface" mode="filelist-even-sources">
    <xsl:if test="not(@internal='yes') and not(@supportsErrorInfo='no') and (position() mod 2) = 0">
        <xsl:value-of select="concat(' \', $G_sNewLine, '&#9;$(VBOX_MAIN_APIWRAPPER_DIR)/', substring(@name, 2), 'Wrap.cpp')"/>
    </xsl:if>
</xsl:template>

<xsl:template match="interface" mode="filelist-odd-sources">
    <xsl:if test="not(@internal='yes') and not(@supportsErrorInfo='no') and (position() mod 2) = 1">
        <xsl:value-of select="concat(' \', $G_sNewLine, '&#9;$(VBOX_MAIN_APIWRAPPER_DIR)/', substring(@name, 2), 'Wrap.cpp')"/>
    </xsl:if>
</xsl:template>

<xsl:template match="interface" mode="filelist-headers">
    <xsl:if test="not(@internal='yes') and not(@supportsErrorInfo='no')">
        <xsl:value-of select="concat(' \', $G_sNewLine, '&#9;$(VBOX_MAIN_APIWRAPPER_DIR)/', substring(@name, 2), 'Wrap.h')"/>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  ignore all if tags except those for XPIDL or MIDL target
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="if" mode="filelist-even-sources">
    <xsl:if test="(@target = 'xpidl') or (@target = 'midl')">
        <xsl:apply-templates mode="filelist-even-sources"/>
    </xsl:if>
</xsl:template>

<xsl:template match="if" mode="filelist-odd-sources">
    <xsl:if test="(@target = 'xpidl') or (@target = 'midl')">
        <xsl:apply-templates mode="filelist-odd-sources"/>
    </xsl:if>
</xsl:template>

<xsl:template match="if" mode="filelist-headers">
    <xsl:if test="(@target = 'xpidl') or (@target = 'midl')">
        <xsl:apply-templates mode="filelist-headers"/>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  library match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="library" mode="filelist-even-sources">
    <xsl:apply-templates mode="filelist-even-sources"/>
</xsl:template>

<xsl:template match="library" mode="filelist-odd-sources">
    <xsl:apply-templates mode="filelist-odd-sources"/>
</xsl:template>

<xsl:template match="library" mode="filelist-headers">
    <xsl:apply-templates mode="filelist-headers"/>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  root match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="/idl">
    <xsl:text>VBOX_MAIN_APIWRAPPER_GEN_SRCS_EVEN := </xsl:text>
    <xsl:apply-templates mode="filelist-even-sources"/>
    <xsl:value-of select="concat($G_sNewLine, $G_sNewLine)"/>

    <xsl:text>VBOX_MAIN_APIWRAPPER_GEN_SRCS_ODD := </xsl:text>
    <xsl:apply-templates mode="filelist-odd-sources"/>
    <xsl:value-of select="concat($G_sNewLine, $G_sNewLine)"/>

    <xsl:text>VBOX_MAIN_APIWRAPPER_GEN_SRCS := $(VBOX_MAIN_APIWRAPPER_GEN_SRCS_EVEN) $(VBOX_MAIN_APIWRAPPER_GEN_SRCS_ODD)</xsl:text>
    <xsl:value-of select="concat($G_sNewLine, $G_sNewLine)"/>

    <xsl:text>VBOX_MAIN_APIWRAPPER_GEN_HDRS := </xsl:text>
    <xsl:apply-templates mode="filelist-headers"/>
    <xsl:value-of select="concat($G_sNewLine, $G_sNewLine)"/>
</xsl:template>

</xsl:stylesheet>
<!-- vi: set tabstop=4 shiftwidth=4 expandtab: -->

