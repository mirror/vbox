<?xml version="1.0"?>
<!--
    usage-to-docbook-manual.xsl:
        XSLT stylesheet that generates docbook command usage xml.

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
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->



<!-- - - - - - - - - - - - - - - - - - - - - - -
  base operation is to copy.
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="node()|@*">
  <xsl:copy>
     <xsl:apply-templates select="node()|@*"/>
  </xsl:copy>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  deal with non-docbook elements.
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="brief">
  <!-- strip this element -->
</xsl:template>


</xsl:stylesheet>

