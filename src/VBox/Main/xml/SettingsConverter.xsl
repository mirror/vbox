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

<xsl:variable name="curVer" select="substring-before(/vb:VirtualBox/@version, '-')"/>
<xsl:variable name="curVerPlat" select="substring-after(/vb:VirtualBox/@version, '-')"/>
<xsl:variable name="curVerFull" select="/vb:VirtualBox/@version"/>

<xsl:template match="/">
  <xsl:comment> Automatically converted from version '<xsl:value-of select="$curVerFull"/>' </xsl:comment>
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
  <xsl:message terminate="yes">
Cannot convert settings from version '<xsl:value-of select="@version"/>'.
The source version is not supported.
  </xsl:message>
</xsl:template>

<!--
 * Accept supported settings versions (source setting files we can convert)
 *
 * Note that in order to simplify conversion from versions prior to the previous
 * one, we support multi-step conversion like this: step 1: 1.0 => 1.1,
 * step 2: 1.1 => 1.2, where 1.2 is the most recent version. If you want to use
 * such multi-step mode, you need to ensure that only 1.0 => 1.1 is possible, by
 * using the 'mode=1.1' attribute on both 'apply-templates' within the starting
 * '/vb:VirtualBox[1.0]' template and within all templates that this
 * 'apply-templates' should apply.
 *
 * If no 'mode' attribute is used as described above, then a direct conversion
 * (1.0 => 1.2 in the above example) will happen when version 1.0 of the settings
 * files is detected. Note that the direct conversion from pre-previous versions
 * will require to patch their conversion templates so that they include all
 * modifications from all newer versions, which is error-prone. It's better to
 * use the milt-step mode.
-->

<!-- 1.1 => 1.2 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.1']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.2','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.2"/>
  </xsl:copy>
</xsl:template>

<!-- 1.2 => 1.3.pre -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.2']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.3.pre','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.3.pre"/>
  </xsl:copy>
</xsl:template>

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.1 => 1.2
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.2">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.2"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.1']/
                     vb:Global/vb:DiskImageRegistry/vb:HardDiskImages//
                     vb:Image"
              mode="v1.2">
  <DiffHardDisk>
    <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
    <VirtualDiskImage>
      <xsl:attribute name="filePath"><xsl:value-of select="@src"/></xsl:attribute>
    </VirtualDiskImage>
    <xsl:apply-templates select="vb:Image"/>
  </DiffHardDisk>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.1']/
                     vb:Global/vb:DiskImageRegistry"
              mode="v1.2">
<DiskRegistry>
  <HardDisks>
    <xsl:for-each select="vb:HardDiskImages/vb:Image">
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
        <xsl:apply-templates select="vb:Image"/>
      </HardDisk>
    </xsl:for-each>
  </HardDisks>
  <xsl:copy-of select="vb:DVDImages"/>
  <xsl:copy-of select="vb:FloppyImages"/>
</DiskRegistry>
</xsl:template>

<!--
 *  Machine settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.1']/
                     vb:Machine//vb:HardDisks"
              mode="v1.2">
  <HardDiskAttachments>
    <xsl:for-each select="vb:HardDisk">
      <HardDiskAttachment>
        <xsl:attribute name="hardDisk"><xsl:value-of select="vb:Image/@uuid"/></xsl:attribute>
        <xsl:apply-templates select="@*"/>
      </HardDiskAttachment>
    </xsl:for-each>
  </HardDiskAttachments>
</xsl:template>

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.2 => 1.3.pre
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.3.pre">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.3.pre"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings 
-->

<!--
 *  Machine settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.2']/
                    vb:Machine//vb:USBController"
              mode="v1.3.pre">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()" mode="v1.3.pre"/>
  </xsl:copy>
  <SATAController enabled="false"/>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.2']/
                     vb:Machine//vb:HardDiskAttachments/vb:HardDiskAttachment"
              mode="v1.3.pre">
  <HardDiskAttachment>
    <xsl:attribute name="hardDisk"><xsl:value-of select="@hardDisk"/></xsl:attribute>
    <xsl:attribute name="bus">
      <xsl:choose>
        <xsl:when test="@bus='ide0'">
          <xsl:text>IDE</xsl:text>
        </xsl:when>
        <xsl:when test="@bus='ide1'">
          <xsl:text>IDE</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@bus"/>' of 'HardDiskAttachment::bus' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
    <xsl:attribute name="channel">0</xsl:attribute>
    <xsl:attribute name="device">
      <xsl:choose>
        <xsl:when test="@device='master'">
          <xsl:text>0</xsl:text>
        </xsl:when>
        <xsl:when test="@device='slave'">
          <xsl:text>1</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@device"/>' of 'HardDiskAttachment::device' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
  </HardDiskAttachment>
</xsl:template>

</xsl:stylesheet>
