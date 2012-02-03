<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:vbox="http://www.virtualbox.org/"
     xmlns:exsl="http://exslt.org/common"
     extension-element-prefixes="exsl">

<!--

    glue-java.xsl:
        XSLT stylesheet that generates Java glue code for XPCOM, MSCOM and JAX-WS from
        VirtualBox.xidl.

     Copyright (C) 2010-2011 Oracle Corporation

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:output
  method="text"
  version="1.0"
  encoding="utf-8"
  indent="no"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'glue-java.xsl'" />
<xsl:variable name="G_virtualBoxPackage" select="concat('org.virtualbox',$G_vboxApiSuffix)" />
<xsl:variable name="G_virtualBoxPackageCom" select="concat('org.virtualbox',$G_vboxApiSuffix,'.',$G_vboxGlueStyle)" />
<xsl:variable name="G_virtualBoxWsdl" select="concat(concat('&quot;vboxwebService',$G_vboxApiSuffix), '.wsdl&quot;')" />
<!-- collect all interfaces with "wsmap='suppress'" in a global variable for
     quick lookup -->
<xsl:variable name="G_setSuppressedInterfaces"
              select="//interface[@wsmap='suppress']" />

<xsl:include href="../webservice/websrv-shared.inc.xsl" />

<xsl:strip-space elements="*"/>

<xsl:template name="fileheader">
  <xsl:param name="name" />
  <xsl:text>/*
 *  Copyright (C) 2010-2011 Oracle Corporation
 *
 *  This file is part of the VirtualBox SDK, as available from
 *  http://www.virtualbox.org.  This library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation, in version 2.1
 *  as it comes in the "COPYING.LIB" file of the VirtualBox SDK distribution.
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *
</xsl:text>
  <xsl:value-of select="concat(' * ',$name)"/>
<xsl:text>
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
 * Generator: src/VBox/Main/glue/glue-java.xsl
 */

</xsl:text>
</xsl:template>

<xsl:template name="startFile">
  <xsl:param name="file" />
  <xsl:param name="package" />

  <xsl:value-of select="concat('&#10;// ##### BEGINFILE &quot;', $G_vboxDirPrefix, $file, '&quot;&#10;&#10;')" />
  <xsl:call-template name="fileheader">
    <xsl:with-param name="name" select="$file" />
  </xsl:call-template>

  <xsl:value-of select="concat('package ',$package,';&#10;&#10;')" />
  <xsl:value-of select="concat('import ',$G_virtualBoxPackageCom,'.*;&#10;')" />

  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
      <xsl:value-of select="'import org.mozilla.interfaces.*;&#10;'" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:value-of select="'import com.jacob.com.*;&#10;'" />
      <xsl:value-of select="'import com.jacob.activeX.ActiveXComponent;&#10;'" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <xsl:value-of select="'import javax.xml.ws.*;&#10;'" />
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'no header rule (startFile)'" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="endFile">
 <xsl:param name="file" />
 <xsl:value-of select="concat('&#10;// ##### ENDFILE &quot;', $file, '&quot;&#10;&#10;')" />
</xsl:template>


<xsl:template name="string-replace">
  <xsl:param name="haystack"/>
  <xsl:param name="needle"/>
  <xsl:param name="replacement"/>
  <xsl:choose>
    <xsl:when test="contains($haystack,$needle)">
      <xsl:value-of select="substring-before($haystack,$needle)"/>
      <xsl:value-of select="$replacement"/>
      <xsl:call-template name="string-replace">
        <xsl:with-param name="haystack" select="substring-after($haystack,$needle)"/>
        <xsl:with-param name="needle" select="$needle"/>
        <xsl:with-param name="replacement" select="$replacement"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$haystack"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- descriptions -->

<xsl:template match="*/text()">
  <!-- TODO: strip out @c/@a for now. long term solution is changing that to a
       tag in the xidl file, and translate it when generating doxygen etc. -->
  <xsl:variable name="rep1">
    <xsl:call-template name="string-replace">
      <xsl:with-param name="haystack" select="."/>
      <xsl:with-param name="needle" select="'@c'"/>
      <xsl:with-param name="replacement" select="''"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="rep2">
    <xsl:call-template name="string-replace">
      <xsl:with-param name="haystack" select="$rep1"/>
      <xsl:with-param name="needle" select="'@a'"/>
      <xsl:with-param name="replacement" select="''"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="rep3">
    <xsl:call-template name="string-replace">
      <xsl:with-param name="haystack" select="$rep2"/>
      <xsl:with-param name="needle" select="'@todo'"/>
      <xsl:with-param name="replacement" select="'TODO'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:value-of select="$rep3"/>
</xsl:template>

<!--
 *  all sub-elements that are not explicitly matched are considered to be
 *  html tags and copied w/o modifications
-->
<xsl:template match="desc//*">
  <xsl:variable name="tagname" select="local-name()"/>
  <xsl:value-of select="concat('&lt;',$tagname,'&gt;')"/>
  <xsl:apply-templates/>
  <xsl:value-of select="concat('&lt;/',$tagname,'&gt;')"/>
</xsl:template>

<xsl:template name="emit_refsig">
  <xsl:param name="context"/>
  <xsl:param name="identifier"/>

  <xsl:choose>
    <xsl:when test="//enum[@name=$context]/const[@name=$identifier]">
      <xsl:value-of select="$identifier"/>
    </xsl:when>
    <xsl:when test="//interface[@name=$context]/method[@name=$identifier]">
      <xsl:value-of select="$identifier"/>
      <xsl:text>(</xsl:text>
      <xsl:for-each select="//interface[@name=$context]/method[@name=$identifier]/param">
        <xsl:if test="@dir!='return'">
          <xsl:if test="position() > 1">
            <xsl:text>,</xsl:text>
          </xsl:if>
          <xsl:choose>
            <xsl:when test="@dir='out'">
              <xsl:text>Holder</xsl:text>
            </xsl:when>
            <xsl:otherwise>
              <xsl:call-template name="typeIdl2Glue">
                <xsl:with-param name="type" select="@type"/>
                <xsl:with-param name="safearray" select="@safearray"/>
                <xsl:with-param name="skiplisttype" select="'yes'"/>
              </xsl:call-template>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:if>
      </xsl:for-each>
      <xsl:text>)</xsl:text>
    </xsl:when>
    <xsl:when test="//interface[@name=$context]/attribute[@name=$identifier]">
      <xsl:call-template name="makeGetterName">
        <xsl:with-param name="attrname" select="$identifier" />
      </xsl:call-template>
      <xsl:text>()</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('unknown reference destination in @see/@link: context=',$context,' identifier=',$identifier)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!--
 *  link
-->
<xsl:template match="desc//link">
  <xsl:text>{@link </xsl:text>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="link" mode="middle">
  <xsl:variable name="linktext">
    <xsl:value-of select="translate(@to,'_','#')"/>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test="substring($linktext,1,1)='#'">
      <xsl:variable name="context">
        <xsl:choose>
          <xsl:when test="local-name(../..)='interface' or local-name(../..)='enum'">
            <xsl:value-of select="../../@name"/>
          </xsl:when>
          <xsl:when test="local-name(../../..)='interface' or local-name(../../..)='enum'">
            <xsl:value-of select="../../../@name"/>
          </xsl:when>
          <xsl:when test="local-name(../../../..)='interface' or local-name(../../../..)='enum'">
            <xsl:value-of select="../../../../@name"/>
          </xsl:when>
          <xsl:when test="local-name(../../../../..)='interface' or local-name(../../../../..)='enum'">
            <xsl:value-of select="../../../../../@name"/>
          </xsl:when>
          <xsl:when test="local-name(../../../../../..)='interface' or local-name(../../../../../..)='enum'">
            <xsl:value-of select="../../../../../../@name"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="fatalError">
              <xsl:with-param name="msg" select="concat('cannot determine context for identifier ',$linktext)" />
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:variable name="linkname">
        <xsl:value-of select="substring($linktext,2)"/>
      </xsl:variable>
      <xsl:text>#</xsl:text>
      <xsl:call-template name="emit_refsig">
        <xsl:with-param name="context" select="$context"/>
        <xsl:with-param name="identifier" select="$linkname"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($linktext,'::')">
      <xsl:variable name="context">
        <xsl:value-of select="substring-before($linktext,'::')"/>
      </xsl:variable>
      <xsl:variable name="linkname">
        <xsl:value-of select="substring-after($linktext,'::')"/>
      </xsl:variable>
      <xsl:value-of select="concat($G_virtualBoxPackage,'.',$context,'#')"/>
      <xsl:call-template name="emit_refsig">
        <xsl:with-param name="context" select="$context"/>
        <xsl:with-param name="identifier" select="$linkname"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="concat($G_virtualBoxPackage,'.',$linktext)"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>
<!--
 *  note
-->
<xsl:template match="desc/note">
  <xsl:if test="not(@internal='yes')">
    <xsl:text>&#10;NOTE: </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#10;</xsl:text>
  </xsl:if>
</xsl:template>

<!--
 *  see
-->
<xsl:template match="desc/see">
  <!-- TODO: quirk in our xidl file: only one <see> tag with <link> nested
       into it, translate this to multiple @see lines and strip the rest.
       Should be replaced in the xidl by multiple <see> without nested tag  -->
  <xsl:text>&#10;</xsl:text>
  <xsl:apply-templates match="link"/>
</xsl:template>

<xsl:template match="desc/see/text()"/>

<xsl:template match="desc/see/link">
  <xsl:text>@see </xsl:text>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<!--
 *  common comment prologue (handles group IDs)
-->
<xsl:template match="desc" mode="begin">
  <xsl:param name="id" select="@group | preceding::descGroup[1]/@id"/>
  <xsl:text>/**&#10;</xsl:text>
  <xsl:if test="$id">
    <xsl:value-of select="concat(' @ingroup ',$id,'&#10;')"/>
  </xsl:if>
</xsl:template>

<!--
 *  common middle part of the comment block
-->
<xsl:template match="desc" mode="middle">
  <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
  <xsl:apply-templates select="note"/>
  <xsl:apply-templates select="see"/>
</xsl:template>

<!--
 *  result part of the comment block
-->
<xsl:template match="desc" mode="results">
  <xsl:if test="result">
    <xsl:text>&#10;Expected result codes:&#10;</xsl:text>
    <xsl:text>&lt;table&gt;&#10;</xsl:text>
    <xsl:for-each select="result">
      <xsl:text>&lt;tr&gt;</xsl:text>
      <xsl:choose>
        <xsl:when test="ancestor::library/result[@name=current()/@name]">
          <xsl:value-of select="concat('&lt;td&gt;@link ::',@name,' ',@name,'&lt;/td&gt;')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('&lt;td&gt;',@name,'&lt;/td&gt;')"/>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:text>&lt;td&gt;</xsl:text>
      <xsl:apply-templates select="text() | *[not(self::note or self::see or
                                                  self::result)]"/>
      <xsl:text>&lt;/td&gt;&lt;tr&gt;&#10;</xsl:text>
    </xsl:for-each>
    <xsl:text>&lt;/table&gt;&#10;</xsl:text>
  </xsl:if>
</xsl:template>

<!--
 *  translates the string to uppercase
-->
<xsl:template name="uppercase">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
    translate($str,'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')
  "/>
</xsl:template>

<!--
 *  comment for interfaces
-->
<xsl:template match="desc" mode="interface">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#10;Interface ID: &lt;tt&gt;{</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="../@uuid"/>
  </xsl:call-template>
  <xsl:text>}&lt;/tt&gt;&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  comment for attribute getters
-->
<xsl:template match="desc" mode="attribute_get">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="text() | *[not(self::note or self::see or self::result)]"/>
  <xsl:apply-templates select="." mode="results"/>
  <xsl:apply-templates select="note"/>
  <xsl:text>&#10;@return </xsl:text>
  <xsl:call-template name="typeIdl2Glue">
    <xsl:with-param name="type" select="../@type"/>
    <xsl:with-param name="safearray" select="../@safearray"/>
  </xsl:call-template>
  <xsl:text>&#10;</xsl:text>
  <xsl:apply-templates select="see"/>
  <xsl:text>&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  comment for attribute setters
-->
<xsl:template match="desc" mode="attribute_set">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="text() | *[not(self::note or self::see or self::result)]"/>
  <xsl:apply-templates select="." mode="results"/>
  <xsl:apply-templates select="note"/>
  <xsl:text>&#10;@param value </xsl:text>
  <xsl:call-template name="typeIdl2Glue">
    <xsl:with-param name="type" select="../@type"/>
    <xsl:with-param name="safearray" select="../@safearray"/>
  </xsl:call-template>
  <xsl:text>&#10;</xsl:text>
  <xsl:apply-templates select="see"/>
  <xsl:text>&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  comment for methods
-->
<xsl:template match="desc" mode="method">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="text() | *[not(self::note or self::see or self::result)]"/>
  <xsl:for-each select="../param">
    <xsl:apply-templates select="desc"/>
  </xsl:for-each>
  <xsl:apply-templates select="." mode="results"/>
  <xsl:apply-templates select="note"/>
  <xsl:apply-templates select="../param/desc/note"/>
  <xsl:apply-templates select="see"/>
  <xsl:text>&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  comment for method parameters
-->
<xsl:template match="method/param/desc">
  <xsl:if test="text() | *[not(self::note or self::see)]">
    <xsl:choose>
      <xsl:when test="../@dir='return'">
        <xsl:text>&#10;@return </xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>&#10;@param </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> </xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:if>
</xsl:template>

<!--
 *  comment for enums
-->
<xsl:template match="desc" mode="enum">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#10;Interface ID: &lt;tt&gt;{</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="../@uuid"/>
  </xsl:call-template>
  <xsl:text>}&lt;/tt&gt;&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  comment for enum values
-->
<xsl:template match="desc" mode="enum_const">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#10;*/&#10;</xsl:text>
</xsl:template>

<!--
 *  ignore descGroups by default (processed in /idl)
-->
<xsl:template match="descGroup"/>



<!-- actual code generation -->

<xsl:template name="genEnum">
  <xsl:param name="enumname" />
  <xsl:param name="filename" />

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filename" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

  <xsl:apply-templates select="desc" mode="enum"/>
  <xsl:value-of select="concat('public enum ', $enumname, ' {&#10;&#10;')" />
  <xsl:for-each select="const">
    <xsl:apply-templates select="desc" mode="enum_const"/>
    <xsl:variable name="enumconst" select="@name" />
    <xsl:value-of select="concat('    ', $enumconst, '(', @value, ')')" />
    <xsl:choose>
      <xsl:when test="not(position()=last())">
        <xsl:text>,&#10;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>;&#10;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>

  <xsl:text>&#10;</xsl:text>
  <xsl:text>    private final int value;&#10;&#10;</xsl:text>

  <xsl:value-of select="concat('    ', $enumname, '(int v) {&#10;')" />
  <xsl:text>        value = v;&#10;</xsl:text>
  <xsl:text>    }&#10;&#10;</xsl:text>

  <xsl:text>    public int value() {&#10;</xsl:text>
  <xsl:text>        return value;&#10;</xsl:text>
  <xsl:text>    }&#10;&#10;</xsl:text>

  <xsl:value-of select="concat('    public static ', $enumname, ' fromValue(long v) {&#10;')" />
  <xsl:value-of select="concat('        for (', $enumname, ' c: ', $enumname, '.values()) {&#10;')" />
  <xsl:text>            if (c.value == (int)v) {&#10;</xsl:text>
  <xsl:text>                return c;&#10;</xsl:text>
  <xsl:text>            }&#10;</xsl:text>
  <xsl:text>        }&#10;</xsl:text>
  <xsl:text>        throw new IllegalArgumentException(Long.toString(v));&#10;</xsl:text>
  <xsl:text>    }&#10;&#10;</xsl:text>

  <xsl:value-of select="concat('    public static ', $enumname, ' fromValue(String v) {&#10;')" />
  <xsl:value-of select="concat('        return valueOf(',$enumname, '.class, v);&#10;')" />
  <xsl:value-of select="       '    }&#10;'" />

  <xsl:text>}&#10;&#10;</xsl:text>

  <xsl:call-template name="endFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

