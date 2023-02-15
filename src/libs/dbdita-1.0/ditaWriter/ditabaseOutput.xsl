<?xml version="1.0" encoding="utf-8" standalone="no"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:gsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

   <xsl:import href="uiDomainOutput.xsl"/>

   <xsl:import href="softwareDomainOutput.xsl"/>

   <xsl:import href="highlightDomainOutput.xsl"/>

   <xsl:import href="programmingDomainOutput.xsl"/>

   <xsl:import href="utilitiesDomainOutput.xsl"/>

   <xsl:import href="topicOutput.xsl"/>

   <xsl:import href="conceptOutput.xsl"/>

   <xsl:import href="taskOutput.xsl"/>

   <xsl:import href="referenceOutput.xsl"/>


   <xsl:param name="REPORT_UNMATCHED" select="'yes'"/>
   <xsl:param name="PROCESS_TEXT" select="'yes'"/>
   <xsl:param name="PROCESS_CHILDREN" select="'yes'"/>

   <xsl:template match="/">
      <xsl:apply-templates select="." mode="topic.topic.in"/>
   </xsl:template>

</xsl:stylesheet>
