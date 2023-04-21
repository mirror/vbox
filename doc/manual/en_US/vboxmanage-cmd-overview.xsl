<?xml version="1.0"?>
<!--
    vboxmanage-cmd-overview.xsl.xsl:
        XSLT stylesheet for generating the VBoxManage commands overview
        DITA section - english US language edition.
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

  <xsl:import href="../vboxmanage-cmd-overview.xsl"/>

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="no"/>
  <xsl:preserve-space elements="*"/>
  <!-- xsl:strip-space elements="*"/ - never -->

  <xsl:template match="/">
    <xsl:call-template name="emit-file-header"/>

    <xsl:element name="topic">
      <xsl:attribute name="id">vboxmanage-cmd-overview</xsl:attribute>

      <xsl:element name="title">
        <xsl:text>Commands Overview</xsl:text>
      </xsl:element>

      <xsl:element name="body">
        <xsl:element name="p">
          <xsl:text>When running </xsl:text><xsl:element name="userinput"><xsl:text>VBoxManage</xsl:text></xsl:element>
          <xsl:text> without parameters or when supplying an invalid command line, the
          following command syntax list is shown. Note that the output will be slightly
          different depending on the host platform. If in doubt, check the output of </xsl:text>
          <xsl:element name="userinput"><xsl:text>VBoxManage</xsl:text></xsl:element>
          <xsl:text> for the commands available on your particular host.</xsl:text>
        </xsl:element>

        <xsl:call-template name="emit-vboxmanage-overview"/>

        <xsl:element name="p">
          <xsl:text>Each time </xsl:text><xsl:element name="userinput"><xsl:text>VBoxManage</xsl:text></xsl:element>
          <xsl:text> is invoked, only one command can be executed. However, a command
          might support several subcommands which then can be invoked in one single
          call. The following sections provide detailed reference information on then
          different commands.</xsl:text>
        </xsl:element>

      </xsl:element>
    </xsl:element>
  </xsl:template>
</xsl:stylesheet>

