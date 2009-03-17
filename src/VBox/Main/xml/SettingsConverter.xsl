<?xml version="1.0"?>

<!--
 *  :tabSize=2:indentSize=2:noTabs=true:
 *  :folding=explicit:collapseFolds=1:
 *
 *  Template to convert old VirtualBox settings files to the most recent format.

     Copyright (C) 2006-2008 Sun Microsystems, Inc.

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

     Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
     Clara, CA 95054 USA or visit http://www.sun.com if you need
     additional information or have any questions.
-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:vb="http://www.innotek.de/VirtualBox-settings"
  xmlns="http://www.innotek.de/VirtualBox-settings"
  exclude-result-prefixes="#default vb xsl xsd"
>

<xsl:output method="xml" indent="yes"/>

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

<!-- 1.3.pre => 1.3 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.3.pre']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.3','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.3"/>
  </xsl:copy>
</xsl:template>

<!-- 1.3 => 1.4 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.3']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.4','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.4"/>
  </xsl:copy>
</xsl:template>

<!-- 1.4 => 1.5 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.4']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.5','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.5"/>
  </xsl:copy>
</xsl:template>

<!-- 1.5 => 1.6 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.5']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.6','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.6"/>
  </xsl:copy>
</xsl:template>

<!-- 1.6 => 1.7 -->
<xsl:template match="/vb:VirtualBox[substring-before(@version,'-')='1.6']">
  <xsl:copy>
    <xsl:attribute name="version"><xsl:value-of select="concat('1.7','-',$curVerPlat)"/></xsl:attribute>
    <xsl:apply-templates select="node()" mode="v1.7"/>
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

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.3.pre => 1.3
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.3"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Global//vb:SystemProperties"
              mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="@*[not(name()='defaultSavedStateFolder')]|node()" mode="v1.3"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Machine settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:AudioAdapter"
              mode="v1.3">
  <xsl:if test="not(../vb:Uart)">
    <UART/>
  </xsl:if>
  <xsl:if test="not(../vb:Lpt)">
    <LPT/>
  </xsl:if>
  <xsl:copy>
    <xsl:apply-templates select="@*[not(name()='driver')]|node()" mode="v1.3"/>
    <xsl:attribute name="driver">
      <xsl:choose>
        <xsl:when test="@driver='null'">Null</xsl:when>
        <xsl:when test="@driver='oss'">OSS</xsl:when>
        <xsl:when test="@driver='alsa'">ALSA</xsl:when>
        <xsl:when test="@driver='pulse'">Pulse</xsl:when>
        <xsl:when test="@driver='coreaudio'">CoreAudio</xsl:when>
        <xsl:when test="@driver='winmm'">WinMM</xsl:when>
        <xsl:when test="@driver='dsound'">DirectSound</xsl:when>
        <xsl:when test="@driver='solaudio'">SolAudio</xsl:when>
        <xsl:when test="@driver='mmpm'">MMPM</xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@driver"/>' of 'AudioAdapter::driver' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
  </xsl:copy>
  <xsl:if test="not(../vb:SharedFolders)">
    <SharedFolders/>
  </xsl:if>
  <xsl:if test="not(../vb:Clipboard)">
    <Clipboard mode="Disabled"/>
  </xsl:if>
  <xsl:if test="not(../vb:Guest)">
    <Guest/>
  </xsl:if>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:RemoteDisplay"
              mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="@*[not(name()='authType')]|node()" mode="v1.3"/>
    <xsl:attribute name="authType">
      <xsl:choose>
        <xsl:when test="@authType='null'">Null</xsl:when>
        <xsl:when test="@authType='guest'">Guest</xsl:when>
        <xsl:when test="@authType='external'">External</xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@authType"/>' of 'RemoteDisplay::authType' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:BIOS/vb:BootMenu"
              mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="@*[not(name()='mode')]|node()" mode="v1.3"/>
    <xsl:attribute name="mode">
      <xsl:choose>
        <xsl:when test="@mode='disabled'">Disabled</xsl:when>
        <xsl:when test="@mode='menuonly'">MenuOnly</xsl:when>
        <xsl:when test="@mode='messageandmenu'">MessageAndMenu</xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@mode"/>' of 'BootMenu::mode' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:USBController/vb:DeviceFilter |
                     vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Global/vb:USBDeviceFilters/vb:DeviceFilter"
              mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="node()" mode="v1.3"/>
    <xsl:for-each select="@*">
      <xsl:choose>
        <xsl:when test="name()='vendorid'">
          <xsl:attribute name="vendorId"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:when test="name()='productid'">
          <xsl:attribute name="productId"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:when test="name()='serialnumber'">
          <xsl:attribute name="serialNumber"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="." mode="v1.3"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:Guest"
              mode="v1.3">
  <xsl:copy>
    <xsl:apply-templates select="node()" mode="v1.3"/>
    <xsl:for-each select="@*">
      <xsl:choose>
        <xsl:when test="name()='MemoryBalloonSize'">
          <xsl:attribute name="memoryBalloonSize"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:when test="name()='StatisticsUpdateInterval'">
          <xsl:attribute name="statisticsUpdateInterval"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="node()" mode="v1.3"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:Uart"
              mode="v1.3">
  <UART>
    <xsl:apply-templates select="@*|node()" mode="v1.3"/>
  </UART>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3.pre']/
                     vb:Machine//vb:Lpt"
              mode="v1.3">
  <LPT>
    <xsl:apply-templates select="@*|node()" mode="v1.3"/>
  </LPT>
