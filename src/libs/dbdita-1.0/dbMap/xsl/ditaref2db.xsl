<?xml version="1.0" encoding="UTF-8" ?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:import href="../../../book/bookmap2docbk.xsl"/>
<xsl:import href="dbmapNamed.xsl"/>

<xsl:output
    method="xml"
    indent="yes"
    omit-xml-declaration="no"
    standalone="no"
    doctype-public="-//OASIS//DTD DocBook XML V4.4//EN"
    doctype-system="http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd"/>

<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Rules for the DITA reference
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="book/ditaref" mode="docbook">
  <xsl:apply-templates select="document(@href,/)/*" mode="dbdita-chapter"/>
</xsl:template>

<xsl:template match="ditaref" mode="docbook">
  <xsl:apply-templates select="document(@href,/)/*" mode="dbdita-section"/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Rules for DocBook elements
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="/">
  <xsl:apply-templates select="." mode="docbook"/>
</xsl:template>

<xsl:template match="*" mode="docbook">
  <xsl:copy>
    <xsl:apply-templates select="@*" mode="docbook"/>
    <xsl:apply-templates select="*|text()|processing-instruction()|comment()"
        mode="docbook"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="@*|text()|processing-instruction()|comment()"
      mode="docbook">
  <xsl:copy-of select="."/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Temporary fix for bug in base docbook/topic2db.xsl transform
   - and a few other tweaks in the base transform
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' topic/itemgroup ')]">
  <xsl:variable name="element" select="local-name(.)"/>
  <xsl:variable name="id" select="concat('elem', generate-id())"/>
  <xsl:call-template name="deflateElementStart">
    <xsl:with-param name="id"       select="$id"/>
    <xsl:with-param name="element"  select="$element"/>
    <xsl:with-param name="parentID" select="''"/>
  </xsl:call-template>
  <xsl:call-template name="makeBlock"/>
  <xsl:call-template name="deflateElementEnd">
    <xsl:with-param name="id"      select="$id"/>
    <xsl:with-param name="element" select="$element"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' topic/related-links ')]">
  <itemizedlist>
    <title>Related links</title>
    <xsl:apply-templates/>
  </itemizedlist>
</xsl:template>

<!-- should emit the shortdesc as an abstract for book or article
     but suppress for all cases as a short-term fix -->
<xsl:template match="*[contains(@class,' topic/shortdesc ')]" mode="abstract"/>

<xsl:template match="*[contains(@class,' topic/link ') and (
      (@format and @format!='dita' and @format!='DITA') or
	  (@scope  and @scope!='local' and @scope!='LOCAL'))]">
  <xsl:apply-imports/>
</xsl:template>

<xsl:template match="*[contains(@class,' topic/link ') and
	  (not(@scope)  or @scope='local' or @scope='LOCAL') and
      (not(@format) or @format='dita' or @format='DITA')]">
  <xsl:variable name="linkID">
    <xsl:call-template name="getLinkID"/>
  </xsl:variable>
  <listitem>
    <para>
      <xref linkend="{$linkID}">
        <xsl:call-template name="setStandardAttr">
          <xsl:with-param name="IDPrefix" select="'xref'"/>
        </xsl:call-template>
      </xref>
    </para>
    <xsl:apply-templates select="*[contains(@class,' topic/desc ')]"/>
  </listitem>
</xsl:template>

<xsl:template name="getLinkID">
  <xsl:param name="href" select="@href"/>
  <xsl:variable name="hasID" select="contains($href,'#')"/>
  <xsl:choose>
  <xsl:when test="$hasID">
    <xsl:value-of select="translate(substring-after($href,'#'),'/','_')"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:value-of
        select="document($href, /)/*[contains(@class,' topic/topic ')]/@id"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>


</xsl:stylesheet>
