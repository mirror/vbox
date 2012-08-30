<?xml version="1.0"?>

<!--
    xidl2docbook.xsl:
        XSLT stylesheet that generates docbook from
        VirtualBox.xidl.

    Copyright (C) 2006-2008 Oracle Corporation

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
  xmlns:exsl="http://exslt.org/common"
  extension-element-prefixes="exsl">

  <xsl:output
             method="xml"
             version="1.0"
             encoding="utf-8"
             indent="yes"/>

  <xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'glue-jaxws.xsl'" />

<!-- collect all interfaces with "wsmap='suppress'" in a global variable for
     quick lookup -->
<xsl:variable name="G_setSuppressedInterfaces"
              select="//interface[@wsmap='suppress']" />

<xsl:template name="makeLinkId">
  <xsl:param name="ifname" />
  <xsl:param name="member" />
  <xsl:value-of select="concat($ifname, '__', $member)"/>
</xsl:template>

<xsl:template name="emitType">
  <xsl:param name="type" />
  <xsl:choose>
    <xsl:when test="$type">
      <xsl:choose>
        <xsl:when test="//interface[@name=$type]">
          <xref>
            <xsl:attribute name="apiref">yes</xsl:attribute>
            <xsl:attribute name="linkend">
              <xsl:value-of select="translate($type, ':', '_')" />
            </xsl:attribute>
            <xsl:value-of select="$type" />
          </xref>
        </xsl:when>
        <xsl:when test="//enum[@name=$type]">
          <xref>
            <xsl:attribute name="apiref">yes</xsl:attribute>
            <xsl:attribute name="linkend">
              <xsl:value-of select="translate($type, ':', '_')" />
            </xsl:attribute>
            <xsl:value-of select="$type" />
          </xref>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$type" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="'void'" />
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="isWebserviceOnly">
  <xsl:for-each select="ancestor-or-self::*">
    <xsl:if test="(name()='if') and (@target='wsdl')">
      <xsl:text>yes</xsl:text>
    </xsl:if>
  </xsl:for-each>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  root match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="/idl">
  <chapter>
    <title id="sdkref_classes">Classes (interfaces)</title>
    <xsl:for-each select="//interface">
      <xsl:sort select="@name"/>

      <!-- ignore those interfaces within module sections; they don't have uuid -->
      <xsl:if test="@uuid">
        <xsl:variable name="ifname" select="@name" />
        <xsl:variable name="wsmap" select="@wsmap" />
        <xsl:variable name="wscpp" select="@wscpp" />
        <xsl:variable name="wsonly"><xsl:call-template name="isWebserviceOnly" /></xsl:variable>
        <xsl:variable name="extends" select="@extends" />
        <xsl:variable name="reportExtends" select="not($extends='$unknown') and not($extends='$errorinfo')" />

        <sect1>
          <xsl:attribute name="id">
            <xsl:value-of select="$ifname" />
          </xsl:attribute>
          <title><xsl:value-of select="$ifname" />
              <xsl:if test="$reportExtends">
              <xsl:value-of select="concat(' (', @extends, ')')" />
            </xsl:if>
          </title>

          <xsl:choose>
            <xsl:when test="$wsmap='suppress'">
              <note>
                This interface is not supported in the web service.
              </note>
            </xsl:when>
            <xsl:when test="$wsmap='struct'">
              <note>With the web service, this interface is mapped to a structure. Attributes that return this interface will not return an object, but a complete structure
              containing the attributes listed below as structure members.</note>
            </xsl:when>
            <xsl:when test="$wsonly='yes'">
              <note>This interface is supported in the web service only, not in COM/XPCOM.</note>
            </xsl:when>
          </xsl:choose>

          <xsl:if test="$reportExtends">
            <note>
                This interface extends
                <xref>
                  <xsl:attribute name="apiref">yes</xsl:attribute>
                  <xsl:attribute name="linkend"><xsl:value-of select="$extends" /></xsl:attribute>
                  <xsl:value-of select="$extends" />
                </xref>
                and therefore supports all its methods and attributes as well.
            </note>
          </xsl:if>

          <xsl:apply-templates select="desc" />

          <xsl:if test="attribute">
            <sect2>
              <title>Attributes</title>
              <xsl:for-each select="attribute">
                <xsl:variable name="attrtype" select="@type" />
                <sect3>
                  <xsl:attribute name="id">
                    <xsl:call-template name="makeLinkId">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="member" select="@name" />
                    </xsl:call-template>
                  </xsl:attribute>
                  <title>
                    <xsl:choose>
                      <xsl:when test="@readonly='yes'">
                        <xsl:value-of select="concat(@name, ' (read-only)')" />
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:value-of select="concat(@name, ' (read/write)')" />
                      </xsl:otherwise>
                    </xsl:choose>
                  </title>
                  <programlisting>
                    <xsl:call-template name="emitType">
                      <xsl:with-param name="type" select="$attrtype" />
                    </xsl:call-template>
                    <xsl:value-of select="concat(' ', $ifname, '::', @name)" />
                    <xsl:if test="(@array='yes') or (@safearray='yes')">
                      <xsl:text>[]</xsl:text>
                    </xsl:if>
                  </programlisting>
                  <xsl:if test="( ($attrtype=($G_setSuppressedInterfaces/@name)) )">
                    <note>
                      This attribute is not supported in the web service.
                    </note>
                  </xsl:if>
                  <xsl:apply-templates select="desc" />
                </sect3>
              </xsl:for-each>
            </sect2>
          </xsl:if>

          <xsl:if test="method">
