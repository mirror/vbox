<?xml version="1.0" encoding="utf-8" standalone="no"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<!-- gsl meaning the XSLT that generate an XSL transform -->
<gsl:stylesheet version="1.0" 
                xmlns:gsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xsl="output.xsl"
                xmlns:xs="http://www.w3.org/2001/XMLSchema"
                xmlns:saxon="http://icl.com/saxon"
                xmlns:xalan="http://xml.apache.org/xalan/redirect"
                xmlns:ditaarch="http://dita.oasis-open.org/architecture/2005/"
                extension-element-prefixes="saxon xalan ditaarch"
                exclude-result-prefixes="xs gsl">

<!-- Principle:
     The output driver knows how to generate valid output and offers
         a target for everything in the schema that can be instanced
         in a DITA XML document that conforms to the document type.
         In addition, the output driver understands the content for
         each context.  Where the content model consists of a simple
         sequence, the output driver pulls what's required at every
         position.  For mixed content, a choice, or complex content 
         models, the output driver pulls the type of content model
         and, if that's not handled, fires all children of the input.
     The input driver knows the input and thus knows how map any input
         context to what's currently expected in the output context.
     -->

<gsl:param name="outdir">.</gsl:param>

<gsl:output
    method="xml"
    indent="yes"
    omit-xml-declaration="no"
    standalone="no"/>

<gsl:namespace-alias stylesheet-prefix="xsl" result-prefix="gsl"/>

<gsl:key name="typedef"
    match="xs:complexType[contains(@name,'.class')]"
    use="substring-before(@name,'.class')"/>

<gsl:key name="groupdef"
    match="xs:group[contains(@name,'.cnt')]"
    use="@name"/>

<gsl:key name="attgroupdef"
    match="xs:attributeGroup[@name]"
    use="@name"/>

<gsl:key name="attdef"
    match="xs:attribute[@name or @ref]"
    use="@name|@ref"/>

<!--

input module files moduleName.elementName.out to emit the element
moduleName.outputElementName.out fires
    moduleName.outputElementName.atts.in on self
    moduleName.outputElementName.content.in on self
moduleName.outputElementName.atts.in fires
    for each attributeName
        moduleName.outputElementName.attributeName.att.in on self
moduleName.outputElementName.attributeName.att.in fires
    if attributeName is a group
        moduleName.outputElementName.attributeName.att.in on self
    if attributeName is a single attribute
        attributeName.att.in on self
        = may override to provide input processing for the attribute
          in a specific output context
attributeName.att.in is noop
    = may override to provide default input processing for the attribute
moduleName.outputElementName.content.in fires
    if the content model has one content group and mixed content
        (as with topic/p)
        groupName.text.in on self
    if the content model has mixed content
        moduleName.outputElementName.child on *|text()
    if the content model has one content group
        groupName.in on self
    if the content model has nested choice and sequence
        (as with task/taskbody)
        moduleName.outputElementName.child on *
    if the content model has a simple sequence
        (as with topic/topic)
        for each child element
            moduleName.outputElementName.moduleName.inputElementName.in on self
    if the content model has a choice
        (as with topic/body)
        moduleName.outputElementName.child on *
moduleName.outputElementName.moduleName.inputElementName.in fires
    moduleName.inputElementName.in on self
    = may override to provide input processing for the element
      in a specific output context
moduleName.inputElementName.in noop
    = may override to provide default input processing for the element
      when expected in a context
groupName.in fires
    if the content model has a sequence
        moduleName.inputElementName.in on self
    if the content model has a content group
        groupName.child on *
        = may override to provide default input processing
          for the content group
groupName.text.in fires
        groupName.child on *|text()
        = may override to provide default input processing
          for the content group in mixed content models
moduleName.outputElementName.child fires
    child on self
groupName.child fires
    child on self
child noop
    = may override to provide default input processing for children

to suppress an input element, write a noop for the child mode
   -->

<!-- TO DO
     driver parameter for whether a migration or processing conversion
         to control whether to emit <required-cleanup> or a comment 
         for unsupported constructs
     parameter to split out generated topics as separate files

     pass isRequired to pull.in.fire
     attributes rules should take instead of generate required parameter
     default rule to put everything unknown in either comment or
     provide some manually-maintained utilities
     rule to generate <dita> element
     alphabetize attributes rules
     who chooses the output document type?
     child topics should be determined from info-types
   -->