</xsl:template>

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.3 => 1.4
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.4">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.4"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:HardDisks/vb:HardDisk |
                     vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:HardDisks//vb:DiffHardDisk"
              mode="v1.4-HardDisk-format-location">
  <xsl:attribute name="format">
    <xsl:choose>
      <xsl:when test="*[self::vb:VirtualDiskImage][1]">VDI</xsl:when>
      <xsl:when test="*[self::vb:VMDKImage][1]">VMDK</xsl:when>
      <xsl:when test="*[self::vb:VHDImage][1]">VHD</xsl:when>
      <xsl:when test="*[self::vb:ISCSIHardDisk][1]">iSCSI</xsl:when>
      <xsl:when test="*[self::vb:CustomHardDisk][1]">
        <xsl:value-of select="@format"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes">
Sub-element '<xsl:value-of select="name(*[1])"/>' of 'HardDisk' element is invalid.
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:attribute>
  <xsl:attribute name="location">
    <xsl:choose>
      <xsl:when test="*[self::vb:VirtualDiskImage][1]">
        <xsl:value-of select="vb:VirtualDiskImage/@filePath"/>
      </xsl:when>
      <xsl:when test="*[self::vb:VMDKImage][1]">
        <xsl:value-of select="vb:VMDKImage/@filePath"/>
      </xsl:when>
      <xsl:when test="*[self::vb:VHDImage][1]">
        <xsl:value-of select="vb:VHDImage/@filePath"/>
      </xsl:when>
      <xsl:when test="*[self::vb:CustomHardDisk][1]">
        <xsl:value-of select="vb:CustomHardDisk/@location"/>
      </xsl:when>
      <xsl:when test="*[self::vb:ISCSIHardDisk][1]">
        <xsl:text>iscsi://</xsl:text>
        <xsl:if test="vb:ISCSIHardDisk/@userName">
          <xsl:value-of select="vb:ISCSIHardDisk/@userName"/>
          <!-- note that for privacy reasons we don't show the password in the
               location string -->
          <xsl:text>@</xsl:text>
        </xsl:if>
        <xsl:if test="vb:ISCSIHardDisk/@server">
          <xsl:value-of select="vb:ISCSIHardDisk/@server"/>
          <xsl:if test="vb:ISCSIHardDisk/@port">
            <xsl:value-of select="concat(':',vb:ISCSIHardDisk/@port)"/>
          </xsl:if>
        </xsl:if>
        <xsl:if test="vb:ISCSIHardDisk/@target">
          <xsl:value-of select="concat('/',vb:ISCSIHardDisk/@target)"/>
        </xsl:if>
        <xsl:if test="vb:ISCSIHardDisk/@lun">
          <xsl:value-of select="concat('/enc',vb:ISCSIHardDisk/@lun)"/>
        </xsl:if>
        <xsl:if test="not(vb:ISCSIHardDisk/@server) or not(vb:ISCSIHardDisk/@target)">
          <xsl:message terminate="yes">
