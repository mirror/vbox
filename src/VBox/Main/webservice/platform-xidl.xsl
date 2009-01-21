<?xml version="1.0"?>

<!--

    platform-xidl.xsl:
        XSLT stylesheet that generates a platform-specific
        VirtualBox.xidl from ../idl/VirtualBox.xidl, which
        is identical to the original except that all <if...>
        sections are resolved (for easier processing).

     Copyright (C) 2006-2008 Sun Microsystems, Inc.

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

     Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
     Clara, CA 95054 USA or visit http://www.sun.com if you need
     additional information or have any questions.
-->

<xsl:stylesheet
  version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output
  method="xml"
  version="1.0"
  encoding="utf-8"
  indent="yes"/>

<xsl:strip-space
  elements="*" />

<!--
    template for "idl" match; this emits the header of the target file
    and recurses into the librarys with interfaces (which are matched below)
    -->
<xsl:template match="/idl">
  <xsl:comment>
  DO NOT EDIT! This is a generated file.
  Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
  Generator: src/VBox/Main/webservice/platform-xidl.xsl
</xsl:comment>

  <idl>
    <xsl:apply-templates />
  </idl>

</xsl:template>

<!--
    template for "if" match: ignore all ifs except those for wsdl
    -->
<xsl:template match="if">
  <xsl:if test="@target='wsdl'">
    <xsl:apply-templates/>
  </xsl:if>
</xsl:template>

<!--
    ignore everything we don't need
    -->
<xsl:template match="cpp|class|enumerator|desc|note">
</xsl:template>

<!--
    and keep the rest intact (including all attributes)
    -->
<xsl:template match="library|module|enum|const|interface|attribute|collection|method|param">
  <xsl:copy><xsl:copy-of select="@*"/><xsl:apply-templates/></xsl:copy>
</xsl:template>


</xsl:stylesheet>