<gsl:template match="/">
  <xsl:stylesheet version="1.0"><gsl:text>
</gsl:text>
    <gsl:apply-templates
          select="( xs:schema/xs:redefine | xs:schema/xs:include )[
              ( xs:schema | xs:schema//xs:include/xs:schema |
                  xs:schema//xs:redefine/xs:schema ) /
                  xs:complexType or
              ( xs:schema | xs:schema//xs:include/xs:schema |
                  xs:schema//xs:redefine/xs:schema ) /
                  xs:group[contains(@name,'.cnt')]]"
        mode="make-transform"/>
    <gsl:text>
</gsl:text>
    <xsl:param name="REPORT_UNMATCHED" select="'yes'"/>
    <xsl:param name="PROCESS_TEXT"     select="'yes'"/>
    <xsl:param name="PROCESS_CHILDREN" select="'yes'"/>
    <gsl:text>
</gsl:text>
    <xsl:template match="/">
      <xsl:apply-templates select="." mode="topic.topic.in"/>
    </xsl:template>
    <gsl:text>
</gsl:text>
  </xsl:stylesheet>
</gsl:template>

<gsl:template match="xs:include|xs:redefine" mode="make-transform">
  <gsl:variable name="fileroot">
    <gsl:choose>
    <gsl:when test="contains(@schemaLocation,'Mod.xsd')">
      <gsl:value-of select="substring-before(@schemaLocation,'Mod.xsd')"/>
    </gsl:when>
    <gsl:otherwise>
      <gsl:value-of select="substring-before(@schemaLocation,'.xsd')"/>
    </gsl:otherwise>
    </gsl:choose>
  </gsl:variable>
  <gsl:variable name="filename" select="concat($fileroot,'Output.xsl')"/>
  <gsl:call-template name="writeout">
    <gsl:with-param name="filename" select="$filename"/>
    <gsl:with-param name="content">
      <gsl:apply-templates select="xs:schema">
        <gsl:with-param name="fileroot" select="$fileroot"/>
      </gsl:apply-templates>
    </gsl:with-param>
  </gsl:call-template>
  <xsl:import href="{$filename}"/><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:schema">
  <gsl:param name="fileroot"/>
  <gsl:variable name="isTopic" select="boolean(
      xs:complexType[@name='topic.class' or @name='map.class'])"/>
  <gsl:variable name="typeDefs"
        select="(. | .//xs:include/xs:schema | .//xs:redefine/xs:schema ) /
            xs:complexType[generate-id(.)=generate-id(
                key('typedef',substring-before(@name,'.class'))[1])]"/>
  <gsl:variable name="groupDefs"
        select="(. | .//xs:include/xs:schema | .//xs:redefine/xs:schema ) /
            xs:group[contains(@name,'.cnt') and generate-id(.)=
                generate-id(key('groupdef',@name)[1])]"/>
  <gsl:variable name="attributeDefs"
        select="//xs:attribute[
              (@name and
                  @name!='xtrc' and @name!='xtrf' and
                  generate-id(.)=generate-id(key('attdef',@name)[1])) or
              (@ref and
                  @ref!='xml:lang' and @ref!='xml:space' and 
                  @ref!='class' and @ref!='ditaarch:DITAArchVersion' and
                  generate-id(.)=generate-id(key('attdef',@ref)[1]))]"/>
  <xsl:stylesheet version="1.0">
    <gsl:text>

</gsl:text>
<gsl:comment>= = = ELEMENT OUTPUT RULES  = = = = = = = = = =</gsl:comment>
    <gsl:text>
</gsl:text>
    <gsl:apply-templates select="$typeDefs" mode="out">
      <gsl:sort select="@name"/>
    </gsl:apply-templates>
      <gsl:text>
</gsl:text>
    <!-- define attributes only in the output for the base topic -->
    <gsl:if test="$isTopic">
<gsl:comment>= = = ATTRIBUTE OUTPUT RULES  = = = = = = = = =</gsl:comment>
      <gsl:text>
</gsl:text>
      <gsl:apply-templates select="$attributeDefs" mode="out">
        <gsl:sort select="@name|@ref"/>
      </gsl:apply-templates>
      <gsl:text>
</gsl:text>
    </gsl:if>
    <gsl:text>