Required attribute 'server' or 'target' is missing from ISCSIHardDisk element!
          </xsl:message>
        </xsl:if>
      </xsl:when>
    </xsl:choose>
  </xsl:attribute>
  <xsl:if test="*[self::vb:ISCSIHardDisk][1]">
    <xsl:choose>
      <xsl:when test="vb:ISCSIHardDisk/@server and vb:ISCSIHardDisk/@port">
        <xsl:element name="Property">
          <xsl:attribute name="name">TargetAddress</xsl:attribute>
          <xsl:attribute name="value">
            <xsl:value-of select="concat(vb:ISCSIHardDisk/@server,
                                         ':',vb:ISCSIHardDisk/@port)"/>
          </xsl:attribute>
        </xsl:element>
      </xsl:when>
      <xsl:when test="vb:ISCSIHardDisk/@server">
        <xsl:element name="Property">
          <xsl:attribute name="name">TargetAddress</xsl:attribute>
          <xsl:attribute name="value">
            <xsl:value-of select="vb:ISCSIHardDisk/@server"/>
          </xsl:attribute>
        </xsl:element>
      </xsl:when>
    </xsl:choose>
    <xsl:if test="vb:ISCSIHardDisk/@target">
      <xsl:element name="Property">
        <xsl:attribute name="name">TargetName</xsl:attribute>
        <xsl:attribute name="value">
          <xsl:value-of select="vb:ISCSIHardDisk/@target"/>
        </xsl:attribute>
      </xsl:element>
    </xsl:if>
    <xsl:if test="vb:ISCSIHardDisk/@userName">
      <xsl:element name="Property">
        <xsl:attribute name="name">InitiatorUsername</xsl:attribute>
        <xsl:attribute name="value">
          <xsl:value-of select="vb:ISCSIHardDisk/@userName"/>
        </xsl:attribute>
      </xsl:element>
    </xsl:if>
    <xsl:if test="vb:ISCSIHardDisk/@password">
      <xsl:element name="Property">
        <xsl:attribute name="name">InitiatorSecret</xsl:attribute>
        <xsl:attribute name="value">
          <xsl:value-of select="vb:ISCSIHardDisk/@password"/>
        </xsl:attribute>
      </xsl:element>
    </xsl:if>
    <xsl:if test="vb:ISCSIHardDisk/@lun">
      <xsl:element name="Property">
        <xsl:attribute name="name">LUN</xsl:attribute>
        <xsl:attribute name="value">
          <xsl:value-of select="concat('enc',vb:ISCSIHardDisk/@lun)"/>
        </xsl:attribute>
      </xsl:element>
    </xsl:if>
  </xsl:if>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:HardDisks/vb:HardDisk"
              mode="v1.4">
  <HardDisk>
    <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
    <xsl:attribute name="type">
      <xsl:choose>
        <xsl:when test="@type='normal'">Normal</xsl:when>
        <xsl:when test="@type='immutable'">Immutable</xsl:when>
        <xsl:when test="@type='writethrough'">Writethrough</xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
