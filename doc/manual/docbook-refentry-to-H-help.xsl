<?xml version="1.0"?>
<!--
    docbook-refentry-to-H-help.xsl:
        XSLT stylesheet for generating command and sub-command
        constants header for the built-in help.

    Copyright (C) 2006-2015 Oracle Corporation

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
  xmlns:str="http://xsltsl.org/string"
  >

  <xsl:import href="@VBOX_PATH_MANUAL_SRC@/string.xsl"/>

  <xsl:output method="text" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <xsl:param name="g_sMode" select="not-specified"/>


  <!-- Default action, do nothing. -->
  <xsl:template match="node()|@*"/>

  <!--
    Generate SCOPE enum for a refentry.  We convert the
    cmdsynopsisdiv/cmdsynopsis IDs into constants.
    -->
  <xsl:template match="refentry">
    <xsl:variable name="sBaseNm">
      <xsl:call-template name="str:to-upper">
        <xsl:with-param name="text" select="translate(substring-after(@id, '-'), '-', '_')"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:choose>
      <!-- Generate subcommand enums and defines -->
      <xsl:when test="$g_sMode = 'subcmd'">
        <xsl:text>
enum
{
#define HELP_SCOPE_</xsl:text>
        <xsl:value-of select="$sBaseNm"/>
        <xsl:value-of select="substring('                                               ',1,56 - string-length($sBaseNm) - 11)"/>
        <xsl:text> RT_BIT_32(HELP_SCOPE_</xsl:text><xsl:value-of select="$sBaseNm"/><xsl:text>_BIT)
        HELP_SCOPE_</xsl:text><xsl:value-of select="$sBaseNm"/><xsl:text>_BIT = 0</xsl:text>
        <xsl:for-each select="./refsynopsisdiv/cmdsynopsis">
          <xsl:variable name="sSubNm">
            <xsl:text>HELP_SCOPE_</xsl:text>
            <xsl:call-template name="str:to-upper">
              <xsl:with-param name="text" select="translate(substring-after(substring-after(@id, '-'), '-'), '-', '_')"/>
            </xsl:call-template>
          </xsl:variable>
          <xsl:text>,
#define </xsl:text>
        <xsl:value-of select="$sSubNm"/>
        <xsl:value-of select="substring('                                               ',1,56 - string-length($sSubNm))"/>
        <xsl:text> RT_BIT_32(</xsl:text><xsl:value-of select="$sSubNm"/><xsl:text>_BIT)
        </xsl:text><xsl:value-of select="$sSubNm"/><xsl:text>_BIT</xsl:text>
        </xsl:for-each>

        <xsl:text>,
    HELP_SCOPE_</xsl:text><xsl:value-of select="$sBaseNm"/><xsl:text>_END
};
</xsl:text>
      </xsl:when>

      <!-- Generate command enum value. -->
      <xsl:when test="$g_sMode = 'cmd'">
        <xsl:text>    HELP_CMD_</xsl:text><xsl:value-of select="$sBaseNm"/><xsl:text>,
</xsl:text>
      </xsl:when>

      <!-- Shouldn't happen. -->
      <xsl:otherwise>
        <xsl:message terminate="yes">Bad or missing g_sMode string parameter value.</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>