</gsl:text>
<gsl:comment>= = = DEFAULT ELEMENT INPUT RULES = = = = = = =</gsl:comment>
    <gsl:text>
</gsl:text>
    <gsl:apply-templates select="$typeDefs" mode="in">
      <gsl:sort select="@name"/>
    </gsl:apply-templates>
    <gsl:text>
</gsl:text>
    <gsl:if test="$groupDefs">
<gsl:comment>= = = DEFAULT CONTENT GROUP INPUT RULES = = = = = = =</gsl:comment>
    <gsl:text>
</gsl:text>
    <gsl:apply-templates select="$groupDefs" mode="in">
      <gsl:sort select="@name"/>
    </gsl:apply-templates>
      <gsl:text>
</gsl:text>
    </gsl:if>
      <gsl:text>
</gsl:text>
    <gsl:if test="$isTopic">
<gsl:comment>= = = DEFAULT ATTRIBUTE INPUT RULES = = = = = =</gsl:comment>
      <gsl:text>
</gsl:text>
      <gsl:apply-templates select="$attributeDefs" mode="in">
        <gsl:sort select="@name|@ref"/>
      </gsl:apply-templates>
      <gsl:text>
</gsl:text>
    </gsl:if>
    <gsl:if test="$isTopic">
      <gsl:text>
</gsl:text>
<gsl:comment>= = = DEFAULT RULES = = = = = = = = = = = = = =</gsl:comment>
      <gsl:text>
</gsl:text>
   <xsl:template match="*" mode="child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unmatched.child">
        <xsl:with-param name="container"  select="$container"/>
        <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:call-template>
   </xsl:template><gsl:text>
</gsl:text>
   <xsl:template name="check.unmatched.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:if test="$REPORT_UNMATCHED='yes'">
        <xsl:comment>
           <xsl:value-of select="name()"/>
           <xsl:text> not processed within </xsl:text>
           <xsl:value-of select="$container"/>
        </xsl:comment>
      </xsl:if>
      <xsl:if test="$PROCESS_CHILDREN='yes'">
        <xsl:apply-templates select="*|text()" mode="child"/>
      </xsl:if>
   </xsl:template><gsl:text>
</gsl:text>
   <xsl:template match="text()" mode="child">
      <xsl:param name="container"/>
      <xsl:if test="$PROCESS_TEXT='yes'">
        <xsl:copy-of select="."/>
      </xsl:if>
   </xsl:template><gsl:text>
</gsl:text>
   <xsl:template name="check.unsupplied.input">
      <xsl:param name="input"/>
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:if test="$isRequired='yes'">
        <xsl:message>
           <xsl:text>required </xsl:text>
           <xsl:value-of select="$input"/>
           <xsl:text> not supplied within </xsl:text>
           <xsl:value-of select="$container"/>
        </xsl:message>
      </xsl:if>
      <xsl:if test="$isRequired='yes' or $REPORT_UNMATCHED='yes'">
        <xsl:comment>
           <xsl:value-of select="$input"/>
           <xsl:text> not supplied within </xsl:text>
           <xsl:value-of select="$container"/>
        </xsl:comment>
      </xsl:if>
    </xsl:template><gsl:text>
</gsl:text>
    </gsl:if>
  </xsl:stylesheet>
</gsl:template>

<gsl:template match="xs:complexType" mode="out">
  <gsl:variable name="type" select="substring-before(@name,'.class')"/>
  <gsl:variable name="module">
    <gsl:apply-templates select="xs:attribute[@ref='class']" mode="package">
      <gsl:with-param name="type" select="$type"/>
    </gsl:apply-templates>
  </gsl:variable>
  <xsl:template match="*" mode="{$module}.{$type}.in">
    <xsl:param name="container"/>
    <xsl:param name="isRequired" select="'no'"/>
    <xsl:call-template name="check.unsupplied.input">
      <xsl:with-param name="container"  select="$container"/>
      <xsl:with-param name="isRequired" select="$isRequired"/>
      <xsl:with-param name="input"      select="' {$module}/{$type} '"/>
    </xsl:call-template>
  </xsl:template><gsl:text>
