<?xml version="1.0"?>
<!--
    vboxmanage-cmd-overview.xsl.xsl:
        XSLT stylesheet for generating the VBoxManage commands overview
        DITA section - common bits.
-->
<!--
    Copyright (C) 2023 Oracle and/or its affiliates.

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
  >

  <!-- Template that is called from the language specific XSLT file. -->
  <xsl:template name="emit-file-header">
    <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE topic PUBLIC "-//OASIS//DTD DITA Topic//EN" "topic.dtd"&gt;
</xsl:text>
  </xsl:template>


  <!-- Template that is called from the language specific XSLT file. -->
  <xsl:template name="emit-vboxmanage-overview">
    <xsl:for-each select="/files/file">
      <xsl:apply-templates select="document(.)/topic/body" mode="subdoc"/>
      <xsl:element name="p">
        <xsl:attribute name="rev">spacer</xsl:attribute>
      </xsl:element>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="node()|@*">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="node()" mode="copy">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()" mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="@*" mode="copy">
    <!-- xsl:message>dbg: @*: name()='<xsl:value-of select="name()"/></xsl:message -->
    <xsl:if test="name() != 'class' and name() != 'ditaarch:DITAArchVersion' and name() != 'domains' ">
      <xsl:copy/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/topic[@rev='refsynopsisdiv']/body" mode="subdoc">
    <xsl:apply-templates select="*" mode="copy"/>
  </xsl:template>

</xsl:stylesheet>