Value '<xsl:value-of select="@type"/>' of 'HardDisk::type' attribute is invalid.
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
    <xsl:apply-templates select="." mode="v1.4-HardDisk-format-location"/>
    <xsl:apply-templates select="vb:DiffHardDisk" mode="v1.4"/>
  </HardDisk>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:HardDisks/vb:HardDisk//
                     vb:DiffHardDisk"
              mode="v1.4">
  <HardDisk>
    <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
    <xsl:apply-templates select="." mode="v1.4-HardDisk-format-location"/>
    <xsl:apply-templates select="vb:DiffHardDisk" mode="v1.4"/>
  </HardDisk>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:DVDImages/vb:Image |
                     vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry/vb:FloppyImages/vb:Image"
              mode="v1.4">
  <Image>
    <xsl:attribute name="uuid"><xsl:value-of select="@uuid"/></xsl:attribute>
    <xsl:attribute name="location"><xsl:value-of select="@src"/></xsl:attribute>
  </Image>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:DiskRegistry"
              mode="v1.4">
  <MediaRegistry>
    <HardDisks>
      <xsl:apply-templates select="vb:HardDisks/vb:HardDisk" mode="v1.4"/>
    </HardDisks>
    <DVDImages>
      <xsl:apply-templates select="vb:DVDImages/vb:Image" mode="v1.4"/>
    </DVDImages>
    <FloppyImages>
      <xsl:apply-templates select="vb:FloppyImages/vb:Image" mode="v1.4"/>
    </FloppyImages>
  </MediaRegistry>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Global/vb:SystemProperties"
              mode="v1.4">
  <SystemProperties>
    <xsl:apply-templates select="@*[not(name()='defaultVDIFolder')]|node()" mode="v1.4"/>
    <!-- use the @defaultVDIFolder value for @defaultHardDiskFolder only when it
         differs from the default (VDI) and otherwise simply delete it to let
         VBoxSVC set the correct new default value -->
    <xsl:if test="@defaultVDIFolder and not(translate(@defaultVDIFolder,'vdi','VDI')='VDI')">
      <xsl:attribute name="defaultHardDiskFolder">
        <xsl:value-of select="@defaultVDIFolder"/>
      </xsl:attribute>
    </xsl:if>
  </SystemProperties>
</xsl:template>

<!--
 *  Machine settings
-->

  <xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.3']/
                     vb:Machine/vb:Hardware"
              mode="v1.4">
  <!-- add version attribute to Hardware if parent Machine has a stateFile attribute -->
  <xsl:copy>
    <xsl:if test="../@stateFile">
      <xsl:attribute name="version">1</xsl:attribute>
    </xsl:if>
    <xsl:apply-templates select="node()" mode="v1.4"/>
  </xsl:copy>
</xsl:template>


<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.4 => 1.5
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->

<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.5">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.5"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<!--
 *  Machine settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.4']/
                     vb:Machine"
              mode="v1.5">
  <xsl:copy>
    <xsl:attribute name="OSType">
      <xsl:choose>
        <xsl:when test="@OSType='unknown'">Other</xsl:when>
        <xsl:when test="@OSType='dos'">DOS</xsl:when>
        <xsl:when test="@OSType='win31'">Windows31</xsl:when>
        <xsl:when test="@OSType='win95'">Windows95</xsl:when>
        <xsl:when test="@OSType='win98'">Windows98</xsl:when>
        <xsl:when test="@OSType='winme'">WindowsMe</xsl:when>
        <xsl:when test="@OSType='winnt4'">WindowsNT4</xsl:when>
        <xsl:when test="@OSType='win2k'">Windows2000</xsl:when>
        <xsl:when test="@OSType='winxp'">WindowsXP</xsl:when>
        <xsl:when test="@OSType='win2k3'">Windows2003</xsl:when>
        <xsl:when test="@OSType='winvista'">WindowsVista</xsl:when>
        <xsl:when test="@OSType='win2k8'">Windows2008</xsl:when>
        <xsl:when test="@OSType='os2warp3'">OS2Warp3</xsl:when>
        <xsl:when test="@OSType='os2warp4'">OS2Warp4</xsl:when>
        <xsl:when test="@OSType='os2warp45'">OS2Warp45</xsl:when>
        <xsl:when test="@OSType='ecs'">OS2eCS</xsl:when>
        <xsl:when test="@OSType='linux22'">Linux22</xsl:when>
        <xsl:when test="@OSType='linux24'">Linux24</xsl:when>
        <xsl:when test="@OSType='linux26'">Linux26</xsl:when>
        <xsl:when test="@OSType='archlinux'">ArchLinux</xsl:when>
        <xsl:when test="@OSType='debian'">Debian</xsl:when>
        <xsl:when test="@OSType='opensuse'">OpenSUSE</xsl:when>
        <xsl:when test="@OSType='fedoracore'">Fedora</xsl:when>
        <xsl:when test="@OSType='gentoo'">Gentoo</xsl:when>
        <xsl:when test="@OSType='mandriva'">Mandriva</xsl:when>
        <xsl:when test="@OSType='redhat'">RedHat</xsl:when>
        <xsl:when test="@OSType='ubuntu'">Ubuntu</xsl:when>
        <xsl:when test="@OSType='xandros'">Xandros</xsl:when>
        <xsl:when test="@OSType='freebsd'">FreeBSD</xsl:when>
        <xsl:when test="@OSType='openbsd'">OpenBSD</xsl:when>
        <xsl:when test="@OSType='netbsd'">NetBSD</xsl:when>
        <xsl:when test="@OSType='netware'">Netware</xsl:when>
        <xsl:when test="@OSType='solaris'">Solaris</xsl:when>
        <xsl:when test="@OSType='opensolaris'">OpenSolaris</xsl:when>
        <xsl:when test="@OSType='l4'">L4</xsl:when>
      </xsl:choose>
    </xsl:attribute>
    <xsl:apply-templates select="@*[name()!='OSType']" mode="v1.5"/>
    <xsl:apply-templates select="node()" mode="v1.5"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.4']/
                     vb:Machine//vb:Hardware/vb:Display"
              mode="v1.5">
  <xsl:copy>
    <xsl:apply-templates select="node()" mode="v1.5"/>
    <xsl:for-each select="@*">
      <xsl:choose>
        <xsl:when test="name()='MonitorCount'">
          <xsl:attribute name="monitorCount"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:when test="name()='Accelerate3D'">
          <xsl:attribute name="accelerate3D"><xsl:value-of select="."/></xsl:attribute>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="." mode="v1.5"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:copy>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.4']/
                     vb:Machine//vb:Hardware/vb:CPU"
              mode="v1.5">
  <xsl:copy>
    <xsl:if test="vb:CPUCount/@count">
      <xsl:attribute name="count"><xsl:value-of select="vb:CPUCount/@count"/></xsl:attribute>
    </xsl:if>
    <xsl:apply-templates select="@*" mode="v1.5"/>
    <xsl:apply-templates select="node()[not(self::vb:CPUCount)]" mode="v1.5"/>
  </xsl:copy>