</gsl:text>
  <xsl:template match="*" mode="{$module}.{$type}.out">
    <gsl:element name="{$type}">
      <gsl:if test="xs:attributeGroup[not(@name) or @name!='global-atts'] |
                xs:attribute[(not(@ref) or
                (@ref!='xml:lang' and @ref!='xml:space' and 
                @ref!='class' and @ref!='ditaarch:DITAArchVersion')) and
                (not(@name) or @name!='translate')]">
        <xsl:apply-templates select="." mode="{$module}.{$type}.atts.in"/>
      </gsl:if>
      <gsl:if test="xs:sequence|xs:choice|xs:group|self::*[@mixed='true']">
        <xsl:apply-templates select="." mode="{$module}.{$type}.content.in"/>
      </gsl:if>
    </gsl:element>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:complexType" mode="in">
  <gsl:variable name="type" select="substring-before(@name,'.class')"/>
  <gsl:variable name="module">
    <gsl:apply-templates select="xs:attribute[@ref='class']" mode="package">
      <gsl:with-param name="type" select="$type"/>
    </gsl:apply-templates>
  </gsl:variable>
  <gsl:if test="xs:attributeGroup[not(@name) or @name!='global-atts'] |
            xs:attribute[(not(@ref) or
            (@ref!='xml:lang' and @ref!='xml:space' and 
            @ref!='class' and @ref!='ditaarch:DITAArchVersion')) and
            (not(@name) or @name!='translate')]">
    <xsl:template match="*" mode="{$module}.{$type}.atts.in">
      <gsl:if test="contains(concat(xs:attribute[@ref='class']/@default,' '),
          '/topic ')">
        <xsl:attribute name="xml:lang">
          <xsl:text>en-us</xsl:text>
        </xsl:attribute>
      </gsl:if>
      <gsl:apply-templates select="xs:attributeGroup" mode="in.fire">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
      </gsl:apply-templates>
      <gsl:apply-templates select="xs:attribute" mode="in.fire">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
      </gsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
    <gsl:apply-templates select="xs:attributeGroup" mode="in.handle">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="type"   select="$type"/>
    </gsl:apply-templates>
    <gsl:apply-templates select="xs:attribute" mode="in.handle">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="type"   select="$type"/>
    </gsl:apply-templates>
  </gsl:if>
  <gsl:if test="xs:sequence or xs:choice or xs:group or
        self::*[@mixed='true']">
    <gsl:variable name="isContentGroup"
          select="boolean(xs:choice and count(xs:choice/*)=1 and
              xs:choice/xs:group[contains(@ref,'.cnt')])"/>
    <gsl:choose>
    <gsl:when test="@mixed='true' and $isContentGroup">
      <gsl:apply-templates select="." mode="group.fire.handle">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
        <gsl:with-param name="isText" select="'yes'"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:when test="@mixed='true'">
      <gsl:apply-templates select="." mode="content.child">
        <gsl:with-param name="module"    select="$module"/>
        <gsl:with-param name="type"      select="$type"/>
        <gsl:with-param name="childlist" select="'*|text()'"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:when test="$isContentGroup">
      <gsl:apply-templates select="." mode="group.fire.handle">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
        <gsl:with-param name="isText" select="'no'"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:when test="xs:sequence/xs:choice or xs:choice/xs:sequence">
      <gsl:apply-templates select="." mode="content.child">
        <gsl:with-param name="module"    select="$module"/>
        <gsl:with-param name="type"      select="$type"/>
        <gsl:with-param name="childlist" select="'*'"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:when test="xs:sequence">
      <xsl:template match="*" mode="{$module}.{$type}.content.in">
        <gsl:apply-templates select="xs:sequence/*" mode="pull.in.fire">
          <gsl:with-param name="module" select="$module"/>
          <gsl:with-param name="type"   select="$type"/>
        </gsl:apply-templates>
      </xsl:template><gsl:text>
</gsl:text>
      <gsl:apply-templates select="xs:sequence/*" mode="pull.in.handle">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:when test="xs:choice and count(xs:choice/*)=1">
      <xsl:template match="*" mode="{$module}.{$type}.content.in">
        <gsl:apply-templates select="xs:choice/*" mode="pull.in.fire">
          <gsl:with-param name="module" select="$module"/>
          <gsl:with-param name="type"   select="$type"/>
        </gsl:apply-templates>
      </xsl:template><gsl:text>
