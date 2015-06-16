<?xml version="1.0"?>
<!--
    docbook-refentry-to-manual-sect1.xsl:
        XSLT stylesheet for transforming a refentry (manpage)
        to a sect1 for the user manual.

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

  <xsl:import href="string.xsl"/>

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->



<!-- - - - - - - - - - - - - - - - - - - - - - -
  base operation is to copy.
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="node()|@*">
  <xsl:copy>
     <xsl:apply-templates select="node()|@*"/>
  </xsl:copy>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -

 - - - - - - - - - - - - - - - - - - - - - - -->

<!-- rename refentry to sect1 -->
<xsl:template match="refentry">
  <sect1>
    <xsl:apply-templates select="node()|@*"/>
  </sect1>
</xsl:template>

<!-- TODO: refsect1 -> sect2 or something... -->

<!-- Remove refentryinfo, keeping the title element. -->
<xsl:template match="refentryinfo">
  <xsl:if test="./title">
    <xsl:copy-of select="./title"/>
  </xsl:if>
</xsl:template>

<!-- Morph refnamediv into a brief description. -->
<xsl:template match="refnamediv">
  <para>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="text" select="normalize-space(./refpurpose)"/>
    </xsl:call-template>
    <xsl:text>.</xsl:text>
  </para>
</xsl:template>

<!-- Remove refmeta. -->
<xsl:template match="refmeta"/>

<!--
 remark extensions:
 -->
<!-- Default: remove all remarks. -->
<xsl:template match="remark"/>

<!-- help-manual - stuff that should only be included in the manual. -->
<xsl:template match="remark[@role = 'help-manual']">
  <xsl:apply-templates/>
</xsl:template>

<!-- help-copy-synopsis - used in refsect2 to copy synopsis from the refsynopsisdiv. -->
<xsl:template match="remark[@role = 'help-copy-synopsis']">
  <xsl:if test="not(parent::refsect2)">
    <xsl:message terminate="yes">The help-copy-synopsis remark is only applicable in refsect2.</xsl:message>
  </xsl:if>
  <xsl:variable name="sSrcId" select="concat('synopsis-',../@id)"/>
  <xsl:if test="not(/refentry/refsynopsisdiv/cmdsynopsis[@id = $sSrcId])">
    <xsl:message terminate="yes">Could not find any cmdsynopsis with id=<xsl:value-of select="$sSrcId"/> in refsynopsisdiv.</xsl:message>
  </xsl:if>
  <xsl:element name="cmdsynopsis">
    <xsl:copy-of select="/refentry/refsynopsisdiv/cmdsynopsis[@id = $sSrcId]/node()"/>
  </xsl:element>
</xsl:template>

<!--
 Captializes the given text.
 -->
<xsl:template name="capitalize">
  <xsl:param name="text"/>
  <xsl:call-template name="str:to-upper">
    <xsl:with-param name="text" select="substring($text,1,1)"/>
  </xsl:call-template>
  <xsl:value-of select="substring($text,2)"/>
</xsl:template>

</xsl:stylesheet>

