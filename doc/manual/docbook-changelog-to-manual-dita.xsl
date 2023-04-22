<?xml version="1.0"?>
<!--
    docbook-changelog-to-manual-dita.xsl:
        XSLT stylesheet for converting the change log from DocBook to Dita.
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
  <xsl:import href="common-formatcfg.xsl"/>

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>


  <!-- - - - - - - - - - - - - - - - - - - - - - -
    parameters
   - - - - - - - - - - - - - - - - - - - - - - -->
  <!-- What to do: 'map' for producing a ditamap with all the version,
                   or 'topic' for producing a single topic (version). -->
  <xsl:param name="g_sMode">not specified</xsl:param>

  <!-- The id of the topic to convert in 'topic' mode. -->
  <xsl:param name="g_idTopic">not specified</xsl:param>


  <!-- - - - - - - - - - - - - - - - - - - - - - -
    base operation is to fail on nodes w/o explicit matching.
   - - - - - - - - - - - - - - - - - - - - - - -->

  <xsl:template match="*">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>unhandled element</xsl:message>
  </xsl:template>


  <!-- - - - - - - - - - - - - - - - - - - - - - -
    Match the root element (chapter) and prodcess it's sect1 entries according to the mode.
   - - - - - - - - - - - - - - - - - - - - - - -->
  <xsl:template match="/chapter">
    <xsl:choose>

      <!-- map: Generate a ditamap for all the versions. -->
      <xsl:when test="$g_sMode = 'map'">
        <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE map PUBLIC "-//OASIS//DTD DITA Map//EN" "map.dtd"&gt;
  </xsl:text>
        <xsl:element name="map" >
          <xsl:element name="title"><xsl:text>Change Log</xsl:text></xsl:element>
          <xsl:for-each select="./sect1">
            <xsl:element name="topicref">
              <xsl:attribute name="href">
                <xsl:call-template name="changelog-title-to-filename"/>
              </xsl:attribute>
            </xsl:element>
          </xsl:for-each>
        </xsl:element>
      </xsl:when>

      <!-- topic: Translate a topic (version) to DITA. -->
      <xsl:when test="$g_sMode = 'topic'">
        <xsl:for-each select="./sect1">
          <xsl:variable name="sId">
            <xsl:call-template name="changelog-title-to-id"/>
          </xsl:variable>
          <xsl:if test="$sId = $g_idTopic">
            <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE topic PUBLIC "-//OASIS//DTD DITA Topic//EN" "topic.dtd"&gt;
