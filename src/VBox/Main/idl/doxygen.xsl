<?xml version="1.0"?>

<!--
 *  A template to generate a generic IDL file from the generic interface
 *  definition expressed in XML. The generated file is intended solely to
 *  generate the documentation using Doxygen.

     Copyright (C) 2006-2007 innotek GmbH

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" indent="yes"/>

<xsl:strip-space elements="*"/>


<!--
//  helper definitions
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  uncapitalizes the first letter only if the second one is not capital
 *  otherwise leaves the string unchanged
-->
<xsl:template name="uncapitalize">
    <xsl:param name="str" select="."/>
    <xsl:choose>
        <xsl:when test="not(contains('ABCDEFGHIJKLMNOPQRSTUVWXYZ', substring($str,2,1)))">
            <xsl:value-of select="
                concat(
                    translate(substring($str,1,1),'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz'),
                    substring($str,2)
                )
            "/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="string($str)"/>
        </xsl:otherwise>
    </xsl:choose>
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
//  Doxygen transformation rules
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  all text elements that are not explicitly matched are normalized
 *  (all whitespace chars are converted to single spaces)
-->
<!--xsl:template match="desc//text()">
    <xsl:value-of select="concat(' ',normalize-space(.),' ')"/>
</xsl:template-->

<!--
 *  all elements that are not explicitly matched are considered to be html tags
 *  and copied w/o modifications
-->
<xsl:template match="desc//*">
    <xsl:copy>
        <xsl:apply-templates/>
    </xsl:copy>
</xsl:template>

<!--
 *  paragraph
-->
<xsl:template match="desc//p">
    <xsl:text>&#x0A;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  link
-->
<xsl:template match="desc//link">
    <xsl:text>@link </xsl:text>
    <!--
     *  sometimes Doxygen is stupid and cannot resolve global enums properly,
     *  thinking they are members of the current class. Fix it by adding ::
     *  in front of any @to value that doesn't start with #.
    -->
    <xsl:choose>
        <xsl:when test="not(starts-with(@to, '#')) and not(contains(@to, '::'))">
            <xsl:text>::</xsl:text>
        </xsl:when>
    </xsl:choose>
    <!--
     *  Doxygen doesn't understand autolinks like Class::func() if Class
     *  doesn't actually contain a func with no arguments. Fix it.
    -->
    <xsl:choose>
        <xsl:when test="substring(@to, string-length(@to)-1)='()'">
            <xsl:value-of select="substring-before(@to, '()')"/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="@to"/>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <xsl:choose>
        <xsl:when test="normalize-space(text())">
            <xsl:value-of select="normalize-space(text())"/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:choose>
                <xsl:when test="starts-with(@to, '#')">
                    <xsl:value-of select="substring-after(@to, '#')"/>
                </xsl:when>
                <xsl:when test="starts-with(@to, '::')">
                    <xsl:value-of select="substring-after(@to, '::')"/>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="@to"/>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text>@endlink</xsl:text>
    <!--
     *  insert a dummy empty B element to distinctly separate @endlink
     *  from the following text
     -->
    <xsl:element name="b"/>
</xsl:template>

<!--
 *  note
-->
<xsl:template match="desc/note">
    <xsl:text>&#x0A;@note </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  see
-->
<xsl:template match="desc/see">
    <xsl:text>&#x0A;@see </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for interfaces
