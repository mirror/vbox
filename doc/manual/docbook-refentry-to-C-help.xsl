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
    <xsl:if test="not(./refsynopsisdiv)"><xsl:message terminate="yes">refentry must have a refsynopsisdiv</xsl:message></xsl:if>
    <xsl:if test="not(./refsect1/title)"><xsl:message terminate="yes">refentry must have a refsynopsisdiv</xsl:message></xsl:if>
    <xsl:if test="not(@id) or @id = ''"><xsl:message terminate="yes">refentry must have an id attribute</xsl:message></xsl:if>

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
      <xsl:text>
    {   </xsl:text><xsl:call-template name="calc-scope"/><xsl:text>,
        "</xsl:text>
        <xsl:apply-templates select="."/>
        <xsl:text>" },
</xsl:text>
    </xsl:for-each>
    <xsl:text>
};
</xsl:text>
  </xsl:template>


  <!--
    Functions
    Functions
    Functions
    -->

  <!-- Figures out the scope of an element. -->
  <xsl:template name="calc-scope">
    <xsl:param name="a_Element" select="."/>
    <xsl:param name="a_cRecursions" select="'1'"/>

    <xsl:choose>
      <!-- Check for an explicit scope remark: <remark role='scope' condition='uninstall'/> -->
      <xsl:when test="$a_Element/remark[@role='scope']">
        <xsl:if test="not($a_Element/remark[@role='scope']/@condition)">
          <xsl:message terminate="yes">remark[role=scope] element must have a condition attribute.</xsl:message>
        </xsl:if>
        <xsl:call-template name="calc-scope-const">
          <xsl:with-param name="sId" select="concat(concat(ancestor::refentry[1]/@id, '-'),
                                                    $a_Element/remark[@role='scope']/@condition)"/>
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
    <xsl:text>HELP_</xsl:text>
    <xsl:value-of select="$sTmp2"/>
  </xsl:template>


  <!--
    To text conversions.
    -->
  <xsl:template match="cmdsynopsis">
    <xsl:if test="text()"><xsl:message terminate="yes">cmdsynopsis with text is not supported.</xsl:message></xsl:if>
    <xsl:text>    </xsl:text>
    <xsl:apply-templates select="node()|@*"/>
  </xsl:template>

  <xsl:template match="command">
    <!--xsl:choose>
      <xsl:when test=" ">
    </xsl:choose-->
    <xsl:apply-templates select="node()|@*"/>
  </xsl:template>

  <xsl:template match="replaceable">
    <xsl:text>&lt;</xsl:text>
    <xsl:apply-templates select="text()|node()|@*"/>
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
    <xsl:apply-templates />
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

  <!-- non-breaking strings -->
  <xsl:template match="command/text()">
    <xsl:call-template name="str:subst">
      <xsl:with-param name="text" select="."/>
      <xsl:with-param name="replace" select="' '"/>
      <xsl:with-param name="with" select="'\b'"/>
      <xsl:with-param name="disable-output-escaping" select="yes"/>
    </xsl:call-template>
  </xsl:template>


</xsl:stylesheet>

