<?xml version="1.0"?>

<!--
    websrv-jax-ws.xsl:
        XSLT stylesheet that generates virtualbox.java from
        VirtualBox.xidl. This generated Java code contains
        a Java wrapper that allows client code to use the
        webservice in an object-oriented way.

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
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:exsl="http://exslt.org/common"
  extension-element-prefixes="exsl">

  <xsl:output method="text"/>

  <xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'glue-jaxws.xsl'" />

<!-- Keep in sync with VBOX_JAVA_PACKAGE in webservices/Makefile.kmk -->
<xsl:variable name="G_virtualBoxPackage" select="'org.virtualbox22'" />
<xsl:variable name="G_virtualBoxPackage2" select="'com.sun.xml.ws.commons.virtualbox22'" />

<xsl:include href="websrv-shared.inc.xsl" />

<!-- collect all interfaces with "wsmap='suppress'" in a global variable for
     quick lookup -->
<xsl:variable name="G_setSuppressedInterfaces"
              select="//interface[@wsmap='suppress']" />


<xsl:template name="fileheader">
  <xsl:param name="name" />
  <xsl:text>/**
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 *
</xsl:text>
  <xsl:value-of select="concat(' * ',$name)"/> 
<xsl:text>
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
 * Generator: src/VBox/Main/webservice/glue-jaxws.xsl
 */

</xsl:text>
</xsl:template>

<xsl:template name="fullClassName">
  <xsl:param name="name" />
  <xsl:param name="origname" />
  <xsl:param name="collPrefix" />
  <xsl:variable name="coll" select="//collection[@name=$name]" />
   <xsl:choose>
     <xsl:when test="//collection[@name=$name]">
       <!-- for collections and safearrays we return element type  -->
       <xsl:call-template name="fullClassName">
         <xsl:with-param name="name" select="concat($collPrefix,//collection[@name=$name]/@type)" />
         <xsl:with-param name="origname"  select="//collection[@name=$name]/@type" />
         <xsl:with-param name="collPrefix" select="$collPrefix" />
       </xsl:call-template>
       <!-- <xsl:value-of select="concat('org.virtualbox.', concat($collPrefix,//collection[@name=$name]/@type))" /> -->
     </xsl:when>
     <xsl:when test="//enum[@name=$name] or //enum[@name=$origname]">
       <xsl:value-of select="concat($G_virtualBoxPackage,  concat('.', $name))" />
     </xsl:when>
      <xsl:when test="$collPrefix and //interface[@name=$origname]/@wsmap='managed'">
         <xsl:value-of select="concat($G_virtualBoxPackage, concat('.', $name))" />
      </xsl:when>
     <xsl:when test="//interface[@name=$name]/@wsmap='struct' or //interface[@name=$origname]/@wsmap='struct'">
       <xsl:value-of select="concat($G_virtualBoxPackage,  concat('.', $name))" />
     </xsl:when>
     <xsl:otherwise>
       <xsl:value-of select="concat($G_virtualBoxPackage2,  concat('.', $name))" />
     </xsl:otherwise>
   </xsl:choose>
</xsl:template>

<!--
    typeIdl2Glue: converts $type into a type as used by the java glue code.
    For example, for an XIDL IMachineCollection, this will return
    "List<com.sun.xml.ws.commons.virtualbox.IMachine>".
 -->
<xsl:template name="typeIdl2Glue">
  <xsl:param name="ifname" />
  <xsl:param name="method" />
  <xsl:param name="name" />
  <xsl:param name="type" />
  <xsl:param name="safearray" />
  <xsl:param name="forceelem" />

  <xsl:variable name="needarray" select="($safearray='yes' or //collection[@name=$type]) and not($forceelem='yes')" />

  <xsl:if test="$needarray">
    <xsl:value-of select="'List&lt;'" />
  </xsl:if>

  <xsl:choose>
    <xsl:when test="$type='wstring'">String</xsl:when>
    <xsl:when test="$type='boolean'">Boolean</xsl:when>
<!--     <xsl:when test="$type='double'">double</xsl:when> -->
<!--     <xsl:when test="$type='float'">float</xsl:when> -->
    <!-- <xsl:when test="$type='octet'">byte</xsl:when> -->
<!--     <xsl:when test="$type='short'">short</xsl:when> -->
    <xsl:when test="$type='unsigned short'">Integer</xsl:when>
    <xsl:when test="$type='long'">Integer</xsl:when>
    <xsl:when test="$type='long long'">Long</xsl:when>
    <xsl:when test="$type='unsigned long'">Long</xsl:when>
    <xsl:when test="$type='unsigned long long'">BigInteger</xsl:when>
    <xsl:when test="$type='result'">Long</xsl:when>
    <xsl:when test="$type='uuid'">UUID</xsl:when>
    <!-- not a standard type: then it better be one of the types defined in the XIDL -->
    <xsl:when test="$type='$unknown'">IUnknown</xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fullClassName">
        <xsl:with-param name="name" select="$type" />
        <xsl:with-param name="collPrefix" select="''"/>
      </xsl:call-template>
    </xsl:otherwise>
 <!--    <xsl:otherwise> -->