</xsl:template>


<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.5 => 1.6
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->


<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.6">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.6"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->

<!--
 *  Machine settings
-->

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.5' and
                                   not(substring-after(@version,'-')='windows')]/
                     vb:Machine//vb:Hardware/vb:Network/vb:Adapter/
                     vb:HostInterface[@TAPSetup or @TAPTerminate]"
              mode="v1.6">
  <!-- just remove the node -->
</xsl:template>


<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *  1.6 => 1.7
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->
<!--
 *  all non-root elements that are not explicitly matched are copied as is
-->
<xsl:template match="@*|node()[../..]" mode="v1.7">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()[../..]" mode="v1.7"/>
  </xsl:copy>
</xsl:template>

<!--
 *  Global settings
-->
<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Global"
              mode="v1.7" >
    <xsl:copy>
        <xsl:apply-templates mode="v1.7" />
        <NetserviceRegistry>
           <DhcpServers>
              <xsl:choose>
                <xsl:when test="substring-after(../@version,'-')='windows'">
                   <DhcpServer networkName="HostInterfaceNetworking-VirtualBox Host-Only Network Adapter" 
                        IPAddress="192.168.56.100" networkMask="255.255.255.0"
                        lowerIP="192.168.56.101" upperIP="192.168.56.254"
                        enabled="1"/>
                </xsl:when>
                <xsl:otherwise>
                   <DhcpServer networkName="HostInterfaceNetworking-vboxnet0" 
                        IPAddress="192.168.56.2" networkMask="255.255.255.0"
                        lowerIP="192.168.56.3" upperIP="192.168.56.255"
                        enabled="1"/>
                </xsl:otherwise>
              </xsl:choose>
           </DhcpServers>
        </NetserviceRegistry>              
    </xsl:copy>
</xsl:template>

<!--
 *  Machine settings
