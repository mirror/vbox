<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:vbox="http://www.virtualbox.org/"
     xmlns:exsl="http://exslt.org/common"
     extension-element-prefixes="exsl">

<!--

    autogen.xsl:
        XSLT stylesheet that generates C++ classes implementing simple
        COM classes implementation from VirtualBox.xidl.

     Copyright (C) 2010 Oracle Corporation

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

<xsl:include href="../webservice/websrv-shared.inc.xsl" />

<!-- $G_kind contains what kind of COM class implementation we generate -->
<xsl:variable name="G_xsltFilename" select="'autogen.xsl'" />

<xsl:template name="fileheader">
  <xsl:param name="name" />
  <xsl:text>/**
 *  Copyright (C) 2010 Oracle Corporation
 *
 *  This file is part of VirtualBox Open Source Edition (OSE), as
 *  available from http://www.virtualbox.org. This file is free software;
 *  you can redistribute it and/or modify it under the terms of the GNU
 *  General Public License (GPL) as published by the Free Software
 *  Foundation, in version 2 as it comes in the "COPYING" file of the
 *  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 *  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
</xsl:text>
  <xsl:value-of select="concat(' * ',$name)"/>
<xsl:text>
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
 * Generator: src/VBox/Main/idl/autogen.xsl
 */

</xsl:text>
</xsl:template>

<xsl:template name="genComEntry">
  <xsl:param name="name" />
  <xsl:variable name="extends">
    <xsl:value-of select="//interface[@name=$name]/@extends" />
  </xsl:variable>

  <xsl:value-of select="concat('      COM_INTERFACE_ENTRY(', $name, ')&#10;')" />
  <xsl:choose>
    <xsl:when test="$extends='$unknown'">
      <xsl:value-of select="   '      COM_INTERFACE_ENTRY(IDispatch)&#10;'" />
    </xsl:when>
    <xsl:when test="//interface[@name=$extends]">
      <xsl:call-template name="genComEntry">
        <xsl:with-param name="name" select="$extends" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('No idea how to process it: ', $extends)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="typeIdl2Back">
  <xsl:param name="type" />
  <xsl:param name="safearray" />
  <xsl:param name="param" />
  <xsl:param name="dir" />

  <xsl:choose>
    <xsl:when test="(($type='wstring') or ($type='uuid')) and ($param='yes')">
      <xsl:value-of select="'BSTR'" />
    </xsl:when>
    <xsl:when test="(($type='wstring') or ($type='uuid')) and not($param='yes')">
      <xsl:value-of select="'Bstr'" />
    </xsl:when>
    <xsl:when test="//enum[@name=$type]">
      <xsl:value-of select="concat($type,'_T')"/>
    </xsl:when>
    <xsl:when test="$type='boolean'">
      <xsl:value-of select="'BOOL'" />
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('Unhandled type: ', $type)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

  <xsl:if test="$dir='out'">
    <xsl:value-of select="'*'"/>
  </xsl:if>
</xsl:template>


<xsl:template name="genSetParam">
  <xsl:param name="member"/>
  <xsl:param name="param"/>
  <xsl:param name="type"/>
  <xsl:param name="safearray"/>
  
  <xsl:value-of select="concat('         ', $member, ' = ', $param, ';&#10;')"/>
</xsl:template>

<xsl:template name="genRetParam">
  <xsl:param name="member"/>
  <xsl:param name="param"/>
  <xsl:param name="type"/>
  <xsl:param name="safearray"/>
  <xsl:choose>
    <xsl:when test="($type='wstring') or ($type = 'uuid')">
      <xsl:value-of select="concat('         ', $member, '.cloneTo(', $param, ');&#10;')"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="concat('         *', $param, ' = ', $member, ';&#10;')"/>
    </xsl:otherwise>    
  </xsl:choose>
</xsl:template>