<!--       <xsl:call-template name="fatalError"> -->
<!--         <xsl:with-param name="msg" select="concat('typeIdl2Glue: Type &quot;', $type, '&quot; in arg &quot;', $name, '&quot; of method &quot;', $ifname, '::', $method, '&quot; is not supported.')" /> -->
<!--       </xsl:call-template> -->
<!--     </xsl:otherwise> -->
  </xsl:choose>

  <xsl:if test="$needarray">
    <xsl:value-of select="'&gt;'" />
  </xsl:if>
</xsl:template>

<!--
    typeIdl2Java: converts $type into a type as used by the JAX-WS backend.
    For example, for an XIDL IMachineCollection, this will return
    "ArrayOfIMachine".
    -->
<xsl:template name="typeIdl2Java">
  <xsl:param name="ifname" />
  <xsl:param name="method" />
  <xsl:param name="name" />
  <xsl:param name="type" />
  <xsl:param name="safearray" />

  <xsl:if test="$safearray">
    <xsl:value-of select="'List&lt;'" />
  </xsl:if>

  <xsl:choose>
    <xsl:when test="$type='wstring'">String</xsl:when>
    <xsl:when test="$type='boolean'">Boolean</xsl:when>
    <!--     <xsl:when test="$type='double'">double</xsl:when> -->
    <!--     <xsl:when test="$type='float'">float</xsl:when> -->
    <!-- <xsl:when test="$type='octet'">byte</xsl:when> -->
    <!--     <xsl:when test="$type='short'">short</xsl:when> -->
    <xsl:when test="$type='unsigned short'">int</xsl:when>
    <xsl:when test="$type='long'">Integer</xsl:when>
    <xsl:when test="$type='long long'">Long</xsl:when>
    <xsl:when test="$type='unsigned long'">Long</xsl:when>
    <xsl:when test="$type='unsigned long long'">BigInteger</xsl:when>
    <xsl:when test="$type='result'">Long</xsl:when>
    <xsl:when test="$type='uuid'">String</xsl:when>
    <xsl:when test="$type='$unknown'">String</xsl:when>
    <xsl:when test="//interface[@name=$type]/@wsmap='managed'">String</xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="fullClassName">
        <xsl:with-param name="name" select="$type" />
        <xsl:with-param name="collPrefix" select="'ArrayOf'"/>
      </xsl:call-template>
    </xsl:otherwise>
   <!--  <xsl:otherwise> -->
<!--       <xsl:call-template name="fatalError"> -->
<!--         <xsl:with-param name="msg" select="concat('typeIdl2Java: Type &quot;', $type, '&quot; in arg &quot;', $name, '&quot; of method &quot;', $ifname, '::', $method, '&quot; is not supported.')" /> -->
<!--       </xsl:call-template> -->
<!--     </xsl:otherwise> -->
  </xsl:choose>
  <xsl:if test="$safearray">
    <xsl:value-of select="'&gt;'" />
  </xsl:if>
</xsl:template>

<xsl:template name="cookOutParam">
  <xsl:param name="ifname"/>
  <xsl:param name="methodname"/>
  <xsl:param name="value"/>
  <xsl:param name="idltype"/>
  <xsl:param name="safearray"/>
  <xsl:choose>
    <xsl:when test="$idltype='uuid'">
      <xsl:choose>
        <xsl:when test="$safearray">
          <xsl:value-of select="concat('Helper.uuidWrap(',$value,')')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('UUID.fromString(',$value,')')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:when test="//collection[@name=$idltype]">
      <xsl:variable name="elemtype">
        <xsl:call-template name="typeIdl2Glue">
          <xsl:with-param name="ifname" select="$ifname" />
          <xsl:with-param name="method" select="$methodname" />
          <xsl:with-param name="name" select="$value" />
          <xsl:with-param name="type" select="$idltype" />
          <xsl:with-param name="forceelem" select="'yes'" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:choose>
        <xsl:when test="contains($elemtype,  $G_virtualBoxPackage)">
          <xsl:value-of select="concat($value,'.getArray()')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat('Helper.wrap(', $elemtype, '.class, port, ((',
                                $value,' == null)? null : ',$value,'.getArray()))')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:when test="//interface[@name=$idltype] or $idltype='$unknown'">
      <xsl:choose>
        <xsl:when test="//interface[@name=$idltype]/@wsmap='struct' and $safearray='yes'">
          <xsl:value-of select="concat('/* 2 */', $value)" />
        </xsl:when>
        <xsl:when test="//interface[@name=$idltype]/@wsmap='struct'">
          <xsl:value-of select="$value" />
        </xsl:when>
        <xsl:when test="$safearray='yes'">
          <xsl:variable name="elemtype">
            <xsl:call-template name="typeIdl2Glue">
              <xsl:with-param name="ifname" select="$ifname" />
              <xsl:with-param name="method" select="$methodname" />
              <xsl:with-param name="name" select="$value" />
              <xsl:with-param name="type" select="$idltype" />
              <xsl:with-param name="safearray" select="'no'" />
              <xsl:with-param name="forceelem" select="'yes'" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="concat('Helper.wrap(',$elemtype, '.class, port, ', $value,')')"/>
        </xsl:when>
        <xsl:otherwise>
           <xsl:variable name="gluetype">
             <xsl:call-template name="typeIdl2Glue">
               <xsl:with-param name="ifname" select="$ifname" />
               <xsl:with-param name="method" select="$methodname" />
               <xsl:with-param name="name" select="$value" />
               <xsl:with-param name="type" select="$idltype" />
               <xsl:with-param name="safearray" select="$safearray" />
             </xsl:call-template>
           </xsl:variable>
           <xsl:value-of select="concat('new ', $gluetype, '(', $value,', port)')" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$value"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="emitArgInMethodImpl">
  <xsl:param name="paramname" select="@name" />
  <xsl:param name="paramtype" select="@type" />
  <!-- per-argument special type handling -->
  <xsl:choose>
    <xsl:when test="//interface[@name=$paramtype] or $paramtype='$unknown'">
      <xsl:choose>
        <xsl:when test="@dir='out'">
          <xsl:value-of select="concat('tmp', $paramname)" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <xsl:when test="@safearray='yes'">
               <xsl:value-of select="concat('Helper.unwrap(',$paramname,')')"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat('((', $paramname, ' == null)?null:', $paramname, '.getRef())')" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:when test="$paramtype='uuid'">
      <xsl:value-of select="concat($paramname, '.toString()')" />
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$paramname" />
    </xsl:otherwise>
  </xsl:choose>
  <xsl:if test="not(position()=last())">
    <xsl:text>, </xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template name="startFile">
  <xsl:param name="file" />
  
  <xsl:value-of select="concat('&#10;// ##### BEGINFILE &quot;', $file, '&quot;&#10;&#10;')" />
  <xsl:call-template name="fileheader">
    <xsl:with-param name="name" select="$file" />
  </xsl:call-template>