</xsl:text>
            <xsl:element name="topic">
              <xsl:attribute name="id">
                <xsl:value-of select="$sId" />
              </xsl:attribute>

              <xsl:element name="title">
                <xsl:value-of select="title/text()"/>
              </xsl:element>

              <xsl:element name="body">
                <xsl:apply-templates mode="topic" select="*"/>
              </xsl:element>
            </xsl:element>

          </xsl:if>
        </xsl:for-each>
      </xsl:when>

      <!-- ids: List of IDs (text, not xml - ugly). -->
      <xsl:when test="$g_sMode = 'ids'">
        <xsl:for-each select="./sect1">
          <xsl:variable name="sId">
            <xsl:call-template name="changelog-title-to-id"/>
          </xsl:variable>
          <xsl:value-of disable-output-escaping='yes' select="concat($sId, '&#x0a;')"/>
        </xsl:for-each>
      </xsl:when>

      <!-- Otherwise: bad input -->
      <xsl:otherwise>
        <xsl:message terminate="yes">Unknown g_sMode value: '<xsl:value-of select="$g_sMode"/></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- - - - - - - - - - - - - - - - - - - - - - -
    Section translation to topic.
   - - - - - - - - - - - - - - - - - - - - - - -->
  <xsl:template mode="topic" match="sect1/title">
    <xsl:if test="*">
      <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Unexpected title child elements!</xsl:message>
    </xsl:if>
    <!-- already generated, so we only need to assert sanity and skip it. -->
  </xsl:template>

  <xsl:template mode="topic" match="para">
    <xsl:element name="p">
      <xsl:apply-templates mode="topic" select="node()|@*"/>
    </xsl:element>
  </xsl:template>

  <xsl:template mode="topic" match="para/text()">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template mode="topic" match="itemizedlist">
    <xsl:element name="ul">
      <xsl:apply-templates mode="topic" select="node()|@*"/>
    </xsl:element>
  </xsl:template>

  <xsl:template mode="topic" match="listitem">
    <xsl:element name="li">
      <xsl:apply-templates mode="topic" select="node()|@*"/>
    </xsl:element>
  </xsl:template>


  <!-- - - - - - - - - - - - - - - - - - - - - - -
    Helper functions.
   - - - - - - - - - - - - - - - - - - - - - - -->

  <!-- Extracts the version part of a changelog section title. -->
  <xsl:template name="get-version-from-changelog-title">
    <xsl:param name="sTitle" select="./title/text()"/>
    <xsl:variable name="sAfterVersion" select="normalize-space(substring-after($sTitle, 'Version '))"/>
    <xsl:variable name="sVersion" select="substring-before($sAfterVersion, ' (')"/>
    <xsl:if test="$sVersion = ''">
      <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Unable to extract version from section title!</xsl:message>
    </xsl:if>

    <xsl:value-of select="$sVersion"/>
  </xsl:template>

  <!-- Outputs an id for a changelog section based on the title. -->
  <xsl:template name="changelog-title-to-id">
    <xsl:param name="sTitle" select="./title/text()"/>
    <xsl:text>changelog-version-</xsl:text>
    <xsl:variable name="sVersion">
      <xsl:call-template name="get-version-from-changelog-title">
        <xsl:with-param name="sTitle" select="$sTitle"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:value-of select="translate($sVersion, '.', '_')"/>
  </xsl:template>

  <!-- Outputs a filename for a changelog section based on the title. -->
  <xsl:template name="changelog-title-to-filename">
    <xsl:param name="sTitle" select="./title/text()"/>
    <xsl:call-template name="changelog-title-to-id">
      <xsl:with-param name="sTitle" select="$sTitle"/>
    </xsl:call-template>
    <xsl:text>.dita</xsl:text>
  </xsl:template>

  <!--
    Debug/Diagnostics: Return the path to the specified node (by default the current).
    -->
  <xsl:template name="get-node-path">
    <xsl:param name="Node" select="."/>
    <xsl:for-each select="$Node">
      <xsl:for-each select="ancestor-or-self::node()">
        <xsl:choose>
          <xsl:when test="name(.) = ''">
            <xsl:value-of select="concat('/text(',')')"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="concat('/', name(.))"/>
            <xsl:choose>
              <xsl:when test="@id">
                <xsl:text>[@id=</xsl:text>
                <xsl:value-of select="@id"/>
                <xsl:text>]</xsl:text>
              </xsl:when>
              <xsl:otherwise>
                <!-- Use generate-id() to find the current node position among its siblings. -->
                <xsl:variable name="id" select="generate-id(.)"/>
                <xsl:for-each select="../node()">
                  <xsl:if test="generate-id(.) = $id">
                    <xsl:text>[</xsl:text><xsl:value-of select="position()"/><xsl:text>]</xsl:text>
                  </xsl:if>
                </xsl:for-each>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

  <!--
    Debug/Diagnostics: Return error message prefix.
    -->
  <xsl:template name="error-prefix">
    <xsl:param name="Node" select="."/>
    <xsl:text>error: </xsl:text>
    <xsl:call-template name="get-node-path">
      <xsl:with-param name="Node" select="$Node"/>
    </xsl:call-template>
    <xsl:text>: </xsl:text>
  </xsl:template>

</xsl:stylesheet>