<xsl:template name="genAttrInitCode">
  <xsl:param name="name" />
  <xsl:param name="obj" />
  <xsl:variable name="extends">
    <xsl:value-of select="//interface[@name=$name]/@extends" />
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$extends='IEvent'">
    </xsl:when>
    <xsl:when test="//interface[@name=$extends]">
      <xsl:call-template name="genAttrInitCode">
        <xsl:with-param name="name" select="$extends" />
        <xsl:with-param name="obj" select="$obj" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('No idea how to process it: ', $name)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>

  
  <xsl:for-each select="//interface[@name=$name]/attribute">
    <xsl:variable name="aType">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="@type" />
        <xsl:with-param name="safearray" select="@safearray" />
        <xsl:with-param name="param" select="'yes'" />
        <xsl:with-param name="dir" select="'in'" />
      </xsl:call-template>
    </xsl:variable>
    
    <xsl:value-of select="concat('              ',$aType, ' a_',@name,' = va_arg(args, ',$aType,');&#10;')"/>
    <xsl:value-of select="concat('              ',$obj, '->set_', @name, '(a_', @name, ');&#10;')"/>
  </xsl:for-each>
</xsl:template>

<xsl:template name="genImplList">
  <xsl:param name="impl" />
  <xsl:param name="name" />
  <xsl:param name="depth" />
  <xsl:param name="parents" />

  <xsl:variable name="extends">
    <xsl:value-of select="//interface[@name=$name]/@extends" />
  </xsl:variable>
  
  <xsl:choose>
    <xsl:when test="$extends='IEvent'">
      <xsl:value-of select="       '#ifdef VBOX_WITH_XPCOM&#10;'" />
      <xsl:value-of select="concat('NS_DECL_CLASSINFO(', $impl, ')&#10;')" />
      <xsl:value-of select="concat('NS_IMPL_THREADSAFE_ISUPPORTS',$depth,'_CI(', $impl, ', ', $name, $parents, ', IEvent)&#10;')" />
      <xsl:value-of select="       '#endif&#10;&#10;'"/>
    </xsl:when>
    <xsl:when test="//interface[@name=$extends]">
      <xsl:call-template name="genImplList">
        <xsl:with-param name="impl" select="$impl" />
        <xsl:with-param name="name" select="$extends" />
        <xsl:with-param name="depth" select="$depth+1" />
        <xsl:with-param name="parents" select="concat($parents, ', ', @name)" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('No idea how to process it: ', $name)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>  

