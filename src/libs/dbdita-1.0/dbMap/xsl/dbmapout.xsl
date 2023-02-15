<?xml version="1.0" encoding="UTF-8"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!-- (c) Copyright IBM Corp. 2006 All Rights Reserved. -->

<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:ditaarch="http://dita.oasis-open.org/architecture/2005/"
                exclude-result-prefixes="ditaarch">

<xsl:output method="xml"
            encoding="utf-8"
            indent="no"
            doctype-public="-//OASIS//DTD DITA Map//EN"
            doctype-system="map.dtd"
/>

<xsl:param name="DBINEXT"     select="'.xml'"/>
<xsl:param name="DBOUTEXT"    select="'.html'"/>
<xsl:param name="DBOUTFORMAT" select="'html'"/>
<xsl:param name="BUILDINFIX"  select="'_GENERATED'"/>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - rewrite map to point at updated DocBook files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="/">
  <xsl:apply-templates/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - rewrite DocBook references to point at output files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      @format='docbook']">
  <topicref>
    <xsl:attribute name="href">
      <xsl:value-of select="concat(
          substring(@href, 1, string-length(@href) - string-length($DBINEXT)),
          $BUILDINFIX,
          $DBOUTEXT)"/>
    </xsl:attribute>
    <xsl:if test="$DBOUTFORMAT and
          string-length(normalize-space($DBOUTFORMAT)) &gt; 0">
      <xsl:attribute name="format">
        <xsl:value-of select="normalize-space($DBOUTFORMAT)"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:apply-templates select="@navtitle|@id"/>
    <xsl:apply-templates select="*|text()|comment()|processing-instruction()"/>
  </topicref>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - default copy actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="*|text()|comment()|processing-instruction()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="@*|text()|comment()|processing-instruction()">
  <xsl:copy/>
</xsl:template>

<!-- suppress copy for defaulted attributes -->
<xsl:template match="@class|@ditaarch:*|@domains"/>


</xsl:stylesheet>