</xsl:template>

<xsl:template name="startExcWrapper">

  <xsl:value-of select="'      try {&#10;'" />

</xsl:template>

<xsl:template name="endExcWrapper">

  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
      <xsl:value-of select="'     } catch (org.mozilla.xpcom.XPCOMException e) {&#10;'" />
      <xsl:value-of select="'          throw new VBoxException(e, e.getMessage());&#10;'" />
      <xsl:value-of select="'     }&#10;'" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:value-of select="'     } catch (com.jacob.com.ComException e) {&#10;'" />
       <xsl:value-of select="'          throw new VBoxException(e, e.getMessage());&#10;'" />
      <xsl:value-of select="'     }&#10;'" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <xsl:value-of select="'     } catch (InvalidObjectFaultMsg e) {&#10;'" />
      <xsl:value-of select="'          throw new VBoxException(e, e.getMessage());&#10;'" />
      <xsl:value-of select="'     } catch (RuntimeFaultMsg e) {&#10;'" />
      <xsl:value-of select="'          throw new VBoxException(e, e.getMessage());&#10;'" />
      <xsl:value-of select="'     }&#10;'" />
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'no header rule (startFile)'" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="wrappedName">
  <xsl:param name="ifname" />

  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
      <xsl:value-of select="concat('org.mozilla.interfaces.',$ifname)" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:value-of select="'com.jacob.com.Dispatch'" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <xsl:value-of select="'String'" />
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'no wrapper naming rule defined (wrappedName)'" />
      </xsl:call-template>
    </xsl:otherwise>

  </xsl:choose>
</xsl:template>

<xsl:template name="fullClassName">
  <xsl:param name="name" />
  <xsl:param name="origname" />
  <xsl:param name="collPrefix" />
   <xsl:choose>
     <xsl:when test="//enum[@name=$name] or //enum[@name=$origname]">
       <xsl:value-of select="concat($G_virtualBoxPackage,  concat('.', $name))" />
     </xsl:when>
     <xsl:when test="//interface[@name=$name]">
       <xsl:value-of select="concat($G_virtualBoxPackage,  concat('.', $name))" />
     </xsl:when>
     <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('fullClassName: Type &quot;', $name, '&quot; is not supported.')" />
      </xsl:call-template>
     </xsl:otherwise>
   </xsl:choose>
</xsl:template>

<xsl:template name="typeIdl2Glue">
  <xsl:param name="type" />
  <xsl:param name="safearray" />
  <xsl:param name="forceelem" />
  <xsl:param name="skiplisttype" />

  <xsl:variable name="needarray" select="($safearray='yes') and not($forceelem='yes')" />
  <xsl:variable name="needlist" select="($needarray) and not($type='octet')" />

  <xsl:if test="($needlist)">
    <xsl:value-of select="'List'" />
    <xsl:if test="not($skiplisttype='yes')">
      <xsl:value-of select="'&lt;'" />
    </xsl:if>
  </xsl:if>

  <xsl:if test="not($needlist) or not($skiplisttype='yes')">
    <!-- look up Java type from IDL type from table array in websrv-shared.inc.xsl -->
    <xsl:variable name="javatypefield" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@javaname" />

    <xsl:choose>
      <xsl:when test="string-length($javatypefield)">
        <xsl:value-of select="$javatypefield" />
      </xsl:when>
      <!-- not a standard type: then it better be one of the types defined in the XIDL -->
      <xsl:when test="$type='$unknown'">IUnknown</xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="fullClassName">
          <xsl:with-param name="name" select="$type" />
          <xsl:with-param name="collPrefix" select="''"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:if>

  <xsl:choose>
    <xsl:when test="($needlist)">
      <xsl:if test="not($skiplisttype='yes')">
        <xsl:value-of select="'&gt;'" />
      </xsl:if>
    </xsl:when>
    <xsl:when test="($needarray)">
      <xsl:value-of select="'[]'" />
    </xsl:when>
  </xsl:choose>
</xsl:template>

<!--
    typeIdl2Back: converts $type into a type as used by the backend.
    -->
<xsl:template name="typeIdl2Back">
  <xsl:param name="type" />
  <xsl:param name="safearray" />
  <xsl:param name="forceelem" />

  <xsl:choose>
    <xsl:when test="($G_vboxGlueStyle='xpcom')">
      <xsl:variable name="needarray" select="($safearray='yes') and not($forceelem='yes')" />

      <xsl:choose>
        <xsl:when test="$type='long long'">
          <xsl:value-of select="'long'" />
        </xsl:when>

        <xsl:when test="$type='unsigned long'">
          <xsl:value-of select="'long'" />
        </xsl:when>

        <xsl:when test="$type='long'">
          <xsl:value-of select="'int'" />
        </xsl:when>

        <xsl:when test="$type='unsigned short'">
          <xsl:value-of select="'int'" />
        </xsl:when>

        <xsl:when test="$type='short'">
          <xsl:value-of select="'short'" />
        </xsl:when>

        <xsl:when test="$type='octet'">
          <xsl:value-of select="'byte'" />
        </xsl:when>

        <xsl:when test="$type='boolean'">
          <xsl:value-of select="'boolean'" />
        </xsl:when>

        <xsl:when test="$type='$unknown'">
          <xsl:value-of select="'nsISupports'"/>
        </xsl:when>

        <xsl:when test="$type='wstring'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:when test="$type='uuid'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:when test="//interface[@name=$type]/@wsmap='struct'">
          <xsl:call-template name="wrappedName">
            <xsl:with-param name="ifname" select="$type" />
          </xsl:call-template>
        </xsl:when>

        <xsl:when test="//interface[@name=$type]">
          <xsl:call-template name="wrappedName">
            <xsl:with-param name="ifname" select="$type" />
          </xsl:call-template>
        </xsl:when>

        <xsl:when test="//enum[@name=$type]">
          <xsl:value-of select="'long'" />
        </xsl:when>

        <xsl:otherwise>
          <xsl:call-template name="fullClassName">
            <xsl:with-param name="name" select="$type" />
          </xsl:call-template>
        </xsl:otherwise>

      </xsl:choose>
      <xsl:if test="$needarray">
        <xsl:value-of select="'[]'" />
      </xsl:if>
    </xsl:when>

    <xsl:when test="($G_vboxGlueStyle='mscom')">
      <xsl:value-of select="'Variant'"/>
    </xsl:when>

    <xsl:when test="($G_vboxGlueStyle='jaxws')">
      <xsl:variable name="needarray" select="($safearray='yes' and not($type='octet')) and not($forceelem='yes')" />

      <xsl:if test="$needarray">
        <xsl:value-of select="'List&lt;'" />
      </xsl:if>
      <xsl:choose>
        <xsl:when test="$type='$unknown'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:when test="//interface[@name=$type]/@wsmap='managed'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:when test="//interface[@name=$type]/@wsmap='struct'">
          <xsl:value-of select="concat($G_virtualBoxPackageCom, '.', $type)" />
        </xsl:when>

        <xsl:when test="//enum[@name=$type]">
          <xsl:value-of select="concat($G_virtualBoxPackageCom, '.', $type)" />
        </xsl:when>

        <!-- we encode byte arrays as Base64 strings. -->
        <xsl:when test="$type='octet'">
          <xsl:value-of select="'/*base64*/String'" />
        </xsl:when>

        <xsl:when test="$type='long long'">
          <xsl:value-of select="'Long'" />
        </xsl:when>

        <xsl:when test="$type='unsigned long'">
          <xsl:value-of select="'Long'" />
        </xsl:when>

        <xsl:when test="$type='long'">
          <xsl:value-of select="'Integer'" />
        </xsl:when>

        <xsl:when test="$type='unsigned short'">
          <xsl:value-of select="'Integer'" />
        </xsl:when>

        <xsl:when test="$type='short'">
          <xsl:value-of select="'Short'" />
        </xsl:when>

        <xsl:when test="$type='boolean'">
          <xsl:value-of select="'Boolean'" />
        </xsl:when>

        <xsl:when test="$type='wstring'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:when test="$type='uuid'">
          <xsl:value-of select="'String'" />
        </xsl:when>

        <xsl:otherwise>
          <xsl:call-template name="fatalError">
            <xsl:with-param name="msg" select="concat('Unhandled type ', $type,' (typeIdl2Back)')" />
          </xsl:call-template>
        </xsl:otherwise>

      </xsl:choose>

      <xsl:if test="$needarray">
        <xsl:value-of select="'&gt;'" />
      </xsl:if>
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Write typeIdl2Back for this style (typeIdl2Back)'" />
      </xsl:call-template>
    </xsl:otherwise>

  </xsl:choose>
</xsl:template>

