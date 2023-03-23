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

<!-- orderedlist -> ol -->
<xsl:template match="orderedlist">
  <xsl:element name="ol">
    <xsl:attribute name="rev">orderedlist</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- listitem in orderedlist -> li -->
<xsl:template match="orderedlist/listitem">
  <xsl:element name="li">
    <xsl:attribute name="rev">listitem</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- cmdsynopsis -> syntaxdiagram
     If sbr is used, this gets a bit more complicated... -->
<xsl:template match="cmdsynopsis[not(sbr)]">
  <xsl:element name="syntaxdiagram">
    <xsl:attribute name="rev">cmdsynopsis</xsl:attribute>
    <xsl:if test="@id">
      <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="cmdsynopsis[sbr]">
  <xsl:element name="syntaxdiagram">
    <xsl:attribute name="rev">cmdsynopsis</xsl:attribute>
    <xsl:if test="@id">
      <xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute>
    </xsl:if>
    <xsl:for-each select="sbr">
      <xsl:variable name="idxSbr" select="position()"/>
      <xsl:element name="groupcomp">
        <xsl:attribute name="rev">sbr/<xsl:value-of select="position()"/></xsl:attribute>

        <xsl:if test="$idxSbr = 1">
          <xsl:apply-templates select="preceding-sibling::node()"/>
        </xsl:if>
        <xsl:if test="$idxSbr != 1">
          <xsl:apply-templates select="preceding-sibling::node()[  count(. | ../sbr[$idxSbr - 1]/following-sibling::node())
                                                                 =     count(../sbr[$idxSbr - 1]/following-sibling::node())]"/>
        </xsl:if>
        <xsl:if test="$idxSbr = last()">
          <xsl:apply-templates select="following-sibling::node()"/>
        </xsl:if>
      </xsl:element>
    </xsl:for-each>
  </xsl:element>
</xsl:template>

<!-- command with text and/or replaceable in cmdsynopsis -> groupseq + kwd -->
<xsl:template match="cmdsynopsis/command | cmdsynopsis/*/command" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">command</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="cmdsynopsis/command/text() | cmdsynopsis/*/command/text()" >
  <xsl:element name="kwd">
    <xsl:attribute name="rev">command/text</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="cmdsynopsis/command/replaceable | cmdsynopsis/*/command/replaceable" >
  <xsl:call-template name="check-children"/>
  <xsl:element name="var">
    <xsl:attribute name="rev">command/replaceable</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- command with text and/or replaceable in not cmdsynopsis -> userinput + cmdname -->
<xsl:template match="command[not(ancestor::cmdsynopsis)]">
  <xsl:element name="userinput">
    <xsl:attribute name="rev">command</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="command[not(ancestor::cmdsynopsis)]/text()">
  <xsl:element name="cmdname">
    <xsl:attribute name="rev">command/text</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="command[not(ancestor::cmdsynopsis)]/replaceable">
  <xsl:call-template name="check-children"/>
  <xsl:element name="varname">
    <xsl:attribute name="rev">command/replaceable</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<!-- arg -->
<xsl:template match="arg/text()">
  <xsl:element name="kwd">
    <xsl:attribute name="rev">arg</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="arg[not(@choice) or @choice='opt']" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">arg[opt]</xsl:attribute>
    <xsl:attribute name="importance">optional</xsl:attribute>

    <xsl:apply-templates />

    <xsl:if test="@rep='repeat'">
      <xsl:element name="repsep">
        <xsl:attribute name="rev">arg[opt,repeat]</xsl:attribute>
        <xsl:text>...</xsl:text>
      </xsl:element>
    </xsl:if>
  </xsl:element>

  <xsl:if test="parent::group">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Expected arg in group to be plain, not optional.</xsl:message>
  </xsl:if>
</xsl:template>

<xsl:template match="arg[@choice='req']" >
  <xsl:element name="groupseq">
    <xsl:attribute name="rev">arg[req]</xsl:attribute>
    <xsl:attribute name="importance">required</xsl:attribute>

    <xsl:apply-templates />

    <xsl:if test="@rep='repeat'">
      <xsl:element name="repsep">
        <xsl:attribute name="rev">arg[opt,repeat]</xsl:attribute>
        <xsl:text>...</xsl:text>
      </xsl:element>
    </xsl:if>
  </xsl:element>

  <xsl:if test="parent::group">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Expected arg in group to be plain, not required.</xsl:message>
  </xsl:if>
</xsl:template>

<xsl:template match="arg[not(ancestor::group) and ancestor::cmdsynopsis and @choice='plain' and (not(@rep) or @rep='norepeat')]" >
  <xsl:element name="kwd">
    <xsl:attribute name="rev">arg[plain]</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="group/arg[replaceable and @choice='plain' and (not(@rep) or @rep='norepeat')]" >
  <xsl:call-template name="check-children">
    <xsl:with-param name="UnsupportedNodes" select="*[not(self::replaceable)]"/>
    <xsl:with-param name="SupportedNames">replaceable or text()</xsl:with-param>
  </xsl:call-template>
  <xsl:element name="var">
    <xsl:attribute name="rev">arg[plain]/replaceable</xsl:attribute>
    <xsl:value-of select="."/>
  </xsl:element>
</xsl:template>

<xsl:template match="group/arg[@choice='plain' and (not(@rep) or @rep='norepeat') and not(replaceable)]" >
  <xsl:call-template name="check-children" />
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
<xsl:template match="para/replaceable | term/replaceable | screen/replaceable" >
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

