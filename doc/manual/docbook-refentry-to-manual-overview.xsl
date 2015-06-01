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
  >

  <xsl:output method="xml" version="1.0" encoding="utf-8" indent="yes"/>
  <xsl:strip-space elements="*"/>


<!-- Base operation is to copy. -->
<xsl:template match="node()|@*">
  <xsl:copy>
     <xsl:apply-templates select="node()|@*"/>
  </xsl:copy>
</xsl:template>

<!--
  The refentry element is the top level one.  We only process the
  refsynopsisdiv sub element within it, since that is all we want.
  -->
<xsl:template match="refentry">
  <xsl:apply-templates select="refsynopsisdiv"/>
</xsl:template>

<!--
  Translate the refsynopsisdiv into a refsect1. There is special handling
  of this in the HTML CSS and in the latex conversion XSLT.
  -->
<xsl:template match="refsynopsisdiv">
  <refsect1>
     <xsl:apply-templates select="node()|@*"/>
  </refsect1>
</xsl:template>

<!-- Remove all remarks (for now). -->
<xsl:template match="remark"/>


</xsl:stylesheet>

