<?xml version="1.0"?>
<!--
    ditamap-to-single-xml.xsl:
        XSLT stylesheet for generate a xml document that includes all the
        topics references by a ditamap file (using xi:include).  The product
        is of course not a valid document, but should suffice for harvesting
        information, like for the link replacements in the help text.
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

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="yes" omit-xml-declaration="no"/>
  <xsl:strip-space elements="*"/>

  <!-- prefaceref -> prefacewrap -->
  <xsl:template match="preface">
    <prefacewrap>
      <xi:include href="{./@href}" xmlns:xi="http://www.w3.org/2001/XInclude" xpointer="xpointer(/topic)" />
      <xsl:apply-templates select="node()" />
    </prefacewrap>
  </xsl:template>

  <!-- topicref -> topicwrap; but not man_xxxx references -->
  <xsl:template match="topicref">
    <xsl:if test="not(contains(@href, 'man_')) and not(contains(@href, '-man.'))">
      <topicwrap>
        <xi:include href="{./@href}" xmlns:xi="http://www.w3.org/2001/XInclude" xpointer="xpointer(/topic)" />
        <xsl:apply-templates select="node()" />
      </topicwrap>
    </xsl:if>
  </xsl:template>

  <!-- chapter/@href -> chapter + xi:include/@href (no wrapper) -->
  <xsl:template match="chapter">
    <xsl:copy>
      <xi:include href="{./@href}" xmlns:xi="http://www.w3.org/2001/XInclude" xpointer="xpointer(/topic)" />
      <xsl:apply-templates select="node()" />
    </xsl:copy>
  </xsl:template>

  <!-- copy everything else -->
  <xsl:template match="node()|@*">
    <xsl:copy>
      <xsl:apply-templates select="node()|@*" />
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

