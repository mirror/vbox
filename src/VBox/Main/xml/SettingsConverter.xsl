<?xml version="1.0"?>

<!--
 *  :tabSize=2:indentSize=2:noTabs=true:
 *  :folding=explicit:collapseFolds=1:
 *
 *  Template to convert old VirtualBox settings files to the most recent format.

     Copyright (C) 2006-2008 innotek GmbH

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:vb="http://www.innotek.de/VirtualBox-settings"
  xmlns="http://www.innotek.de/VirtualBox-settings"
  exclude-result-prefixes="#default vb xsl xsd"
>

<xsl:output method = "xml" indent = "yes"/>

<xsl:variable name="recentVer" select="1.2"/>

<xsl:variable name="curVer" select="substring-before(/vb:VirtualBox/@version, '-')"/>
<xsl:variable name="curVerPlat" select="substring-after(/vb:VirtualBox/@version, '-')"/>
<xsl:variable name="curVerFull" select="/vb:VirtualBox/@version"/>

<xsl:template match="/">
  <xsl:comment> Automatically converted from version '<xsl:value-of select="$curVerFull"/>' to version '<xsl:value-of select="concat($recentVer,'-',$curVerPlat)"/>' </xsl:comment>
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>

<!--
 *  comments outside the root node are gathered to a single line, fix this
-->
<xsl:template match="/comment()">
  <xsl:copy-of select="."/>
</xsl:template>

<!--
 *  Forbid non-VirtualBox root nodes
-->

<xsl:template match="/*">
  <xsl:message terminate="yes">
Cannot convert an unknown XML file with the root node '<xsl:value-of select="name()"/>'!
  </xsl:message>
</xsl:template>

<!--
 *  Forbid all unsupported VirtualBox settings versions
-->

<xsl:template match="/vb:VirtualBox">
  <xsl:if test="@version=concat($recentVer,'-',$curVerPlat)">
    <xsl:message terminate="yes">
Cannot convert settings from version '<xsl:value-of select="@version"/>' to version '<xsl:value-of select="concat($recentVer,'-',$curVerPlat)"/>'.
The source is already at the most recent version.
    </xsl:message>
  </xsl:if>
  <xsl:message terminate="yes">
Cannot convert settings from version '<xsl:value-of select="@version"/>' to version '<xsl:value-of select="concat($recentVer,'-',$curVerPlat)"/>'.
The source version is not supported.
  </xsl:message>
</xsl:template>

<!--
 *  Accept supported settings versions (source setting filess to convert)
-->

<!-- @todo temporary -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.999']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat($recentVer,'-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.1']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat($recentVer,'-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  Individual conversions
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<xsl:template match="VirtualBox[substring-before(@version,'-')='1.1']/
                     Global/DiskImageRegistry/HardDiskImages//
                     Image">
  <DiffHardDisk>
    <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
    <VirtualDiskImage>
      <xsl:attribute name="filePath"><xsl:value-of select="@src"/></xsl:attribute>
    </VirtualDiskImage>
    <xsl:apply-templates select="Image"/>
  </DiffHardDisk>
</xsl:template>

<xsl:template match="VirtualBox[substring-before(@version,'-')='1.1']/
                     Global/DiskImageRegistry">
<DiskRegistry>
  <HardDisks>
    <xsl:for-each select="HardDiskImages/Image">
      <HardDisk>
        <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
        <xsl:attribute name="type">
          <xsl:choose>
            <xsl:when test="@independent='immutable'">immutable</xsl:when>
            <xsl:when test="@independent='mutable'">immutable</xsl:when>
            <xsl:otherwise>normal</xsl:otherwise>
          </xsl:choose>
        </xsl:attribute>
        <VirtualDiskImage>
          <xsl:attribute name="filePath"><xsl:value-of select="@src"/></xsl:attribute>
        </VirtualDiskImage>
        <xsl:apply-templates select="Image"/>
      </HardDisk>
    </xsl:for-each>
  </HardDisks>
  <xsl:copy-of select="DVDImages"/>
  <xsl:copy-of select="FloppyImages"/>
</DiskRegistry>
</xsl:template>

<!--
 *  Machine settings
-->

<xsl:template match="VirtualBox[substring-before(@version,'-')='1.1']/
                     Machine//HardDisks">
  <HardDiskAttachments>
    <xsl:for-each select="HardDisk">
      <HardDiskAttachment>
        <xsl:attribute name="hardDisk"><xsl:value-of select="Image/@uuid"/></xsl:attribute>
        <xsl:apply-templates select="@*"/>
      </HardDiskAttachment>
    </xsl:for-each>
  </HardDiskAttachments>
</xsl:template>

</xsl:stylesheet>
