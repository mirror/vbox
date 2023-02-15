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

<!-- TO DO:
     support locked titles
     navtitle to titleabbrev conversion
     metadata to info conversion
     map references
     parameters to control insertion of toc and index

     This transform ought to make use of map2docbook.xsl, but
     mode="dbheading" doesn't get fired when the import is enabled.
     
<xsl:import href="../../../../xsl/map2docbook.xsl"/>
<xsl:import href="dbmapNamed.xsl"/>
    -->

<xsl:output
    method="xml"
    indent="no"
    omit-xml-declaration="no"
    standalone="no"
    doctype-public="-//OASIS//DTD DocBook XML V4.4//EN"
    doctype-system="http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd"
    />    

<xsl:template match="/">
  <xsl:apply-templates select="*"/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Rules for DocBook heads or references
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' docbookMap/docbookMap ')]">
  <xsl:apply-templates select="*" mode="docbookmap"/>
</xsl:template>

<xsl:template match="*[contains(@class,' docbookMap/sethead ') or
          contains(@class,' docbookMap/bookhead ') or
          contains(@class,' docbookMap/referencehead ') or
          contains(@class,' docbookMap/parthead ')]" mode="docbookmap">
  <!-- DOESN'T GET FIRED WHEN map2docbook.xsl IS IMPORTED -->
  <xsl:apply-templates select="." mode="dbheading"/>
</xsl:template>

<xsl:template match="*" mode="dbheading">
  <xsl:param name="element" select="substring-before(
            substring-after(@class,' docbookMap/'),
            'head ')"/>
  <xsl:element name="{$element}">
    <xsl:copy-of select="@id"/>
    <!-- TO DO: {$element}info here -->
    <title><xsl:value-of select="@navtitle"/></title>
    <xsl:apply-templates select="*" mode="docbookmap"/>
  </xsl:element>
</xsl:template>

<xsl:template match="*[contains(@class,' docbookMap/dedicationref ') or
          contains(@class,' docbookMap/prefaceref ') or
          contains(@class,' docbookMap/chapterref ') or
          contains(@class,' docbookMap/sectionref ') or
          contains(@class,' docbookMap/refentryref ') or
          contains(@class,' docbookMap/refsynopsisdivref ') or
          contains(@class,' docbookMap/refsectionref ') or
          contains(@class,' docbookMap/partintroref ') or
          contains(@class,' docbookMap/appendixref ') or
          contains(@class,' docbookMap/bibliographyref ') or
          contains(@class,' docbookMap/bibliodivref ') or
          contains(@class,' docbookMap/biblioentryref ') or
          contains(@class,' docbookMap/glossaryref ') or
          contains(@class,' docbookMap/glossdivref ') or
          contains(@class,' docbookMap/glossentryref ') or
          contains(@class,' docbookMap/colophonref ')]" mode="docbookmap">
  <xsl:apply-templates select="." mode="dbreference"/>
</xsl:template>

<xsl:template match="*" mode="dbreference">
  <xsl:variable name="compositionNode" select="."/>
  <xsl:apply-templates select="document(@href, /)/*" mode="dbcompose">
    <xsl:with-param name="compositionNode" select="$compositionNode"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*" mode="dbcompose">
  <xsl:param name="compositionNode"/>
  <xsl:variable name="targetType">
    <xsl:choose>
    <xsl:when test="$compositionNode/@type">
      <xsl:value-of select="$compositionNode/@type"/>
    </xsl:when>
    <xsl:when test="$compositionNode/@class">
      <xsl:value-of select="substring-before(
          substring-after($compositionNode/@class,' docbookMap/'),
          'ref ')"/>
    </xsl:when>
    </xsl:choose>
  </xsl:variable>
  <xsl:if test="$targetType and string-length($targetType) &gt; 0 and
          local-name()!=$targetType">
    <xsl:message>
      <xsl:text>Referenced element </xsl:text>
      <xsl:value-of select="local-name()"/>
      <xsl:text> doesn't match referencing type </xsl:text>
      <xsl:value-of select="$targetType"/>
    </xsl:message>
  </xsl:if>
  <xsl:copy>
    <xsl:copy-of select="@*"/>
    <xsl:copy-of select="*|text()|processing-instruction()"/>
    <xsl:apply-templates
          select="$compositionNode/*[contains(@class,' map/topicref ')]"
          mode="docbookmap"/>
  </xsl:copy>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Contextual rules
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<!-- ENABLE ONCE IMPORT IS WORKING
<xsl:template match="*[contains(@class,' map/topicref ') and
    following-sibling::*[contains(@class,' docbookMap/chapterref ') or
        contains(@class,' docbookMap/appendixref ')] and
    not(preceding-sibling::*[contains(@class,' docbookMap/appendixref ')])]"
        mode="docbookmap">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'chapter'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and
    preceding-sibling::*[contains(@class,' docbookMap/appendixref ')]]"
        mode="docbookmap">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'appendix'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' map/map ')] /
    *[contains(@class,' map/topicref ') and
      not(following-sibling::*[contains(@class,' docbookMap/chapterref ') or
          contains(@class,' docbookMap/appendixref ')]) and
      not(preceding-sibling::*[contains(@class,' docbookMap/chapterref ') or
          contains(@class,' docbookMap/appendixref ')])]"
        mode="docbookmap">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'chapter'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' docbookMap/prefaceref ') or
          contains(@class,' docbookMap/chapterref ') or
          contains(@class,' docbookMap/sectionref ') or
          contains(@class,' docbookMap/partintroref ') or
          contains(@class,' docbookMap/appendixref ')]/
            *[contains(@class,' map/topicref ')]"
        mode="docbookmap">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'section'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' docbookMap/refentryref ') or
          contains(@class,' docbookMap/refsectionref ')]/
            *[contains(@class,' map/topicref ')]"
        mode="docbookmap">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'refsection'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[not(@class)]">
  <xsl:copy>
    <xsl:copy-of select="@*"/>
    <xsl:apply-templates/>
  </xsl:copy>
</xsl:template>
  -->

</xsl:stylesheet>