<xsl:template name="genAttrCode">
  <xsl:param name="name" />
  <xsl:param name="depth" />
  <xsl:param name="parents" />

  <xsl:variable name="extends">
    <xsl:value-of select="//interface[@name=$name]/@extends" />
  </xsl:variable>

  <xsl:for-each select="//interface[@name=$name]/attribute">
    <xsl:variable name="mName">
      <xsl:value-of select="concat('m_', @name)" />
    </xsl:variable>
    <xsl:variable name="mType">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="@type" />
        <xsl:with-param name="safearray" select="@safearray" />
        <xsl:with-param name="param" select="'no'" />
        <xsl:with-param name="dir" select="'in'" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="pName">
      <xsl:value-of select="concat('a_', @name)" />
    </xsl:variable>
    <xsl:variable name="pTypeOut">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="@type" />
        <xsl:with-param name="safearray" select="@safearray" />
        <xsl:with-param name="param" select="'yes'" />
        <xsl:with-param name="dir" select="'out'" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="pTypeIn">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="@type" />
        <xsl:with-param name="safearray" select="@safearray" />
        <xsl:with-param name="param" select="'yes'" />
        <xsl:with-param name="dir" select="'in'" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="capsName">
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:value-of select="       '&#10;'" />
    <xsl:value-of select="concat('    // attribute ', @name,'&#10;')" />
    <xsl:value-of select="       'private:&#10;'" />
    <xsl:value-of select="concat('    ', $mType, '    ', $mName,';&#10;')" />
    <xsl:value-of select="       'public:&#10;'" />
    <xsl:value-of select="concat('    STDMETHOD(COMGETTER(', $capsName,'))(',$pTypeOut, ' ', $pName,') {&#10;')" />
    <xsl:call-template name="genRetParam">
      <xsl:with-param name="type" select="@type" />
      <xsl:with-param name="member" select="$mName" />
      <xsl:with-param name="param" select="$pName" />
      <xsl:with-param name="safearray" select="@safearray" />
    </xsl:call-template>
    <xsl:value-of select="       '         return S_OK;&#10;'" />
    <xsl:value-of select="       '    }&#10;'" />

    <xsl:value-of select="       '    // purely internal setter&#10;'" />
    <xsl:value-of select="concat('    int set_', @name,'(',$pTypeIn, ' ', $pName,') {&#10;')" />
    <xsl:call-template name="genSetParam">
      <xsl:with-param name="type" select="@type" />
      <xsl:with-param name="member" select="$mName" />
      <xsl:with-param name="param" select="$pName" />
      <xsl:with-param name="safearray" select="@safearray" />
    </xsl:call-template>
    <xsl:value-of select="       '         return S_OK;&#10;'" />
    <xsl:value-of select="       '    }&#10;'" />
  </xsl:for-each>

  <xsl:choose>
    <xsl:when test="$extends='IEvent'">
      <xsl:value-of select="   '    // skipping IEvent attributes &#10;'" />
    </xsl:when>
    <xsl:when test="//interface[@name=$extends]">
      <xsl:call-template name="genAttrCode">
        <xsl:with-param name="name" select="$extends" />
        <xsl:with-param name="depth" select="$depth+1" />
        <xsl:with-param name="parents" select="concat($parents, ', ', @name)" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fatalError">
        <xsl:with-param name="msg" select="concat('No idea how to process it: ', $extends)" />
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="genEventImpl">
  <xsl:param name="implName" />

  <xsl:value-of select="concat('class ATL_NO_VTABLE ',$implName,
                        ' : public VirtualBoxBase, VBOX_SCRIPTABLE_IMPL(',
                        @name, ')&#10;{&#10;')" />
  <xsl:value-of select="'public:&#10;'" />
  <xsl:value-of select="concat('   DECLARE_NOT_AGGREGATABLE(', $implName, ')&#10;')" />
  <xsl:value-of select="       '   DECLARE_PROTECT_FINAL_CONSTRUCT()&#10;'" />
  <xsl:value-of select="concat('   BEGIN_COM_MAP(', $implName, ')&#10;')" />
  <xsl:call-template name="genComEntry">
    <xsl:with-param name="name" select="@name" />
  </xsl:call-template>
  <xsl:value-of select="       '   END_COM_MAP()&#10;'" />
  <xsl:value-of select="concat('   ', $implName, '() {}&#10;')" />
  <xsl:value-of select="concat('   virtual ~', $implName, '() {}&#10;')" />
  <xsl:text><![CDATA[
    HRESULT FinalConstruct()
    {
        return mEvent.createObject();
    }
    void FinalRelease() {
        mEvent->FinalRelease();
    }
    STDMETHOD(COMGETTER(Type)) (VBoxEventType_T *aType)
    {
        return mEvent->COMGETTER(Type) (aType);
    }
    STDMETHOD(COMGETTER(Source)) (IEventSource * *aSource)
    {
        return mEvent->COMGETTER(Source) (aSource);
    }
    STDMETHOD(COMGETTER(Waitable)) (BOOL *aWaitable)
    {
        return mEvent->COMGETTER(Waitable) (aWaitable);
    }
    STDMETHOD(SetProcessed)()
    {
       return mEvent->SetProcessed();
    }
    STDMETHOD(WaitProcessed)(LONG aTimeout, BOOL *aResult)
    {
        return mEvent->WaitProcessed(aTimeout, aResult);
    }
    HRESULT init (IEventSource* aSource, VBoxEventType_T aType, BOOL aWaitable)
    {
        return mEvent->init(aSource, aType, aWaitable);
    }
    void uninit()
    {
        mEvent->uninit();
    }
]]></xsl:text>
  <xsl:value-of select="       'private:&#10;'" />
  <xsl:value-of select="       '    ComObjPtr&lt;VBoxEvent&gt;      mEvent;&#10;'" />

  <xsl:call-template name="genAttrCode">
    <xsl:with-param name="name" select="@name" />
  </xsl:call-template>
  <xsl:value-of select="'};&#10;'" />

  
  <xsl:call-template name="genImplList">
    <xsl:with-param name="impl" select="$implName" />
    <xsl:with-param name="name" select="@name" />
    <xsl:with-param name="depth" select="'2'" />
    <xsl:with-param name="parents" select="''" />
  </xsl:call-template> 