<!-- replaceable in computeroutput or filename -> varname -->
<xsl:template match="computeroutput/replaceable | filename/replaceable" >
  <xsl:element name="varname">
    <xsl:attribute name="rev">computeroutput/replaceable</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- group under arg and cmdsynopsis -> groupchoice -->
<xsl:template match="arg/group[@choice='plain'] | cmdsynopsis/group[@choice='plain']">
  <xsl:element name="groupchoice">
    <xsl:attribute name="rev">group</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="arg/group[@choice='req'] | cmdsynopsis/group[@choice='req']">
  <xsl:element name="groupchoice">
    <xsl:attribute name="rev">group</xsl:attribute>
    <xsl:attribute name="importance">required</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="cmdsynopsis/group[(@choice='opt' or not(@choice)) and (not(@rep) or @rep='norepeat')]" >
  <xsl:if test="not(./arg[@choice='plain'])">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Did not expect group[@choice=opt] to have children other than arg[@choice=plain]:
      <xsl:for-each select="node()"><xsl:value-of select="concat(' ', name())"/>[@choice=<xsl:value-of select="@choice"/>]</xsl:for-each>
    </xsl:message>
  </xsl:if>
  <xsl:element name="groupchoice">
    <xsl:attribute name="rev">group[opt]</xsl:attribute>
    <xsl:attribute name="importance">optional</xsl:attribute>
    <xsl:value-of select="."/>
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

<!-- literal w/o sub-elements -> codeph -->
<xsl:template match="literal[not(*)]" >
  <xsl:element name="codeph">
    <xsl:attribute name="rev">literal</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- literal with replaceable sub-elements -> synph -->
<xsl:template match="literal[replaceable]" >
  <xsl:element name="synph">
    <xsl:attribute name="rev">literal/replaceable</xsl:attribute>
    <xsl:for-each select="node()">
      <xsl:choose>
        <xsl:when test="self::text()">
          <xsl:element name="kwd">
            <xsl:value-of select="."/>
          </xsl:element>
        </xsl:when>
        <xsl:when test="self::replaceable">
          <xsl:if test="./*">
            <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Unexpected literal/replaceable child</xsl:message>
          </xsl:if>
          <xsl:element name="var">
            <xsl:value-of select="."/>
          </xsl:element>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Unexpected literal child:
            <xsl:value-of select="name(.)" />
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
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

<!-- computeroutput -> systemoutput-->
<xsl:template match="computeroutput">
  <xsl:element name="systemoutput">
    <xsl:attribute name="rev">computeroutput</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- xref -> xref, but attributes differ. -->
<xsl:template match="xref">
  <xsl:element name="xref">
    <xsl:attribute name="href"><xsl:value-of select="@linkend"/></xsl:attribute>
    <xsl:if test="contains(@linkend, 'http')"><xsl:message terminate="yes">xref/linkend with http</xsl:message></xsl:if>
  </xsl:element>
</xsl:template>

<!-- ulink -> xref -->
<xsl:template match="ulink">
  <xsl:element name="xref">
    <xsl:attribute name="rev">ulink</xsl:attribute>
    <xsl:attribute name="scope">external</xsl:attribute> <!-- Just assumes this is external. -->
    <xsl:attribute name="href"><xsl:value-of select="@url"/></xsl:attribute>
  </xsl:element>
</xsl:template>

<!-- emphasis -> i -->
<xsl:template match="emphasis">
  <xsl:if test="*">
    <xsl:message terminate="yes"><xsl:call-template name="error-prefix"/>Did not expect emphasis to have children!</xsl:message>
  </xsl:if>
  <xsl:element name="i">
    <xsl:attribute name="rev">emphasis</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<!-- note -> note -->
<xsl:template match="note">
  <xsl:copy>
    <xsl:apply-templates />
  </xsl:copy>
</xsl:template>

<!-- citetitle -> cite -->
<xsl:template match="citetitle">
  <xsl:element name="cite">
    <xsl:attribute name="rev">citetitle</xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
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

<!--
  Debug/Diagnostics: Print list of nodes (by default all children of current node).
  -->
<xsl:template name="list-nodes">
  <xsl:param name="Nodes" select="node()"/>
  <xsl:for-each select="$Nodes">
    <xsl:if test="position() != 1">
      <xsl:text>, </xsl:text>
    </xsl:if>
    <xsl:choose>
      <xsl:when test="name(.) = ''">
        <xsl:text>text:text()</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="name(.)"/>
        <xsl:if test="@id">
          <xsl:text>[@id=</xsl:text>
          <xsl:value-of select="@id"/>
          <xsl:text>]</xsl:text>
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>
</xsl:template>

<xsl:template name="check-children">
  <xsl:param name="Node"             select="."/>
  <xsl:param name="UnsupportedNodes" select="*"/>
  <xsl:param name="SupportedNames"   select="'none'"/>
  <xsl:if test="count($UnsupportedNodes) != 0">
    <xsl:message terminate="yes">
      <xsl:call-template name="get-node-path">
        <xsl:with-param name="Node" select="$Node"/>
      </xsl:call-template>
      <!-- -->: error: Only <xsl:value-of select="$SupportedNames"/> are supported as children to <!-- -->
      <xsl:value-of select="name($Node)"/>
      <!-- -->
Unsupported children: <!-- -->
      <xsl:call-template name="list-nodes">
        <xsl:with-param name="Nodes" select="$UnsupportedNodes"/>
      </xsl:call-template>
    </xsl:message>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>