<!--             <sect2> -->
<!--               <title>Methods</title> -->
              <xsl:for-each select="method">
                <xsl:sort select="@name" />
                <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
                <sect2>
                  <xsl:attribute name="id">
                    <xsl:call-template name="makeLinkId">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="member" select="@name" />
                    </xsl:call-template>
                  </xsl:attribute>
                  <title>
                    <xsl:value-of select="@name" />
                  </title>
                  <xsl:if test="   (param[@type=($G_setSuppressedInterfaces/@name)])
                                or (param[@mod='ptr'])" >
                    <note>
                      This method is not supported in the web service.
                    </note>
                  </xsl:if>
                  <!-- make a set of all parameters with in and out direction -->
                  <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />
                  <programlisting>
                    <!--emit return type-->
                    <xsl:call-template name="emitType">
                      <xsl:with-param name="type" select="$returnidltype" />
                    </xsl:call-template>
                    <xsl:if test="(param[@dir='return']/@array='yes') or (param[@dir='return']/@safearray='yes')">
                      <xsl:text>[]</xsl:text>
                    </xsl:if>
                    <xsl:value-of select="concat(' ', $ifname, '::', @name, '(')" />
                    <xsl:if test="$paramsinout">
                      <xsl:for-each select="$paramsinout">
                        <xsl:text>&#10;</xsl:text>
                        <xsl:value-of select="concat('           [', @dir, '] ')" />
                        <xsl:if test="@mod = 'ptr'">
                          <xsl:text>[ptr] </xsl:text>
                        </xsl:if>
                        <xsl:call-template name="emitType">
                          <xsl:with-param name="type" select="@type" />
                        </xsl:call-template>
                        <emphasis role="bold">
                          <xsl:value-of select="concat(' ', @name)" />
                        </emphasis>
                        <xsl:if test="(@array='yes') or (@safearray='yes')">
                          <xsl:text>[]</xsl:text>
                        </xsl:if>
                        <xsl:if test="not(position()=last())">
                          <xsl:text>, </xsl:text>
                        </xsl:if>
                      </xsl:for-each>
                    </xsl:if>
                    <xsl:text>)</xsl:text>
                  </programlisting>

                  <xsl:if test="$paramsinout">
                    <glosslist>
                      <xsl:for-each select="$paramsinout">
                        <glossentry>
                          <glossterm>
                            <xsl:value-of select="@name" />
                          </glossterm>
                          <glossdef>
                            <para>
                              <xsl:apply-templates select="desc" />
                            </para>
                          </glossdef>
                        </glossentry>
                      </xsl:for-each>
                    </glosslist>
                  </xsl:if>

                  <!-- dump the description here -->
                  <xsl:apply-templates select="desc" />

                  <xsl:if test="desc/result">
                    <para>If this method fails, the following error codes may be reported:</para>
                    <itemizedlist>
                      <xsl:for-each select="desc/result">
                        <listitem>
                          <para><code><xsl:value-of select="@name" />: </code>
                            <xsl:apply-templates />
                          </para>
                        </listitem>
                      </xsl:for-each>
                    </itemizedlist>
                  </xsl:if>
                </sect2>
              </xsl:for-each>