<xsl:template name="cookOutParamXpcom">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:variable name="isstruct"
                select="//interface[@name=$idltype]/@wsmap='struct'" />

  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="elemgluetype">
    <xsl:if test="$safearray='yes'">
      <xsl:call-template name="typeIdl2Glue">
        <xsl:with-param name="type" select="$idltype" />
        <xsl:with-param name="safearray" select="'no'" />
        <xsl:with-param name="forceelem" select="'yes'" />
      </xsl:call-template>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:variable name="elembacktype">
            <xsl:call-template name="typeIdl2Back">
              <xsl:with-param name="type" select="$idltype" />
              <xsl:with-param name="safearray" select="$safearray" />
              <xsl:with-param name="forceelem" select="'yes'" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('Helper.wrap2(',$elemgluetype, '.class, ', $elembacktype, '.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="concat('(', $value, ' != null) ? new ', $gluetype, '(', $value,') : null')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="//enum[@name=$idltype]">
       <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:variable name="elembacktype">
            <xsl:call-template name="typeIdl2Back">
              <xsl:with-param name="type" select="$idltype" />
              <xsl:with-param name="safearray" select="$safearray" />
              <xsl:with-param name="forceelem" select="'yes'" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('Helper.wrapEnum(',$elemgluetype, '.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($gluetype,'.fromValue(', $value,')')"/>
        </xsl:otherwise>
       </xsl:choose>
    </xsl:when>

    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="($safearray='yes') and ($idltype='octet')">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$safearray='yes'">
          <xsl:value-of select="concat('Helper.wrap(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$value"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="cookOutParamMscom">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>

  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$safearray='yes'">
      <xsl:variable name="elemgluetype">
        <xsl:call-template name="typeIdl2Glue">
          <xsl:with-param name="type" select="$idltype" />
          <xsl:with-param name="safearray" select="'no'" />
          <xsl:with-param name="forceelem" select="'yes'" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:choose>
        <xsl:when test="($idltype='octet')">
          <xsl:value-of select="concat('Helper.wrapBytes(', $value, '.toSafeArray())')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('Helper.wrap(', $elemgluetype, '.class, ', $value,'.toSafeArray())')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
      <xsl:value-of select="concat('Helper.wrapDispatch(',$gluetype, '.class, ', $value,'.getDispatch())')"/>
    </xsl:when>

    <xsl:when test="//enum[@name=$idltype]">
      <xsl:value-of select="concat($gluetype,'.fromValue(', $value,'.getInt())')"/>
    </xsl:when>

    <xsl:when test="$idltype='wstring'">
      <xsl:value-of select="concat($value,'.getString()')"/>
    </xsl:when>

    <xsl:when test="$idltype='uuid'">
      <xsl:value-of select="concat($value,'.getString()')"/>
    </xsl:when>

     <xsl:when test="$idltype='boolean'">
      <xsl:value-of select="concat($value,'.toBoolean()')"/>
    </xsl:when>

    <xsl:when test="$idltype='unsigned short'">
      <xsl:value-of select="concat('(int)', $value,'.getShort()')"/>
    </xsl:when>

     <xsl:when test="$idltype='short'">
      <xsl:value-of select="concat($value,'.getShort()')"/>
    </xsl:when>

    <xsl:when test="$idltype='long'">
      <xsl:value-of select="concat($value,'.getInt()')"/>
    </xsl:when>


    <xsl:when test="$idltype='unsigned long'">
      <xsl:value-of select="concat('(long)', $value,'.getInt()')"/>
    </xsl:when>

    <xsl:when test="$idltype='long'">
      <xsl:value-of select="concat($value,'.getInt()')"/>
    </xsl:when>

    <xsl:when test="$idltype='long long'">
      <xsl:value-of select="concat($value,'.getLong()')"/>
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('Unhandled type' , $idltype, ' (cookOutParamMscom)')" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

</xsl:template>

<xsl:template name="cookOutParamJaxws">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>

  <xsl:variable name="isstruct"
                select="//interface[@name=$idltype]/@wsmap='struct'" />

  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$safearray='yes'">
      <xsl:variable name="elemgluetype">
        <xsl:call-template name="typeIdl2Glue">
          <xsl:with-param name="type" select="$idltype" />
          <xsl:with-param name="safearray" select="''" />
          <xsl:with-param name="forceelem" select="'yes'" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:variable name="elembacktype">
        <xsl:call-template name="typeIdl2Back">
          <xsl:with-param name="type" select="$idltype" />
          <xsl:with-param name="safearray" select="''" />
          <xsl:with-param name="forceelem" select="'yes'" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:choose>
        <xsl:when test="$isstruct">
          <xsl:value-of select="concat('Helper.wrap2(',$elemgluetype, '.class, ', $elembacktype, '.class, port, ', $value,')')"/>
        </xsl:when>
        <xsl:when test="//enum[@name=$idltype]">
          <xsl:value-of select="concat('Helper.convertEnums(',$elembacktype, '.class, ', $elemgluetype, '.class, ', $value,')')"/>
        </xsl:when>
        <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
          <xsl:value-of select="concat('Helper.wrap(',$elemgluetype,'.class, port, ', $value,')')"/>
        </xsl:when>
        <xsl:when test="$idltype='octet'">
          <xsl:value-of select="concat('Helper.decodeBase64(',$value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="$value" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="//enum[@name=$idltype]">
          <xsl:value-of select="concat($gluetype,'.fromValue(', $value,'.value())')"/>
        </xsl:when>
        <xsl:when test="$idltype='boolean'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='long long'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='unsigned long long'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='long'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='unsigned long'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='short'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='unsigned short'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='wstring'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$idltype='uuid'">
          <xsl:value-of select="$value"/>
        </xsl:when>
        <xsl:when test="$isstruct">
          <xsl:value-of select="concat('(', $value, ' != null) ? new ', $gluetype, '(', $value,', port) : null')" />
        </xsl:when>
        <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
          <!-- if the MOR string is empty, that means NULL, so return NULL instead of an object then -->
          <xsl:value-of select="concat('(', $value, '.length() > 0) ? new ', $gluetype, '(', $value,', port) : null')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="fatalError">
            <xsl:with-param name="msg" select="concat('Unhandled  type ', $idltype, ' (cookOutParamJaxws)')" />
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>

</xsl:template>

<xsl:template name="cookOutParam">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:choose>
    <xsl:when test="($G_vboxGlueStyle='xpcom')">
      <xsl:call-template name="cookOutParamXpcom">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="($G_vboxGlueStyle='mscom')">
      <xsl:call-template name="cookOutParamMscom">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="($G_vboxGlueStyle='jaxws')">
      <xsl:call-template name="cookOutParamJaxws">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Unhandled style(cookOutParam)'" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="cookInParamXpcom">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:variable name="isstruct"
                select="//interface[@name=$idltype]/@wsmap='struct'" />
  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="backtype">
    <xsl:call-template name="typeIdl2Back">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="elemgluetype">
    <xsl:if test="$safearray='yes'">
      <xsl:call-template name="typeIdl2Glue">
        <xsl:with-param name="type" select="$idltype" />
        <xsl:with-param name="safearray" select="'no'" />
        <xsl:with-param name="forceelem" select="'yes'" />
      </xsl:call-template>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="//interface[@name=$idltype]">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:variable name="elembacktype">
            <xsl:call-template name="typeIdl2Back">
              <xsl:with-param name="type" select="$idltype" />
              <xsl:with-param name="safearray" select="$safearray" />
              <xsl:with-param name="forceelem" select="'yes'" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('Helper.unwrap2(',$elemgluetype, '.class, ', $elembacktype, '.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="concat('(', $value, ' != null) ? ', $value, '.getTypedWrapped() : null')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

     <xsl:when test="$idltype='$unknown'">
       <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:value-of select="concat('Helper.unwrap2(',$elemgluetype, '.class, nsISupports.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="concat('(', $value, ' != null) ? (nsISupports)', $value, '.getWrapped() : null')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="//enum[@name=$idltype]">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:value-of select="concat('Helper.unwrapEnum(', $elemgluetype, '.class,', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($value,'.value()')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="($idltype='octet') and ($safearray='yes')">
      <xsl:value-of select="$value"/>
    </xsl:when>

    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:choose>
            <xsl:when test="$idltype='boolean'">
                <xsl:value-of select="concat('Helper.unwrapBoolean(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='long') or ($idltype='unsigned long') or ($idltype='integer')">
                <xsl:value-of select="concat('Helper.unwrapInteger(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='short') or ($idltype='unsigned short')">
                <xsl:value-of select="concat('Helper.unwrapUShort(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='unsigned long long') or ($idltype='long long')">
                <xsl:value-of select="concat('Helper.unwrapULong(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='wstring') or ($idltype='uuid')">
                <xsl:value-of select="concat('Helper.unwrapStr(',$value,')')"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="$value"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$value"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="cookInParamMscom">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>

  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="backtype">
    <xsl:call-template name="typeIdl2Back">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="elemgluetype">
    <xsl:if test="$safearray='yes'">
      <xsl:call-template name="typeIdl2Glue">
        <xsl:with-param name="type" select="$idltype" />
        <xsl:with-param name="safearray" select="'no'" />
        <xsl:with-param name="forceelem" select="'yes'" />
      </xsl:call-template>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="//interface[@name=$idltype]">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:variable name="elembacktype">
            <xsl:call-template name="typeIdl2Back">
              <xsl:with-param name="type" select="$idltype" />
              <xsl:with-param name="safearray" select="$safearray" />
              <xsl:with-param name="forceelem" select="'yes'" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('Helper.unwrap2(',$elemgluetype, '.class, ', $elembacktype, '.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="concat('(', $value, ' != null) ? ', $value, '.getTypedWrapped() : null')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

     <xsl:when test="$idltype='$unknown'">
       <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:value-of select="concat('Helper.unwrap2(',$elemgluetype, '.class, Dispatch.class, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:value-of select="concat('(', $value, ' != null) ? (Dispatch)', $value, '.getWrapped() : null')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="//enum[@name=$idltype]">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
          <xsl:value-of select="concat('Helper.unwrapEnum(', $elemgluetype, '.class,',$value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($value,'.value()')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="$idltype='boolean'">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
           <xsl:value-of select="concat('Helper.unwrapBool(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('new Variant(',$value,')')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="($idltype='short') or ($idltype='unsigned short')">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
           <xsl:value-of select="concat('Helper.unwrapShort(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('new Variant(',$value,')')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>


    <xsl:when test="($idltype='long') or ($idltype='unsigned long')">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
           <xsl:value-of select="concat('Helper.unwrapInt(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('new Variant(',$value,')')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="($idltype='wstring') or ($idltype='uuid')">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
           <xsl:value-of select="concat('Helper.unwrapString(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('new Variant(',$value,')')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="($idltype='unsigned long long') or ($idltype='long long')">
      <xsl:choose>
        <xsl:when test="$safearray='yes'">
           <xsl:value-of select="concat('Helper.unwrapLong(', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('new Variant(',$value,'.longValue())')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>

    <xsl:when test="($idltype='octet') and ($safearray='yes')">
      <xsl:value-of select="concat('Helper.encodeBase64(', $value,')')"/>
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('Unhandled type: ', $idltype)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

</xsl:template>

<xsl:template name="cookInParamJaxws">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:variable name="isstruct"
                select="//interface[@name=$idltype]/@wsmap='struct'" />

  <xsl:variable name="gluetype">
    <xsl:call-template name="typeIdl2Glue">
      <xsl:with-param name="type" select="$idltype" />
      <xsl:with-param name="safearray" select="$safearray" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="elemgluetype">
    <xsl:if test="$safearray='yes'">
      <xsl:call-template name="typeIdl2Glue">
        <xsl:with-param name="type" select="$idltype" />
        <xsl:with-param name="safearray" select="'no'" />
        <xsl:with-param name="forceelem" select="'yes'" />
      </xsl:call-template>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
     <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
      <xsl:choose>
        <xsl:when test="@safearray='yes'">
          <xsl:choose>
            <xsl:when test="$idltype='boolean'">
                <xsl:value-of select="concat('Helper.unwrapBoolean(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='long') or ($idltype='unsigned long') or ($idltype='integer')">
                <xsl:value-of select="concat('Helper.unwrapInteger(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='short') or ($idltype='unsigned short')">
                <xsl:value-of select="concat('Helper.unwrapUShort(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='unsigned long long') or ($idltype='long long')">
                <xsl:value-of select="concat('Helper.unwrapULong(',$value,')')"/>
            </xsl:when>
            <xsl:when test="($idltype='wstring') or ($idltype='uuid')">
                <xsl:value-of select="concat('Helper.unwrapStr(',$value,')')"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="concat('((', $value, ' == null) ? null :', $value, '.getWrapped())')" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('((', $value, ' == null) ? null :', $value, '.getWrapped())')" />
        </xsl:otherwise>
      </xsl:choose>
     </xsl:when>

     <xsl:when test="//enum[@name=$idltype]">
       <xsl:choose>
         <xsl:when test="$safearray='yes'">
           <xsl:variable name="elembacktype">
             <xsl:call-template name="typeIdl2Back">
               <xsl:with-param name="type" select="$idltype" />
               <xsl:with-param name="safearray" select="'no'" />
               <xsl:with-param name="forceelem" select="'yes'" />
             </xsl:call-template>
           </xsl:variable>
           <xsl:value-of select="concat('Helper.convertEnums(', $elemgluetype, '.class,',  $elembacktype, '.class,', $value,')')"/>
         </xsl:when>
         <xsl:otherwise>
           <xsl:variable name="backtype">
             <xsl:call-template name="typeIdl2Back">
               <xsl:with-param name="type" select="$idltype" />
               <xsl:with-param name="safearray" select="'no'" />
               <xsl:with-param name="forceelem" select="'yes'" />
             </xsl:call-template>
           </xsl:variable>
           <xsl:value-of select="concat($backtype, '.fromValue(', $value, '.name())')"/>
         </xsl:otherwise>
       </xsl:choose>
     </xsl:when>

     <xsl:when test="($idltype='octet') and ($safearray='yes')">
       <xsl:value-of select="concat('Helper.encodeBase64(',$value,')')"/>
     </xsl:when>

     <xsl:otherwise>
       <xsl:value-of select="$value"/>
     </xsl:otherwise>
  </xsl:choose>

</xsl:template>

<xsl:template name="cookInParam">
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:choose>
    <xsl:when test="($G_vboxGlueStyle='xpcom')">
      <xsl:call-template name="cookInParamXpcom">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="($G_vboxGlueStyle='mscom')">
      <xsl:call-template name="cookInParamMscom">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="($G_vboxGlueStyle='jaxws')">
      <xsl:call-template name="cookInParamJaxws">
        <xsl:with-param name="value" select="$value" />
        <xsl:with-param name="idltype" select="$idltype" />
        <xsl:with-param name="safearray" select="$safearray" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Unhandled style (cookInParam)'" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- Invoke backend method, including parameter conversion -->
<xsl:template name="genBackMethodCall">
  <xsl:param name="ifname"/>
  <xsl:param name="methodname"/>
  <xsl:param name="retval"/>

  <xsl:choose>
    <xsl:when test="($G_vboxGlueStyle='xpcom')">
      <xsl:value-of select="'                '" />
      <xsl:if test="param[@dir='return']">
        <xsl:value-of select="concat($retval, ' = ')" />
      </xsl:if>
      <xsl:value-of select="concat('getTypedWrapped().', $methodname,'(')"/>
      <xsl:for-each select="param">
         <xsl:choose>
           <xsl:when test="@dir='return'">
             <xsl:if test="@safearray='yes'">
               <xsl:value-of select="'null'" />
             </xsl:if>
           </xsl:when>
           <xsl:when test="@dir='out'">
             <xsl:if test="@safearray='yes'">
               <xsl:value-of select="'null, '" />
             </xsl:if>
             <xsl:value-of select="concat('tmp_', @name)" />
           </xsl:when>
           <xsl:when test="@dir='in'">
             <xsl:if test="(@safearray='yes') and not(@type = 'octet')">
                <xsl:value-of select="concat(@name,'.size(), ')" />
             </xsl:if>
             <xsl:variable name="unwrapped">
               <xsl:call-template name="cookInParam">
                 <xsl:with-param name="value" select="@name" />
                 <xsl:with-param name="idltype" select="@type" />
                 <xsl:with-param name="safearray" select="@safearray" />
               </xsl:call-template>
             </xsl:variable>
             <xsl:value-of select="$unwrapped"/>
           </xsl:when>
           <xsl:otherwise>
             <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('Unsupported param dir: ', @dir, '&quot;.')" />
              </xsl:call-template>
           </xsl:otherwise>
         </xsl:choose>
         <xsl:if test="not(position()=last()) and not(following-sibling::param[1]/@dir='return' and not(following-sibling::param[1]/@safearray='yes'))">
           <xsl:value-of select="', '"/>
         </xsl:if>
      </xsl:for-each>
      <xsl:value-of select="');&#10;'"/>
    </xsl:when>

    <xsl:when test="($G_vboxGlueStyle='mscom')">
      <xsl:value-of select="'                '" />
      <xsl:if test="param[@dir='return']">
        <xsl:value-of select="concat($retval, ' = ')" />
      </xsl:if>
      <xsl:value-of select="concat('Helper.invoke(getTypedWrapped(), &quot;',  $methodname, '&quot; ')"/>
      <xsl:for-each select="param[not(@dir='return')]">
        <xsl:value-of select="', '"/>
        <xsl:choose>
           <xsl:when test="@dir='out'">
             <xsl:value-of select="concat('tmp_', @name)" />
           </xsl:when>
           <xsl:when test="@dir='in'">
             <xsl:variable name="unwrapped">
               <xsl:call-template name="cookInParam">
                 <xsl:with-param name="value" select="@name" />
                 <xsl:with-param name="idltype" select="@type" />
                 <xsl:with-param name="safearray" select="@safearray" />
               </xsl:call-template>
             </xsl:variable>
             <xsl:value-of select="$unwrapped"/>
           </xsl:when>
        </xsl:choose>
      </xsl:for-each>
      <xsl:value-of select="');&#10;'"/>
    </xsl:when>

     <xsl:when test="($G_vboxGlueStyle='jaxws')">
       <xsl:variable name="jaxwsmethod">
         <xsl:call-template name="makeJaxwsMethod">
           <xsl:with-param name="ifname" select="$ifname" />
           <xsl:with-param name="methodname" select="$methodname" />
         </xsl:call-template>
       </xsl:variable>
       <xsl:variable name="portArg">
         <xsl:if test="not(//interface[@name=$ifname]/@wsmap='global')">
           <xsl:value-of select="'obj'"/>
         </xsl:if>
       </xsl:variable>
       <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />

       <xsl:value-of select="'        '" />
       <xsl:if test="param[@dir='return'] and not(param[@dir='out'])">
         <xsl:value-of select="concat($retval, ' = ')" />
       </xsl:if>
       <xsl:value-of select="concat('port.', $jaxwsmethod, '(', $portArg)" />
       <xsl:if test="$paramsinout and not($portArg='')">
         <xsl:value-of select="', '"/>
       </xsl:if>

       <!-- jax-ws has an oddity: if both out params and a return value exist,
            then the return value is moved to the function's argument list... -->
       <xsl:choose>
         <xsl:when test="param[@dir='out'] and param[@dir='return']">
           <xsl:for-each select="param">
             <xsl:choose>
               <xsl:when test="@dir='return'">
                 <xsl:value-of select="$retval"/>
               </xsl:when>
               <xsl:when test="@dir='out'">
                 <xsl:value-of select="concat('tmp_', @name)" />
               </xsl:when>
               <xsl:otherwise>
                 <xsl:call-template name="cookInParam">
                   <xsl:with-param name="value" select="@name" />
                   <xsl:with-param name="idltype" select="@type" />
                   <xsl:with-param name="safearray" select="@safearray" />
                 </xsl:call-template>
               </xsl:otherwise>
             </xsl:choose>
             <xsl:if test="not(position()=last())">
               <xsl:value-of select="', '"/>
             </xsl:if>
           </xsl:for-each>
         </xsl:when>
         <xsl:otherwise>
           <xsl:for-each select="$paramsinout">
             <xsl:choose>
               <xsl:when test="@dir='return'">
                 <xsl:value-of select="$retval"/>
               </xsl:when>
               <xsl:when test="@dir='out'">
                 <xsl:value-of select="concat('tmp_', @name)" />
               </xsl:when>
               <xsl:otherwise>
                 <xsl:call-template name="cookInParam">
                   <xsl:with-param name="value" select="@name" />
                   <xsl:with-param name="idltype" select="@type" />
                   <xsl:with-param name="safearray" select="@safearray" />
                 </xsl:call-template>
               </xsl:otherwise>
             </xsl:choose>
             <xsl:if test="not(position()=last())">
               <xsl:value-of select="', '"/>
             </xsl:if>
           </xsl:for-each>
         </xsl:otherwise>
       </xsl:choose>
       <xsl:value-of select="');&#10;'"/>
     </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Style unknown (genBackMethodCall)'" />
      </xsl:call-template>
    </xsl:otherwise>

  </xsl:choose>
</xsl:template>

<xsl:template name="genGetterCall">
  <xsl:param name="ifname"/>
  <xsl:param name="gettername"/>
  <xsl:param name="backtype"/>
  <xsl:param name="retval"/>

  <xsl:choose>

   <xsl:when test="$G_vboxGlueStyle='xpcom'">
     <xsl:value-of select="concat('         ', $backtype, ' ', $retval,' = getTypedWrapped().', $gettername,'(')" />
     <xsl:if test="@safearray">
       <xsl:value-of select="'null'" />
     </xsl:if>
     <xsl:value-of select="');&#10;'" />
   </xsl:when>

   <xsl:when test="$G_vboxGlueStyle='mscom'">
     <xsl:value-of select="concat('         ', $backtype, ' ', $retval,' = Dispatch.get(getTypedWrapped(), &quot;', @name,'&quot;);&#10;')" />
   </xsl:when>

   <xsl:when test="$G_vboxGlueStyle='jaxws'">
     <xsl:variable name="jaxwsGetter">
       <xsl:call-template name="makeJaxwsMethod">
         <xsl:with-param name="ifname" select="$ifname" />
         <xsl:with-param name="methodname" select="$gettername" />
       </xsl:call-template>
     </xsl:variable>
     <xsl:value-of select="concat('         ', $backtype, ' ', $retval,' = port.', $jaxwsGetter, '(obj);&#10;')" />
   </xsl:when>

   <xsl:otherwise>
     <xsl:call-template name="fatalError">
       <xsl:with-param name="msg" select="'Style unknown (genGetterCall)'" />
     </xsl:call-template>
   </xsl:otherwise>

  </xsl:choose>
</xsl:template>

<xsl:template name="genSetterCall">
  <xsl:param name="ifname"/>
  <xsl:param name="settername"/>
  <xsl:param name="value"/>

  <xsl:choose>
   <xsl:when test="$G_vboxGlueStyle='xpcom'">
     <xsl:value-of select="concat('         getTypedWrapped().', $settername, '(', $value,');&#10;')" />
   </xsl:when>

   <xsl:when test="$G_vboxGlueStyle='mscom'">
     <xsl:value-of select="concat('         Dispatch.put(getTypedWrapped(), &quot;', @name,'&quot;, ',$value, ');&#10;')" />
   </xsl:when>

   <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <xsl:variable name="jaxwsSetter">
        <xsl:call-template name="makeJaxwsMethod">
          <xsl:with-param name="ifname" select="$ifname" />
          <xsl:with-param name="methodname" select="$settername" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:value-of select="concat('        port.', $jaxwsSetter, '(obj, ', $value,');&#10;')" />
   </xsl:when>

   <xsl:otherwise>
     <xsl:call-template name="fatalError">
       <xsl:with-param name="msg" select="'Style unknown (genSetterCall)'" />
     </xsl:call-template>
   </xsl:otherwise>

  </xsl:choose>
</xsl:template>

<xsl:template name="genStructWrapperJaxws">
  <xsl:param name="ifname"/>

  <xsl:value-of select="concat('    private ', $G_virtualBoxPackageCom,'.',$ifname, ' real;&#10;')"/>
  <xsl:value-of select="'    private VboxPortType port;&#10;&#10;'"/>

  <xsl:value-of select="concat('    public ', $ifname, '(', $G_virtualBoxPackageCom,'.',$ifname,' real, VboxPortType port) {&#10;      this.real = real; &#10;      this.port = port;  &#10;    }&#10;')"/>

  <xsl:for-each select="attribute">
    <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
    <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
    <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
    <xsl:variable name="attrsafearray"><xsl:value-of select="@safearray" /></xsl:variable>

    <xsl:if test="not($attrreadonly)">
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Non read-only struct (genStructWrapperJaxws)'" />
      </xsl:call-template>
    </xsl:if>

    <!-- Emit getter -->
    <xsl:variable name="backgettername">
      <xsl:choose>
        <!-- Stupid, but backend boolean getters called isFoo(), not getFoo() -->
        <xsl:when test="$attrtype = 'boolean'">
          <xsl:variable name="capsname">
            <xsl:call-template name="capitalize">
              <xsl:with-param name="str" select="$attrname" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('is', $capsname)" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="makeGetterName">
            <xsl:with-param name="attrname" select="$attrname" />
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="gluegettername">
      <xsl:call-template name="makeGetterName">
        <xsl:with-param name="attrname" select="$attrname" />
      </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="gluegettertype">
      <xsl:call-template name="typeIdl2Glue">
        <xsl:with-param name="type" select="$attrtype" />
        <xsl:with-param name="safearray" select="@safearray" />
      </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="backgettertype">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="$attrtype" />
        <xsl:with-param name="safearray" select="@safearray" />
      </xsl:call-template>
    </xsl:variable>

    <xsl:value-of select="concat('    public ', $gluegettertype, ' ', $gluegettername, '() {&#10;')" />
    <xsl:value-of select="concat('            ', $backgettertype, ' retVal = real.', $backgettername, '();&#10;')" />
    <xsl:variable name="wrapped">
      <xsl:call-template name="cookOutParam">
        <xsl:with-param name="value" select="'retVal'" />
        <xsl:with-param name="idltype" select="$attrtype" />
        <xsl:with-param name="safearray" select="@safearray" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:value-of select="concat('            return ', $wrapped, ';&#10;')" />
    <xsl:value-of select="       '    }&#10;'" />

  </xsl:for-each>

</xsl:template>

<!-- Interface method wrapper -->
<xsl:template name="genMethod">
  <xsl:param name="ifname"/>
  <xsl:param name="methodname"/>

  <xsl:choose>
    <xsl:when test="(param[@mod='ptr']) or (($G_vboxGlueStyle='jaxws') and (param[@type=($G_setSuppressedInterfaces/@name)]))" >
      <xsl:comment>
        <xsl:value-of select="concat('Skipping method ', $methodname, ' for it has parameters with suppressed types')" />
      </xsl:comment>
    </xsl:when>
    <xsl:otherwise>
      <xsl:variable name="hasReturnParms" select="param[@dir='return']" />
      <xsl:variable name="hasOutParms" select="param[@dir='out']" />
      <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
      <xsl:variable name="returnidlsafearray" select="param[@dir='return']/@safearray" />
      <xsl:variable name="returngluetype">
        <xsl:choose>
          <xsl:when test="$returnidltype">
            <xsl:call-template name="typeIdl2Glue">
              <xsl:with-param name="type" select="$returnidltype" />
              <xsl:with-param name="safearray" select="$returnidlsafearray" />
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>void</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:variable name="retValValue">
        <xsl:choose>
          <xsl:when test="(param[@dir='out']) and ($G_vboxGlueStyle='jaxws')">
            <xsl:value-of select="'retVal.value'"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="'retVal'"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:apply-templates select="desc" mode="method"/>
      <xsl:value-of select="concat('    public ', $returngluetype, ' ', $methodname, '(')" />
      <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />
      <xsl:for-each select="exsl:node-set($paramsinout)">
        <xsl:variable name="paramgluetype">
          <xsl:call-template name="typeIdl2Glue">
            <xsl:with-param name="type" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="@dir='out'">
            <xsl:value-of select="concat('Holder&lt;', $paramgluetype, '&gt; ', @name)" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="concat($paramgluetype, ' ', @name)" />
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="not(position()=last())">
          <xsl:value-of select="', '" />
        </xsl:if>
      </xsl:for-each>
      <xsl:value-of select="') {&#10;'"/>

      <xsl:call-template name="startExcWrapper"/>

      <!-- declare temp out params -->
      <xsl:for-each select="param[@dir='out']">
        <xsl:variable name="backouttype">
          <xsl:call-template name="typeIdl2Back">
            <xsl:with-param name="type" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="$G_vboxGlueStyle='xpcom'">
            <xsl:value-of select="concat('        ', $backouttype, '[]     tmp_', @name, ' = (', $backouttype, '[])java.lang.reflect.Array.newInstance(',$backouttype,'.class, 1);&#10;')"/>
          </xsl:when>
          <xsl:when test="$G_vboxGlueStyle='mscom'">
            <xsl:value-of select="concat('        Variant    tmp_', @name, ' = new Variant();&#10;')"/>
          </xsl:when>
          <xsl:when test="$G_vboxGlueStyle='jaxws'">
            <xsl:value-of select="concat('        javax.xml.ws.Holder&lt;', $backouttype,'&gt;   tmp_', @name, ' = new  javax.xml.ws.Holder&lt;', $backouttype,'&gt;();&#10;')"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="fatalError">
              <xsl:with-param name="msg" select="'Handle out param (genMethod)'" />
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>

      <!-- declare return param, if any -->
      <xsl:if test="$hasReturnParms">
        <xsl:variable name="backrettype">
          <xsl:call-template name="typeIdl2Back">
            <xsl:with-param name="type" select="$returnidltype" />
            <xsl:with-param name="safearray" select="$returnidlsafearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="(param[@dir='out']) and ($G_vboxGlueStyle='jaxws')">
            <xsl:value-of select="concat('        javax.xml.ws.Holder&lt;', $backrettype, '&gt;',
                                  ' retVal = new javax.xml.ws.Holder&lt;', $backrettype,
                                  '&gt;();&#10;')"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="concat('        ', $backrettype, '     retVal;&#10;')"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:if>

      <!-- Method call -->
      <xsl:call-template name="genBackMethodCall">
        <xsl:with-param name="ifname" select="$ifname" />
        <xsl:with-param name="methodname" select="$methodname" />
        <xsl:with-param name="retval" select="'retVal'" />
      </xsl:call-template>

       <!-- return out params -->
      <xsl:for-each select="param[@dir='out']">
        <xsl:variable name="varval">
          <xsl:choose>
            <xsl:when test="$G_vboxGlueStyle='xpcom'">
              <xsl:value-of select="concat('tmp_',@name,'[0]')" />
            </xsl:when>
            <xsl:when test="$G_vboxGlueStyle='mscom'">
               <xsl:value-of select="concat('tmp_',@name)" />
            </xsl:when>
            <xsl:when test="$G_vboxGlueStyle='jaxws'">
              <xsl:value-of select="concat('tmp_',@name,'.value')" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="'Style unknown (genMethod, outparam)'" />
              </xsl:call-template>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:variable name="wrapped">
          <xsl:call-template name="cookOutParam">
            <xsl:with-param name="value" select="$varval" />
            <xsl:with-param name="idltype" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('        ', @name, '.value = ',$wrapped,';&#10;')"/>
      </xsl:for-each>

      <xsl:if test="$hasReturnParms">
        <!-- actual 'return' statement -->
        <xsl:variable name="wrapped">
          <xsl:call-template name="cookOutParam">
            <xsl:with-param name="value" select="$retValValue" />
            <xsl:with-param name="idltype" select="$returnidltype" />
            <xsl:with-param name="safearray" select="$returnidlsafearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('        return ', $wrapped, ';&#10;')" />
      </xsl:if>
      <xsl:call-template name="endExcWrapper"/>

      <xsl:value-of select="'    }&#10;'"/>
    </xsl:otherwise>
  </xsl:choose>

</xsl:template>

<!-- Callback interface method -->
<xsl:template name="genCbMethodDecl">
  <xsl:param name="ifname"/>
  <xsl:param name="methodname"/>

  <xsl:choose>
    <xsl:when test="(param[@mod='ptr'])" >
      <xsl:comment>
        <xsl:value-of select="concat('Skipping method ', $methodname, ' for it has parameters with suppressed types')" />
      </xsl:comment>
    </xsl:when>
    <xsl:otherwise>
      <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
      <xsl:variable name="returnidlsafearray" select="param[@dir='return']/@safearray" />
      <xsl:variable name="returngluetype">
        <xsl:choose>
          <xsl:when test="$returnidltype">
            <xsl:call-template name="typeIdl2Glue">
              <xsl:with-param name="type" select="$returnidltype" />
              <xsl:with-param name="safearray" select="$returnidlsafearray" />
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>void</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:value-of select="concat('    public ', $returngluetype, ' ', $methodname, '(')" />
      <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />
      <xsl:for-each select="exsl:node-set($paramsinout)">
        <xsl:variable name="paramgluetype">
          <xsl:call-template name="typeIdl2Glue">
            <xsl:with-param name="type" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="@dir='out'">
            <xsl:value-of select="concat('Holder&lt;', $paramgluetype, '&gt; ', @name)" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="concat($paramgluetype, ' ', @name)" />
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="not(position()=last())">
          <xsl:text>, </xsl:text>
        </xsl:if>
      </xsl:for-each>
      <xsl:value-of select="');&#10;'"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- queryInterface wrapper -->
<xsl:template name="genQI">
  <xsl:param name="ifname"/>
  <xsl:param name="uuid" />

  <xsl:value-of select="concat('    public static ', $ifname, ' queryInterface(IUnknown obj) {&#10;')" />
  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
      <xsl:variable name="backtype">
        <xsl:call-template name="typeIdl2Back">
          <xsl:with-param name="type" select="$ifname" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:value-of select="       '      nsISupports nsobj = obj != null ? (nsISupports)obj.getWrapped() : null;&#10;'"/>
      <xsl:value-of select="       '      if (nsobj == null) return null;&#10;'"/>
      <xsl:value-of select="concat('      ',$backtype, ' qiobj = Helper.queryInterface(nsobj, &quot;{',$uuid,'}&quot;, ',$backtype,'.class);&#10;')" />
      <xsl:value-of select="concat('      return qiobj == null ? null : new ', $ifname, '(qiobj);&#10;')" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:value-of select="concat('       return', ' obj == null ? null : new ', $ifname, '((com.jacob.com.Dispatch)obj.getWrapped());&#10;')" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <!-- bad, need to check that we really can be casted to this type -->
      <xsl:value-of select="concat('       return obj == null ?  null : new ', $ifname, '(obj.getWrapped(), obj.getRemoteWSPort());&#10;')" />
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Style unknown (genQI)'" />
      </xsl:call-template>
    </xsl:otherwise>

  </xsl:choose>
  <xsl:value-of select="           '    }&#10;'" />
</xsl:template>


<xsl:template name="genCbMethodImpl">
  <xsl:param name="ifname"/>
  <xsl:param name="methodname"/>

  <xsl:choose>
    <xsl:when test="(param[@mod='ptr'])" >
      <xsl:comment>
        <xsl:value-of select="concat('Skipping method ', $methodname, ' for it has parameters with suppressed types')" />
      </xsl:comment>
    </xsl:when>
    <xsl:otherwise>
      <xsl:variable name="hasReturnParms" select="param[@dir='return']" />
      <xsl:variable name="hasOutParms" select="param[@dir='out']" />
      <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
      <xsl:variable name="returnidlsafearray" select="param[@dir='return']/@safearray" />
      <xsl:variable name="returnbacktype">
        <xsl:choose>
          <xsl:when test="$returnidltype">
            <xsl:call-template name="typeIdl2Back">
              <xsl:with-param name="type" select="$returnidltype" />
              <xsl:with-param name="safearray" select="$returnidlsafearray" />
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>void</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />
      <xsl:choose>
        <xsl:when test="$G_vboxGlueStyle='xpcom'">
          <xsl:value-of select="concat('    public ', $returnbacktype, ' ', $methodname, '(')" />
          <xsl:for-each select="exsl:node-set($paramsinout)">
            <xsl:variable name="parambacktype">
              <xsl:call-template name="typeIdl2Back">
                <xsl:with-param name="type" select="@type" />
                <xsl:with-param name="safearray" select="@safearray" />
              </xsl:call-template>
            </xsl:variable>
            <xsl:choose>
              <xsl:when test="@dir='out'">
                <xsl:value-of select="concat($parambacktype, '[] ', @name)" />
              </xsl:when>
              <xsl:otherwise>
                <xsl:if test="@safearray">
                  <xsl:value-of select="concat('long len_',@name,', ')" />
                </xsl:if>
                <xsl:value-of select="concat($parambacktype, ' ', @name)" />
              </xsl:otherwise>
            </xsl:choose>
            <xsl:if test="not(position()=last())">
              <xsl:text>, </xsl:text>
            </xsl:if>
          </xsl:for-each>
          <xsl:value-of select="') {&#10;'"/>
        </xsl:when>

        <xsl:when test="$G_vboxGlueStyle='mscom'">
          <xsl:variable name="capsname">
            <xsl:call-template name="capitalize">
              <xsl:with-param name="str" select="$methodname" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('    public ', $returnbacktype, ' ', $capsname, '(')" />
          <xsl:value-of select="'Variant _args[]'"/>
          <xsl:value-of select="') {&#10;'"/>
          <xsl:for-each select="exsl:node-set($paramsinout)">
            <xsl:variable name="parambacktype">
              <xsl:call-template name="typeIdl2Back">
                <xsl:with-param name="type" select="@type" />
                <xsl:with-param name="safearray" select="@safearray" />
              </xsl:call-template>
            </xsl:variable>
            <xsl:value-of select="concat('        ', $parambacktype, ' ', @name, '=_args[', count(preceding-sibling::param),'];&#10;')" />
          </xsl:for-each>
        </xsl:when>

         <xsl:otherwise>
           <xsl:call-template name="fatalError">
             <xsl:with-param name="msg" select="'Style unknown (genSetterCall)'" />
           </xsl:call-template>
         </xsl:otherwise>

      </xsl:choose>

      <!-- declare temp out params -->
      <xsl:for-each select="param[@dir='out']">
        <xsl:variable name="glueouttype">
          <xsl:call-template name="typeIdl2Glue">
            <xsl:with-param name="type" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('        Holder&lt;', $glueouttype, '&gt;     tmp_', @name, ' = new Holder&lt;', $glueouttype, '&gt;();&#10;')"/>
      </xsl:for-each>

      <!-- declare return param, if any -->
      <xsl:if test="$hasReturnParms">
        <xsl:variable name="gluerettype">
          <xsl:call-template name="typeIdl2Glue">
            <xsl:with-param name="type" select="$returnidltype" />
            <xsl:with-param name="safearray" select="$returnidlsafearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('        ', $gluerettype, '     retVal = &#10;')"/>
      </xsl:if>

      <!-- Method call -->
      <xsl:value-of select="concat('        sink.', $methodname,'(')"/>
      <xsl:for-each select="param[not(@dir='return')]">
         <xsl:choose>
           <xsl:when test="@dir='out'">
             <xsl:value-of select="concat('tmp_', @name)" />
           </xsl:when>
           <xsl:when test="@dir='in'">
             <xsl:variable name="wrapped">
               <xsl:call-template name="cookOutParam">
                 <xsl:with-param name="value" select="@name" />
                 <xsl:with-param name="idltype" select="@type" />
                 <xsl:with-param name="safearray" select="@safearray" />
               </xsl:call-template>
             </xsl:variable>
             <xsl:value-of select="$wrapped"/>
           </xsl:when>
           <xsl:otherwise>
             <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('Unsupported param dir: ', @dir, '&quot;.')" />
              </xsl:call-template>
           </xsl:otherwise>
         </xsl:choose>
         <xsl:if test="not(position()=last())">
           <xsl:value-of select="', '"/>
         </xsl:if>
      </xsl:for-each>
      <xsl:value-of select="');&#10;'"/>

       <!-- return out params -->
      <xsl:for-each select="param[@dir='out']">

        <xsl:variable name="unwrapped">
          <xsl:call-template name="cookInParam">
            <xsl:with-param name="value" select="concat('tmp_',@name,'.value')" />
            <xsl:with-param name="idltype" select="@type" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="$G_vboxGlueStyle='xpcom'">
            <xsl:value-of select="concat('        ', @name, '[0] = ',$unwrapped,';&#10;')"/>
          </xsl:when>
          <xsl:when test="$G_vboxGlueStyle='mscom'">
            <xsl:value-of select="concat('        _args[',count(preceding-sibling::param),'] = ',$unwrapped,';&#10;')"/>
          </xsl:when>
        </xsl:choose>
      </xsl:for-each>

      <xsl:if test="$hasReturnParms">
        <!-- actual 'return' statement -->
        <xsl:variable name="unwrapped">
          <xsl:call-template name="cookInParam">
            <xsl:with-param name="value" select="'retVal'" />
            <xsl:with-param name="idltype" select="$returnidltype" />
            <xsl:with-param name="safearray" select="$returnidlsafearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('        return ', $unwrapped, ';&#10;')" />
      </xsl:if>
      <xsl:value-of select="'    }&#10;'"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- Interface method -->
<xsl:template name="genIfaceWrapper">
  <xsl:param name="ifname"/>

  <xsl:variable name="wrappedType">
    <xsl:call-template name="wrappedName">
      <xsl:with-param name="ifname" select="$ifname" />
    </xsl:call-template>
  </xsl:variable>

  <!-- Constructor -->
  <xsl:choose>
      <xsl:when test="($G_vboxGlueStyle='jaxws')">
        <xsl:value-of select="concat('    public ', $ifname, '(String wrapped, VboxPortType port) {&#10;')" />
        <xsl:value-of select="       '          super(wrapped, port);&#10;'"/>
        <xsl:value-of select="       '    }&#10;'"/>
      </xsl:when>

      <xsl:when test="($G_vboxGlueStyle='xpcom') or ($G_vboxGlueStyle='mscom')">
        <xsl:value-of select="concat('    public ', $ifname, '(',  $wrappedType,' wrapped) {&#10;')" />
        <xsl:value-of select="       '          super(wrapped);&#10;'"/>
        <xsl:value-of select="       '    }&#10;'"/>

        <!-- Typed wrapped object accessor -->
        <xsl:value-of select="concat('    public ', $wrappedType, ' getTypedWrapped() {&#10;')" />
        <xsl:value-of select="concat('         return (', $wrappedType, ') getWrapped();&#10;')" />
        <xsl:value-of select="       '    }&#10;'" />
      </xsl:when>

      <xsl:otherwise>
        <xsl:call-template name="fatalError">
          <xsl:with-param name="msg" select="'Style unknown (root, ctr)'" />
        </xsl:call-template>
      </xsl:otherwise>
  </xsl:choose>
  <!-- Attributes -->
  <xsl:for-each select="attribute[not(@mod='ptr')]">
    <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
    <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
    <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
    <xsl:variable name="attrsafearray"><xsl:value-of select="@safearray" /></xsl:variable>

    <xsl:choose>
      <xsl:when test="($G_vboxGlueStyle='jaxws') and ($attrtype=($G_setSuppressedInterfaces/@name))">
        <xsl:value-of select="concat('  // skip attribute ',$attrname, ' of suppressed type ', $attrtype, '&#10;&#10;')" />
      </xsl:when>

      <xsl:otherwise>
        <!-- emit getter method -->
        <xsl:apply-templates select="desc" mode="attribute_get"/>
        <xsl:variable name="gettername">
          <xsl:call-template name="makeGetterName">
            <xsl:with-param name="attrname" select="$attrname" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="gluetype">
          <xsl:call-template name="typeIdl2Glue">
            <xsl:with-param name="type" select="$attrtype" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="backtype">
          <xsl:call-template name="typeIdl2Back">
            <xsl:with-param name="type" select="$attrtype" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="wrapped">
          <xsl:call-template name="cookOutParam">
            <xsl:with-param name="value" select="'retVal'" />
            <xsl:with-param name="idltype" select="$attrtype" />
            <xsl:with-param name="safearray" select="@safearray" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="concat('    public ', $gluetype, ' ', $gettername, '() {&#10;')" />

        <xsl:call-template name="startExcWrapper"/>

        <!-- Actual getter implementation -->
        <xsl:call-template name="genGetterCall">
          <xsl:with-param name="ifname" select="$ifname" />
          <xsl:with-param name="gettername" select="$gettername" />
          <xsl:with-param name="backtype" select="$backtype" />
          <xsl:with-param name="retval" select="'retVal'" />
        </xsl:call-template>

        <xsl:value-of select="concat('         return ', $wrapped, ';&#10;')" />
        <xsl:call-template name="endExcWrapper"/>

        <xsl:value-of select=       "'    }&#10;'" />
        <xsl:if test="not(@readonly='yes')">
          <!-- emit setter method -->
          <xsl:apply-templates select="desc" mode="attribute_set"/>
          <xsl:variable name="settername"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
          <xsl:variable name="unwrapped">
            <xsl:call-template name="cookInParam">
              <xsl:with-param name="ifname" select="$ifname" />
              <xsl:with-param name="value" select="'value'" />
              <xsl:with-param name="idltype" select="$attrtype" />
              <xsl:with-param name="safearray" select="@safearray" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('    public void ', $settername, '(', $gluetype, ' value) {&#10;')" />
          <xsl:call-template name="startExcWrapper"/>
          <!-- Actual setter implementation -->
          <xsl:call-template name="genSetterCall">
            <xsl:with-param name="ifname" select="$ifname" />
            <xsl:with-param name="settername" select="$settername" />
            <xsl:with-param name="value" select="$unwrapped" />
          </xsl:call-template>
          <xsl:call-template name="endExcWrapper"/>
          <xsl:value-of select=       "'    }&#10;'" />
        </xsl:if>

      </xsl:otherwise>
    </xsl:choose>

  </xsl:for-each>

  <!-- emit queryInterface() *to* this class -->
  <xsl:call-template name="genQI">
    <xsl:with-param name="ifname" select="$ifname" />
    <xsl:with-param name="uuid" select="@uuid" />
  </xsl:call-template>

  <!-- emit methods -->
  <xsl:for-each select="method">
    <xsl:call-template name="genMethod">
      <xsl:with-param name="ifname" select="$ifname" />
      <xsl:with-param name="methodname" select="@name" />
    </xsl:call-template>
  </xsl:for-each>

</xsl:template>

<xsl:template name="genIface">
  <xsl:param name="ifname" />
  <xsl:param name="filename" />

  <xsl:variable name="wsmap" select="@wsmap" />

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filename" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

  <xsl:text>import java.util.List;&#10;&#10;</xsl:text>

  <xsl:apply-templates select="desc" mode="interface"/>

  <xsl:choose>
    <xsl:when test="($wsmap='struct') and ($G_vboxGlueStyle='jaxws')">
      <xsl:value-of select="concat('public class ', $ifname, ' {&#10;&#10;')" />
      <xsl:call-template name="genStructWrapperJaxws">
        <xsl:with-param name="ifname" select="$ifname" />
      </xsl:call-template>
    </xsl:when>

    <xsl:otherwise>
      <xsl:variable name="extends" select="//interface[@name=$ifname]/@extends" />
      <xsl:choose>
        <xsl:when test="($extends = '$unknown') or ($extends = '$dispatched') or ($extends = '$errorinfo')">
          <xsl:value-of select="concat('public class ', $ifname, ' extends IUnknown {&#10;&#10;')" />
        </xsl:when>
        <xsl:when test="//interface[@name=$extends]">
          <xsl:value-of select="concat('public class ', $ifname, ' extends ', $extends, ' {&#10;&#10;')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="fatalError">
            <xsl:with-param name="msg" select="concat('Interface generation: interface &quot;', $ifname, '&quot; has invalid &quot;extends&quot; value ', $extends, '.')" />
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:call-template name="genIfaceWrapper">
        <xsl:with-param name="ifname" select="$ifname" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

  <!-- end of class -->
  <xsl:value-of select="'}&#10;'" />

  <xsl:call-template name="endFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

</xsl:template>

<xsl:template name="genCb">
  <xsl:param name="ifname" />
  <xsl:param name="filename" />
  <xsl:param name="filenameimpl" />

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filename" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

  <xsl:text>import java.util.List;&#10;</xsl:text>

  <xsl:value-of select="concat('public interface ', $ifname, ' {&#10;')" />

  <!-- emit methods declarations-->
  <xsl:for-each select="method">
    <xsl:call-template name="genCbMethodDecl">
      <xsl:with-param name="ifname" select="$ifname" />
      <xsl:with-param name="methodname" select="@name" />
    </xsl:call-template>
  </xsl:for-each>

  <xsl:value-of select="'}&#10;&#10;'" />

  <xsl:call-template name="endFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filenameimpl" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

  <xsl:text>import java.util.List;&#10;</xsl:text>

  <xsl:variable name="backtype">
    <xsl:call-template name="typeIdl2Back">
      <xsl:with-param name="type" select="$ifname" />
    </xsl:call-template>
  </xsl:variable>

  <!-- emit glue methods body -->
  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
       <xsl:value-of select="concat('class ', $ifname, 'Impl  extends nsISupportsBase implements ', $backtype, ' {&#10;')" />
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:value-of select="concat('public class ', $ifname, 'Impl {&#10;')" />
    </xsl:when>
  </xsl:choose>

  <xsl:value-of select="concat('   ', $ifname, ' sink;&#10;')" />

  <xsl:value-of select="concat('   ', $ifname, 'Impl(', $ifname,' sink) {&#10;')" />
  <xsl:value-of        select="'      this.sink = sink;&#10;'" />
  <xsl:value-of        select="'    }&#10;'" />

  <!-- emit methods implementations -->
  <xsl:for-each select="method">
    <xsl:call-template name="genCbMethodImpl">
      <xsl:with-param name="ifname" select="$ifname" />
      <xsl:with-param name="methodname" select="@name" />
    </xsl:call-template>
  </xsl:for-each>

  <xsl:value-of select="'}&#10;&#10;'" />

  <xsl:call-template name="endFile">
      <xsl:with-param name="file" select="$filenameimpl" />
    </xsl:call-template>
</xsl:template>

<xsl:template name="emitHandwritten">

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'Holder.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

 <xsl:text><![CDATA[
public class Holder<T>
{
   public T value;

   public Holder()
   {
   }
   public Holder(T value)
   {
       this.value = value;
   }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'Holder.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'VBoxException.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

 <xsl:text><![CDATA[
public class VBoxException extends RuntimeException
{
   private Throwable wrapped;
   private String    msg;

   public VBoxException(Throwable wrapped, String msg)
   {
      this.wrapped = wrapped;
      this.msg = msg;
   }
   public Throwable getWrapped()
   {
       return wrapped;
   }
   public String getMessage()
   {
       return msg;
   }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'VBoxException.java'" />
 </xsl:call-template>


</xsl:template>

<xsl:template name="emitHandwrittenXpcom">

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'IUnknown.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackageCom" />
  </xsl:call-template>

 <xsl:text><![CDATA[
public class IUnknown
{
   private Object obj;
   public IUnknown(Object obj)
   {
       this.obj = obj;
   }

   public Object getWrapped()
   {
       return this.obj;
   }

   public void setWrapped(Object obj)
   {
       this.obj = obj;
   }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'IUnknown.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'Helper.java'" />
   <xsl:with-param name="package" select="$G_virtualBoxPackageCom" />
 </xsl:call-template>

<xsl:text><![CDATA[

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

public class Helper {
    public static List<Short> wrap(byte[] vals) {
        if (vals==null)
            return null;

        List<Short> ret = new ArrayList<Short>(vals.length);
        for (short v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static List<Integer> wrap(int[] vals) {
        if (vals==null)
             return null;

        List<Integer> ret = new ArrayList<Integer>(vals.length);
        for (int v : vals) {
             ret.add(v);
        }
        return ret;
    }

    public static List<Long> wrap(long[] vals) {
        if (vals==null)
             return null;

        List<Long> ret = new ArrayList<Long>(vals.length);
        for (long v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static List<String> wrap(String[] vals) {
        if (vals==null)
            return null;
        List<String> ret = new ArrayList<String>(vals.length);
        for (String v : vals) {
            ret.add(v);
        }
        return ret;
    }

    public static <T> List<T> wrap(Class<T> wrapperClass, T[] thisPtrs) {
        if (thisPtrs==null)
             return null;

        List<T> ret = new ArrayList<T>(thisPtrs.length);
        for (T thisPtr : thisPtrs) {
             ret.add(thisPtr);
        }
        return ret;
    }

    public static <T> List<T> wrapEnum(Class<T> wrapperClass, long values[]) {
        try {
            if (values==null)
                 return null;
            Constructor<T> c = wrapperClass.getConstructor(int.class);
            List<T> ret = new ArrayList<T>(values.length);
            for (long v : values) {
                ret.add(c.newInstance(v));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }
    public static short[] unwrapUShort(List<Short> vals) {
        if (vals==null)
           return null;

        short[] ret = new short[vals.size()];
        int i = 0;
        for (short l : vals) {
                ret[i++] = l;
        }
        return ret;
    }

    public static int[] unwrapInteger(List<Integer> vals) {
        if (vals == null)
           return null;

        int[] ret = new int[vals.size()];
        int i = 0;
        for (int l : vals) {
                ret[i++] = l;
        }
        return ret;
    }

    public static long[] unwrapULong(List<Long> vals) {
        if (vals == null)
           return null;

        long[] ret = new long[vals.size()];
        int i = 0;
        for (long l : vals) {
                ret[i++] = l;
        }
        return ret;
    }

    public static boolean[] unwrapBoolean(List<Boolean> vals) {
        if (vals==null)
           return null;

        boolean[] ret = new boolean[vals.size()];
        int i = 0;
        for (boolean l : vals) {
                ret[i++] = l;
        }
        return ret;
    }

    public static String[] unwrapStr(List<String> vals) {
        if (vals==null)
            return null;

        String[] ret = new String[vals.size()];
        int i = 0;
        for (String l : vals) {
                ret[i++] = l;
        }
        return ret;
    }

    public static <T extends Enum <T>> long[] unwrapEnum(Class<T> enumClass, List<T> values) {
        if (values == null)  return null;

        long result[] = new long[values.size()];
        try {
           java.lang.reflect.Method valueM = enumClass.getMethod("value");
           int i = 0;
           for (T v : values) {
             result[i++] = (Integer)valueM.invoke(v);
           }
           return result;
        } catch (NoSuchMethodException e) {
           throw new AssertionError(e);
        } catch(SecurityException e) {
           throw new AssertionError(e);
        } catch (IllegalAccessException e) {
           throw new AssertionError(e);
        } catch (IllegalArgumentException e) {
           throw new AssertionError(e);
        } catch (InvocationTargetException e) {
           throw new AssertionError(e);
        }
    }

    public static <T1, T2> List<T1> wrap2(Class<T1> wrapperClass1, Class<T2> wrapperClass2, T2[] thisPtrs) {
        try {
            if (thisPtrs==null)
                return null;

            Constructor<T1> c = wrapperClass1.getConstructor(wrapperClass2);
            List<T1> ret = new ArrayList<T1>(thisPtrs.length);
            for (T2 thisPtr : thisPtrs) {
                ret.add(c.newInstance(thisPtr));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    @SuppressWarnings( "unchecked")
    public static <T> T[] unwrap(Class<T> wrapperClass, List<T> thisPtrs) {
        if (thisPtrs==null)
            return null;
        if (thisPtrs.size() == 0)
            return null;
        return (T[])thisPtrs.toArray((T[])Array.newInstance(wrapperClass, thisPtrs.size()));
    }

    @SuppressWarnings( "unchecked" )
    public static <T> T queryInterface(Object obj, String uuid, Class<T> iface)
    {
        return (T)queryInterface(obj, uuid);
    }

    public static Object queryInterface(Object obj, String uuid)
    {
         try {
            /* Kind of ugly, but does the job of casting */
            org.mozilla.xpcom.Mozilla moz = org.mozilla.xpcom.Mozilla.getInstance();
            long xpobj = moz.wrapJavaObject(obj, uuid);
            return moz.wrapXPCOMObject(xpobj, uuid);
         } catch (Exception e) {
             return null;
         }
    }

    @SuppressWarnings("unchecked")
    public static <T1 extends IUnknown,T2> T2[] unwrap2(Class<T1> wrapperClass1, Class<T2> wrapperClass2, List<T1> thisPtrs) {
        if (thisPtrs==null)  return null;

        T2 ret[] = (T2[])Array.newInstance(wrapperClass2, thisPtrs.size());
        int i = 0;
        for (T1 obj : thisPtrs) {
          ret[i++] = (T2)obj.getWrapped();
        }
        return ret;
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'Helper.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

 <xsl:text><![CDATA[

import java.io.File;

import org.mozilla.xpcom.*;
import org.mozilla.interfaces.*;

public class VirtualBoxManager
{
    private Mozilla             mozilla;
    private IVirtualBox         vbox;
    private nsIComponentManager componentManager;

    private VirtualBoxManager(Mozilla mozilla)
    {
        this.mozilla = mozilla;
        this.componentManager = mozilla.getComponentManager();
        this.vbox = new IVirtualBox((org.mozilla.interfaces.IVirtualBox) this.componentManager
                    .createInstanceByContractID("@virtualbox.org/VirtualBox;1",
                                                null,
                                                org.mozilla.interfaces.IVirtualBox.IVIRTUALBOX_IID));
    }

    public void connect(String url, String username, String passwd)
    {
        throw new RuntimeException("Connect doesn't make sense for local bindings");
    }

    public void disconnect()
    {
        throw new RuntimeException("Disconnect doesn't make sense for local bindings");
    }

    public static void initPerThread()
    {
    }

    public static void deinitPerThread()
    {
    }

    public IVirtualBox getVBox()
    {
        return this.vbox;
    }

    public ISession getSessionObject()
    {
        return new ISession((org.mozilla.interfaces.ISession) componentManager
                .createInstanceByContractID("@virtualbox.org/Session;1", null,
                                            org.mozilla.interfaces.ISession.ISESSION_IID));
    }

    public ISession openMachineSession(IMachine m) throws Exception
    {
        ISession s = getSessionObject();
        m.lockMachine(s, LockType.Shared);
        return s;
    }

    public void closeMachineSession(ISession s)
    {
          if (s != null)
            s.unlockMachine();
    }

    private static boolean hasInstance = false;
    private static boolean isMozillaInited = false;

    public static synchronized VirtualBoxManager createInstance(String home)
    {
        if (hasInstance)
            throw new VBoxException(null, "only one instance at the time allowed");
        if (home == null || "".equals(home))
            home = System.getProperty("vbox.home");

        if (home == null)
            throw new RuntimeException("vbox.home Java property must be defined to use XPCOM bridge");

        File grePath = new File(home);

        Mozilla mozilla = Mozilla.getInstance();
        if (!isMozillaInited)
        {
           mozilla.initialize(grePath);
           try {
             mozilla.initXPCOM(grePath, null);
             isMozillaInited = true;
           } catch (Exception e) {
             e.printStackTrace();
             return null;
           }
        }

        hasInstance = true;

        return new VirtualBoxManager(mozilla);
    }

    public IEventListener createListener(Object sink)
    {
         return new IEventListener(new EventListenerImpl(sink));
    }
    public void cleanup()
    {
        deinitPerThread();
        // cleanup, we don't do that, as XPCOM bridge doesn't cleanly
        // shuts down, so we prefer to avoid native shutdown
        // mozilla.shutdownXPCOM(null);
        mozilla = null;
        hasInstance = false;
    }

    public boolean progressBar(IProgress p, int wait)
    {
        long end = System.currentTimeMillis() + wait;
        while (!p.getCompleted())
        {
            mozilla.waitForEvents(0);
            p.waitForCompletion(wait);
            if (System.currentTimeMillis() >= end)
                return false;
        }

        return true;
    }

    public boolean startVm(String name, String type, int timeout)
    {
        IMachine m = vbox.findMachine(name);
        if (m == null)
            return false;
        ISession session = getSessionObject();

        if (type == null)
            type = "gui";
        IProgress p = m.launchVMProcess(session, type, "");
        progressBar(p, timeout);
        session.unlockMachine();
        return true;
    }

    public void waitForEvents(long tmo)
    {
        mozilla.waitForEvents(tmo);
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'EventListenerImpl.java'" />
   <xsl:with-param name="package" select="$G_virtualBoxPackage" />
 </xsl:call-template>

 <xsl:text><![CDATA[
 import org.mozilla.interfaces.*;

 public class EventListenerImpl extends nsISupportsBase implements org.mozilla.interfaces.IEventListener
 {
    private Object obj;
    private java.lang.reflect.Method handleEvent;
    EventListenerImpl(Object obj)
    {
       this.obj = obj;
       try {
             this.handleEvent = obj.getClass().getMethod("handleEvent", IEvent.class);
        } catch (Exception e) {
             e.printStackTrace();
        }
     }
     public void handleEvent(org.mozilla.interfaces.IEvent ev)
     {
       try {
          if (obj != null && handleEvent != null)
              handleEvent.invoke(obj, ev != null ? new IEvent(ev) : null);
       } catch (Exception e) {
               e.printStackTrace();
       }
      }
 }]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'EventListenerImpl.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'VBoxObjectBase.java'" />
   <xsl:with-param name="package" select="$G_virtualBoxPackage" />
 </xsl:call-template>

<xsl:text><![CDATA[
abstract class nsISupportsBase implements org.mozilla.interfaces.nsISupports
{
    public  org.mozilla.interfaces.nsISupports queryInterface(String iid)
    {
        return org.mozilla.xpcom.Mozilla.queryInterface(this, iid);
    }
}

]]></xsl:text><xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'VBoxObjectBase.java'" />
 </xsl:call-template>

</xsl:template>


<xsl:template name="emitHandwrittenMscom">

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'IUnknown.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackageCom" />
  </xsl:call-template>

 <xsl:text><![CDATA[
public class IUnknown
{
   private Object obj;
   public IUnknown(Object obj)
   {
       this.obj = obj;
   }

   public Object getWrapped()
   {
       return this.obj;
   }

   public void setWrapped(Object obj)
   {
       this.obj = obj;
   }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'IUnknown.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'Helper.java'" />
   <xsl:with-param name="package" select="$G_virtualBoxPackageCom" />
 </xsl:call-template>

<xsl:text><![CDATA[

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import com.jacob.com.*;

public class Helper {
    public static List<Short> wrap(short[] vals) {
        if (vals==null) return null;
        if (vals.length == 0)  return Collections.emptyList();

        List<Short> ret = new ArrayList<Short>(vals.length);
        for (short v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static List<Integer> wrap(int[] vals) {
        if (vals == null) return null;
        if (vals.length == 0) return Collections.emptyList();

        List<Integer> ret = new ArrayList<Integer>(vals.length);
        for (int v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static List<Long> wrap(long[] vals) {
        if (vals==null) return null;
        if (vals.length == 0)  return Collections.emptyList();

        List<Long> ret = new ArrayList<Long>(vals.length);
        for (long v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static List<String> wrap(String[] vals) {
        if (vals==null) return null;
        if (vals.length == 0)  return Collections.emptyList();

        List<String> ret = new ArrayList<String>(vals.length);
        for (String v : vals) {
                ret.add(v);
        }
        return ret;
    }

    public static <T> T wrapDispatch(Class<T> wrapperClass, Dispatch d)
    {
      try {
         if (d == null || d.m_pDispatch == 0)
              return null;
         Constructor<T> c = wrapperClass.getConstructor(Dispatch.class);
         return (T)c.newInstance(d);
      } catch (NoSuchMethodException e) {
         throw new AssertionError(e);
      } catch (InstantiationException e) {
         throw new AssertionError(e);
      } catch (IllegalAccessException e) {
         throw new AssertionError(e);
      } catch (InvocationTargetException e) {
         throw new AssertionError(e);
      }
    }

    @SuppressWarnings("unchecked")
    public static <T> Object wrapVariant(Class<T> wrapperClass, Variant v)
    {
       if (v == null)
          return null;

       short vt = v.getvt();
       switch (vt)
       {
           case Variant.VariantNull:
              return null;
           case Variant.VariantBoolean:
              return v.getBoolean();
           case Variant.VariantByte:
              return v.getByte();
           case Variant.VariantShort:
              return v.getShort();
           case Variant.VariantInt:
              return v.getInt();
           case Variant.VariantLongInt:
              return v.getLong();
           case Variant.VariantString:
              return v.getString();
           case Variant.VariantDispatch:
              return wrapDispatch(wrapperClass, v.getDispatch());
           default:
              throw new RuntimeException("unhandled variant type "+vt);
       }
    }

    public static byte[] wrapBytes(SafeArray sa) {
        if (sa==null)  return null;

        int saLen = sa.getUBound() - sa.getLBound() + 1;

        byte[] ret = new byte[saLen];
        int j = 0;
        for (int i = sa.getLBound(); i <= sa.getUBound(); i++)
        {
           Variant v = sa.getVariant(i);
           // come upo with more effective approach!!!
           ret[j++] = v.getByte();
        }
        return ret;
    }

    @SuppressWarnings("unchecked")
    public static <T> List<T> wrap(Class<T> wrapperClass, SafeArray sa) {
        if (sa==null)  return null;

        int saLen = sa.getUBound() - sa.getLBound() + 1;
        if (saLen == 0) return Collections.emptyList();

        List<T> ret = new ArrayList<T>(saLen);
        for (int i = sa.getLBound(); i <= sa.getUBound(); i++)
        {
           Variant v = sa.getVariant(i);
           ret.add((T)wrapVariant(wrapperClass, v));
        }
        return ret;
    }

    public static <T> List<T> wrapEnum(Class<T> wrapperClass, SafeArray sa) {
        try {
             if (sa==null) return null;

             int saLen = sa.getUBound() - sa.getLBound() + 1;
             if (saLen == 0) return Collections.emptyList();
             List<T> ret = new ArrayList<T>(saLen);
             Constructor<T> c = wrapperClass.getConstructor(int.class);
             for (int i = sa.getLBound(); i <= sa.getUBound(); i++)
             {
                 Variant v = sa.getVariant(i);
                 ret.add(c.newInstance(v.getInt()));
             }
             return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    public static SafeArray unwrapInt(List<Integer> vals) {
        if (vals==null) return null;
        SafeArray ret = new SafeArray(Variant.VariantInt, vals.size());
        int i = 0;
        for (int l : vals) {
                ret.setInt(i++,  l);
        }
        return ret;
    }

    public static SafeArray unwrapLong(List<Long> vals) {
        if (vals==null) return null;
        SafeArray ret = new SafeArray(Variant.VariantLongInt, vals.size());
        int i = 0;
        for (long l : vals) {
                ret.setLong(i++,  l);
        }
        return ret;
    }

    public static SafeArray unwrapBool(List<Boolean> vals) {
        if (vals==null) return null;

        SafeArray result = new SafeArray(Variant.VariantBoolean, vals.size());
        int i = 0;
        for (boolean l : vals) {
                result.setBoolean(i++, l);
        }
        return result;
    }


    public static SafeArray unwrapBytes(byte[] vals) {
        if (vals==null)  return null;

        SafeArray result = new SafeArray(Variant.VariantByte, vals.length);
        int i = 0;
        for (byte l : vals) {
                result.setByte(i++, l);
        }
        return result;
    }


    public static <T extends Enum <T>> SafeArray unwrapEnum(Class<T> enumClass, List<T> values) {
        if (values == null)  return null;

        SafeArray result = new SafeArray(Variant.VariantInt, values.size());
        try {
           java.lang.reflect.Method valueM = enumClass.getMethod("value");
           int i = 0;
           for (T v : values) {
             result.setInt(i++, (Integer)valueM.invoke(v));
           }
           return result;
        } catch (NoSuchMethodException e) {
           throw new AssertionError(e);
        } catch(SecurityException e) {
           throw new AssertionError(e);
        } catch (IllegalAccessException e) {
           throw new AssertionError(e);
        } catch (IllegalArgumentException e) {
           throw new AssertionError(e);
        } catch (InvocationTargetException e) {
           throw new AssertionError(e);
        }
    }
    public static SafeArray unwrapString(List<String> vals) {
        if (vals==null)
              return null;
        SafeArray result = new SafeArray(Variant.VariantString, vals.size());
        int i = 0;
        for (String l : vals) {
                result.setString(i++, l);
        }
        return result;
    }

    public static <T1, T2> List<T1> wrap2(Class<T1> wrapperClass1, Class<T2> wrapperClass2, T2[] thisPtrs) {
        try {
            if (thisPtrs==null) return null;
            if (thisPtrs.length == 0)  return Collections.emptyList();

            Constructor<T1> c = wrapperClass1.getConstructor(wrapperClass2);
            List<T1> ret = new ArrayList<T1>(thisPtrs.length);
            for (T2 thisPtr : thisPtrs) {
                ret.add(c.newInstance(thisPtr));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    @SuppressWarnings("unchecked")
    public static <T> T[] unwrap(Class<T> wrapperClass, List<T> thisPtrs) {
        if (thisPtrs==null) return null;
        return (T[])thisPtrs.toArray((T[])Array.newInstance(wrapperClass, thisPtrs.size()));
    }

    @SuppressWarnings("unchecked")
    public static <T1 extends IUnknown,T2> T2[] unwrap2(Class<T1> wrapperClass1, Class<T2> wrapperClass2, List<T1> thisPtrs) {
        if (thisPtrs==null) return null;

        T2 ret[] = (T2[])Array.newInstance(wrapperClass2, thisPtrs.size());
        int i = 0;
        for (T1 obj : thisPtrs) {
          ret[i++] = (T2)obj.getWrapped();
        }
        return ret;
    }

    /* We have very long invoke lists sometimes */
    public static Variant invoke(Dispatch d, String method, Object ... args)
    {
        return Dispatch.callN(d, method, args);
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'Helper.java'" />
 </xsl:call-template>


 <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

 <xsl:text><![CDATA[

import com.jacob.activeX.ActiveXComponent;
import com.jacob.com.ComThread;
import com.jacob.com.Dispatch;
import com.jacob.com.Variant;
import com.jacob.com.SafeArray;
import com.jacob.com.DispatchEvents;

public class VirtualBoxManager
{
    private IVirtualBox         vbox;

    private VirtualBoxManager()
    {
        initPerThread();
        vbox = new IVirtualBox(new ActiveXComponent("VirtualBox.VirtualBox"));
    }

    public static void initPerThread()
    {
         ComThread.InitMTA();
    }

    public static void deinitPerThread()
    {
         ComThread.Release();
    }

    public void connect(String url, String username, String passwd)
    {
        throw new RuntimeException("Connect doesn't make sense for local bindings");
    }

    public void disconnect()
    {
        throw new RuntimeException("Disconnect doesn't make sense for local bindings");
    }

    public IVirtualBox getVBox()
    {
        return this.vbox;
    }

    public ISession getSessionObject()
    {
        return new ISession(new ActiveXComponent("VirtualBox.Session"));
    }

    public ISession openMachineSession(IMachine m)
    {
        ISession s = getSessionObject();
        m.lockMachine(s, LockType.Shared);
        return s;
    }

    public void closeMachineSession(ISession s)
    {
          if (s != null)
          s.unlockMachine();
    }

    private static boolean hasInstance = false;

    public static synchronized VirtualBoxManager createInstance(String home)
    {
        if (hasInstance)
          throw new VBoxException(null, "only one instance at the time allowed");

        hasInstance = true;
        return new VirtualBoxManager();
    }

    public void cleanup()
    {
        deinitPerThread();
        hasInstance = false;
    }

    public boolean progressBar(IProgress p, int wait)
    {
        long end = System.currentTimeMillis() + wait;
        while (!p.getCompleted())
        {
            p.waitForCompletion(wait);
            if (System.currentTimeMillis() >= end)
                return false;
        }

        return true;
    }

    public boolean startVm(String name, String type, int timeout)
    {
        IMachine m = vbox.findMachine(name);
        if (m == null)
            return false;
        ISession session = getSessionObject();
        if (type == null)
            type = "gui";
        IProgress p = m.launchVMProcess(session, type, "");
        progressBar(p, timeout);
        session.unlockMachine();
        return true;
    }

    public void waitForEvents(long tmo)
    {
        // what to do here?
        try {
          Thread.sleep(tmo);
        } catch (InterruptedException ie) {
        }
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
 </xsl:call-template>

</xsl:template>

<xsl:template name="emitHandwrittenJaxws">

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'IUnknown.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
  </xsl:call-template>

 <xsl:text><![CDATA[
public class IUnknown
{
   protected String obj;
   protected final  VboxPortType port;

   public IUnknown(String obj, VboxPortType port)
   {
       this.obj = obj;
       this.port = port;
   }

   public final String getWrapped()
   {
       return this.obj;
   }

   public final VboxPortType getRemoteWSPort()
   {
      return this.port;
   }

   public synchronized void releaseRemote() throws WebServiceException
   {
      if (obj == null) {
        return;
      }
      try {
          this.port.iManagedObjectRefRelease(obj);
          this.obj = null;
      } catch (InvalidObjectFaultMsg e) {
          throw new WebServiceException(e);
      } catch (RuntimeFaultMsg e) {
          throw new WebServiceException(e);
      }
   }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'IUnknown.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'Helper.java'" />
   <xsl:with-param name="package" select="$G_virtualBoxPackage" />
 </xsl:call-template>

<xsl:text><![CDATA[

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.math.BigInteger;

public class Helper {
    public static <T> List<T> wrap(Class<T> wrapperClass, VboxPortType pt, List<String> thisPtrs) {
        try {
            if(thisPtrs==null) return null;

            Constructor<T> c = wrapperClass.getConstructor(String.class, VboxPortType.class);
            List<T> ret = new ArrayList<T>(thisPtrs.size());
            for (String thisPtr : thisPtrs) {
                ret.add(c.newInstance(thisPtr,pt));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    public static <T1, T2> List<T1> wrap2(Class<T1> wrapperClass1, Class<T2> wrapperClass2, VboxPortType pt, List<T2> thisPtrs) {
        try {
            if(thisPtrs==null)  return null;

            Constructor<T1> c = wrapperClass1.getConstructor(wrapperClass2, VboxPortType.class);
            List<T1> ret = new ArrayList<T1>(thisPtrs.size());
            for (T2 thisPtr : thisPtrs) {
                ret.add(c.newInstance(thisPtr,pt));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (InstantiationException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    public static <T extends IUnknown> List<String> unwrap(List<T> thisPtrs) {
        if (thisPtrs==null)  return null;

        List<String> ret = new ArrayList<String>(thisPtrs.size());
        for (T obj : thisPtrs) {
          ret.add(obj.getWrapped());
        }
        return ret;
    }

    @SuppressWarnings("unchecked" )
    public static <T1 extends Enum <T1>, T2 extends Enum <T2>> List<T2> convertEnums(Class<T1> fromClass,
                                                                                     Class<T2> toClass,
                                                                                     List<T1>  values) {
        try {
            if (values==null)
                 return null;
            java.lang.reflect.Method fromValue = toClass.getMethod("fromValue", String.class);
            List<T2> ret = new ArrayList<T2>(values.size());
            for (T1 v : values) {
                // static method is called with null this
                ret.add((T2)fromValue.invoke(null, v.name()));
            }
            return ret;
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        } catch (IllegalAccessException e) {
            throw new AssertionError(e);
        } catch (InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }

    /* Pretty naive Base64 encoder/decoder. */
    private static final char[] valToChar = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".toCharArray();
    private static final int[] charToVal = new int[256];

    /* Initialize recoding alphabet. */
    static
    {
        for (int i = 0; i < charToVal.length; i++)
            charToVal[i] = -1;

        for (int i = 0; i < valToChar.length; i++)
           charToVal[valToChar[i]] = i;

        charToVal['='] = 0;
    }

    public static String encodeBase64(byte[] data)
    {
        if (data == null)
            return null;

        if (data.length == 0)
            return "";

        int fullTriplets = data.length / 3;
        int resultLen = ((data.length - 1) / 3 + 1) * 4;
        char[] result = new char[resultLen];
        int dataIndex = 0, stringIndex = 0;

        for (int i = 0; i < fullTriplets; i++)
        {
            int ch1 = data[dataIndex++] & 0xff;
            result[stringIndex++] = valToChar[ch1 >> 2];
            int ch2 = data[dataIndex++] & 0xff;
            result[stringIndex++] = valToChar[((ch1 << 4) & 0x3f) | (ch2 >> 4)];
            int ch3 = data[dataIndex++] & 0xff;
            result[stringIndex++] = valToChar[((ch2 << 2) & 0x3f) | (ch3 >> 6)];
            result[stringIndex++] = valToChar[ch3 & 0x3f];
        }

        switch (data.length - dataIndex)
        {
            case 0:
                // do nothing
                break;
            case 1:
            {
                int ch1 = data[dataIndex++] & 0xff;
                result[stringIndex++] = valToChar[ch1 >> 2];
                result[stringIndex++] = valToChar[(ch1 << 4) & 0x3f];
                result[stringIndex++] = '=';
                result[stringIndex++] = '=';
                break;
            }
            case 2:
            {
                int ch1 = data[dataIndex++] & 0xff;
                result[stringIndex++] = valToChar[ch1 >> 2];
                int ch2 = data[dataIndex++] & 0xff;
                result[stringIndex++] = valToChar[((ch1 << 4) & 0x3f) | (ch2 >> 4)];
                result[stringIndex++] = valToChar[(ch2 << 2) & 0x3f];
                result[stringIndex++] = '=';
                break;
            }
            default:
                throw new RuntimeException("bug!");
        }

        return new String(result);
    }

    private static int skipInvalid(String str, int stringIndex)
    {
        while (charToVal[str.charAt(stringIndex)] < 0)
            stringIndex++;

        return stringIndex;
    }

    public static byte[] decodeBase64(String str)
    {
        if (str == null)
            return null;

        int stringLength = str.length();
        if (stringLength == 0)
            return new byte[0];

        int validChars = 0, padChars = 0;
        for (int i = 0; i < str.length(); i++)
        {
            char ch = str.charAt(i);

            if (charToVal[ch] >= 0)
                validChars++;

            if (ch == '=')
                padChars++;
        }

        if ((validChars * 3 % 4) != 0)
            throw new RuntimeException("invalid encoded string "+str);

        int resultLength = validChars * 3 / 4 - padChars;
        byte[] result = new byte[resultLength];

        int dataIndex = 0, stringIndex = 0;
        int quadraplets = validChars / 4;

        for (int i=0; i<quadraplets; i++)
        {
            stringIndex = skipInvalid(str, stringIndex);
            int ch1 = str.charAt(stringIndex++);
            stringIndex = skipInvalid(str, stringIndex);
            int ch2 = str.charAt(stringIndex++);
            stringIndex = skipInvalid(str, stringIndex);
            int ch3 = str.charAt(stringIndex++);
            stringIndex = skipInvalid(str, stringIndex);
            int ch4 = str.charAt(stringIndex++);

            result[dataIndex++] = (byte)(((charToVal[ch1] << 2) | charToVal[ch2] >> 4) & 0xff);
            /* we check this to ensure that we don't override data with '=' padding. */
            if (dataIndex < result.length)
                result[dataIndex++] = (byte)(((charToVal[ch2] << 4) | charToVal[ch3] >> 2) & 0xff);
            if (dataIndex < result.length)
                result[dataIndex++] = (byte)(((charToVal[ch3] << 6) | charToVal[ch4]) & 0xff);
        }

        return result;
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'Helper.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
    <xsl:with-param name="package" select="$G_virtualBoxPackage" />
 </xsl:call-template>

import java.net.URL;
import java.math.BigInteger;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import javax.xml.namespace.QName;
import javax.xml.ws.BindingProvider;
import javax.xml.ws.Holder;
import javax.xml.ws.WebServiceException;

class PortPool
{
    private final static String wsdlFile = <xsl:value-of select="$G_virtualBoxWsdl" />;

 <xsl:text><![CDATA[
private Map<VboxPortType, Integer> known;
    private boolean initStarted;
    private VboxService svc;

    PortPool(boolean usePreinit)
    {
        known = new HashMap<VboxPortType, Integer>();

        if (usePreinit)
        {
           new Thread(new Runnable()
              {
                 public void run()
                 {
                    // need to sync on something else but 'this'
                    synchronized (known)
                    {
                      initStarted = true;
                      known.notify();
                    }

                    preinit();
                 }
               }).start();

           synchronized (known)
           {
              while (!initStarted)
              {
                 try {
                   known.wait();
                 } catch (InterruptedException e) {
                 break;
                 }
              }
           }
        }
    }

    private synchronized void preinit()
    {
        VboxPortType port = getPort();
        releasePort(port);
    }

    synchronized VboxPortType getPort()
    {
        VboxPortType port = null;
        int ttl = 0;

        for (VboxPortType cur: known.keySet())
        {
            int value = known.get(cur);
            if ((value & 0x10000) == 0)
            {
                port = cur;
                ttl = value & 0xffff;
                break;
            }
        }

        if (port == null)
        {
            if (svc == null) {
                URL wsdl = PortPool.class.getClassLoader().getResource(wsdlFile);
                if (wsdl == null)
                    throw new LinkageError(wsdlFile+" not found, but it should have been in the jar");
                svc = new VboxService(wsdl,
                                      new QName("http://www.virtualbox.org/Service",
                                                "vboxService"));
            }
            port = svc.getVboxServicePort();
            // reuse this object 0x10 times
            ttl = 0x10;
        }
        // mark as used
        known.put(port, new Integer(0x10000 | ttl));
        return port;
    }

    synchronized void releasePort(VboxPortType port)
    {
        Integer val = known.get(port);
        if (val == null || val == 0)
        {
            // know you not
            return;
        }

        int v = val;
        int ttl = v & 0xffff;
        // decrement TTL, and throw away port if used too much times
        if (--ttl <= 0)
        {
            known.remove(port);
        }
        else
        {
            v = ttl; // set new TTL and clear busy bit
            known.put(port, v);
        }
    }
}


public class VirtualBoxManager
{
    private static PortPool pool = new PortPool(true);
    protected VboxPortType port;

    private IVirtualBox         vbox;

    private VirtualBoxManager()
    {
    }

    public static void initPerThread()
    {
    }

    public static void deinitPerThread()
    {
    }

    public void connect(String url, String username, String passwd)
    {
        this.port = pool.getPort();
        try {
            ((BindingProvider)port).getRequestContext().
                put(BindingProvider.ENDPOINT_ADDRESS_PROPERTY, url);
            String handle = port.iWebsessionManagerLogon(username, passwd);
            this.vbox = new IVirtualBox(handle, port);
        } catch (Throwable t) {
            if (this.port != null && pool != null) {
                pool.releasePort(this.port);
                this.port = null;
            }
            // we have to throw smth derived from RuntimeException
            throw new VBoxException(t, t.getMessage());
        }
    }

    public void connect(String url, String username, String passwd,
                        Map<String, Object> requestContext, Map<String, Object> responseContext)
    {
        this.port = pool.getPort();

        try {
            ((BindingProvider)port).getRequestContext();
            if (requestContext != null)
                ((BindingProvider)port).getRequestContext().putAll(requestContext);

            if (responseContext != null)
                ((BindingProvider)port).getResponseContext().putAll(responseContext);

            ((BindingProvider)port).getRequestContext().
                put(BindingProvider.ENDPOINT_ADDRESS_PROPERTY, url);
            String handle = port.iWebsessionManagerLogon(username, passwd);
            this.vbox = new IVirtualBox(handle, port);
        } catch (Throwable t) {
            if (this.port != null && pool != null) {
                pool.releasePort(this.port);
                this.port = null;
            }
            // we have to throw smth derived from RuntimeException
            throw new VBoxException(t, t.getMessage());
        }
    }

    public void disconnect()
    {
        try {
           if (this.vbox != null)
              port.iWebsessionManagerLogoff(this.vbox.getWrapped());
        } catch (InvalidObjectFaultMsg e) {
            throw new VBoxException(e, e.getMessage());
        } catch (RuntimeFaultMsg e) {
            throw new VBoxException(e, e.getMessage());
        } finally {
            if (this.port != null) {
                pool.releasePort(this.port);
                this.port = null;
            }
        }
    }

    public IVirtualBox getVBox()
    {
        return this.vbox;
    }

    public ISession getSessionObject()
    {
        if (this.vbox == null)
            throw new RuntimeException("connect first");
        try {
           String handle = port.iWebsessionManagerGetSessionObject(this.vbox.getWrapped());
           return new ISession(handle, port);
        } catch (InvalidObjectFaultMsg e) {
            throw new VBoxException(e, e.getMessage());
        } catch (RuntimeFaultMsg e) {
            throw new VBoxException(e, e.getMessage());
        }
    }

    public ISession openMachineSession(IMachine m) throws Exception
    {
        ISession s = getSessionObject();
        m.lockMachine(s, LockType.Shared);
        return s;
    }

    public void closeMachineSession(ISession s)
    {
          if (s != null)
            s.unlockMachine();
    }

    public static synchronized VirtualBoxManager createInstance(String home)
    {
        return new VirtualBoxManager();
    }

    public IEventListener createListener(Object sink)
    {
         throw new RuntimeException("no active listeners here");
    }
    public void cleanup()
    {
        deinitPerThread();
    }

    public boolean progressBar(IProgress p, int wait)
    {
        long end = System.currentTimeMillis() + wait;
        while (!p.getCompleted())
        {
            p.waitForCompletion(wait);
            if (System.currentTimeMillis() >= end)
                return false;
        }

        return true;
    }

    public boolean startVm(String name, String type, int timeout)
    {
        IMachine m = vbox.findMachine(name);
        if (m == null)
            return false;
        ISession session = getSessionObject();

        if (type == null)
            type = "gui";
        IProgress p = m.launchVMProcess(session, type, "");
        progressBar(p, timeout);
        session.unlockMachine();
        return true;
    }

    public void waitForEvents(long tmo)
    {
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'VirtualBoxManager.java'" />
 </xsl:call-template>

</xsl:template>


<xsl:template match="/">

  <xsl:if test="not($G_vboxApiSuffix)">
    <xsl:call-template name="fatalError">
      <xsl:with-param name="msg" select="'G_vboxApiSuffix must be given'" />
    </xsl:call-template>
  </xsl:if>

  <!-- Handwritten files -->
  <xsl:call-template name="emitHandwritten"/>

  <xsl:choose>
    <xsl:when test="$G_vboxGlueStyle='xpcom'">
      <xsl:call-template name="emitHandwrittenXpcom"/>
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='mscom'">
      <xsl:call-template name="emitHandwrittenMscom"/>
    </xsl:when>

    <xsl:when test="$G_vboxGlueStyle='jaxws'">
      <xsl:call-template name="emitHandwrittenJaxws"/>
    </xsl:when>

    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="'Style unknown (root)'" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

  <!-- Enums -->
  <xsl:for-each select="//enum">
    <xsl:call-template name="genEnum">
      <xsl:with-param name="enumname" select="@name" />
      <xsl:with-param name="filename" select="concat(@name, '.java')" />
    </xsl:call-template>
  </xsl:for-each>

  <!-- Interfaces -->
  <xsl:for-each select="//interface">
    <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>
    <xsl:variable name="module" select="current()/ancestor::module/@name"/>

    <xsl:choose>
      <xsl:when test="$G_vboxGlueStyle='jaxws'">
        <xsl:if test="not($module) and not(@wsmap='suppress') and not(@wsmap='global')">
          <xsl:call-template name="genIface">
            <xsl:with-param name="ifname" select="@name" />
            <xsl:with-param name="filename" select="concat(@name, '.java')" />
          </xsl:call-template>
        </xsl:if>
      </xsl:when>

      <xsl:otherwise>
        <!-- We don't need WSDL-specific interfaces here -->
        <xsl:if test="not($self_target='wsdl') and not($module)">
          <xsl:call-template name="genIface">
            <xsl:with-param name="ifname" select="@name" />
            <xsl:with-param name="filename" select="concat(@name, '.java')" />
          </xsl:call-template>
        </xsl:if>
      </xsl:otherwise>

    </xsl:choose>
  </xsl:for-each>
</xsl:template>
</xsl:stylesheet>
