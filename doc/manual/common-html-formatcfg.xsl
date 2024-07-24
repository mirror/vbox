<?xml version="1.0"?>

<!--
    Copyright (C) 2006-2023 Oracle and/or its affiliates.

    This file is part of VirtualBox base platform packages, as
    available from https://www.virtualbox.org.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, in version 3 of the
    License.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <https://www.gnu.org/licenses>.

    SPDX-License-Identifier: GPL-3.0-only
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- Must strip spaces in 'arg' elements too, we'll get extra spaces when
     using 'replaceable'. Adding option too, just in case. -->
<xsl:strip-space elements="arg option"/>

<!-- Our hand written css styling. -->
<xsl:template name="user.head.content">
 <style type="text/css">
  <xsl:comment>
   body
   {
     font-family:  Verdana, Sans-serif, Arial, 'Trebuchet MS', 'Times New Roman';
     font-size: small;
   }
   h2.title
   {
     font-family: Verdana, Sans-serif, Arial, 'Trebuchet MS', 'Times New Roman';
     margin: 5px 0 0;
     padding: 1px 5px 1px;
     border: 1px solid #6b89d4;
     -moz-border-radius: 0.3em;
     background: #e6edff;
   }
   .titlepage
   {
     text-align: center;
   }
   .refsynopsisdiv, .refsect1, .refsect2, .refsect3
   {
     text-align: left;
   }
   .warning
   {
     padding: 5px;
     border: 1px solid #ff0011;
     -moz-border-radius: 0.3em;
     background: #ffbbbb;
   }
   .warning .title { margin: 0px 0px 5px 0px; }
   .warning p { margin: 1px; }
   .note
   {
     padding: 1px 5px 1px;
     border: 1px solid #84c43b;
     -moz-border-radius: 0.3em;
     background: #d7e9a7;
   }
   .note .title { margin: 0px 0px 5px 0px; }
   .note p { margin: 1px; }
   .cmdsynopsis
   {
     font-family: monospace;
   }
   .refsynopsisdiv        > .cmdsynopsis p, .refsect1        > .cmdsynopsis p,
   .refsynopsisdiv .sect2 > .cmdsynopsis p, .refsect1 .sect2 > .cmdsynopsis p
   {
     margin-top: 0px;
     margin-bottom: 0px;
   }
   .cmdsynopsis p
   {
     padding-left: 3.4em;
     text-indent: -2.2em;
   }
   p.nextcommand
   {
     margin-top:    0px;
     margin-bottom: 0px;
   }
   p.lastcommand
   {
     margin-top:    0px;
   }
   .refentry * h3
   {
     font-size: large;
   }
   .refentry * h4
   {
     font-size: larger;
   }
   .refentry * h5
   {
     font-size: larger;
   }

  </xsl:comment>
 </style>
</xsl:template>


<!-- Ignore/skip the remark that the command overview inclusion file
     uses as the root element. -->
<xsl:template match="remark[@role='VBoxManage-overview']">
  <xsl:apply-templates select="node()"/>
</xsl:template>


<!-- To use CSS to correctly insert hanging indent when soft wrapping and
  <sbr>'ing a synopsis, we must place each command in its own <p>.  The default
  is to must issue a <br /> before each <command>, except the first one.
  Note! This is a bit ugly as we're going contrary to the grain of XSLT here
        starting with an closing . -->
<xsl:template match="cmdsynopsis/command">
  <xsl:text disable-output-escaping="yes"><![CDATA[</p><p class="nextcommand">]]></xsl:text>
  <xsl:call-template name="inline.monoseq"/>
  <xsl:text> </xsl:text>
</xsl:template>
<xsl:template match="cmdsynopsis/command[last()]">
  <xsl:text disable-output-escaping="yes"><![CDATA[</p><p class="lastcommand">]]></xsl:text>
  <xsl:call-template name="inline.monoseq"/>
  <xsl:text> </xsl:text>
</xsl:template>

</xsl:stylesheet>