package <xsl:value-of select="$G_virtualBoxPackage2" />;

import <xsl:value-of select="$G_virtualBoxPackage" />.VboxPortType;
import <xsl:value-of select="$G_virtualBoxPackage" />.VboxService;
import <xsl:value-of select="$G_virtualBoxPackage" />.InvalidObjectFaultMsg;
import <xsl:value-of select="$G_virtualBoxPackage" />.RuntimeFaultMsg;
import javax.xml.ws.WebServiceException;
</xsl:template>

<xsl:template name="endFile">
 <xsl:param name="file" />
 <xsl:value-of select="concat('&#10;// ##### ENDFILE &quot;', $file, '&quot;&#10;&#10;')" />
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  root match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="/idl">
 <xsl:call-template name="startFile">
  <xsl:with-param name="file" select="'IUnknown.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public class IUnknown
{
   protected String _this; /* almost final, could only be set in finalizer */
   protected final  VboxPortType port;

   public IUnknown(String _this, VboxPortType port)
   {
      this._this = _this;
      this.port = port;
   }

   public final String getRef()
   {
      return _this;
   }

   public final VboxPortType getRemoteWSPort()
   {
      return port;
   }

   public synchronized void releaseRemote() throws WebServiceException
   {
      if (_this == null) {
        return;
      }
      try {
          port.iManagedObjectRefRelease(_this);
          _this = null;
      } catch (InvalidObjectFaultMsg e) {
          throw new WebServiceException(e);
      } catch (RuntimeFaultMsg e) {
          throw new WebServiceException(e);
      }
   }

   /*
   protected void finalize()
   {
      try {
         releaseRemote();
      } catch (WebServiceException e) {
      }
   } */

   // may need to support some sort of QueryInterface, to make this class useable
   // not only as common baseclass
}
]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'IUnknown.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
   <xsl:with-param name="file" select="'Helper.java'" />
 </xsl:call-template>