-->
<xsl:template match="interface/desc">
    <xsl:text>/**&#x0A;</xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:apply-templates select="note"/>
    <xsl:apply-templates select="see"/>
@par Interface ID:
<tt>{<xsl:call-template name="uppercase">
        <xsl:with-param name="str" select="../@uuid"/>
    </xsl:call-template>}</tt>
    <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for attributes
-->
<xsl:template match="attribute/desc">
    <xsl:text>/**&#x0A;</xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:apply-templates select="note"/>
    <xsl:if test="../@mod='ptr'">
        <xsl:text>

@warning This attribute is non-scriptable. In particular, this also means that an
attempt to get or set it from a process other than the process that has created and
owns the object will most likely fail or crash your application.
</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="see"/>
    <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for methods
-->
<xsl:template match="method/desc">
    <xsl:text>/**&#x0A;</xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:for-each select="../param">
        <xsl:apply-templates select="desc"/>
    </xsl:for-each>
    <xsl:apply-templates select="note"/>
    <xsl:apply-templates select="../param/desc/note"/>
    <xsl:if test="../param/@mod='ptr'">
        <xsl:text>

@warning This method is non-scriptable. In particluar, this also means that an
attempt to call it from a process other than the process that has created and
owns the object will most likely fail or crash your application.
</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="see"/>
    <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for method parameters
-->
<xsl:template match="method/param/desc">
    <xsl:text>&#x0A;@param </xsl:text>
    <xsl:value-of select="../@name"/>
    <xsl:text> </xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for interfaces
-->
<xsl:template match="enum/desc">
    <xsl:text>/**&#x0A;</xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:apply-templates select="note"/>
    <xsl:apply-templates select="see"/>
@par Interface ID:
<tt>{<xsl:call-template name="uppercase">
        <xsl:with-param name="str" select="../@uuid"/>
    </xsl:call-template>}</tt>
    <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for enum values
-->
<xsl:template match="enum/const/desc">
    <xsl:text>/** @brief </xsl:text>
    <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
    <xsl:apply-templates select="note"/>
    <xsl:apply-templates select="see"/>
    <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
//  templates
/////////////////////////////////////////////////////////////////////////////
-->


<!--
 *  header
-->
<xsl:template match="/idl">
/*
 *  DO NOT EDIT! This is a generated file.
 *
 *  Doxygen IDL definition for VirualBox Main API (COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/doxygen.xsl
 *
 *  This IDL is generated using some generic OMG IDL-like syntax SOLELY
 *  for the purpose of generating the documentation using Doxygen and
 *  is not syntactically valid.
 *
 *  DO NOT USE THIS HEADER IN ANY OTHER WAY!
 */

/** @mainpage
 *
 *  Welcome to the <b>VirtualBox Main documentation.</b> This describes the
 *  so-called VirtualBox "Main API", which comprises all public COM interfaces
 *  and components provided by the VirtualBox server and by the VirtualBox client
 *  library.
 *
 *  VirtualBox employs a client-server design, meaning that whenever any part of
 *  VirtualBox is running -- be it the Qt GUI, the VBoxManage command-line
 *  interface or any virtual machine --, a background server process named
 *  VBoxSVC runs in the background. This allows multiple processes to cooperate
 *  without conflicts. Some of the COM objects described by this Main documentation
 *  "live" in that server process, others "live" in the local client process. In
 *  any case, processes that use the Main API are using inter-process communication
 *  to communicate with these objects, but the details of this are hidden by the COM API.
 *
 *  On Windows platforms, the VirtualBox Main API uses Microsoft COM, a native COM
 *  implementation. On all other platforms, Mozilla XPCOM, an open-source COM
 *  implementation, is used.
 *
 *  All the parts that a typical VirtualBox user interacts with (the Qt GUI,
 *  the VBoxManage command-line interface and the VBoxVRDP server) are technically
 *  front-ends to the Main API and only use the interfaces that are documented
 *  in this Main API documentation. This ensures that, with any given release
 *  version of VirtualBox, all capabilities of the product that could be useful
 *  to an external client program are always exposed by way of this API.
 *
 *  The complete API is described in a source IDL file, called VirtualBox.idl.
 *  This contains all public interfaces exposed by the Main API. Two interfaces
 *  are of supreme importance and will be needed in order for any front-end program
 *  to do anything useful: these are IVirtualBox and ISession. It is recommended
 *  to read the documentation of these interfaces first.
 *
 *  @note VirtualBox.idl is automatically generated from a generic internal file
 *  to define all interfaces in a platform-independent way for documentation
 *  purposes. This generated file is not a syntactically valid IDL file and
 *  <i>must not</i> be used for programming.
 */
    <xsl:text>&#x0A;</xsl:text>
    <xsl:apply-templates/>
</xsl:template>


<!--
 *  accept all <if>s
-->
<xsl:template match="if">
    <xsl:apply-templates/>
</xsl:template>


<!--
 *  cpp_quote (ignore)