<!--             </sect2> -->
          </xsl:if>

        </sect1>
      </xsl:if>
    </xsl:for-each>
  </chapter>

  <chapter>
    <title id="sdkref_enums">Enumerations (enums)</title>
    <xsl:for-each select="//enum">
      <xsl:sort select="@name"/>

      <xsl:variable name="ifname" select="@name" />
      <xsl:variable name="wsmap" select="@wsmap" />
      <xsl:variable name="wscpp" select="@wscpp" />

      <sect1>
        <xsl:attribute name="id">
          <xsl:value-of select="$ifname" />
        </xsl:attribute>
        <title><xsl:value-of select="$ifname" /></title>

        <xsl:apply-templates select="desc" />

        <glosslist>
          <xsl:for-each select="const">
            <glossentry>
              <glossterm>
                <xsl:attribute name="id">
                  <xsl:call-template name="makeLinkId">
                    <xsl:with-param name="ifname" select="$ifname" />
                    <xsl:with-param name="member" select="@name" />
                  </xsl:call-template>
                </xsl:attribute>
                <xsl:value-of select="@name" />
              </glossterm>
              <glossdef>
                <xsl:apply-templates select="desc" />
              </glossdef>
            </glossentry>
          </xsl:for-each>
        </glosslist>
      </sect1>
    </xsl:for-each>
  </chapter>

</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  if
 - - - - - - - - - - - - - - - - - - - - - - -->

<!--
 *  ignore all |if|s except those for WSDL target
-->
<xsl:template match="if">
    <xsl:if test="@target='wsdl'">
        <xsl:apply-templates/>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  cpp
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="cpp">
<!--  ignore this -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
     result
     - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="result">
  <!--  ignore this, we handle them explicitly in method loops -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  library
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="library">
  <xsl:apply-templates />
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  class
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="module/class">
<!--  TODO swallow for now -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  enum
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="enum">
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  const
 - - - - - - - - - - - - - - - - - - - - - - -->

<!--
<xsl:template match="const">
  <xsl:apply-templates />
</xsl:template>
-->

<!-- - - - - - - - - - - - - - - - - - - - - - -
  desc
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="desc">
  <xsl:apply-templates />
</xsl:template>

<xsl:template name="getCurrentInterface">
  <xsl:for-each select="ancestor-or-self::*">
    <xsl:if test="name()='interface'">
      <xsl:value-of select="@name"/>
    </xsl:if>
  </xsl:for-each>
</xsl:template>

