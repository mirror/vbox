<?xml version="1.0" encoding="UTF-8"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!-- (c) Copyright IBM Corp. 2006 All Rights Reserved. -->

<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:ditaarch="http://dita.oasis-open.org/architecture/2005/">

<xsl:output method="text"
            encoding="utf-8"
/>

<xsl:param name="DBINEXT"     select="'.xml'"/>
<xsl:param name="DBOUTEXT"    select="'.html'"/>
<xsl:param name="BUILDINFIX"  select="'_GENERATED'"/>


<!-- key on references to DocBook content -->
<xsl:key name="docbookref"
  match="*[contains(@class,' map/topicref ') and @href and @format='docbook']"
  use="@href"/>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - process map to list DocBook files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="/">
  <xsl:apply-templates select=".//*[contains(@class,' map/topicref ') and
      @href and @format='docbook' and
      generate-id(.)=generate-id(key('docbookref',@href)[1])]"
    mode="docbookref"/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - list DocBook files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      @format='docbook']" mode="docbookref">
  <xsl:choose>
  <xsl:when test="$DBOUTEXT and
        string-length(normalize-space($DBOUTEXT)) &gt; 0">
    <xsl:value-of select="substring(@href, 1,
              string-length(@href) - string-length($DBINEXT))"/>
    <xsl:value-of select="$BUILDINFIX"/>
    <xsl:value-of select="$DBOUTEXT"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:value-of select="@href"/>
  </xsl:otherwise>
  </xsl:choose>
  <xsl:text>
</xsl:text>
</xsl:template>

</xsl:stylesheet>