</gsl:text>
      <gsl:apply-templates select="xs:choice/*" mode="pull.in.handle">
        <gsl:with-param name="module" select="$module"/>
        <gsl:with-param name="type"   select="$type"/>
      </gsl:apply-templates>
    </gsl:when>
    <gsl:otherwise>
      <gsl:apply-templates select="." mode="content.child">
        <gsl:with-param name="module"    select="$module"/>
        <gsl:with-param name="type"      select="$type"/>
        <gsl:with-param name="childlist" select="'*'"/>
      </gsl:apply-templates>
    </gsl:otherwise>
    </gsl:choose>
  </gsl:if>
</gsl:template>

<gsl:template match="xs:group[contains(@name,'.cnt')]" mode="in">
  <gsl:variable name="typemodule">
    <gsl:apply-templates
        select="key('typedef',@ref)[1]/xs:attribute[@ref='class']"
        mode="package">
      <gsl:with-param name="type" select="@ref"/>
    </gsl:apply-templates>
  </gsl:variable>
  <gsl:choose>
  <gsl:when test="xs:sequence">
    <xsl:template match="*" mode="{@name}.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <gsl:apply-templates select="xs:sequence/*" mode="global.in.fire">
        <gsl:with-param name="container"  select="'$container'"/>
        <gsl:with-param name="isRequired" select="'$isRequired'"/>
        <gsl:with-param name="typemodule" select="$typemodule"/>
      </gsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
  </gsl:when>
  <gsl:when test="xs:choice">
    <xsl:template match="*" mode="{@name}.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="{@name}.child">
        <xsl:with-param name="container"  select="$container"/>
        <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
    <xsl:template match="*" mode="{@name}.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="{@name}.child">
        <xsl:with-param name="container"  select="$container"/>
        <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
    <xsl:template match="*|text()" mode="{@name}.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
        <xsl:with-param name="container"  select="$container"/>
        <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
  </gsl:when>
  <gsl:otherwise>
    <xsl:message>Unknown content model for content group</xsl:message>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

<gsl:template match="xs:attributeGroup[@name='global-atts']" mode="out"/>

<gsl:template match="xs:attributeGroup" mode="out">
  <gsl:param name="module"/>
  <gsl:variable name="group">
    <gsl:choose>
    <gsl:when test="contains(@name,'-atts')">
      <gsl:value-of
          select="concat(substring-before(@name,'-atts'),'.atts')"/>
    </gsl:when>
    <gsl:otherwise>
      <gsl:value-of select="@name"/>
    </gsl:otherwise>
    </gsl:choose>
  </gsl:variable>
  <xsl:template match="*" mode="{$module}.{$group}.in">
    <xsl:param name="container"/>
    <gsl:apply-templates select="xs:attributeGroup|xs:attribute" mode="base">
      <gsl:with-param name="module" select="$module"/>
    </gsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:attribute" mode="out">
  <gsl:variable name="attname" select="@name|@ref"/>
  <xsl:template match="*" mode="{$attname}.att.out">
    <xsl:param name="value"/>
    <xsl:attribute name="{$attname}">
      <xsl:value-of select="$value"/>
    </xsl:attribute>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:attribute" mode="in">
  <gsl:variable name="attname" select="@name|@ref"/>
  <xsl:template match="*" mode="{$attname}.att.in">
    <xsl:param name="isRequired" select="'no'"/>
    <xsl:param name="container"/>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:sequence" mode="out">
<gsl:message>match="xs:sequence" mode="out" shouldn't fire</gsl:message>
</gsl:template>

<gsl:template match="xs:group[@ref='info-types']" mode="pull.in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <!--SHOULDN'T BE HARD CODED TO TOPIC-->
  <xsl:apply-templates select="." mode="{$module}.{$type}.topic.topic.in"/>
</gsl:template>

<gsl:template match="xs:group[@ref='info-types']" mode="pull.in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <!--SHOULDN'T BE HARD CODED TO TOPIC-->
  <xsl:template match="*" mode="{$module}.{$type}.topic.topic.in">
    <xsl:apply-templates select="." mode="topic.topic.in">
      <xsl:with-param name="container" select="' {$module}/{$type} '"/>
    </xsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:group[@ref and @ref!='info-types' and 
          not(contains(@ref,'.cnt'))]|xs:element[@ref]"
      mode="pull.in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:variable name="typemodule">
    <gsl:apply-templates
        select="key('typedef',@ref)[1]/xs:attribute[@ref='class']"
        mode="package">
      <gsl:with-param name="type" select="@ref"/>
    </gsl:apply-templates>
  </gsl:variable>
  <gsl:choose>
  <gsl:when test="$typemodule and $typemodule!=''">
    <xsl:apply-templates select="."
        mode="{$module}.{$type}.{$typemodule}.{@ref}.in"/>
  </gsl:when>
  <gsl:otherwise>
    <gsl:comment>COULD NOT DETERMINE MODULE TO FIRE TYPE</gsl:comment>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

<gsl:template match="xs:group[@ref and @ref!='info-types' and 
          not(contains(@ref,'.cnt'))]|xs:element[@ref]"
      mode="global.in.fire">
  <gsl:param name="container"/>
  <gsl:param name="isRequired" select="'no'"/>
  <gsl:param name="typemodule">
    <gsl:apply-templates
        select="key('typedef',@ref)[1]/xs:attribute[@ref='class']"
        mode="package">
      <gsl:with-param name="type" select="@ref"/>
    </gsl:apply-templates>
  </gsl:param>
  <gsl:choose>
  <gsl:when test="$typemodule and $typemodule!=''">
    <xsl:apply-templates select="." mode="{$typemodule}.{@ref}.in">
      <xsl:with-param name="isRequired" select="'{$isRequired}'"/>
      <xsl:with-param name="container"  select="'{$container}'"/>
    </xsl:apply-templates>
  </gsl:when>
  <gsl:otherwise>
    <gsl:comment>COULD NOT DETERMINE MODULE TO FIRE TYPE GLOBALLY</gsl:comment>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

<gsl:template match="xs:group[@ref and @ref!='info-types' and 
          not(contains(@ref,'.cnt'))]|xs:element[@ref]"
      mode="pull.in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="typemodule">
    <gsl:apply-templates
        select="key('typedef',@ref)[1]/xs:attribute[@ref='class']"
        mode="package">
      <gsl:with-param name="type" select="@ref"/>
    </gsl:apply-templates>
  </gsl:param>
  <gsl:variable name="minOccurs" select="(.|..)/@minOccurs[1]"/>
  <gsl:variable name="isRequired">
    <gsl:choose>
    <gsl:when test="not($minOccurs) or $minOccurs='1'">
        <gsl:text>yes</gsl:text>
    </gsl:when>
    <gsl:otherwise>
        <gsl:text>no</gsl:text>
    </gsl:otherwise>
    </gsl:choose>
  </gsl:variable>
  <gsl:choose>
  <gsl:when test="$typemodule and $typemodule!=''">
    <xsl:template match="*" mode="{$module}.{$type}.{$typemodule}.{@ref}.in">
      <gsl:apply-templates select="." mode="global.in.fire">
        <gsl:with-param name="isRequired" select="$isRequired"/>
        <gsl:with-param name="container"
              select="concat(' ',$module,'/',$type,' ')"/>
        <gsl:with-param name="typemodule" select="$typemodule"/>
      </gsl:apply-templates>
    </xsl:template><gsl:text>
</gsl:text>
  </gsl:when>
  <gsl:otherwise>
    <gsl:comment>COULD NOT DETERMINE MODULE TO HANDLE TYPE</gsl:comment>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

<gsl:template match="xs:group[@ref and @ref!='info-types' and 
          contains(@ref,'.cnt')]"
      mode="group.in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="isText" select="'yes'"/>
  <gsl:choose>
  <gsl:when test="$isText='yes'">
    <xsl:apply-templates select="." mode="{@ref}.text.in"/>
  </gsl:when>
  <gsl:otherwise>
    <xsl:apply-templates select="." mode="{@ref}.in"/>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

<gsl:template match="*" mode="content.child">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="childlist" select="'*|text()'"/>
  <xsl:template match="*" mode="{$module}.{$type}.content.in">      
    <xsl:apply-templates select="{$childlist}"
          mode="{$module}.{$type}.child"/>
  </xsl:template><gsl:text>
</gsl:text>
  <gsl:apply-templates select="." mode="child.in.handle">
    <gsl:with-param name="module"    select="$module"/>
    <gsl:with-param name="type"      select="$type"/>
    <gsl:with-param name="childlist" select="$childlist"/>
  </gsl:apply-templates>
</gsl:template>

<gsl:template match="*" mode="child.in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="childlist" select="'*|text()'"/>
  <xsl:template match="{$childlist}" mode="{$module}.{$type}.child">
    <xsl:param name="isRequired" select="'no'"/>
    <xsl:apply-templates select="." mode="child">
      <xsl:with-param name="container"  select="' {$module}/{$type} '"/>
      <xsl:with-param name="isRequired" select="$isRequired"/>
    </xsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="*" mode="group.fire.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="isText" select="'yes'"/>
  <xsl:template match="*" mode="{$module}.{$type}.content.in">
    <gsl:apply-templates select="xs:choice/*" mode="group.in.fire">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="type"   select="$type"/>
      <gsl:with-param name="isText" select="$isText"/>
    </gsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:group" name="typeout" mode="out">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="subtype" select="@ref"/>
  <xsl:apply-templates select="." mode="{$module}.{$subtype}.in">
    <xsl:with-param name="container" select="' {$module}/{$type} '"/>
  </xsl:apply-templates>
</gsl:template>

<gsl:template match="xs:attributeGroup[contains(@ref,'-atts')]">
<gsl:message>match="xs:attributeGroup[contains(@ref,'-atts')]" shouldn't fire</gsl:message>
</gsl:template>

<gsl:template match="xs:attributeGroup[@name='global-atts']" mode="in.fire"/>
<gsl:template match="xs:attributeGroup[@name='global-atts']" mode="in.handle"/>

<gsl:template match="xs:attributeGroup[contains(@ref,'-atts')]" mode="in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:if test="@ref!='global-atts'">
    <gsl:call-template name="attgroup">
      <gsl:with-param name="module"  select="$module"/>
      <gsl:with-param name="type"    select="$type"/>
      <gsl:with-param name="attgroup"
          select="concat(substring-before(@ref,'-atts'),'.atts')"/>
    </gsl:call-template>
  </gsl:if>
</gsl:template>

<gsl:template match="xs:attributeGroup[contains(@ref,'-atts')]"
      mode="in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:if test="@ref!='global-atts'">
    <gsl:call-template name="attgroupHandle">
      <gsl:with-param name="module"  select="$module"/>
      <gsl:with-param name="type"    select="$type"/>
      <gsl:with-param name="attgroup"
          select="concat(substring-before(@ref,'-atts'),'.atts')"/>
    </gsl:call-template>
  </gsl:if>
</gsl:template>

<gsl:template match="xs:attributeGroup">
<gsl:message>match="xs:attributeGroup" shouldn't fire</gsl:message>
</gsl:template>

<gsl:template match="xs:attributeGroup" name="attgroup" mode="in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="attgroup" select="@ref"/>
  <xsl:apply-templates select="." mode="{$module}.{$type}.{$attgroup}.in"/>
</gsl:template>

<gsl:template match="xs:attributeGroup" name="attgroupHandle" mode="in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:param name="attgroup" select="@ref"/>
  <gsl:variable name="attgroupdef" select="key('attgroupdef',@ref)[1]"/>
  <xsl:template match="*" mode="{$module}.{$type}.{$attgroup}.in">
    <gsl:apply-templates select="$attgroupdef/xs:attributeGroup"
          mode="in.fire">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="type"   select="$type"/>
    </gsl:apply-templates>
    <gsl:apply-templates select="$attgroupdef/xs:attribute" mode="in.fire">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="type"   select="$type"/>
    </gsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
  <gsl:apply-templates select="$attgroupdef/xs:attributeGroup"
        mode="in.handle">
    <gsl:with-param name="module" select="$module"/>
    <gsl:with-param name="type"   select="$type"/>
  </gsl:apply-templates>
  <gsl:apply-templates select="$attgroupdef/xs:attribute" mode="in.handle">
    <gsl:with-param name="module" select="$module"/>
    <gsl:with-param name="type"   select="$type"/>
  </gsl:apply-templates>
</gsl:template>

<gsl:template match="xs:attributeGroup[contains(@ref,'-atts')]" mode="base">
  <gsl:param name="module"/>
  <gsl:if test="@ref!='global-atts'">
    <gsl:call-template name="baseattgroup">
      <gsl:with-param name="module" select="$module"/>
      <gsl:with-param name="attgroup"
          select="concat(substring-before(@ref,'-atts'),'.atts')"/>
    </gsl:call-template>
  </gsl:if>
</gsl:template>

<gsl:template match="xs:attributeGroup" name="baseattgroup" mode="base">
  <gsl:param name="module"/>
  <gsl:param name="attgroup" select="@ref"/>
  <xsl:apply-templates select="." mode="{$module}.{$attgroup}.in">
    <xsl:with-param name="container" select="$container"/>
  </xsl:apply-templates>
</gsl:template>

<gsl:template match="xs:attribute[@ref='xml:lang' or @name='translate' or 
    @ref='xml:space' or
    @ref='class' or @ref='ditaarch:DITAArchVersion']" mode="in.fire"/>

<gsl:template match="xs:attribute[@ref='xml:lang' or @name='translate' or 
    @ref='xml:space' or
    @ref='class' or @ref='ditaarch:DITAArchVersion']" mode="in.handle"/>

<gsl:template match="xs:attribute">
<gsl:message>match="xs:attribute" shouldn't fire</gsl:message>
</gsl:template>

<gsl:template match="xs:attribute" mode="in.fire">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <xsl:apply-templates select="."
      mode="{$module}.{$type}.{@ref|@name}.att.in"/>
</gsl:template>

<gsl:template match="xs:attribute" mode="in.handle">
  <gsl:param name="module"/>
  <gsl:param name="type"/>
  <gsl:variable name="isRequired">
    <gsl:choose>
    <gsl:when test="@use and @use='required'">
        <gsl:text>yes</gsl:text>
    </gsl:when>
    <gsl:otherwise>
        <gsl:text>no</gsl:text>
    </gsl:otherwise>
    </gsl:choose>
  </gsl:variable>
  <xsl:template match="*" mode="{$module}.{$type}.{@ref|@name}.att.in">
    <xsl:apply-templates select="." mode="{@ref|@name}.att.in">
      <xsl:with-param name="isRequired" select="'{$isRequired}'"/>
    </xsl:apply-templates>
  </xsl:template><gsl:text>
</gsl:text>
</gsl:template>

<gsl:template match="xs:attribute[@ref='xml:lang' or @name='translate' or 
    @ref='xml:space' or
    @ref='class' or @ref='ditaarch:DITAArchVersion']" mode="base"/>

<gsl:template match="xs:attribute" mode="base">
  <gsl:param name="module"/>
  <xsl:apply-templates select="." mode="{$module}.{@ref|@name}.att.in">
    <xsl:with-param name="container" select="$container"/>
  </xsl:apply-templates>
</gsl:template>

<gsl:template match="xs:attribute[@ref='class']" mode="package">
  <gsl:param name="type"/>
  <gsl:variable name="value" select="
      normalize-space(
        substring-before(concat(@default,' '),
          concat('/',$type,' ')
        )
      )"/>
  <gsl:call-template name="get-last-token">
    <gsl:with-param name="value" select="$value"/>
  </gsl:call-template>
</gsl:template>

<gsl:template name="get-last-token">
  <gsl:param name="value"/>
  <gsl:param name="delimiter" select="' '"/>
  <gsl:variable name="nextvalue" select="substring-after($value, $delimiter)"/>
  <gsl:choose>
  <gsl:when test="$nextvalue and $nextvalue!=''">
    <gsl:call-template name="get-last-token">
      <gsl:with-param name="value"     select="$nextvalue"/>
      <gsl:with-param name="delimiter" select="$delimiter"/>
    </gsl:call-template>
  </gsl:when>
  <gsl:otherwise>
    <gsl:value-of select="$value"/>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - write actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<gsl:template name="writeout">
  <gsl:param name="filename"/>
  <gsl:param name="content"/>
  <gsl:choose>
  <gsl:when test="element-available('saxon:output')">
    <saxon:output href="{$outdir}/{$filename}"
        method="xml"
        encoding="utf-8"
        indent="yes">
      <gsl:copy-of select="$content"/>
    </saxon:output>
  </gsl:when>
  <gsl:when test="element-available('xalan:write')">
    <xalan:write file="{$outdir}/{$filename}">
      <gsl:copy-of select="$content"/>
    </xalan:write>
  </gsl:when>
  <gsl:otherwise>
    <gsl:message terminate="yes">
      <gsl:text>Cannot write</gsl:text>
    </gsl:message>
  </gsl:otherwise>
  </gsl:choose>
</gsl:template>

</gsl:stylesheet>
