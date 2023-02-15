<?xml version="1.0" ?>
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

<xsl:import href="../ditaWriter/ditabaseOutput.xsl"/>

<xsl:import href="dbReader.xsl"/>

<xsl:variable name="include-prolog" select="false()"/>
<xsl:variable name="include-outputclass" select="false()"/>
<xsl:variable name="replace-cmdname-with-userinput" select="true()"/>

<xsl:output
    method="xml"
    indent="yes"
    omit-xml-declaration="no"
    standalone="no"/>


<xsl:output
    encoding='UTF-8'
    doctype-public="-//OASIS//DTD DITA Reference//EN"
    doctype-system="reference.dtd"/>

<xsl:template match="/">
    <xsl:apply-templates select="." mode="topic.topic.in"/>
</xsl:template>

</xsl:stylesheet>
