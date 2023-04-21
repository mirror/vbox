<?xml version="1.0"?>
<!--
    dita-refentry-flat-topic-ids.xsl:
        XSLT stylesheet for extracting all the topic IDs from a DITA manual page.
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

  <xsl:output method="text" version="1.0" encoding="utf-8" indent="no"/>

  <xsl:template match="/">
    <xsl:for-each select="//topic">
      <xsl:if test="position() != 1"><xsl:text> </xsl:text></xsl:if>
      <xsl:value-of select="@id"/>
    </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>

