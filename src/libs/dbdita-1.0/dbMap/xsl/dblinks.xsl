<?xml version="1.0" encoding="UTF-8"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!-- (c) Copyright IBM Corp. 2006 All Rights Reserved. -->

<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml">

<xsl:output
    method="xml"
    encoding="UTF-8"
    indent="no"
    doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"
    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"/>

<xsl:param name="ulink.target" select="'contentwin'"/>

<!-- strings should be externalized for localization, of course -->

<xsl:template match="simplelist[@role='navigationlinks']">
  <xsl:apply-imports/>
</xsl:template>

<xsl:template match="member[@role='parent']">
  <div class="rellinktitle" style="display: block; font-weight: bold;">
    <xsl:text>Parent:</xsl:text>
  </div>
  <div class="rellink" style="display: block; padding-left: 4em;">
    <xsl:apply-imports/>
  </div>
</xsl:template>

<xsl:template match="member[@role='preceding']">
  <div class="rellinktitle" style="display: block; font-weight: bold;">
    <xsl:text>Previous:</xsl:text>
  </div>
  <div class="rellink" style="display: block; padding-left: 4em;">
    <xsl:apply-imports/>
  </div>
</xsl:template>

<xsl:template match="member[@role='following']">
  <div class="rellinktitle" style="display: block; font-weight: bold;">
    <xsl:text>Next:</xsl:text>
  </div>
  <div class="rellink" style="display: block; padding-left: 4em;">
    <xsl:apply-imports/>
  </div>
</xsl:template>

<xsl:template match="member[@role='child']">
  <xsl:if test="not(preceding-sibling::member[@role='child'])">
    <div class="rellinktitle" style="display: block; font-weight: bold;">
      <xsl:text>Children:</xsl:text>
    </div>
  </xsl:if>
  <div class="rellink" style="display: block; padding-left: 4em;">
    <xsl:apply-imports/>
  </div>
</xsl:template>

<xsl:template match="simplelist[@role='relatedlinks']">
  <div class="rellinktitle" style="display: block; font-weight: bold;">
    <xsl:text>Related information:</xsl:text>
  </div>
  <xsl:apply-imports/>
</xsl:template>

<xsl:template match="member[@role='related']">
  <div class="rellink" style="display: block; padding-left: 4em;">
    <xsl:apply-imports/>
  </div>
</xsl:template>

</xsl:stylesheet>
