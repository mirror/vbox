<?xml version="1.0"?>

<!--
 *  A template to generate a header that will contain some important constraints
 *  extracted from the VirtualBox XML Schema (VirtualBox-settings-*.xsd).
 *  The output file name must be SchemaDefs.h.
 *
 *  This template depends on XML Schema structure (type names and constraints)
 *  and should be reviewed on every Schema change.

     Copyright (C) 2006-2007 innotek GmbH
    
     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
>
<xsl:output method="text"/>

<xsl:strip-space elements="*"/>

<!--
//  helpers
////////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  Extract the specified value and assign it to an enum member with the given
 *  name
-->
<xsl:template name="defineEnumMember">
    <xsl:param name="member"/>
    <xsl:param name="select"/>
    <xsl:if test="$select">
      <xsl:value-of select="concat($member, ' = ', $select, ',&#x0A;')"/>
    </xsl:if>
</xsl:template>

<!--
//  templates
////////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  shut down all implicit templates
-->
<xsl:template match="*"/>

<!--
 *  header
-->
<xsl:template match="/">
<xsl:text>
/*
 *  DO NOT EDIT.
 *
 *  This header is automatically generated from the VirtualBox XML Schema
 *  and contains selected schema constraints defined in C.
 */

#ifndef ____H_SCHEMADEFS
#define ____H_SCHEMADEFS

struct SchemaDefs
{
    enum
    {
</xsl:text>

  <xsl:apply-templates select="xsd:schema"/>

<xsl:text>    };
};

#endif // ____H_SCHEMADEFS
</xsl:text>
</xsl:template>

<!--
 *  extract schema definitions
-->
<xsl:template match="xsd:schema">

  <!-- process include statements -->
  <xsl:for-each select="xsd:include">
    <xsl:apply-templates select="document(@schemaLocation)/xsd:schema"/>
  </xsl:for-each>

  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MinGuestRAM'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TMemory']/xsd:attribute[@name='RAMSize']//xsd:minInclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MaxGuestRAM'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TMemory']/xsd:attribute[@name='RAMSize']//xsd:maxInclusive/@value
      "/>
  </xsl:call-template>

  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MinGuestVRAM'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TDisplay']/xsd:attribute[@name='VRAMSize']//xsd:minInclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MaxGuestVRAM'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TDisplay']/xsd:attribute[@name='VRAMSize']//xsd:maxInclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MaxGuestMonitors'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TDisplay']/xsd:attribute[@name='MonitorCount']//xsd:maxInclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        NetworkAdapterCount'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TNetworkAdapter']/xsd:attribute[@name='slot']//xsd:maxExclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        SerialPortCount'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TUartPort']/xsd:attribute[@name='slot']//xsd:maxExclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        ParallelPortCount'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TLptPort']/xsd:attribute[@name='slot']//xsd:maxExclusive/@value
      "/>
  </xsl:call-template>
  <xsl:call-template name="defineEnumMember">
      <xsl:with-param name="member" select="'        MaxBootPosition'"/>
      <xsl:with-param name="select" select="
        xsd:complexType[@name='TBoot']//xsd:element[@name='Order']//xsd:attribute[@name='position']//xsd:maxInclusive/@value
      "/>
  </xsl:call-template>

</xsl:template>

</xsl:stylesheet>
