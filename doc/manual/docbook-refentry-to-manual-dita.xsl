<?xml version="1.0"?>
<!--
    docbook-refentry-to-manual-sect1.xsl:
        XSLT stylesheet for converting a refentry (manpage)
        to dita for use in the user manual.
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
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->



<!-- - - - - - - - - - - - - - - - - - - - - - -
  base operation is to fail on nodes w/o explicit matching.
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="*">
  <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>unhandled element</xsl:message>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  transformations starting with root and going deeper.
 - - - - - - - - - - - - - - - - - - - - - - -->

<!-- Rename refentry to reference.
     Also we need to wrap the refsync and refsect1 elements in a refbody. -->
<xsl:template match="refentry">
  <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE reference PUBLIC "-//OASIS//DTD DITA Reference//EN" "reference.dtd"&gt;</xsl:text>

  <xsl:element name="reference">
    <xsl:if test="not(@id)">
      <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>refentry must have an id attribute!</xsl:message>
    </xsl:if>
    <xsl:attribute name="rev">refentry</xsl:attribute>
    <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>

    <!-- Copy title element from refentryinfo -->
    <xsl:if test="./refentryinfo/title">
      <xsl:copy-of select="./refentryinfo/title"/>
    </xsl:if>

    <!-- Create a shortdesc element from the text in refnamediv/refpurpose -->
    <xsl:if test="./refnamediv/refpurpose">
      <xsl:element name="shortdesc">
        <xsl:attribute name="rev">refnamediv/refpurpose</xsl:attribute>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="text" select="normalize-space(./refnamediv/refpurpose)"/>
        </xsl:call-template>
      </xsl:element>
    </xsl:if>

    <!-- Put everything else side a refbody element -->
    <xsl:element name="refbody">
      <xsl:apply-templates />
    </xsl:element>

  </xsl:element>
</xsl:template>

<!-- Remove refentryinfo (we extracted the title element already). -->
<xsl:template match="refentryinfo" />

<!-- Remove refmeta (manpage info). -->
<xsl:template match="refmeta"/>

<!-- Remove the refnamediv (we extracted a shortdesc from it already). -->
<xsl:template match="refnamediv"/>

<!-- Morph the refsynopsisdiv part into a refsyn section. -->
<xsl:template match="refsynopsisdiv">
  <xsl:if test="name(*[1]) != 'cmdsynopsis'"><xsl:message terminate="yes">Expected refsynopsisdiv to start with cmdsynopsis</xsl:message></xsl:if>
  <xsl:if test="title"><xsl:message terminate="yes">No title element supported in refsynopsisdiv</xsl:message></xsl:if>

  <xsl:element name="refsyn">
    <xsl:attribute name="rev">refsynopsisdiv</xsl:attribute>
    <xsl:element name="title">
      <xsl:text>Synopsis</xsl:text>
    </xsl:element>
    <xsl:apply-templates />
  </xsl:element>

</xsl:template>

<!-- refsect1 -> section -->
<xsl:template match="refsect1">
  <xsl:if test="not(title)"><xsl:message terminate="yes">refsect1 requires title</xsl:message></xsl:if>
  <xsl:element name="section">
    <xsl:attribute name="rev">refsect1</xsl:attribute>
    <xsl:if test="@id">
      <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- refsect2 -> sectiondiv. -->
<xsl:template match="refsect2">
  <xsl:if test="not(title)"><xsl:message terminate="yes">refsect2 requires title</xsl:message></xsl:if>
  <xsl:element name="sectiondiv">
    <xsl:attribute name="rev">refsect2</xsl:attribute>
    <xsl:if test="@id">
      <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>
    </xsl:if>

    <xsl:apply-templates />

  </xsl:element>
</xsl:template>

<!-- refsect2/title -> b -->
<xsl:template match="refsect2/title">
  <xsl:element name="b">
    <xsl:attribute name="rev">refsect2/title</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- refsect1/title -> title -->
<xsl:template match="refsect1/title">
  <xsl:copy>
    <xsl:apply-templates />
  </xsl:copy>
</xsl:template>

<!-- para -> p -->
<xsl:template match="para">
  <xsl:element name="p">
    <xsl:attribute name="rev">para</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- note in a section -> note (no change needed) -->
<xsl:template match="refsect1/note | refsect2/note">
  <xsl:copy>
    <xsl:apply-templates />
  </xsl:copy>
</xsl:template>

<!-- variablelist -> dl -->
<xsl:template match="variablelist">
  <xsl:element name="dl">
    <xsl:attribute name="rev">variablelist</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- varlistentry -> dlentry -->
<xsl:template match="varlistentry">
  <xsl:element name="dlentry">
    <xsl:attribute name="rev">varlistentry</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- term (in varlistentry) -> dt -->