<!-- <link to="DeviceType::HardDisk"/> -->
<xsl:template match="link">
  <xref>
    <xsl:attribute name="apiref">yes</xsl:attribute>
    <xsl:variable name="tmp" select="@to" />
    <xsl:variable name="enumNameFromCombinedName">
        <xsl:value-of select="substring-before($tmp, '_')" />
    </xsl:variable>
    <xsl:variable name="enumValueFromCombinedName">
        <xsl:value-of select="substring-after($tmp, '_')" />
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="//interface[@name=$tmp] or //enum[@name=$tmp]"><!-- link to interface only -->
        <xsl:attribute name="linkend"><xsl:value-of select="@to" /></xsl:attribute>
        <xsl:value-of select="$tmp" />
      </xsl:when>
      <xsl:when test="//enum[@name=$enumNameFromCombinedName]">
        <xsl:attribute name="linkend">
          <xsl:value-of select="concat($enumNameFromCombinedName, '__', $enumValueFromCombinedName)" />
        </xsl:attribute>
        <xsl:value-of select="$enumValueFromCombinedName" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="currentif">
          <xsl:call-template name="getCurrentInterface" />
        </xsl:variable>
        <xsl:variable name="if"><!-- interface -->
          <xsl:choose>
            <xsl:when test="contains(@to, '#')">
              <xsl:value-of select="$currentif" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="substring-before(@to, '::')" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:variable name="member"><!-- member in that interface -->
          <xsl:choose>
            <xsl:when test="contains(@to, '#')">
              <xsl:value-of select="substring-after(@to, '#')" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="substring-after(@to, '::')" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>

        <xsl:attribute name="linkend"><xsl:value-of select="concat($if, '__', $member)" /></xsl:attribute>
        <xsl:variable name="autotextsuffix">
          <xsl:choose>
            <!-- if link points to a method, append "()" -->
            <xsl:when test="//interface[@name=$if]/method[@name=$member]">
              <xsl:value-of select="'()'" />
            </xsl:when>
            <!-- if link points to a safearray attribute, append "[]" -->
            <xsl:when test="//interface[@name=$if]/attribute[@name=$member]/@safearray = 'yes'">
              <xsl:value-of select="'[]'" />
            </xsl:when>
            <xsl:when test="//interface[@name=$if]/attribute[@name=$member]">
            </xsl:when>
            <xsl:when test="//enum[@name=$if]/const[@name=$member]">
            </xsl:when>
            <xsl:when test="//result[@name=$tmp]">
            </xsl:when>
            <xsl:otherwise>
              <xsl:message terminate="yes">
                <xsl:value-of select="concat('Invalid link pointing to &quot;', $tmp, '&quot;')" />
              </xsl:message>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="./text()"><!-- link text given in source -->
            <xsl:apply-templates />
          </xsl:when>
          <xsl:when test="$if=$currentif"><!-- "near" link to method or attribute in current interface -->
            <xsl:value-of select="concat($member, $autotextsuffix)" />
          </xsl:when>
          <xsl:otherwise><!-- "far" link to other method or attribute -->
            <xsl:value-of select="concat($if, '::', $member, $autotextsuffix)" />
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xref>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  note
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="note">
  <xsl:if test="not(@internal='yes')">
    <note>
      <xsl:apply-templates />
    </note>
  </xsl:if>
</xsl:template>

<xsl:template match="tt">
  <computeroutput>
    <xsl:apply-templates />
  </computeroutput>
</xsl:template>

<xsl:template match="b">
  <emphasis role="bold">
    <xsl:apply-templates />
  </emphasis>
</xsl:template>

<xsl:template match="i">
  <emphasis>
    <xsl:apply-templates />
  </emphasis>
</xsl:template>

<xsl:template match="see">
  <xsl:text>See also: </xsl:text>
  <xsl:apply-templates />
</xsl:template>

<xsl:template match="ul">
  <itemizedlist>
    <xsl:apply-templates />
  </itemizedlist>
</xsl:template>

<xsl:template match="ol">
  <orderedlist>
    <xsl:apply-templates />
  </orderedlist>
</xsl:template>

<xsl:template match="li">
  <listitem>
    <xsl:apply-templates />
  </listitem>
</xsl:template>

<xsl:template match="h3">
  <emphasis role="bold">
    <xsl:apply-templates />
  </emphasis>
</xsl:template>

<xsl:template match="pre">
  <screen><xsl:apply-templates /></screen>
</xsl:template>

<xsl:template match="table">
  <xsl:apply-templates /> <!-- todo -->
</xsl:template>

</xsl:stylesheet>
