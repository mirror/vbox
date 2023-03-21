<?xml version="1.0"?>
<!--
    docbook-refentry-to-manual-dita-pre.xsl:
        XSLT stylesheet for preprocessing a refentry (manpage)
        before converting it to dita for use in the user manual.

    This just applies remark elements.
-->
<!--
    Copyright (C) 2006-2023 Oracle and/or its affiliates.

    This file is part of VirtualBox base platform packages, as
    available from https://www.virtualbox.org.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, in version 3 of the
    License.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <https://www.gnu.org/licenses>.

    SPDX-License-Identifier: GPL-3.0-only
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
  base operation is to copy.
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="node()|@*">
  <xsl:copy>
     <xsl:apply-templates select="node()|@*"/>
  </xsl:copy>
</xsl:template>


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


</xsl:stylesheet>