<xsl:template match="varlistentry/term">
  <xsl:element name="dt">
    <xsl:attribute name="rev">term</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- listitem (in varlistentry) -> dd -->
<xsl:template match="varlistentry/listitem">
  <xsl:element name="dd">
    <xsl:attribute name="rev">listitem</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- itemizedlist -> ul -->
<xsl:template match="itemizedlist">
  <xsl:element name="ul">
    <xsl:attribute name="rev">itemizedlist</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- listitem in itemizedlist -> li -->
<xsl:template match="itemizedlist/listitem">
  <xsl:element name="li">
    <xsl:attribute name="rev">listitem</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- command in cmdsynopsis -> syntaxdiagram -->
<xsl:template match="cmdsynopsis">
  <xsl:element name="syntaxdiagram">
    <xsl:attribute name="rev">cmdsynopsis</xsl:attribute>
    <xsl:if test="@id">
      <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- command in cmdsynopsis -> groupseq + kwd -->
<xsl:template match="cmdsynopsis/command | cmdsynopsis/*/command" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">command</xsl:attribute>
    <xsl:element name="kwd">
      <xsl:attribute name="rev">command</xsl:attribute>
      <xsl:apply-templates />
    </xsl:element>
  </xsl:element>
</xsl:template>

<!-- command in not cmdsynopsis -> userinput -->
<xsl:template match="command">
  <xsl:element name="userinput">
    <xsl:attribute name="rev">command</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- arg -->
<xsl:template match="arg/text()">
  <xsl:element name="kwd">
    <xsl:attribute name="rev">arg</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="arg[(not(@choice) or @choice='opt') and (not(@rep) or @rep='norepeat')]" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">arg[opt,norepeat]</xsl:attribute>
    <xsl:attribute name="importance">optional</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="arg[@choice='req' and (not(@rep) or @rep='norepeat')]" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">arg[req,norepeat]</xsl:attribute>
    <xsl:attribute name="importance">required</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="arg[@choice='plain' and (not(@rep) or @rep='norepeat')]" >
  <xsl:if test="./*">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Expected only text under arg[choice=plain]</xsl:message>
  </xsl:if>
  <xsl:element name="kwd">
    <xsl:attribute name="rev">arg[plain]</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<!-- replaceable under arg -> var -->
<xsl:template match="arg/replaceable" >
  <xsl:element name="var">
    <xsl:attribute name="rev">replaceable</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- replaceable in para or term -> synph+var -->
<xsl:template match="para/replaceable | term/replaceable" >
  <xsl:element name="synph">
    <xsl:attribute name="rev">replaceable</xsl:attribute>
    <xsl:element name="var">
      <xsl:attribute name="rev">replaceable</xsl:attribute>
      <xsl:apply-templates />
    </xsl:element>
  </xsl:element>
</xsl:template>

<!-- replaceable in option -> var -->
<xsl:template match="option/replaceable" >
  <xsl:element name="var">
    <xsl:attribute name="rev">option/replaceable</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- group under cmdsynopsis -> groupchoice -->
<xsl:template match="arg/group[@choice='plain'] | cmdsynopsis/group[@choice='plain']">
  <xsl:element name="groupchoice">
    <xsl:attribute name="rev">group</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>


<!-- option -->
<xsl:template match="option/text()" >
  <xsl:element name="kwd">
    <xsl:attribute name="rev">option</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="option" >
  <xsl:element name="synph">
    <xsl:attribute name="rev">option</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- literal -> codeph -->
<xsl:template match="literal" >
  <xsl:element name="codeph">
    <xsl:attribute name="rev">literal</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- filename -> filepath -->
<xsl:template match="filename" >
  <xsl:element name="filepath">
    <xsl:attribute name="rev">filename</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- screen - pass thru -->
<xsl:template match="screen" >
  <xsl:copy>
    <xsl:apply-templates />
  </xsl:copy>
</xsl:template>



<!--
 remark extensions:
 -->
<!-- Default: remove all remarks. -->
<xsl:template match="remark"/>


<!--
 Captializes the given text.
 -->
<xsl:template name="capitalize">
  <xsl:param name="text"/>
  <xsl:call-template name="str:to-upper">
    <xsl:with-param name="text" select="substring($text,1,1)"/>
  </xsl:call-template>
  <xsl:value-of select="substring($text,2)"/>
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
          <xsl:text>text()</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('/', name(.))"/>
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:text>[@id=</xsl:text>
              <xsl:value-of select="@id"/>
              <xsl:text>]</xsl:text>
            </xsl:when>
            <xsl:when test="position() > 1">
              <xsl:text>[</xsl:text><xsl:value-of select="position()"/><xsl:text>]</xsl:text>
            </xsl:when>
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