-->
<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshots"
              mode="v1.7">
  <xsl:for-each select="vb:Snapshot">
    <xsl:apply-templates select="." mode="v1.7"/>
  </xsl:for-each>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:HardDiskAttachments |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:HardDiskAttachments |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:Snapshots//vb:Snapshot/vb:HardDiskAttachments"
              mode="v1.7">
  <StorageControllers>
    <StorageController name="IDE">
      <xsl:choose>
        <xsl:when test="not(../vb:Hardware/vb:BIOS/vb:IDEController)">
          <xsl:attribute name="type">PIIX3</xsl:attribute>
        </xsl:when>
        <xsl:otherwise>
          <xsl:attribute name="type"><xsl:value-of select="../vb:Hardware/vb:BIOS/vb:IDEController/@type"/></xsl:attribute>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:attribute name="PortCount">2</xsl:attribute>
      <xsl:for-each select="./vb:HardDiskAttachment[@bus = 'IDE']">
         <xsl:apply-templates select="." mode="v1.7-attached-device"/>
      </xsl:for-each>
    </StorageController>
    <xsl:if test="../vb:Hardware/vb:SATAController/@enabled='true'">
      <StorageController name="SATA">
        <xsl:attribute name="type">AHCI</xsl:attribute>
        <xsl:attribute name="PortCount">
          <xsl:value-of select="../vb:Hardware/vb:SATAController/@PortCount"/>
        </xsl:attribute>
        <xsl:attribute name="IDE0MasterEmulationPort">
          <xsl:value-of select="../vb:Hardware/vb:SATAController/@IDE0MasterEmulationPort"/>
        </xsl:attribute>
        <xsl:attribute name="IDE0SlaveEmulationPort">
          <xsl:value-of select="../vb:Hardware/vb:SATAController/@IDE0SlaveEmulationPort"/>
        </xsl:attribute>
        <xsl:attribute name="IDE1MasterEmulationPort">
          <xsl:value-of select="../vb:Hardware/vb:SATAController/@IDE1MasterEmulationPort"/>
        </xsl:attribute>
        <xsl:attribute name="IDE1SlaveEmulationPort">
          <xsl:value-of select="../vb:Hardware/vb:SATAController/@IDE1SlaveEmulationPort"/>
        </xsl:attribute>
        <xsl:for-each select="./vb:HardDiskAttachment[@bus = 'SATA']">
           <xsl:apply-templates select="." mode="v1.7-attached-device"/>
        </xsl:for-each>
      </StorageController>
    </xsl:if>
  </StorageControllers>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:HardDiskAttachments/vb:HardDiskAttachment |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:HardDiskAttachments/vb:HardDiskAttachment |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:Snapshots//vb:Snapshot/vb:HardDiskAttachments/
                     vb:HardDiskAttachment"
              mode="v1.7-attached-device">
  <AttachedDevice>
    <xsl:attribute name="type">HardDisk</xsl:attribute>
    <xsl:attribute name="port"><xsl:value-of select="@channel"/></xsl:attribute>
    <xsl:attribute name="device"><xsl:value-of select="@device"/></xsl:attribute>
    <xsl:element name="Image">
      <xsl:attribute name="uuid"><xsl:value-of select="@hardDisk"/></xsl:attribute>
    </xsl:element>
  </AttachedDevice>
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine//vb:Hardware/vb:BIOS/vb:IDEController |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine//vb:Snapshot/vb:Hardware/vb:BIOS/vb:IDEController |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:Snapshots//vb:Snapshot/vb:Hardware/vb:BIOS/vb:IDEController"
              mode="v1.7">
  <!-- just remove the node -->
</xsl:template>

<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Hardware/vb:SATAController |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:Hardware/vb:SATAController |
                     vb:VirtualBox[substring-before(@version,'-')='1.6']/
                     vb:Machine/vb:Snapshot/vb:Snapshots//vb:Snapshot/vb:Hardware/vb:SATAController"
              mode="v1.7">
  <!-- just remove the node -->
</xsl:template>

<!--
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-->


<!-- @todo add lastStateChange with the current timestamp if missing.
  *  current-dateTime() is available only in XPath 2.0 so we will need to pass
  *  the current time as a parameter to the XSLT processor. -->
<!--
<xsl:template match="vb:VirtualBox[substring-before(@version,'-')='Xo.Yo']/
                     vb:Machine"
              mode="X.Y">
  <xsl:copy>
    <xsl:if test="not(@lastStateChange)">
      <xsl:attribute name="lastStateChange">
        <xsl:value-of select="current-dateTime()"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:apply-templates select="@*|node()" mode="vX.Y"/>
  </xsl:copy>
</xsl:template>
-->

</xsl:stylesheet>
