<?xml version="1.0"?>
<!--
    docbook-refentry-to-manual-sect1.xsl:
        XSLT stylesheet for nicking the refsynopsisdiv bit of a
        refentry (manpage) for use in the command overview section
        in the user manual.

    Copyright (C) 2006-2015 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet
  version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:str="http://xsltsl.org/string"
  >

  <xsl:import href="@VBOX_PATH_MANUAL_SRC@/string.xsl"/>

  <xsl:output method="text" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>


  <!-- Default action, do nothing. -->
  <xsl:template match="node()|@*"/>


  <!--
    main() - because we need to order the output in a specific manner
             that is contrary to the data flow in the refentry, this is
             going to look a bit more like a C program than a stylesheet.
    -->
  <xsl:template match="refentry">
    <!-- Assert refetry expectations. -->
    <xsl:if test="not(./refsynopsisdiv)">     <xsl:message terminate="yes">refentry must have a refsynopsisdiv</xsl:message></xsl:if>
    <xsl:if test="not(./refentryinfo/title)"> <xsl:message terminate="yes">refentry must have a refentryinfo with title</xsl:message></xsl:if>
    <xsl:if test="not(./refsect1/title)">     <xsl:message terminate="yes">refentry must have a refsect1 with title</xsl:message></xsl:if>
    <xsl:if test="not(@id) or @id = ''">      <xsl:message terminate="yes">refentry must have an id attribute</xsl:message></xsl:if>

    <!-- variables -->
    <xsl:variable name="sBaseId" select="@id"/>
    <xsl:variable name="sDataBaseSym" select="concat('g_', translate(@id, '-', '_'))"/>


    <!--
      Convert the refsynopsisdiv into REFENTRY::Synopsis data.
      -->
    <xsl:text>

static const REFENTRYSTR </xsl:text><xsl:value-of select="$sDataBaseSym"/><xsl:text>_synopsis[] =
{</xsl:text>
    <xsl:for-each select="./refsynopsisdiv/cmdsynopsis">
      <!-- Assert synopsis expectations -->
      <xsl:if test="not(@id) or substring-before(@id, '-') != 'synopsis'">
        <xsl:message terminate="yes">The refsynopsisdiv/cmdsynopsis elements must have an id starting with 'synopsis-'.</xsl:message>
      </xsl:if>
      <xsl:if test="not(../../refsect1/refsect2[@id=./@id])">
        <xsl:message terminate="yes">No refsect2 with id="<xsl:value-of select="@id"/>" found.</xsl:message>
      </xsl:if>

      <!-- Do the work. -->
      <xsl:apply-templates select="."/>

    </xsl:for-each>
    <xsl:text>
};</xsl:text>


    <!--
      Convert the whole manpage to help text.
      -->
    <xsl:text>