-->
<xsl:template match="cpp">
</xsl:template>


<!--
 *  #ifdef statement (@if attribute)
-->
<xsl:template match="@if" mode="begin">
    <xsl:text>#if </xsl:text>
    <xsl:value-of select="."/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>
<xsl:template match="@if" mode="end">
    <xsl:text>#endif&#x0A;</xsl:text>
</xsl:template>


<!--
 *  libraries
-->
<xsl:template match="library">
    <!-- all enums go first -->
    <xsl:apply-templates select="enum | if/enum"/>
    <!-- everything else but enums -->
    <xsl:apply-templates select="*[not(self::enum) and not(self::if[enum])]"/>
</xsl:template>


<!--
 *  interfaces
-->
<xsl:template match="interface">
    <xsl:apply-templates select="desc"/>
    <xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : </xsl:text>
    <xsl:value-of select="@extends"/>
    <xsl:text>&#x0A;{&#x0A;</xsl:text>
    <!-- attributes (properties) -->
    <xsl:apply-templates select="attribute"/>
    <!-- methods -->
    <xsl:apply-templates select="method"/>
    <!-- 'if' enclosed elements, unsorted -->
    <xsl:apply-templates select="if"/>
    <!-- -->
    <xsl:text>}; /* interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  attributes
-->
<xsl:template match="interface//attribute | collection//attribute">
  <xsl:if test="@array">
      <xsl:message terminate="yes">
          <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
          <xsl:text>'array' attributes are not supported, use 'safearray="yes"' instead.</xsl:text>
      </xsl:message>
  </xsl:if>
    <xsl:apply-templates select="@if" mode="begin"/>
    <xsl:apply-templates select="desc"/>
    <xsl:text>    </xsl:text>
    <xsl:if test="@readonly='yes'">
        <xsl:text>readonly </xsl:text>
    </xsl:if>
    <xsl:text>attribute </xsl:text>
    <xsl:apply-templates select="@type"/>
    <xsl:text> </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>;&#x0A;</xsl:text>
    <xsl:apply-templates select="@if" mode="end"/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  methods
-->
<xsl:template match="interface//method | collection//method">
    <xsl:apply-templates select="@if" mode="begin"/>
    <xsl:apply-templates select="desc"/>
    <xsl:text>    void </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:if test="param">
        <xsl:text> (&#x0A;</xsl:text>
        <xsl:for-each select="param [position() != last()]">
            <xsl:text>        </xsl:text>
            <xsl:apply-templates select="."/>
            <xsl:text>,&#x0A;</xsl:text>
        </xsl:for-each>
        <xsl:text>        </xsl:text>
        <xsl:apply-templates select="param [last()]"/>
        <xsl:text>&#x0A;    );&#x0A;</xsl:text>
    </xsl:if>
    <xsl:if test="not(param)">
        <xsl:text>();&#x0A;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@if" mode="end"/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  co-classes
-->
<xsl:template match="module/class">
    <!-- class and contract id: later -->
    <!-- CLSID_xxx declarations for XPCOM, for compatibility with Win32: later -->
</xsl:template>


<!--
 *  enumerators
-->
<xsl:template match="enumerator">
    <xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : $unknown&#x0A;{&#x0A;</xsl:text>
    <!-- HasMore -->
    <xsl:text>    void hasMore ([retval] out boolean more);&#x0A;&#x0A;</xsl:text>
    <!-- GetNext -->
    <xsl:text>    void getNext ([retval] out </xsl:text>
    <xsl:apply-templates select="@type"/>
    <xsl:text> next);&#x0A;&#x0A;</xsl:text>
    <!-- -->
    <xsl:text>}; /* interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  collections
-->
<xsl:template match="collection">
    <xsl:if test="not(@readonly='yes')">
        <xsl:message terminate="yes">
            <xsl:value-of select="concat(@name,': ')"/>
            <xsl:text>non-readonly collections are not currently supported</xsl:text>
        </xsl:message>
    </xsl:if>
    <xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : $unknown&#x0A;{&#x0A;</xsl:text>
    <!-- Count -->
    <xsl:text>    readonly attribute unsigned long count;&#x0A;&#x0A;</xsl:text>
    <!-- GetItemAt -->
    <xsl:text>    void getItemAt (in unsigned long index, [retval] out </xsl:text>
    <xsl:apply-templates select="@type"/>
    <xsl:text> item);&#x0A;&#x0A;</xsl:text>
    <!-- Enumerate -->
    <xsl:text>    void enumerate ([retval] out </xsl:text>
    <xsl:apply-templates select="@enumerator"/>
    <xsl:text> enumerator);&#x0A;&#x0A;</xsl:text>
    <!-- other extra attributes (properties) -->
    <xsl:apply-templates select="attribute"/>
    <!-- other extra methods -->
    <xsl:apply-templates select="method"/>
    <!-- 'if' enclosed elements, unsorted -->
    <xsl:apply-templates select="if"/>
    <!-- -->
    <xsl:text>}; /* interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enums
-->
<xsl:template match="enum">
    <xsl:apply-templates select="desc"/>
    <xsl:text>enum </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>&#x0A;{&#x0A;</xsl:text>
    <xsl:for-each select="const">
        <xsl:apply-templates select="desc"/>
        <xsl:text>    </xsl:text>
        <xsl:value-of select="@name"/> = <xsl:value-of select="@value"/>
        <xsl:text>,&#x0A;</xsl:text>
    </xsl:for-each>
    <xsl:text>};&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  method parameters
-->
<xsl:template match="method/param">
    <xsl:if test="@array">
        <xsl:if test="@dir='return'">
            <xsl:message terminate="yes">
                <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
                <xsl:text>return 'array' parameters are not supported, use 'safearray="yes"' instead.</xsl:text>
            </xsl:message>
        </xsl:if>
        <xsl:text>[array, </xsl:text>
        <xsl:choose>
            <xsl:when test="../param[@name=current()/@array]">
                <xsl:if test="../param[@name=current()/@array]/@dir != @dir">
                    <xsl:message terminate="yes">
                        <xsl:value-of select="concat(../../@name,'::',../@name,': ')"/>
                        <xsl:value-of select="concat(@name,' and ',../param[@name=current()/@array]/@name)"/>
                        <xsl:text> must have the same direction</xsl:text>
                    </xsl:message>
                </xsl:if>
                <xsl:text>size_is(</xsl:text>
                    <xsl:if test="@dir='out'">
                        <xsl:text>, </xsl:text>
                    </xsl:if>
                    <xsl:if test="../param[@name=current()/@array]/@dir='out'">
                        <xsl:text>*</xsl:text>
                    </xsl:if>
                    <xsl:value-of select="@array"/>
                <xsl:text>)</xsl:text>
            </xsl:when>
            <xsl:otherwise>
                <xsl:message terminate="yes">
                    <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
                    <xsl:text>array attribute refers to non-existent param: </xsl:text>
                    <xsl:value-of select="@array"/>
                </xsl:message>
            </xsl:otherwise>
        </xsl:choose>
        <xsl:text>] </xsl:text>
    </xsl:if>
    <xsl:choose>
        <xsl:when test="@dir='in'">in </xsl:when>
        <xsl:when test="@dir='out'">out </xsl:when>
        <xsl:when test="@dir='return'">[retval] out </xsl:when>
        <xsl:otherwise>in</xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates select="@type"/>
    <xsl:text> </xsl:text>
    <xsl:value-of select="@name"/>
</xsl:template>


<!--
 *  attribute/parameter type conversion
-->
<xsl:template match="
    attribute/@type | param/@type |
    enumerator/@type | collection/@type | collection/@enumerator
">
    <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>

    <xsl:if test="../@array and ../@safearray='yes'">
        <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
            <xsl:text>either 'array' or 'safearray="yes"' attribute is allowed, but not both!</xsl:text>
        </xsl:message>
    </xsl:if>

    <xsl:choose>
        <!-- modifiers (ignored for 'enumeration' attributes)-->
        <xsl:when test="name(current())='type' and ../@mod">
            <xsl:choose>
                <xsl:when test="../@mod='ptr'">
                    <xsl:choose>
                        <!-- standard types -->
                        <!--xsl:when test=".='result'">??</xsl:when-->
                        <xsl:when test=".='boolean'">booeanPtr</xsl:when>
                        <xsl:when test=".='octet'">octetPtr</xsl:when>
                        <xsl:when test=".='short'">shortPtr</xsl:when>
                        <xsl:when test=".='unsigned short'">ushortPtr</xsl:when>
                        <xsl:when test=".='long'">longPtr</xsl:when>
                        <xsl:when test=".='long long'">llongPtr</xsl:when>
                        <xsl:when test=".='unsigned long'">ulongPtr</xsl:when>
                        <xsl:when test=".='unsigned long long'">ullongPtr</xsl:when>
                        <xsl:when test=".='char'">charPtr</xsl:when>
                        <!--xsl:when test=".='string'">??</xsl:when-->
                        <xsl:when test=".='wchar'">wcharPtr</xsl:when>
                        <!--xsl:when test=".='wstring'">??</xsl:when-->
                        <xsl:otherwise>
                            <xsl:message terminate="yes">
                                <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
                                <xsl:text>attribute 'mod=</xsl:text>
                                <xsl:value-of select="concat('&quot;',../@mod,'&quot;')"/>
                                <xsl:text>' cannot be used with type </xsl:text>
                                <xsl:value-of select="concat('&quot;',current(),'&quot;!')"/>
                            </xsl:message>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:message terminate="yes">
                        <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
                        <xsl:value-of select="concat('value &quot;',../@mod,'&quot; ')"/>
                        <xsl:text>of attibute 'mod' is invalid!</xsl:text>
                    </xsl:message>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>
        <!-- no modifiers -->
        <xsl:otherwise>
            <xsl:choose>
                <!-- standard types -->
                <xsl:when test=".='result'">result</xsl:when>
                <xsl:when test=".='boolean'">boolean</xsl:when>
                <xsl:when test=".='octet'">octet</xsl:when>
                <xsl:when test=".='short'">short</xsl:when>
                <xsl:when test=".='unsigned short'">unsigned short</xsl:when>
                <xsl:when test=".='long'">long</xsl:when>
                <xsl:when test=".='long long'">long long</xsl:when>
                <xsl:when test=".='unsigned long'">unsigned long</xsl:when>
                <xsl:when test=".='unsigned long long'">unsigned long long</xsl:when>
                <xsl:when test=".='char'">char</xsl:when>
                <xsl:when test=".='wchar'">wchar</xsl:when>
                <xsl:when test=".='string'">string</xsl:when>
                <xsl:when test=".='wstring'">wstring</xsl:when>
                <!-- UUID type -->
                <xsl:when test=".='uuid'">uuid</xsl:when>
                <!-- system interface types -->
                <xsl:when test=".='$unknown'">$unknown</xsl:when>
                <xsl:otherwise>
                    <xsl:choose>
                        <!-- enum types -->
                        <xsl:when test="
                            (ancestor::library/enum[@name=current()]) or
                            (ancestor::library/if[@target=$self_target]/enum[@name=current()])
                        ">
                            <xsl:value-of select="."/>
                        </xsl:when>
                        <!-- custom interface types -->
                        <xsl:when test="
                            (name(current())='enumerator' and
                             ((ancestor::library/enumerator[@name=current()]) or
                              (ancestor::library/if[@target=$self_target]/enumerator[@name=current()]))
                            ) or
                            ((ancestor::library/interface[@name=current()]) or
                             (ancestor::library/if[@target=$self_target]/interface[@name=current()])
                            ) or
                            ((ancestor::library/collection[@name=current()]) or
                             (ancestor::library/if[@target=$self_target]/collection[@name=current()])
                            )
                        ">
                            <xsl:value-of select="."/>
                        </xsl:when>
                        <!-- other types -->
                        <xsl:otherwise>
                            <xsl:message terminate="yes">
                                <xsl:text>Unknown parameter type: </xsl:text>
                                <xsl:value-of select="."/>
                            </xsl:message>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:otherwise>
    </xsl:choose>
  <xsl:if test="../@safearray='yes'">
    <xsl:text>[]</xsl:text>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>