<xsl:text><![CDATA[

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.UUID;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

class Helper {
    public static <T> List<T> wrap(Class<T> wrapperClass, VboxPortType pt, List<String> thisPtrs) {
        try {
            if(thisPtrs==null)  return Collections.emptyList();

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

    public static List<UUID> uuidWrap(List<String> uuidVals) {
         List<UUID> ret = new ArrayList<UUID>(uuidVals.size());
         for (String uuid : uuidVals) {
              ret.add(UUID.fromString(uuid));
         }
         return ret;
    }

    public static <T extends IUnknown> List<String> unwrap(List<T> thisPtrs) {
        if (thisPtrs==null)  return Collections.emptyList();

        List<String> ret = new ArrayList<String>();
        for (T obj : thisPtrs) {
          ret.add(obj.getRef());
        }
        return ret;
    }
}
]]></xsl:text>

 <xsl:call-template name="endFile">
  <xsl:with-param name="file" select="'Helper.java'" />
 </xsl:call-template>

 <xsl:call-template name="startFile">
  <xsl:with-param name="file" select="'IWebsessionManager.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
import java.net.URL;
import java.math.BigInteger;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.UUID;
import javax.xml.namespace.QName;
import javax.xml.ws.BindingProvider;
import javax.xml.ws.Holder;
import javax.xml.ws.WebServiceException;

class PortPool
{
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
        for (VboxPortType cur: known.keySet())
        {
            if (known.get(cur) == 0)
            {
                port = cur;
                break;
            }
        }

        if (port == null)
        {
            if (svc == null) {
                URL wsdl = PortPool.class.getClassLoader().getResource("vboxwebService.wsdl");
                if (wsdl == null)
                    throw new LinkageError("vboxwebService.wsdl not found, but it should have been in the jar");
                svc = new VboxService(wsdl,
                                      new QName("http://www.virtualbox.org/Service",
                                                "vboxService"));
            }
            port = svc.getVboxServicePort();
        }
        known.put(port, new Integer(1));
        return port;
    }

    synchronized void releasePort(VboxPortType port)
    {
        Integer val =  known.get(port);
        if (val == null || val == 0)
        {
            // know you not
            return;
        }
        known.put(port, val - 1);
    }
}

public class IWebsessionManager {

    private static PortPool pool = new PortPool(true);
    protected VboxPortType port;

    public IWebsessionManager(URL url)
    {
        connect(url);
    }

    public IWebsessionManager(String url)
    {
        connect(url);
    }

    public IWebsessionManager(URL url, Map<String, Object> requestContext, Map<String, Object> responseContext)
    {
        connect(url.toExternalForm(), requestContext, responseContext);
    }

    public IWebsessionManager(String url, Map<String, Object> requestContext, Map<String, Object> responseContext)
    {
        connect(url, requestContext, responseContext);
    }

    public void connect(URL url)
    {
        connect(url.toExternalForm());
    }

    public void connect(String url)
    {
        this.port = pool.getPort();
        ((BindingProvider)port).getRequestContext().
                put(BindingProvider.ENDPOINT_ADDRESS_PROPERTY, url);
    }

    public void connect(String url, Map<String, Object> requestContext, Map<String, Object> responseContext)
    {
         this.port = pool.getPort();

         ((BindingProvider)port).getRequestContext();
         if (requestContext != null)
               ((BindingProvider)port).getRequestContext().putAll(requestContext);

         if (responseContext != null)
               ((BindingProvider)port).getResponseContext().putAll(responseContext);

         ((BindingProvider)port).getRequestContext().
                put(BindingProvider.ENDPOINT_ADDRESS_PROPERTY, url);
    }


    public void disconnect(IVirtualBox refIVirtualBox)
    {
        logoff(refIVirtualBox);
        pool.releasePort(port);
    }

    public void cleanupUnused()
    {
       System.gc();
       Runtime.getRuntime().runFinalization();
    }

    /* method IWebsessionManager::logon(
            [in] wstring username,
            [in] wstring password,
            [return] IVirtualBox return)
     */
    public IVirtualBox logon(String username, String password) {
        try {
            String retVal = port.iWebsessionManagerLogon(username, password);
            return new IVirtualBox(retVal, port);
        } catch (InvalidObjectFaultMsg e) {
            throw new WebServiceException(e);
        } catch (RuntimeFaultMsg e) {
            throw new WebServiceException(e);
        }
    }

    /* method IWebsessionManager::getSessionObject(
            [in] IVirtualBox refIVirtualBox,
            [return] ISession return)
     */
    public ISession getSessionObject(IVirtualBox refIVirtualBox) {
        try {
            String retVal = port.iWebsessionManagerGetSessionObject(((refIVirtualBox == null)?null:refIVirtualBox.getRef()));
            return new ISession(retVal, port);
        } catch (InvalidObjectFaultMsg e) {
            throw new WebServiceException(e);
        } catch (RuntimeFaultMsg e) {
            throw new WebServiceException(e);
        }
    }

    /* method IWebsessionManager::logoff(
            [in] IVirtualBox refIVirtualBox)
     */
    public void logoff(IVirtualBox refIVirtualBox) {
        try {
            port.iWebsessionManagerLogoff(((refIVirtualBox == null)?null:refIVirtualBox.getRef()));
        } catch (InvalidObjectFaultMsg e) {
            throw new WebServiceException(e);
        } catch (RuntimeFaultMsg e) {
            throw new WebServiceException(e);
        }
    }
}
]]></xsl:text>
 <xsl:call-template name="endFile">
  <xsl:with-param name="file" select="'IWebsessionManager.java'" />
 </xsl:call-template>

  <xsl:text>// ######## COLLECTIONS&#10;&#10;</xsl:text>

  <xsl:for-each select="//collection">
    <xsl:variable name="type" select="@type" />
    <xsl:variable name="arrayoftype" select="concat('ArrayOf', @type)" />
    <xsl:variable name="filename" select="$arrayoftype" />

    <xsl:value-of select="concat('&#10;// ##### BEGINFILE &quot;', $filename, '.java&quot;&#10;&#10;')" />

    <xsl:call-template name="startFile">
      <xsl:with-param name="file" select="concat($filename, '.java')" />
    </xsl:call-template>

    <xsl:text>import java.util.ArrayList;&#10;</xsl:text>
    <xsl:text>import java.util.List;&#10;</xsl:text>
    <xsl:text>import javax.xml.bind.annotation.XmlAccessType;&#10;</xsl:text>
    <xsl:text>import javax.xml.bind.annotation.XmlAccessorType;&#10;</xsl:text>
    <xsl:text>import javax.xml.bind.annotation.XmlType;&#10;&#10;</xsl:text>

    <xsl:text>@XmlAccessorType(XmlAccessType.FIELD)&#10;</xsl:text>
    <xsl:value-of select="concat('@XmlType(name = &quot;', $arrayoftype, '&quot;, propOrder = {&#10;')" />
    <xsl:text>    "array"&#10;</xsl:text>
    <xsl:text>})&#10;&#10;</xsl:text>
    <xsl:value-of select="concat('public class ', $arrayoftype, ' {&#10;&#10;')" />

    <xsl:text>    protected List&lt;String&gt; array;&#10;&#10;</xsl:text>

    <xsl:text>    public List&lt;String&gt; getArray() {&#10;</xsl:text>
    <xsl:text>        if (array == null) {&#10;</xsl:text>
    <xsl:text>            array = new ArrayList&lt;String&gt;();&#10;</xsl:text>
    <xsl:text>        }&#10;</xsl:text>
    <xsl:text>        return this.array;&#10;</xsl:text>
    <xsl:text>    }&#10;&#10;</xsl:text>
    <xsl:text>}&#10;</xsl:text>
    <xsl:call-template name="endFile">
      <xsl:with-param name="file" select="concat($filename, '.java')" />
    </xsl:call-template>
    
  </xsl:for-each>

  <xsl:text>// ######## ENUMS&#10;&#10;</xsl:text>

  <xsl:for-each select="//enum">
    <xsl:variable name="enumname" select="@name" />
    <xsl:variable name="filename" select="$enumname" />

    <xsl:call-template name="startFile">
      <xsl:with-param name="file" select="concat($filename, '.java')" />
    </xsl:call-template>

    <xsl:text>import javax.xml.bind.annotation.XmlEnum;&#10;</xsl:text>
    <xsl:text>import javax.xml.bind.annotation.XmlEnumValue;&#10;</xsl:text>
    <xsl:text>import javax.xml.bind.annotation.XmlType;&#10;&#10;</xsl:text>

    <xsl:value-of select="concat('@XmlType(name = &quot;', $enumname, '&quot;)&#10;')" />
    <xsl:text>@XmlEnum&#10;</xsl:text>
    <xsl:value-of select="concat('public enum ', $enumname, ' {&#10;&#10;')" />
    <xsl:for-each select="const">
      <xsl:variable name="enumconst" select="@name" />
      <xsl:value-of select="concat('    @XmlEnumValue(&quot;', $enumconst, '&quot;)&#10;')" />
      <xsl:value-of select="concat('    ', $enumconst, '(&quot;', $enumconst, '&quot;)')" />
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
    <xsl:text>    private final String value;&#10;&#10;</xsl:text>

    <xsl:value-of select="concat('    ', $enumname, '(String v) {&#10;')" />
    <xsl:text>        value = v;&#10;</xsl:text>
    <xsl:text>    }&#10;&#10;</xsl:text>

    <xsl:text>    public String value() {&#10;</xsl:text>
    <xsl:text>        return value;&#10;</xsl:text>
    <xsl:text>    }&#10;&#10;</xsl:text>

    <xsl:value-of select="concat('    public static ', $enumname, ' fromValue(String v) {&#10;')" />
    <xsl:value-of select="concat('        for (', $enumname, ' c: ', $enumname, '. values()) {&#10;')" />
    <xsl:text>            if (c.value.equals(v)) {&#10;</xsl:text>
    <xsl:text>                return c;&#10;</xsl:text>
    <xsl:text>            }&#10;</xsl:text>
    <xsl:text>        }&#10;</xsl:text>
    <xsl:text>        throw new IllegalArgumentException(v);&#10;</xsl:text>
    <xsl:text>    }&#10;&#10;</xsl:text>

    <xsl:text>}&#10;&#10;</xsl:text>

    <xsl:call-template name="endFile">
      <xsl:with-param name="file" select="concat($filename, '.java')" />
    </xsl:call-template>

  </xsl:for-each>

  <xsl:text>// ######## INTERFACES &#10;&#10;</xsl:text>

  <xsl:for-each select="//interface">
    <xsl:variable name="ifname" select="@name" />
    <xsl:variable name="filename" select="$ifname" />
    <xsl:variable name="wsmap" select="@wsmap" />
    <xsl:variable name="wscpp" select="@wscpp" />

    <xsl:if test="not($wsmap='suppress') and not($wsmap='struct') and not ($wsmap='global')">
      <xsl:call-template name="startFile">
        <xsl:with-param name="file" select="concat($filename, '.java')" />
      </xsl:call-template>

      <xsl:text>import java.math.BigInteger;&#10;</xsl:text>
      <xsl:text>import java.util.List;&#10;</xsl:text>
      <xsl:text>import java.util.UUID;&#10;</xsl:text>
      <xsl:text>import javax.xml.ws.Holder;&#10;</xsl:text>
      <xsl:text>import javax.xml.ws.WebServiceException;&#10;</xsl:text>      

      <xsl:choose>
        <xsl:when test="$wsmap='struct'">

          <xsl:value-of select="concat('public class ', $ifname, ' {&#10;&#10;')" />

        </xsl:when>

        <xsl:otherwise>

          <!-- interface (class) constructor -->
          <xsl:value-of select="concat('public class ', $ifname, ' extends IUnknown {&#10;&#10;')" />
          <xsl:value-of select="concat('    public static ', $ifname, ' cast(IUnknown other) {&#10;')" />
          <xsl:value-of select="concat('        return new ', $ifname,
                              '(other.getRef(), other.getRemoteWSPort());&#10;    }&#10;&#10;')"/>
          <xsl:value-of select="concat('    public ', $ifname, '(String _this, VboxPortType port) {&#10;')" />
          <xsl:text>        super(_this,port);&#10;</xsl:text>
          <xsl:text>    }&#10;</xsl:text>

          <!-- attributes -->
          <xsl:for-each select="attribute">
            <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
            <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
            <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
            <xsl:variable name="attrsafearray"><xsl:value-of select="@safearray" /></xsl:variable>

            <xsl:choose>
              <xsl:when test="( $attrtype=($G_setSuppressedInterfaces/@name) )">
                <xsl:value-of select="concat('&#10;    // Skipping attribute ', $attrtype, ' for it is of suppressed type ', $attrtype, '&#10;')" />
              </xsl:when>
              <xsl:otherwise>
                <xsl:choose>
                  <xsl:when test="@readonly='yes'">
                    <xsl:value-of select="concat('&#10;    // read-only attribute ', $ifname, '::', $attrname, ' of type ', $attrtype, '&#10;')" />
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:value-of select="concat('&#10;    // read/write attribute ', $ifname, '::', $attrname, ' of type ', $attrtype, '&#10;')" />
                  </xsl:otherwise>
                </xsl:choose>
                <!-- emit getter method -->
                <xsl:variable name="gettername"><xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
                <xsl:variable name="jaxwsGetter"><xsl:call-template name="makeJaxwsMethod"><xsl:with-param name="ifname" select="$ifname" /><xsl:with-param name="methodname" select="$gettername" /></xsl:call-template></xsl:variable>
                <xsl:variable name="gluegettertype">
                  <xsl:call-template name="typeIdl2Glue">
                    <xsl:with-param name="ifname" select="$ifname" />
                    <xsl:with-param name="method" select="$gettername" />
                    <xsl:with-param name="name" select="$attrname" />
                    <xsl:with-param name="type" select="$attrtype" />
                    <xsl:with-param name="safearray" select="@safearray" />
                  </xsl:call-template>
                </xsl:variable>
                <xsl:variable name="javagettertype">
                  <xsl:call-template name="typeIdl2Java">
                    <xsl:with-param name="ifname" select="$ifname" />
                    <xsl:with-param name="method" select="$gettername" />
                    <xsl:with-param name="name" select="$attrname" />
                    <xsl:with-param name="type" select="$attrtype" />
                    <xsl:with-param name="safearray" select="@safearray" />
                  </xsl:call-template>
                </xsl:variable>
                <xsl:value-of select="concat('    public ', $gluegettertype, ' ', $gettername, '() {&#10;')" />
                <xsl:text>        try {&#10;</xsl:text>
                <xsl:value-of select="concat('            ', $javagettertype, ' retVal = port.', $jaxwsGetter, '(_this);&#10;')" />
                <xsl:variable name="wrapped">
                  <xsl:call-template name="cookOutParam">
                    <xsl:with-param name="ifname" select="$ifname" />
                    <xsl:with-param name="method" select="$gettername" />
                    <xsl:with-param name="value" select="'retVal'" />
                    <xsl:with-param name="idltype" select="$attrtype" />
                    <xsl:with-param name="safearray" select="@safearray" />
                  </xsl:call-template>
                </xsl:variable>
                <xsl:value-of select="concat('            return ', $wrapped, ';&#10;')" />
                <xsl:text>        } catch (InvalidObjectFaultMsg e) {&#10;</xsl:text>
                <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                <xsl:text>        } catch (RuntimeFaultMsg e) {&#10;</xsl:text>
                <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                <xsl:text>        }&#10;</xsl:text>
                <xsl:text>    }&#10;</xsl:text>
                <xsl:if test="not(@readonly='yes')">
                  <!-- emit setter -->
                  <xsl:variable name="settername"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
                  <xsl:variable name="jaxwsSetter"><xsl:call-template name="makeJaxwsMethod"><xsl:with-param name="ifname" select="$ifname" /><xsl:with-param name="methodname" select="$settername" /></xsl:call-template></xsl:variable>
                  <xsl:variable name="javasettertype">
                    <xsl:call-template name="typeIdl2Java">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="method" select="$settername" />
                      <xsl:with-param name="name" select="$attrname" />
                      <xsl:with-param name="type" select="$attrtype" />
                    </xsl:call-template>
                  </xsl:variable>
                  <xsl:value-of select="concat('    public void ', $settername, '(', $javasettertype, ' value) {&#10;')" />
                  <xsl:text>        try {&#10;</xsl:text>
                  <xsl:value-of select="concat('            port.', $jaxwsSetter, '(_this, value);&#10;')" />
                  <xsl:text>        } catch (InvalidObjectFaultMsg e) {&#10;</xsl:text>
                  <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                  <xsl:text>        } catch (RuntimeFaultMsg e) {&#10;</xsl:text>
                  <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                  <xsl:text>        }&#10;</xsl:text>
                  <xsl:text>    }&#10;</xsl:text>
                </xsl:if>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:for-each> <!-- attribute -->

          <!-- emit real methods -->
          <xsl:for-each select="method">
            <xsl:variable name="methodname"><xsl:value-of select="@name" /></xsl:variable>
            <xsl:variable name="portArg">
                <xsl:if test="not($wsmap='global')">
                  <xsl:value-of select="'_this'"/>
                </xsl:if>
            </xsl:variable>

            <!-- method header: return value "int", method name, soap arguments -->
            <!-- skip this method if it has parameters of a type that has wsmap="suppress" -->
            <xsl:choose>
              <xsl:when test="param[@type=($G_setSuppressedInterfaces/@name)]">
                <xsl:comment><xsl:value-of select="concat('Skipping method ', $methodname, ' for it has parameters with suppressed types')" /></xsl:comment>
              </xsl:when>
              <xsl:otherwise>
                <xsl:variable name="fHasReturnParms" select="param[@dir='return']" />
                <xsl:variable name="fHasOutParms" select="param[@dir='out']" />

                <xsl:value-of select="concat('&#10;    /* method ', $ifname, '::', $methodname, '(')" />
                <xsl:for-each select="param">
                  <xsl:value-of select="concat('&#10;            [', @dir, '] ', @type, ' ', @name)" />
                  <xsl:if test="@safearray='yes'">
                    <xsl:text>[]</xsl:text>
                  </xsl:if>
                  <xsl:if test="not(position()=last())">
                    <xsl:text>,</xsl:text>
                  </xsl:if>
                </xsl:for-each>
                <xsl:text>)&#10;     */&#10;</xsl:text>
                <!-- method implementation -->
                <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
                <xsl:variable name="returnidlsafearray" select="param[@dir='return']/@safearray" />
                <xsl:variable name="returngluetype">
                  <xsl:choose>
                    <xsl:when test="$returnidltype">
                      <xsl:call-template name="typeIdl2Glue">
                        <xsl:with-param name="ifname" select="$ifname" />
                        <xsl:with-param name="method" select="$methodname" />
                        <xsl:with-param name="name" select="@name" />
                        <xsl:with-param name="type" select="$returnidltype" />
                        <xsl:with-param name="safearray" select="param[@dir='return']/@safearray" />
                      </xsl:call-template>
                    </xsl:when>
                    <xsl:otherwise>
                      <xsl:text>void</xsl:text>
                    </xsl:otherwise>
                  </xsl:choose>
                </xsl:variable>
                <xsl:value-of select="concat('    public ', $returngluetype, ' ', $methodname, '(')" />
                <!-- make a set of all parameters with in and out direction -->
                <xsl:variable name="paramsinout" select="param[@dir='in' or @dir='out']" />
                <xsl:for-each select="exsl:node-set($paramsinout)">
                  <xsl:variable name="paramgluetype">
                    <xsl:call-template name="typeIdl2Glue">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="method" select="$methodname" />
                      <xsl:with-param name="name" select="@name" />
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
                <xsl:text>) {&#10;</xsl:text>
                <xsl:text>        try {&#10;</xsl:text>
                <xsl:if test="param[@dir='out']">
                  <xsl:for-each select="param[@dir='out']">
                    <xsl:variable name="paramtype" select="@type" />
                    <xsl:if test="//interface[@name=$paramtype] or $paramtype='$unknown'">
                      <xsl:choose>
                        <xsl:when test="@safearray='yes'">
                           <xsl:value-of select="concat('            Holder&lt;List&lt;String&gt;&gt; tmp', @name, ' = new Holder&lt;List&lt;String&gt;&gt;(); &#10;')" />
                        </xsl:when>
                        <xsl:otherwise>
                          <xsl:value-of select="concat('            Holder&lt;String&gt; tmp', @name, ' = new Holder&lt;String&gt;(); &#10;')" />
                          </xsl:otherwise>
                        </xsl:choose>
                    </xsl:if>
                  </xsl:for-each>
                </xsl:if>

                <xsl:text>            </xsl:text>

                <!-- make the function call: first have a stack variable for the return value, if any -->
                <!-- XSLT doesn't allow variable override in inner blocks -->
                <xsl:variable name="retValValue">
                  <xsl:choose>
                    <xsl:when test="param[@dir='out']">
                      <xsl:value-of select="'retVal.value'"/>
                    </xsl:when>
                    <xsl:otherwise>
                      <xsl:value-of select="'retVal'"/>
                    </xsl:otherwise>
                  </xsl:choose>
                </xsl:variable>

                <xsl:if test="$returnidltype">
                  <xsl:variable name="javarettype">
                    <xsl:call-template name="typeIdl2Java">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="method" select="$methodname" />
                      <xsl:with-param name="name" select="@name" />
                      <xsl:with-param name="type" select="$returnidltype" />
                      <xsl:with-param name="safearray" select="$returnidlsafearray" />
                    </xsl:call-template>
                  </xsl:variable>
                  <xsl:choose>
                    <xsl:when test="param[@dir='out']">
                      <!-- create a new object for return value -->
                       <xsl:value-of select="
                                      concat('Holder&lt;', $javarettype, '&gt;',
                                             ' ', 'retVal = new Holder&lt;', $javarettype,
                                             '&gt;();&#xa;            ')"/>
                    </xsl:when>
                    <xsl:otherwise>
                      <xsl:value-of select="$javarettype"/>
                      <xsl:text> retVal = </xsl:text>
                    </xsl:otherwise>
                  </xsl:choose>
                </xsl:if>
                <!-- function name and arguments -->
                <xsl:variable name="jaxwsmethod"><xsl:call-template name="makeJaxwsMethod"><xsl:with-param name="ifname" select="$ifname" /><xsl:with-param name="methodname" select="$methodname" /></xsl:call-template></xsl:variable>
                <xsl:value-of select="concat('port.', $jaxwsmethod, '(', $portArg)" />
                <xsl:if test="$paramsinout and not($portArg='')">
                <xsl:text>, </xsl:text>
                </xsl:if>
                <!-- jax-ws has an oddity: if both out params and a return value exist, then the return value is moved to the function's argument list... -->
                <xsl:choose>
                  <xsl:when test="param[@dir='out'] and param[@dir='return']">
                    <xsl:for-each select="param">
                      <xsl:choose>
                        <xsl:when test="@dir='return'">
                          <xsl:text>retVal</xsl:text>
                        </xsl:when>
                        <xsl:otherwise>
                          <xsl:call-template name="emitArgInMethodImpl">
                            <xsl:with-param name="paramname" select="@name" />
                            <xsl:with-param name="paramtype" select="@type" />
                          </xsl:call-template>
                        </xsl:otherwise>
                      </xsl:choose>
                    </xsl:for-each>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:for-each select="$paramsinout">
                      <xsl:call-template name="emitArgInMethodImpl">
                        <xsl:with-param name="paramname" select="@name" />
                        <xsl:with-param name="paramtype" select="@type" />
                      </xsl:call-template>
                    </xsl:for-each>
                  </xsl:otherwise>
                </xsl:choose>
                <xsl:text>);&#10;</xsl:text>
                <!-- now copy temp out parameters to their actual destination -->
                <xsl:for-each select="param[@dir='out']">
                  <xsl:variable name="paramtype" select="@type" />
                  <xsl:if test="//interface[@name=$paramtype] or $paramtype='$unknown'">
                    <xsl:variable name="paramname" select="@name" />
                     <xsl:variable name="wrapped">
                        <xsl:call-template name="cookOutParam">
                          <xsl:with-param name="ifname" select="$ifname" />
                          <xsl:with-param name="method" select="$methodname" />
                          <xsl:with-param name="value" select="concat('tmp',@name,'.value')" />
                          <xsl:with-param name="idltype" select="@type" />
                          <xsl:with-param name="safearray" select="@safearray" />
                        </xsl:call-template>
                     </xsl:variable>
                    <xsl:value-of select="concat('            ',$paramname,'.value = ',
                                                  $wrapped,';&#10;')"/>
                  </xsl:if>
                </xsl:for-each>
                <!-- next line with return + glue type -->
                <xsl:if test="$returnidltype">
                  <xsl:variable name="retval">
                    <xsl:call-template name="cookOutParam">
                      <xsl:with-param name="ifname" select="$ifname" />
                      <xsl:with-param name="method" select="$methodname" />
                      <xsl:with-param name="value" select="$retValValue" />
                      <xsl:with-param name="idltype" select="$returnidltype" />
                      <xsl:with-param name="safearray" select="$returnidlsafearray" />
                    </xsl:call-template>
                  </xsl:variable>
                  <xsl:value-of select="concat('            return ', $retval, ';&#10;')"/>
                </xsl:if>
                <xsl:text>        } catch (InvalidObjectFaultMsg e) {&#10;</xsl:text>
                <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                <xsl:text>        } catch (RuntimeFaultMsg e) {&#10;</xsl:text>
                <xsl:text>            throw new WebServiceException(e);&#10;</xsl:text>
                <xsl:text>        }&#10;</xsl:text>
                <xsl:text>    }&#10;</xsl:text>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:for-each>

        </xsl:otherwise>
      </xsl:choose>
      <!-- end of class -->
      <xsl:text>}&#10;</xsl:text>
      <xsl:value-of select="concat('&#10;// ##### ENDFILE &quot;', $filename, '.java&quot;&#10;&#10;')" />
      <xsl:call-template name="endFile">
        <xsl:with-param name="file" select="concat($filename, '.java')" />
      </xsl:call-template>
    </xsl:if>
  </xsl:for-each>

<!--   <xsl:apply-templates /> -->
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
<!--  TODO swallow for now -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  note
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="note">
<!--  TODO -->
  <xsl:apply-templates />
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  interface
  - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface">

</xsl:template>


</xsl:stylesheet>