static const REFENTRYSTR </xsl:text><xsl:value-of select="$sDataBaseSym"/><xsl:text>_full_help[] =
{</xsl:text>
    <!-- We start by combining the refentry title and the refpurpose into a short description. -->
    <xsl:text>
    {   </xsl:text><xsl:call-template name="calc-scope"/><xsl:text>,
        "</xsl:text>
        <xsl:apply-templates select="./refentryinfo/title/node()"/>
        <xsl:text> -- </xsl:text>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="text">
            <xsl:apply-templates select="./refnamediv/refpurpose/node()"/>
          </xsl:with-param>
        </xsl:call-template>
        <xsl:text>." },
    {   REFENTRYSTR_SCOPE_SAME, "" },</xsl:text>

    <!-- The follows the usage (synopsis) section. -->
    <xsl:text>
    {   REFENTRYSTR_SCOPE_GLOBAL,
        "Usage:" },</xsl:text>
        <xsl:apply-templates select="./refsynopsisdiv/node()"/>

    <!-- Then comes the description and other refsect1 -->
    <xsl:for-each select="./refsect1">
      <xsl:if test="name(*[1]) != 'title'"><xsl:message terminate="yes">Expected title as the first element in refsect1.</xsl:message></xsl:if>
      <xsl:if test="text()"><xsl:message terminate="yes">No text supported in refsect1.</xsl:message></xsl:if>
      <xsl:if test="not(./remark[@role='help-skip'])">
        <xsl:text>
    {   </xsl:text><xsl:call-template name="calc-scope-refsect1"/><xsl:text>, "" },
    {   REFENTRYSTR_SCOPE_SAME,
        "</xsl:text><xsl:apply-templates select="title/node()"/><xsl:text>:" },</xsl:text>
        <xsl:apply-templates select="./*[position() > 1]"/>
      </xsl:if>
    </xsl:for-each>

    <xsl:text>
};</xsl:text>

    <!--
      Generate the refentry structure.
      -->
    <xsl:text>
static const REFENTRY </xsl:text><xsl:value-of select="$sDataBaseSym"/><xsl:text> =
{
    /* .idinternal = */   HELP_CMD_</xsl:text>
    <xsl:call-template name="str:to-upper">
      <xsl:with-param name="text" select="translate(substring-after(@id, '-'), '-', '_')"/>
    </xsl:call-template>
    <xsl:text>,
    /* .Synopsis   = */   { </xsl:text>
    <xsl:value-of select="$sDataBaseSym"/><xsl:text>_synopsis, RT_ELEMENTS(</xsl:text>
    <xsl:value-of select="$sDataBaseSym"/><xsl:text>_synopsis) },
    /* .Help       = */   { </xsl:text>
    <xsl:value-of select="$sDataBaseSym"/><xsl:text>_full_help, RT_ELEMENTS(</xsl:text>
    <xsl:value-of select="$sDataBaseSym"/><xsl:text>_full_help) },
    /* pszBrief    = */   "</xsl:text>
    <xsl:apply-templates select="./refnamediv/refpurpose/node()"/>
    <xsl:text>"
};
</xsl:text>
  </xsl:template>


  <!--
    Convert command synopsis to text.
    -->
  <xsl:template match="cmdsynopsis">
    <xsl:if test="text()"><xsl:message terminate="yes">cmdsynopsis with text is not supported.</xsl:message></xsl:if>
    <xsl:text>
    {   </xsl:text><xsl:call-template name="calc-scope-cmdsynopsis"/><xsl:text>,
        "</xsl:text><xsl:call-template name="emit-indentation"/><xsl:apply-templates select="*|@*"/><xsl:text>" },</xsl:text>
  </xsl:template>

  <xsl:template match="sbr">
    <xsl:text>" },
    {   REFENTRYSTR_SCOPE_SAME,
        "    </xsl:text><xsl:call-template name="emit-indentation"/>
  </xsl:template>

  <xsl:template match="command|option|computeroutput">
    <xsl:apply-templates select="node()|@*"/>
  </xsl:template>

  <xsl:template match="replaceable">
    <xsl:text>&lt;</xsl:text>
    <xsl:apply-templates select="node()|@*"/>
    <xsl:text>&gt;</xsl:text>
  </xsl:template>

  <xsl:template match="arg|group">
    <!-- separator char if we're not the first child -->
    <xsl:if test="position() > 1">
      <xsl:choose>
        <xsl:when test="ancestor-or-self::*/@sepchar"><xsl:value-of select="ancestor-or-self::*/@sepchar"/></xsl:when>
        <xsl:otherwise><xsl:text> </xsl:text></xsl:otherwise>
      </xsl:choose>
    </xsl:if>
    <!-- open wrapping -->
    <xsl:choose>
      <xsl:when test="@choice = 'opt' or not(@choice) or @choice = ''"> <xsl:text>[</xsl:text></xsl:when>
      <xsl:when test="@choice = 'req'">                                 <xsl:text></xsl:text></xsl:when>
      <xsl:when test="@choice = 'plain'"/>
      <xsl:otherwise><xsl:message terminate="yes">Invalid arg choice: "<xsl:value-of select="@choice"/>"</xsl:message></xsl:otherwise>
    </xsl:choose>
    <!-- render the arg (TODO: may need to do more work here) -->
    <xsl:apply-templates select="node()|@*"/>
    <!-- repeat wrapping -->
    <xsl:choose>
      <xsl:when test="@rep = 'norepeat' or not(@rep) or @rep = ''"/>
      <xsl:when test="@rep = 'repeat'">                                 <xsl:text>...</xsl:text></xsl:when>
      <xsl:otherwise><xsl:message terminate="yes">Invalid rep choice: "<xsl:value-of select="@rep"/>"</xsl:message></xsl:otherwise>
    </xsl:choose>
    <!-- close wrapping -->
    <xsl:choose>
      <xsl:when test="@choice = 'opt' or not(@choice) or @choice = ''"> <xsl:text>]</xsl:text></xsl:when>
      <xsl:when test="@choice = 'req'">                                 <xsl:text></xsl:text></xsl:when>
    </xsl:choose>
  </xsl:template>


  <!--
    refsect2
    -->
  <xsl:template match="refsect2">
    <!-- assertions -->
    <xsl:if test="text()"><xsl:message terminate="yes">refsect2 shouldn't contain text</xsl:message></xsl:if>
    <xsl:if test="count(./cmdsynopsis) > 1"><xsl:message terminate="yes">Only a single cmdsynopsis is currently supported in a refsect2.</xsl:message></xsl:if>
    <xsl:if test="count(./title) > 1"><xsl:message terminate="yes">Only a single title in refsect2</xsl:message></xsl:if>

    <!-- title / command synopsis - sets the scope. -->
    <xsl:text>
    {   </xsl:text><xsl:call-template name="calc-scope-refsect2"/><xsl:text>,
        "</xsl:text><xsl:call-template name="emit-indentation"/>
    <xsl:choose>
      <xsl:when test="name(./*[1]) = 'cmdsynopsis'">
        <xsl:text>  </xsl:text>
        <xsl:apply-templates select="./cmdsynopsis/node()|./cmdsynopsis/@*"/>
      </xsl:when>
      <xsl:when test="name(./*[1]) = 'title'">
        <xsl:apply-templates select="./title/text()"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes">Expected either a title or cmdsynopsis as the first element in a refsect2.
            (Put scoping remarks after.)</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>" },</xsl:text>

    <!-- Format the text in the section -->
    <xsl:apply-templates select="./*[position() > 1]"/>

    <!-- Add a blank line, unless we're the last element in this refsect1. -->
    <xsl:if test="position() != last()">
      <xsl:text>
    {   REFENTRYSTR_SCOPE_SAME, "" },</xsl:text>
    </xsl:if>
  </xsl:template>


  <!--
    para
    -->
  <xsl:template match="para">
    <xsl:if test="position() != 1 or not(parent::listitem)">
      <xsl:text>
    {   REFENTRYSTR_SCOPE_SAME, "" },</xsl:text>
    </xsl:if>
    <xsl:call-template name="process-mixed"/>
  </xsl:template>


  <!--
    variablelist
    -->
  <xsl:template match="variablelist">
    <xsl:if test="*[not(self::varlistentry)]|text()">
      <xsl:message terminate="yes">Only varlistentry elements are supported in variablelist</xsl:message>
    </xsl:if>
    <xsl:for-each select="./varlistentry">
      <xsl:if test="count(*) != 2 or not(term) or not(listitem)">
        <xsl:message terminate="yes">Expected exactly one term and one listentry member in varlistentry element.</xsl:message>
      </xsl:if>
      <xsl:apply-templates select="*"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="varlistentry/term">
    <xsl:call-template name="process-mixed"/>
  </xsl:template>

  <xsl:template match="varlistentry/listitem">
    <xsl:if test="text() or *[not(self::para)]">
      <xsl:message terminate="yes">Expected varlistentry/listitem to only contain para elements</xsl:message>
    </xsl:if>
    <xsl:apply-templates select="*"/>
  </xsl:template>


  <!--
    Text escaping for C.
    -->
  <xsl:template match="text()" name="escape_text">
    <xsl:choose>

      <xsl:when test="contains(., '\') or contains(., '&quot;')">
        <xsl:variable name="sTmp">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="normalize-space(.)"/>
            <xsl:with-param name="replace" select="'\'"/>
            <xsl:with-param name="with" select="'\\'"/>
            <xsl:with-param name="disable-output-escaping" select="yes"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:call-template name="str:subst">
          <xsl:with-param name="text" select="$sTmp"/>
          <xsl:with-param name="replace" select="'&quot;'"/>
          <xsl:with-param name="with" select="'\&quot;'"/>
          <xsl:with-param name="disable-output-escaping" select="yes"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:otherwise>
        <xsl:value-of select="normalize-space(.)"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Elements producing non-breaking strings (single line). -->
  <xsl:template match="command/text()|option/text()|computeroutput/text()" name="escape_fixed_text">
    <xsl:choose>

      <xsl:when test="contains(., '\') or contains(., '&quot;')">
        <xsl:variable name="sTmp1">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="."/>
            <xsl:with-param name="replace" select="'\'"/>
            <xsl:with-param name="with" select="'\\'"/>
            <xsl:with-param name="disable-output-escaping" select="yes"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="sTmp2">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$sTmp1"/>
            <xsl:with-param name="replace" select="'&quot;'"/>
            <xsl:with-param name="with" select="'\&quot;'"/>
            <xsl:with-param name="disable-output-escaping" select="yes"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:call-template name="str:subst">
          <xsl:with-param name="text" select="$sTmp2"/>
          <xsl:with-param name="replace" select="' '"/>
          <xsl:with-param name="with" select="'\b'"/>
          <xsl:with-param name="disable-output-escaping" select="yes"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:when test="contains(., ' ')">
        <xsl:call-template name="str:subst">
          <xsl:with-param name="text" select="."/>
          <xsl:with-param name="replace" select="' '"/>
          <xsl:with-param name="with" select="'\b'"/>
          <xsl:with-param name="disable-output-escaping" select="yes"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:otherwise>
        <xsl:value-of select="."/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!--
    Unsupported elements and elements handled directly.
    -->
  <xsl:template match="synopfragment|synopfragmentref|title|refsect1">
    <xsl:message terminate="yes">The <xsl:value-of select="name()"/> element is not supported</xsl:message>
  </xsl:template>

  <!--
    Fail on misplaced scoping remarks.
    -->
  <xsl:template match="remark[@role = 'help-scope']">
    <xsl:choose>
      <xsl:when test="parent::refsect1"/>
      <xsl:when test="parent::refsect2"/>
      <xsl:when test="parent::cmdsynopsis and ancestor::refsynopsisdiv"/>
      <xsl:otherwise>
        <xsl:message terminate="yes">Misplaced remark/@role=help-scope element.
Only supported on: refsect1, refsect2, refsynopsisdiv/cmdsynopsis</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!--
    Warn about unhandled elements
    -->
  <xsl:template match="*">
    <xsl:message terminate="no">Warning: Unhandled element: <xsl:value-of select="name(.)"/></xsl:message>
  </xsl:template>


  <!--
    Functions
    Functions
    Functions
    -->

  <!--
    Processes mixed children, i.e. both text and regular elements.
    Normalizes whitespace. -->
  <xsl:template name="process-mixed">
    <xsl:text>
    {   REFENTRYSTR_SCOPE_SAME,
        "</xsl:text><xsl:call-template name="emit-indentation"/>

    <xsl:for-each select="node()[not(self::remark)]">
      <xsl:if test="position() != 1">
        <xsl:text> </xsl:text>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="name() = ''">
          <xsl:call-template name="escape_text"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="."/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>

    <xsl:text>" },</xsl:text>
  </xsl:template>


<!-- TODO: scoping must be reworked! -->
  <!-- Figures out the scope of a refsect1 element. -->
  <xsl:template name="calc-scope-refsect1">
    <xsl:choose>
      <xsl:when test="title[text() = 'Description']">
        <xsl:text>REFENTRYSTR_SCOPE_GLOBAL</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="calc-scope"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Figures out the scope of a refsect2 element. -->
  <xsl:template name="calc-scope-refsect2">
    <xsl:choose>
      <xsl:when test="0">
        <xsl:text>REFENTRYSTR_SCOPE_GLOBAL</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="calc-scope"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Figures out the scope of a refsect1 element. -->
  <xsl:template name="calc-scope-cmdsynopsis">
    <xsl:choose>
      <xsl:when test="ancestor::refsynopsisdiv">
        <xsl:call-template name="calc-scope"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>REFENTRYSTR_SCOPE_SAME</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Figures out the scope of an element. -->
  <xsl:template name="calc-scope">
    <xsl:param name="a_Element" select="."/>
    <xsl:param name="a_cRecursions" select="'1'"/>
    <xsl:variable name="ScopeRemark" select="$a_Element/remark[@role='help-scope']"/>

    <xsl:choose>
      <!-- Check for an explicit scope remark: <remark role='scope' condition='uninstall'/> -->
      <xsl:when test="$ScopeRemark">
        <xsl:if test="not($ScopeRemark/@condition)">
          <xsl:message terminate="yes">remark[role=scope] element must have a condition attribute.</xsl:message>
        </xsl:if>

        <xsl:call-template name="calc-scope-const">
          <xsl:with-param name="sId" select="concat(concat(ancestor::refentry[1]/@id, '-'), $ScopeRemark/@condition)"/>
        </xsl:call-template>
      </xsl:when>

      <!-- Try derive it from the @id tag, if any. -->
      <xsl:when test="substring(@id, 1, 3) = 'vbox'">
        <xsl:call-template name="calc-scope-const">
          <xsl:with-param name="sId" select="$a_Element/@id"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@id">
        <xsl:call-template name="calc-scope-const">
          <xsl:with-param name="sId" select="substring-after($a_Element/@id, '-')"/>
        </xsl:call-template>
      </xsl:when>

      <!-- Recursively try the parent element. -->
      <xsl:when test="($a_cRecursions) > 10">
        <xsl:message terminal="yes">calc-scope recursion too deep.</xsl:message>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="calc-scope">
          <xsl:with-param name="a_Element" select="$a_Element/.."/>
          <xsl:with-param name="a_cRecursions" select="$a_cRecursions + 1"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Calculates a scope constant from a ID like value. -->
  <xsl:template name="calc-scope-const">
    <xsl:param name="sId" select="@id"/>
    <xsl:if test="$sId = ''"><xsl:message terminate="yes">refentry: command Missing </xsl:message></xsl:if>
    <xsl:variable name="sTmp1" select="translate($sId, '-', '_')"/>
    <xsl:variable name="sTmp2">
      <xsl:call-template name="str:to-upper">
        <xsl:with-param name="text" select="$sTmp1"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:text>HELP_SCOPE_</xsl:text>
    <xsl:value-of select="$sTmp2"/>
  </xsl:template>

  <!-- Calculates and emits indentation. -->
  <xsl:template name="emit-indentation">
    <xsl:if test="ancestor::refsect1">
      <xsl:text>  </xsl:text>
    </xsl:if>
    <xsl:if test="ancestor::refsect2">
      <xsl:text>  </xsl:text>
    </xsl:if>
    <xsl:if test="ancestor::refsect3">
      <xsl:text>  </xsl:text>
    </xsl:if>
    <xsl:if test="ancestor::varlistentry">
      <xsl:if test="ancestor-or-self::term">
        <xsl:text> </xsl:text>
      </xsl:if>
      <xsl:if test="ancestor-or-self::listitem">
        <xsl:text>    </xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <!-- Captializes the given text. -->
  <xsl:template name="capitalize">
    <xsl:param name="text"/>
    <xsl:call-template name="str:to-upper">
      <xsl:with-param name="text" select="substring($text,1,1)"/>
    </xsl:call-template>
    <xsl:value-of select="substring($text,2)"/>
  </xsl:template>

</xsl:stylesheet>