</xsl:template>


<xsl:template name="genSwitchCase">
  <xsl:param name="implName" />
  <xsl:variable name="waitable">
    <xsl:choose>
      <xsl:when test="@waitable='yes'">
        <xsl:value-of select="'TRUE'"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="'FALSE'"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable> 
  <xsl:value-of select="concat('         case VBoxEventType_', @id, ':&#10;')"/>
  <xsl:value-of select="       '         {&#10;'"/>
  <xsl:value-of select="concat('              ComObjPtr&lt;', $implName, '&gt; obj;&#10;')"/>
  <xsl:value-of select="       '              obj.createObject();&#10;'"/>
  <xsl:value-of select="concat('              obj->init(source, aType, ', $waitable, ');&#10;')"/>  
  <xsl:call-template name="genAttrInitCode">
    <xsl:with-param name="name" select="@name" />
    <xsl:with-param name="obj" select="'obj'" />
  </xsl:call-template>
  <xsl:value-of select="       '              obj.queryInterfaceTo(mEvent.asOutParam());&#10;'"/>
  <xsl:value-of select="       '              break;&#10;'"/>
  <xsl:value-of select="       '         }&#10;'"/>
</xsl:template>

<xsl:template name="genCommonEventCode">
  <xsl:text><![CDATA[
HRESULT VBoxEventDesc::init(IEventSource* source, VBoxEventType_T aType, ...)
{
    va_list args;
    va_start(args, aType);
    switch (aType)
    {
]]></xsl:text>

  <xsl:for-each select="//interface[@autogen=$G_kind]">
    <xsl:variable name="implName">
      <xsl:value-of select="concat(@name,'Impl')" />
    </xsl:variable>
    <xsl:call-template name="genSwitchCase">
      <xsl:with-param name="implName" select="$implName" />
    </xsl:call-template>
  </xsl:for-each>

  <xsl:text><![CDATA[
         default:
            if (0) AssertFailed();
    }
    va_end(args);

    return S_OK;
}
]]></xsl:text>

</xsl:template>


<xsl:template match="/">
  <xsl:call-template name="fileheader">
    <xsl:with-param name="name" select="'VBoxEvents.cpp'" />
  </xsl:call-template>

  <xsl:value-of select="'#include &quot;EventImpl.h&quot;&#10;&#10;'" />

  <!-- Interfaces -->
  <xsl:for-each select="//interface[@autogen=$G_kind]">
    <xsl:value-of select="concat('// ', @name,  ' implementation code &#10;')" />
    <xsl:variable name="implName">
      <xsl:value-of select="concat(@name,'Impl')" />
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="$G_kind='VBoxEvent'">
        <xsl:call-template name="genEventImpl">
          <xsl:with-param name="implName" select="$implName" />
        </xsl:call-template>
      </xsl:when>
    </xsl:choose>
  </xsl:for-each>

  <!-- Global code -->
   <xsl:choose>
      <xsl:when test="$G_kind='VBoxEvent'">
        <xsl:call-template name="genCommonEventCode">
        </xsl:call-template>
      </xsl:when>
   </xsl:choose>   
</xsl:template>

</xsl:stylesheet>
