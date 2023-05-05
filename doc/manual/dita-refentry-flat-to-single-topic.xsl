<?xml version="1.0"?>
<!--
    dita-refentry-flat-to-single-topic.xsl:
        XSLT stylesheet for help converting a flat DITA manual page with nested
        topic elements into individual topics and a map file for including them
         in the right order.
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

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="no"/>
  <xsl:preserve-space elements="*"/>
  <!-- xsl:strip-space elements="*"/ - never -->

  <!-- Script parameters: -->
  <xsl:param name="g_sMode" select="not-specified"/>
  <xsl:param name="g_idTopic" select="not-specified"/>

  <!--
  Process root element according to g_sMode.
  -->
  <xsl:template match="/">
    <xsl:choose>
      <!-- map file -->
      <xsl:when test="$g_sMode = 'map'">
        <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE map PUBLIC "-//OASIS//DTD DITA Map//EN" "map.dtd"&gt;
  </xsl:text>
        <xsl:element name="map" >
          <xsl:element name="title">
            <xsl:value-of select="/topic/title"/>
          </xsl:element>
          <xsl:apply-templates mode="map"/>
        </xsl:element>
      </xsl:when>

      <!-- topic extraction -->
      <xsl:when test="$g_sMode = 'topic'">
        <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE topic PUBLIC "-//OASIS//DTD DITA Topic//EN" "topic.dtd"&gt;
</xsl:text>
        <xsl:apply-templates mode="topic" select="//topic[@id = $g_idTopic]"/>
      </xsl:when>


      <!-- Bad mode parameter: -->
      <xsl:otherwise>
        <xsl:message terminate="yes">Invalid g_sMode value!"</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!--
    map: Default operation is to supress all output, except for topic elements
         which are transformed to topicref.
  -->
  <xsl:template match="node()|@*" mode="map">
    <xsl:apply-templates mode="map"/>
  </xsl:template>

  <xsl:template match="topic" mode="map">
    <xsl:element name="topicref">
      <xsl:attribute name="href">
        <xsl:value-of select="concat(@id,'.dita')"/>
      </xsl:attribute>
      <xsl:if test="count(./ancestor::*) != 0">
        <xsl:attribute name="toc">no</xsl:attribute>
      </xsl:if>
      <xsl:apply-templates mode="map"/>
    </xsl:element>
  </xsl:template>

  <!--
    topic: Default action is to copy everything except non-matching topic elements
           We suppress class, domains and xmlns attributes in a hackish way here,
           because the xmlns:ditaarch stuff confuses 1.8.5 (making it hang)...
  -->
  <xsl:template match="node()" mode="topic">
    <xsl:copy>
      <xsl:apply-templates mode="topic" select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="@*" mode="topic">
    <!-- xsl:message>dbg: @*: name()='<xsl:value-of select="name()"/></xsl:message -->
    <xsl:if test="name() != 'class' and name() != 'ditaarch:DITAArchVersion' and name() != 'domains' ">
      <xsl:copy/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="topic" mode="topic">
    <xsl:if test="@id = $g_idTopic">
      <xsl:element name="topic">
        <xsl:apply-templates mode="topic" select="node()|@*"/>
      </xsl:element>
    </xsl:if>
  </xsl:template>


</xsl:stylesheet>

