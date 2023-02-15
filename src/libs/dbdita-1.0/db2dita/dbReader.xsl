<?xml version="1.0" ?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:param name="IDENTIFY_SOURCE" select="'yes'"/>

<!-- TO DO
     parameter to split out generated topics as separate files
     optional apiRef specialization - reference for oo*
     concept specialization
     map and bookmap output
     upgrade to DITA 1.1 and DocBook 5
     adjust references to elements that don't appear in output
     bibliography content
     productionset content as syntaxdiagram
     imagedata with format="linespecific"
     areaspec in callouts
     emit out-of-content index terms in nearest content or prolog
     synph instead of syntaxdiagram for syntax within phrase contexts
     map based on systemitem@class
     -->

<!-- DEFAULT ATTRIBUTE RULES - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[@userlevel]" mode="audience.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="audience.att.out">
    <xsl:with-param name="value" select="@userlevel"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@id]" mode="id.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="id.att.out">
    <xsl:with-param name="value" select="@id"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@importance]" mode="importance.att.in">
  <xsl:param name="isRequired"/>
</xsl:template>

<xsl:template match="*[@condition or @conformance or @security]"
      mode="otherprops.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="otherprops.att.out">
    <xsl:with-param name="value">
      <xsl:for-each select="@condition|@conformance|@security">
        <xsl:if test="position() &gt; 1">
          <xsl:text> </xsl:text>
        </xsl:if>
        <xsl:value-of select="."/>
      </xsl:for-each>
    </xsl:with-param>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@role]" mode="outputclass.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="outputclass.att.out">
    <xsl:with-param name="value" select="@role"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[not(@role)]" mode="outputclass.att.in">
  <xsl:if test="$IDENTIFY_SOURCE='yes'">
    <xsl:variable name="value">
      <xsl:text>db.</xsl:text>
      <xsl:value-of select="local-name(.)"/>
    </xsl:variable>
    <xsl:apply-templates select="." mode="outputclass.att.out">
      <xsl:with-param name="value" select="$value"/>
    </xsl:apply-templates>
  </xsl:if>
</xsl:template>

<xsl:template match="*[@arch or @os]" mode="platform.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="platform.att.out">
    <xsl:with-param name="value">
      <xsl:for-each select="@arch|@os">
        <xsl:if test="position() &gt; 1">
          <xsl:text> </xsl:text>
        </xsl:if>
        <xsl:value-of select="."/>
      </xsl:for-each>
    </xsl:with-param>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@revision or @revisionflag]" mode="rev.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="rev.att.out">
    <xsl:with-param name="value">
      <xsl:for-each select="@revision|@revisionflag">
        <xsl:if test="position() &gt; 1">
          <xsl:text> </xsl:text>
        </xsl:if>
        <xsl:value-of select="."/>
      </xsl:for-each>
    </xsl:with-param>
  </xsl:apply-templates>
</xsl:template>

<!-- ENUMERATED in DITA 1.0 BUT NOT Docbook 4
<xsl:template match="*[@status]" mode="status.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="status.att.out">
    <xsl:with-param name="value" select="@status"/>
  </xsl:apply-templates>
</xsl:template>
    -->


<!-- DIVISIONS AND SECTION TOPICS  - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="appendix|article|bibliography|book|chapter|
            colophon|dedication|glossary|glossdiv|msgset|msgentry|
            note|part|partintro|preface|reference|refentry|
            sect1|sect2|sect3|sect4|sect5|section|
            set|simplemsgentry|simplesect"
        mode="isTopic">
  <xsl:text>yes</xsl:text>
</xsl:template>

<xsl:template match="*" mode="isTopic">
  <xsl:text>no</xsl:text>
</xsl:template>

<xsl:template match="/|*[appendix or article or bibliography or book or
            chapter or colophon or dedication or glossary or glossdiv or
            glossentry or msgset or msgentry or part or partintro or 
            preface or qandaset or reference or refentry or sect1 or sect2 or
            sect3 or sect4 or sect5 or section or set or simplemsgentry or
            simplesect or task]"
        mode="topic.topic.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="appendix|article|bibliography|book|chapter|
            colophon|dedication|glossary|glossdiv|glossentry|msgset|msgentry|
            part|partintro|preface|qandaset|reference|refentry|sect1|sect2|
            sect3|sect4|sect5|section|set|simplemsgentry|simplesect|task"
        mode="topic.topic.out"/>
</xsl:template>

<xsl:template match="*[not(@id)]" mode="topic.topic.id.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates select="." mode="topicId"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="id.att.out">
    <xsl:with-param name="value" select="$value"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[title]" mode="topicId">
  <xsl:call-template name="makeTopicId">
    <xsl:with-param name="titleElement" select="title"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[not(title)]" mode="topicId">
  <xsl:call-template name="makeTopicId">
    <xsl:with-param name="titleElement"
          select="*[position()=1 and contains(local-name(),'info')]/title"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="colophon[not(title)]|dedication[not(title)]|
          msgset[not(title)]|msgentry[not(msg/title)]|partintro[not(title)]|
          simplemsgentry"
      mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template
      match="colophon|dedication|msgset|msgentry|partintro|simplemsgentry"
      mode="topic.title.id.atts.in"/>

<xsl:template
      match="colophon|dedication|msgset|msgentry|partintro|simplemsgentry"
      mode="title.cnt.text.in"/>

<xsl:template match="colophon|dedication|partintro|simplesect" mode="topicId">
  <xsl:call-template name="makeTopicId">
    <xsl:with-param name="titleElement" select="title"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="refentry[not(refsynopsisdiv/refsect2) and
          not(refsect1/refsect2) and not(refsection/refsection)]"
      mode="topic.topic.out">
  <xsl:apply-templates select="." mode="reference.reference.out"/>
</xsl:template>

<xsl:template
        match="refentry[not(refentryinfo/title) and refmeta/refentrytitle]"
        mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="refmeta/refentrytitle" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="refentry[not(refentryinfo/title) and 
            not(refmeta/refentrytitle)]"
        mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="refnamediv" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="refentry" mode="reference.refbody.in">
  <xsl:apply-templates select="." mode="reference.refbody.out"/>
</xsl:template>

<xsl:template match="refentry" mode="reference.refbody.content.in">
  <xsl:apply-templates select="refsynopsisdiv|refsect1|refsection"
      mode="reference.refbody.child"/>
</xsl:template>

<xsl:template match="refentry" mode="reference.reference.topic.topic.in"/>

<xsl:template match="refentry" mode="topic.topic.topic.topic.in">
  <xsl:apply-templates select="refsynopsisdiv|refsect1|refsection"
      mode="topic.topic.out"/>
</xsl:template>

<xsl:template match="refname" mode="title.cnt.child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="topic.keyword.out"/>
</xsl:template>

<xsl:template match="refpurpose" mode="title.cnt.child"/>

<xsl:template match="refentry" mode="topic.shortdesc.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="refnamediv/refpurpose"
        mode="topic.shortdesc.out"/>
</xsl:template>

<xsl:template match="*[title]" mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="*[not(title) and
          *[position()=1 and contains(local-name(),'info')]/title]"
      mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="*[position()=1]/title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.shortdesc.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <!-- TO DO: MIGHT OR MIGHT NOT WANT TO GET FROM ABSTRACT -->
</xsl:template>

<xsl:template match="*[titleabbrev]" mode="topic.titlealts.in">
  <xsl:apply-templates select="." mode="topic.titlealts.out"/>
</xsl:template>

<xsl:template match="*[titleabbrev]" mode="topic.navtitle.in">
  <xsl:apply-templates select="titleabbrev" mode="topic.navtitle.out"/>
</xsl:template>

<!-- DocBook subtitle DOESN'T MATCH ANYTHING IN PARTICULAR IN DITA -->

<xsl:template match="*[*[position()=1 and contains(local-name(),'info')]]"
      mode="topic.prolog.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <!-- TO DO: artheader|docinfo ??? -->
  <xsl:apply-templates select="*[position()=1]" mode="topic.prolog.out"/>
</xsl:template>

<xsl:template match="*[position()=1 and contains(local-name(),'info')]"
        mode="topic.metadata.in">
  <xsl:apply-templates select="." mode="topic.metadata.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.body.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="topic.body.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.body.atts.in">
  <xsl:apply-templates select="." mode="topic.body.outputclass.att.in"/>
</xsl:template>

<!-- TO DO: MAY NEED TO DISTINGUISH section -->
<xsl:template match="*" mode="topic.body.content.in">
  <xsl:apply-templates select="ackno|address|anchor|annotation|
            bibliolist|blockquote|bridgehead|calloutlist|caution|classsynopsis|
            cmdsynopsis|constraintdef|constructorsynopsis|destructorsynopsis|
            epigraph|equation|example|fieldsynopsis|figure|formalpara|
            funcsynopsis|glosslist|important|indexterm|informalequation|
            informalexample|informalfigure|informaltable|itemizedlist|
            literallayout|mediaobject|methodsynopsis|orderedlist|
            para|procedure|productionset|programlisting|
            programlistingco|remark|revhistory|screen|screenco|
            screenshot|segmentedlist|sidebar|simpara|simplelist|synopsis|
            table|tip|variablelist|warning"
        mode="topic.body.child"/>
</xsl:template>

<xsl:template match="*" mode="topic.related-links.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
</xsl:template>

<xsl:template match="title" mode="topic.fig.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="title" mode="note.cnt.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="title" mode="para.cnt.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="title" mode="longquote.cnt.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="bridgehead" mode="child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="bridgehead" mode="topic.p.content.in">
  <xsl:apply-templates select="." mode="hi-d.b.out"/>
</xsl:template>

<xsl:template match="title" mode="child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="bridgehead" mode="hi-d.b.atts.in">
  <xsl:apply-templates select="." mode="hi-d.b.outputclass.att.in"/>
</xsl:template>

<xsl:template match="msgentry[msg/title]" mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="msg/title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="msgentry" mode="topicId">
  <xsl:call-template name="makeTopicId">
    <xsl:with-param name="titleElement" select="msg/title"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="msgentry" mode="topic.body.content.in">
  <xsl:apply-templates
      select="msg/msgmain|msg/msgsub|msg/msgrel|msginfo|msgexplan"
      mode="topic.section.out"/>
</xsl:template>

<xsl:template match="msgmain|msgsub|msgrel" mode="section.cnt.text.in">
  <xsl:apply-templates select="msgtext" mode="section.cnt.text.in"/>
</xsl:template>

<xsl:template match="msginfo" mode="section.cnt.text.in">
  <xsl:apply-templates select="msglevel|msgorig|msgaud" mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="simplemsgentry" mode="topicId">
  <xsl:call-template name="makeTopicId"/>
</xsl:template>

<xsl:template match="simplemsgentry" mode="topic.body.content.in">
  <xsl:apply-templates select="msgtext|msgexplan" mode="topic.section.out"/>
</xsl:template>


<!-- AMPHIBIANS (TOPICS ONLY UNDER SOME CIRCUMSTANCES) - - - - - - - - - -->
<xsl:template match="task[following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning]"
        mode="isTopic">
  <xsl:text>no</xsl:text>
</xsl:template>

<xsl:template match="task[not(following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning)]"
        mode="isTopic">
  <xsl:text>yes</xsl:text>
</xsl:template>

<xsl:template match="task[following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning]" mode="topic.topic.out"/>

<xsl:template match="task[not(following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning)]" mode="topic.topic.out">
  <xsl:apply-templates select="." mode="task.task.out"/>
</xsl:template>

<xsl:template match="task" mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="title|blockinfo/title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="task" mode="topicId">
  <xsl:call-template name="makeTopicId">
    <xsl:with-param name="titleElement" select="title|blockinfo/title"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="task" mode="task.taskbody.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="taskprerequisites" mode="task.prereq.out"/>
  <xsl:apply-templates select="tasksummary"       mode="task.context.out"/>
  <xsl:apply-templates select="procedure"         mode="task.steps.out"/>
  <xsl:apply-templates select="example"           mode="topic.example.out"/>
  <xsl:apply-templates select="taskrelated"       mode="task.postreq.out"/>
</xsl:template>

<xsl:template match="qandaset[following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning]"
        mode="isTopic">
  <xsl:text>no</xsl:text>
</xsl:template>

<xsl:template match="qandaset[not(following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning)]"
        mode="isTopic">
  <xsl:text>yes</xsl:text>
</xsl:template>

<xsl:template match="qandadiv" mode="isTopic">
  <xsl:apply-templates select=".." mode="isTopic"/>
</xsl:template>

<xsl:template match="qandaentry[parent::answer]" mode="isTopic">
  <xsl:text>no</xsl:text>
</xsl:template>

<xsl:template match="qandaentry[not(parent::answer)]" mode="isTopic">
  <xsl:apply-templates select=".." mode="isTopic"/>
</xsl:template>

<xsl:template match="qandaset[following-sibling::ackno or
            following-sibling::address or
            following-sibling::annotation or following-sibling::bibliolist or 
            following-sibling::blockquote or following-sibling::bridgehead or 
            following-sibling::calloutlist or following-sibling::caution or 
            following-sibling::classsynopsis or 
            following-sibling::cmdsynopsis or 
            following-sibling::constraintdef or 
            following-sibling::constructorsynopsis or 
            following-sibling::destructorsynopsis or 
            following-sibling::epigraph or following-sibling::equation or 
            following-sibling::example or following-sibling::fieldsynopsis or 
            following-sibling::figure or following-sibling::formalpara or 
            following-sibling::funcsynopsis or following-sibling::glosslist or 
            following-sibling::important or 
            following-sibling::informalequation or 
            following-sibling::informalexample or 
            following-sibling::informalfigure or 
            following-sibling::informaltable or 
            following-sibling::itemizedlist or 
            following-sibling::literallayout or 
            following-sibling::mediaobject or 
            following-sibling::methodsynopsis or 
            following-sibling::note or 
            following-sibling::orderedlist or following-sibling::para or 
            following-sibling::procedure or 
            following-sibling::productionset or 
            following-sibling::programlisting or 
            following-sibling::programlistingco or 
            following-sibling::remark or 
            following-sibling::revhistory or following-sibling::screen or 
            following-sibling::screenco or following-sibling::screenshot or 
            following-sibling::segmentedlist or following-sibling::sidebar or 
            following-sibling::simpara or following-sibling::simplelist or 
            following-sibling::synopsis or following-sibling::table or 
            following-sibling::tip or following-sibling::variablelist or 
            following-sibling::warning]" mode="topic.topic.out"/>

<xsl:template match="qandaset" mode="topic.body.id.atts.in"/>

<xsl:template match="qandaset[not(title) and
          not(*[position()=1 and contains(local-name(),'info')]/title)]"
      mode="topic.title.in">
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template match="qandaset" mode="topic.title.id.atts.in"/>

<xsl:template match="qandaset" mode="title.cnt.text.in">
  <xsl:if test="@defaultlabel and @defaultlabel!='none'">
    <xsl:value-of select="@defaultlabel"/>
  </xsl:if>
</xsl:template>

<xsl:template match="*[qandadiv or qandaentry]" mode="topic.topic.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="qandadiv|qandaentry" mode="topic.topic.out"/>
</xsl:template>

<xsl:template match="qandadiv[not(title) and
          not(*[position()=1 and contains(local-name(),'info')]/title)]"
      mode="topic.title.in">
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template match="qandaentry[not(*[position()=1 and 
          contains(local-name(),'info')]/title)]"
      mode="topic.title.in">
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template match="qandadiv|qandaentry" mode="topic.title.id.atts.in"/>

<xsl:template match="qandadiv|qandaentry" mode="topic.body.id.atts.in"/>

<xsl:template match="qandadiv|qandaentry" mode="title.cnt.text.in"/>

<xsl:template match="answer|question" mode="topic.body.child">
  <xsl:apply-templates select="." mode="topic.section.out"/>
</xsl:template>

<xsl:template match="*[answer or question]" mode="topic.body.content.in">
  <xsl:apply-templates select="answer|question" mode="topic.section.out"/>
</xsl:template>

<xsl:template match="label" mode="section.cnt.child">
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template match="qandaentry" mode="section.cnt.child"/>

<xsl:template match="refsynopsisdiv[refsect2 and not(refsect2/refsect3)]|
          refsect1[refsect2 and not(refsect2/refsect3)]|
          refsect2[refsect3]|
          refsection[refsection and not(refsection/refsection)]"
      mode="topic.topic.out">
  <xsl:apply-templates select="." mode="reference.reference.out"/>
</xsl:template>

<xsl:template match="refsynopsisdiv|refsect1|refsect2|refsection"
      mode="reference.refbody.in">
  <xsl:apply-templates select="." mode="reference.refbody.out"/>
</xsl:template>

<xsl:template match="refsynopsisdiv[refsect2 and not(refsect2/refsect3)]|
          refsect1[refsect2 and not(refsect2/refsect3)]"
      mode="reference.refbody.content.in">
  <xsl:apply-templates select="refsect2" mode="reference.refbody.child"/>
</xsl:template>

<xsl:template match="refsect2[refsect3]" mode="reference.refbody.content.in">
  <xsl:apply-templates select="refsect3" mode="reference.refbody.child"/>
</xsl:template>

<xsl:template match="refsection[refsection and not(refsection/refsection)]"
      mode="reference.refbody.content.in">
  <xsl:apply-templates select="refsection" mode="reference.refbody.child"/>
</xsl:template>

<xsl:template match="refsynopsisdiv|refsect1|refsect2|refsection"
    mode="reference.reference.topic.topic.in"/>

<xsl:template match="refsynopsisdiv|refsect1"
      mode="topic.topic.topic.topic.in">
  <xsl:apply-templates select="refsect2" mode="topic.topic.out"/>
</xsl:template>

<xsl:template match="refsect2" mode="topic.topic.topic.topic.in"/>

<xsl:template match="refsection" mode="topic.topic.topic.topic.in">
  <xsl:apply-templates select="refsection" mode="topic.topic.out"/>
</xsl:template>

<xsl:template match="refsynopsisdiv" mode="reference.refbody.child">
  <xsl:apply-templates select="." mode="reference.refsyn.out"/>
</xsl:template>

<xsl:template match="refsect1|refsect2|refsect3|refsection"
      mode="reference.refbody.child">
  <xsl:apply-templates select="." mode="topic.section.out"/>
</xsl:template>

<xsl:template match="glosslist" mode="child">
  <xsl:apply-templates select="*[local-name()!='glossentry']" mode="child"/>
  <xsl:apply-templates select="." mode="topic.dl.out"/>
</xsl:template>

<xsl:template match="glosslist" mode="topic.dlhead.in"/>

<xsl:template match="glosslist" mode="topic.dlentry.in">
  <xsl:apply-templates select="glossentry" mode="topic.dlentry.out"/>
</xsl:template>

<xsl:template match="glossentry[parent::glosslist]" mode="isTopic">
  <xsl:text>no</xsl:text>
</xsl:template>

<xsl:template match="glossentry[not(parent::glosslist)]" mode="isTopic">
  <xsl:text>yes</xsl:text>
</xsl:template>

<xsl:template match="glossentry" mode="topic.dt.in">
  <xsl:apply-templates select="glossterm" mode="topic.dt.out"/>
</xsl:template>

<xsl:template match="glossentry[glossdef]" mode="topic.dd.in">
  <xsl:apply-templates select="glossdef" mode="topic.dd.out"/>
</xsl:template>

<xsl:template match="glossentry[glosssee]" mode="topic.dd.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.dd.out"/>
</xsl:template>

<xsl:template match="glossentry" mode="topic.dd.atts.in"/>

<xsl:template match="glossentry[glosssee]" mode="defn.cnt.text.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="glosssee" mode="child">
    <xsl:with-param name="container"  select="$container"/>
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="glosssee[@otherterm]" mode="child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.xref.out"/>
</xsl:template>

<xsl:template match="glosssee[not(@otherterm)]" mode="child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:comment>TO DO: glossseealso WITHOUT @otherterm</xsl:comment>
</xsl:template>

<xsl:template match="glossentry" mode="topic.title.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="glossterm" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="glossentry[acronym or abbrev]" mode="topic.titlealts.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:comment>TO DO</xsl:comment>
</xsl:template>

<xsl:template match="glossentry[indexterm or revhistory]"
      mode="topic.prolog.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.prolog.out"/>
</xsl:template>

<xsl:template match="glossentry" mode="topic.metadata.in">
  <xsl:apply-templates select="." mode="topic.metadata.out"/>
</xsl:template>

<xsl:template match="glossentry[glossdef]" mode="topic.body.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.body.out"/>
</xsl:template>

<xsl:template match="glossentry[glossdef]" mode="topic.body.content.in">
  <xsl:apply-templates select="glossdef" mode="topic.section.out"/>
</xsl:template>

<!--TO DO: glossseealso AND glosssee WITHOUT @otherterm -->

<xsl:template match="glossseealso[@otherterm]" mode="child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.xref.out"/>
</xsl:template>

<xsl:template match="glossseealso[@otherterm]" mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:call-template name="makeRef"/>
</xsl:template>

<xsl:template match="glossseealso[not(@otherterm)]" mode="child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:comment>TO DO: glossseealso WITHOUT @otherterm</xsl:comment>
</xsl:template>

<xsl:template match="glossentry[glosssee]" mode="topic.related-links.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.related-links.out"/>
</xsl:template>

<xsl:template match="glosssee[@otherterm]" mode="topic.related-links.child">
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="." mode="topic.link.out"/>
</xsl:template>

<xsl:template match="glosssee[@otherterm]" mode="href.att.in">
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:call-template name="makeRef"/>
</xsl:template>

<xsl:template match="glosssee[not(@otherterm)]"
      mode="topic.related-links.child">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:comment>TO DO: glosssee WITHOUT @otherterm</xsl:comment>
</xsl:template>

<xsl:template match="glosssee" mode="topic.linktext.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates mode="child"/>
</xsl:template>


<!-- LISTS - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="itemizedlist|stepalternatives" mode="child">
  <xsl:apply-templates
      select="*[local-name()!='listitem' and local-name()!='step']"
      mode="child"/>
  <xsl:apply-templates select="." mode="topic.ul.out"/>
</xsl:template>

<xsl:template match="*[itemizedlist or stepalternatives]" mode="topic.ul.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="itemizedlist|stepalternatives"
        mode="topic.ul.out"/>
</xsl:template>

<xsl:template match="*[@spacing='compact']" mode="compact.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:choose>
    <xsl:when test="@spacing='compact'">
      <xsl:text>yes</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>no</xsl:text>
    </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:apply-templates select="." mode="compact.att.out">
    <xsl:with-param name="value" select="$value"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="orderedlist|procedure|substeps" mode="child">
  <xsl:apply-templates
      select="*[local-name()!='listitem' and local-name()!='step']"
      mode="child"/>
  <xsl:apply-templates select="." mode="topic.ol.out"/>
</xsl:template>

<xsl:template match="*[orderedlist or procedure or substeps]"
      mode="topic.ol.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="orderedlist|procedure|substeps"
      mode="topic.ol.out"/>
</xsl:template>

<xsl:template match="*[listitem or step]" mode="topic.li.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="listitem|step" mode="topic.li.out"/>
</xsl:template>

<xsl:template match="variablelist" mode="child">
  <xsl:apply-templates select="*[local-name()!='varlistentry']" mode="child"/>
  <xsl:apply-templates select="." mode="topic.dl.out"/>
</xsl:template>

<xsl:template match="*[variablelist or glosslist]" mode="topic.dl.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="variablelist|glosslist" mode="topic.dl.out"/>
</xsl:template>

<xsl:template match="variablelist" mode="topic.dlhead.in"/>

<xsl:template match="variablelist" mode="topic.dlentry.in">
  <xsl:apply-templates select="varlistentry" mode="topic.dlentry.out"/>
</xsl:template>

<xsl:template match="varlistentry" mode="topic.dt.in">
  <xsl:apply-templates select="term" mode="topic.dt.out"/>
</xsl:template>

<xsl:template match="varlistentry" mode="topic.dd.in">
  <xsl:apply-templates select="listitem" mode="topic.dd.out"/>
</xsl:template>

<xsl:template match="segmentedlist" mode="child">
  <xsl:apply-templates select="title" mode="child"/>
  <xsl:apply-templates select="." mode="topic.simpletable.out"/>
</xsl:template>

<!-- TO DO: ANY OTHER INPUT ELEMENTS THAT MAP TO simpletable ??? -->
<xsl:template match="*[segmentedlist]" mode="topic.simpletable.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="segmentedlist" mode="topic.simpletable.out"/>
</xsl:template>

<xsl:template match="*[segtitle]" mode="topic.sthead.in">
  <xsl:apply-templates select="." mode="topic.sthead.out"/>
</xsl:template>

<xsl:template match="*[seglistitem]" mode="topic.strow.in">
  <xsl:apply-templates select="seglistitem" mode="topic.strow.out"/>
</xsl:template>

<xsl:template match="*[seg or segtitle]" mode="topic.stentry.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="seg|segtitle" mode="topic.stentry.out"/>
</xsl:template>

<xsl:template match="simplelist" mode="child">
  <xsl:apply-templates select="." mode="topic.sl.out"/>
</xsl:template>

<xsl:template match="*[simplelist]" mode="topic.sl.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="simplelist" mode="topic.sl.out"/>
</xsl:template>

<xsl:template match="*[member]" mode="topic.sli.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="member" mode="topic.sli.out"/>
</xsl:template>

<xsl:template match="procedure" mode="task.step.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="step" mode="task.step.out"/>
</xsl:template>

<xsl:template match="step[not(title)]" mode="task.step.content.in">
  <xsl:apply-templates select=".|substeps|stepalternatives"
        mode="task.step.child"/>
</xsl:template>

<xsl:template match="step[title]" mode="task.step.content.in">
  <xsl:apply-templates select="title|substeps|stepalternatives"
        mode="task.step.child"/>
</xsl:template>

<xsl:template match="step" mode="task.step.child">
  <xsl:variable name="firstInfo" select="*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        not(preceding-sibling::substeps) and
        not(preceding-sibling::stepalternatives)]"/>
  <xsl:apply-templates select="." mode="task.cmd.out"/>
  <xsl:if test="$firstInfo">
    <xsl:apply-templates select="." mode="task.info.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="step" mode="task.cmd.content.in"/>

<xsl:template match="step" mode="itemgroup.cnt.text.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:variable name="firstInfo" select="*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        not(preceding-sibling::substeps) and
        not(preceding-sibling::stepalternatives)]"/>
  <xsl:apply-templates select="$firstInfo" mode="itemgroup.cnt.child">
    <xsl:with-param name="container"  select="$container"/>
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="title" mode="task.step.child">
  <xsl:variable name="firstInfo" select="following-sibling::*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        not(preceding-sibling::substeps) and
        not(preceding-sibling::stepalternatives)]"/>
  <xsl:apply-templates select="." mode="task.cmd.out"/>
  <xsl:if test="$firstInfo">
    <xsl:apply-templates select="." mode="task.info.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="title" mode="itemgroup.cnt.text.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:variable name="firstInfo" select="following-sibling::*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        not(preceding-sibling::substeps) and
        not(preceding-sibling::stepalternatives)]"/>
  <xsl:apply-templates select="$firstInfo" mode="itemgroup.cnt.child">
    <xsl:with-param name="container"  select="$container"/>
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="substeps" mode="task.step.child">
  <xsl:variable name="nextInfo" select="following-sibling::*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        generate-id(preceding-sibling::*[local-name()='substeps' or
            local-name()='stepalternatives'][1])=generate-id(current())]"/>
  <xsl:apply-templates select="." mode="task.substeps.out"/>
  <xsl:if test="$nextInfo">
    <xsl:apply-templates select="." mode="task.info.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="substeps" mode="task.substep.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="step" mode="task.substep.out"/>
</xsl:template>

<xsl:template match="step" mode="task.substep.content.in">
  <!-- TO DO: REALLY HAS A DIFFERENT CONTENT MODEL SO NEEDS DIFFERENT HANDLING -->
  <xsl:apply-templates select="." mode="task.step.content.in"/>
</xsl:template>

<xsl:template match="stepalternatives" mode="task.step.child">
  <xsl:variable name="nextInfo" select="following-sibling::*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        generate-id(preceding-sibling::*[local-name()='substeps' or
            local-name()='stepalternatives'][1])=generate-id(current())]"/>
  <xsl:apply-templates select="." mode="task.choices.out"/>
  <xsl:if test="$nextInfo">
    <xsl:apply-templates select="." mode="task.info.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="stepalternatives" mode="task.choice.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="step" mode="task.choice.out"/>
</xsl:template>

<xsl:template match="substeps|stepalternatives" mode="itemgroup.cnt.text.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:variable name="nextInfo" select="following-sibling::*[
        local-name()!='substeps' and
        local-name()!='stepalternatives' and
        generate-id(preceding-sibling::*[local-name()='substeps' or
            local-name()='stepalternatives'][1])=generate-id(current())]"/>
  <xsl:apply-templates select="$nextInfo" mode="itemgroup.cnt.child">
    <xsl:with-param name="container"  select="$container"/>
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<!-- TO DO -->
<xsl:template match="productionset" mode="child"/>


<!-- BLOCKS  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="ackno|formalpara|para|simpara" mode="child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="*[ackno or formalpara or para or simpara]"
      mode="topic.p.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="ackno|formalpara|para|simpara"
      mode="topic.p.out"/>
</xsl:template>

<xsl:template match="formalpara" mode="para.cnt.text.in">
  <xsl:apply-templates select="title|indexterm" mode="para.cnt.child"/>
  <xsl:apply-templates select="para" mode="para.cnt.text.in"/>
</xsl:template>

<xsl:template match="blockquote|epigraph" mode="child">
  <xsl:apply-templates select="." mode="topic.lq.out"/>
</xsl:template>

<xsl:template match="*[blockquote or epigraph]" mode="topic.lq.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="blockquote|epigraph" mode="topic.lq.out"/>
</xsl:template>

<xsl:template match="attribution" mode="longquote.cnt.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>


<!-- ADMONITIONS - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="caution|important|note|tip|warning" mode="child">
  <xsl:apply-templates select="." mode="topic.note.out"/>
</xsl:template>

<xsl:template match="caution|important|tip" mode="type.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="type.att.out">
    <xsl:with-param name="value" select="local-name()"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="warning" mode="type.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="type.att.out">
    <xsl:with-param name="value" select="'attention'"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[caution or important or note or tip or warning]"
        mode="topic.note.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="caution|important|note|tip|warning"
        mode="topic.note.out"/>
</xsl:template>


<!-- VERBATIM BLOCKS - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="address|literallayout[@class='monospaced']|
          programlisting|screen|synopsis"
      mode="child">
  <xsl:apply-templates select="." mode="topic.pre.out"/>
</xsl:template>

<xsl:template match="address|literallayout[@class='monospaced']|
          programlisting|screen|synopsis"
      mode="pre.cnt.child">
  <xsl:apply-templates mode="pre.cnt.child"/>
</xsl:template>

<xsl:template match="inlinemediaobject[
              imageobject/imagedata[@fileref and @format='linespecific']]|
          mediaobject[
              imageobject/imagedata[@fileref and @format='linespecific']]"
      mode="pre.cnt.child">
  <xsl:value-of select="imageobject/imagedata/@fileref"/>
</xsl:template>

<xsl:template match="address/*" mode="child">
  <xsl:apply-templates select="*|text()" mode="child"/>
</xsl:template>

<xsl:template match="*[address or literallayout[@class='monospaced'] or 
            programlisting or programlistingco or screen or screenco or
            synopsis]"
        mode="topic.pre.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="address|literallayout[@class='monospaced']|
            programlisting|programlistingco/programlisting|
            screen|screenco/screen|synopsis"
        mode="topic.pre.out"/>
</xsl:template>

<xsl:template match="literallayout[not(@class) or @class!='monospaced']"
      mode="child">
  <xsl:apply-templates select="." mode="topic.lines.out"/>
</xsl:template>

<xsl:template match="*[literallayout[not(@class) or @class!='monospaced']]"
        mode="topic.lines.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates
        select="literallayout[not(@class) or @class!='monospaced']"
        mode="topic.lines.out"/>
</xsl:template>


<!-- FORMATTING PHRASES  - - - - - - - - - - - - - - - - - - - - - - - - -->
<!-- TO DO: REVIEW FOR ACTUAL emphasis role VALUES -->
<!-- TO DO: UNKNOWN emphasis SHOULD BE <ph> SO PROBABLY BEST AS A CASE -->
<xsl:template
      match="emphasis[not(@role) or @role='em' or @role='italic' or @role='i']"
      mode="child">
  <xsl:apply-templates select="." mode="hi-d.i.out"/>
</xsl:template>

<xsl:template match="*[emphasis[not(@role) or @role='em' or 
            @role='italic' or @role='i']]"
        mode="hi-d.i.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="emphasis[not(@role) or @role='em' or 
            @role='italic' or @role='i']"
        mode="hi-d.i.out"/>
</xsl:template>

<xsl:template
      match="emphasis[@role='strong' or @role='bold' or @role='b']"
      mode="child">
  <xsl:apply-templates select="." mode="hi-d.b.out"/>
</xsl:template>

<xsl:template match="*[emphasis[@role='strong' or @role='bold' or @role='b']]"
        mode="hi-d.b.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates
        select="emphasis[@role='strong' or @role='bold' or @role='b']"
        mode="hi-d.b.out"/>
</xsl:template>

<xsl:template
      match="emphasis[@role='underline' or @role='underscore' or @role='u']"
      mode="child">
  <xsl:apply-templates select="." mode="hi-d.u.out"/>
</xsl:template>

<xsl:template
      match="*[emphasis[@role='underline' or @role='underscore' or @role='u']]"
      mode="hi-d.u.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates
        select="emphasis[@role='underline' or @role='underscore' or @role='u']"
        mode="hi-d.u.out"/>
</xsl:template>

<xsl:template match="emphasis[@role='strikethrough']" mode="child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="emphasis[@role='strikethrough']" mode="words.cnt.child">
  <xsl:apply-templates mode="words.cnt.child"/>
</xsl:template>

<xsl:template match="emphasis[@role='monospace' or @role='mono' or 
          @role='teletype' or @role='tt']"
      mode="child">
  <xsl:apply-templates select="." mode="hi-d.tt.out"/>
</xsl:template>

<xsl:template match="*[emphasis[@role='monospace' or @role='mono' or
            @role='teletype' or @role='tt']]"
        mode="hi-d.tt.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="emphasis[@role='monospace' or @role='mono' or
            @role='teletype' or @role='tt']"
        mode="hi-d.tt.out"/>
</xsl:template>

<xsl:template match="subscript" mode="child">
  <xsl:apply-templates select="." mode="hi-d.sub.out"/>
</xsl:template>

<xsl:template match="*[subscript]" mode="hi-d.sub.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="subscript" mode="hi-d.sub.out"/>
</xsl:template>

<xsl:template match="superscript" mode="child">
  <xsl:apply-templates select="." mode="hi-d.sup.out"/>
</xsl:template>

<xsl:template match="*[superscript]" mode="hi-d.sup.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="superscript" mode="hi-d.sup.out"/>
</xsl:template>

<xsl:template match="abbrev|acronym|action|authorinitials|citebiblioid|
          corpcredit|email|errortype|foreignphrase|hardware|keycap|keycode|
          keycombo|keysym|markup|medialabel|mousebutton|optional|orgname|
          othercredit|personname|phrase|productname|productnumber|termdef|uri"
      mode="child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="abbrev|acronym|action|authorinitials|citebiblioid|
          corpcredit|email|errortype|foreignphrase|hardware|keycap|keycode|
          keycombo|keysym|markup|medialabel|mousebutton|optional|orgname|
          othercredit|personname|phrase|productname|productnumber|termdef|uri"
      mode="words.cnt.child">
  <xsl:apply-templates mode="words.cnt.child"/>
</xsl:template>

<xsl:template match="*[author or abbrev or acronym or action or 
          authorinitials or citebiblioid or corpauthor or corpcredit or 
          email or errortype or foreignphrase or hardware or keycap or 
          keycode or keycombo or keysym or markup or medialabel or
          mousebutton or optional or orgname or othercredit or
          personname or phrase or productname or productnumber or
          termdef or uri]"
      mode="topic.ph.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="author|abbrev|acronym|action|authorinitials|
          citebiblioid|corpauthor|corpcredit|email|errortype|foreignphrase|
          hardware|keycap|keycode|keycombo|keysym|markup|medialabel|
          mousebutton|optional|orgname|othercredit|personname|phrase|
          productname|productnumber|termdef|uri"
      mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="anchor" mode="child">
  <xsl:apply-templates select="." mode="topic.state.out"/>
</xsl:template>

<xsl:template match="anchor" mode="topic.state.name.att.in">
  <xsl:param name="isRequired" select="'yes'"/>
  <xsl:apply-templates select="." mode="name.att.out">
    <xsl:with-param name="value" select="'anchor'"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="anchor" mode="topic.state.value.att.in">
  <xsl:param name="isRequired" select="'yes'"/>
  <xsl:apply-templates select="." mode="value.att.out">
    <xsl:with-param name="value" select="@id"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[anchor]" mode="topic.state.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="anchor" mode="topic.state.out"/>
</xsl:template>



<!-- SEMANTIC PHRASES  - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="code|initializer|literal|returnvalue" mode="child">
  <xsl:apply-templates select="." mode="pr-d.codeph.out"/>
</xsl:template>

<xsl:template match="*[code or initializer or literal or returnvalue]"
        mode="pr-d.codeph.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="code|initializer|literal|returnvalue"
        mode="pr-d.codeph.out"/>
</xsl:template>

<xsl:template match="quote" mode="child">
  <xsl:apply-templates select="." mode="topic.q.out"/>
</xsl:template>

<xsl:template match="*[quote]" mode="topic.q.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="quote" mode="topic.q.out"/>
</xsl:template>

<xsl:template match="trademark" mode="child">
  <xsl:apply-templates select="." mode="topic.tm.out"/>
</xsl:template>

<xsl:template match="*[trademark]" mode="topic.tm.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="trademark" mode="topic.tm.out"/>
</xsl:template>

<xsl:template match="*" mode="tmtype.att.in">
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:param name="container"/>
  <xsl:variable name="value">
    <xsl:choose>
    <xsl:when test="not(@class) or @class='trade'">
      <xsl:text>tm</xsl:text>
    </xsl:when>
    <xsl:when test="@class='registered'">
      <xsl:text>reg</xsl:text>
    </xsl:when>
    <xsl:when test="@class='service'">
      <xsl:text>service</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>tm</xsl:text>
    </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:apply-templates select="." mode="tmtype.att.out">
    <xsl:with-param name="value" select="$value"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*" mode="topic.tm.content.in">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>


<!-- UI PHRASES  - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="computeroutput|prompt" mode="child">
  <xsl:apply-templates select="." mode="sw-d.systemoutput.out"/>
</xsl:template>

<xsl:template match="*[computeroutput or prompt]" mode="sw-d.systemoutput.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="computeroutput|prompt" 
        mode="sw-d.systemoutput.out"/>
</xsl:template>

<xsl:template match="userinput" mode="child">
  <xsl:apply-templates select="." mode="sw-d.userinput.out"/>
</xsl:template>

<xsl:template match="*[userinput]" mode="sw-d.userinput.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="userinput" mode="sw-d.userinput.out"/>
</xsl:template>

<xsl:template match="menuchoice" mode="child">
  <xsl:apply-templates select="." mode="ui-d.menucascade.out"/>
</xsl:template>

<xsl:template match="*[menuchoice]" mode="ui-d.menucascade.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="menuchoice" mode="ui-d.menucascade.out"/>
</xsl:template>

<xsl:template match="menuchoice" mode="ui-d.menucascade.content.in">
  <xsl:apply-templates select="*[position()!=last()]"
        mode="ui-d.uicontrol.out"/>
  <xsl:apply-templates select="." mode="ui-d.uicontrol.out"/>
</xsl:template>

<xsl:template match="menuchoice" mode="ui-d.uicontrol.content.in">
  <xsl:apply-templates select="shortcut" mode="ui-d.shortcut.out"/>
  <xsl:apply-templates select="*[position()=last()]"
        mode="ui-d.uicontrol.child"/>
</xsl:template>

<xsl:template
      match="guibutton|guilabel|guimenu|guimenuitem|guisubmenu|interface"
      mode="child">
  <xsl:apply-templates select="." mode="ui-d.uicontrol.out"/>
</xsl:template>

<xsl:template
      match="guibutton|guilabel|guimenu|guimenuitem|guisubmenu|interface"
      mode="ui-d.uicontrol.child">
  <xsl:apply-templates mode="ui-d.uicontrol.child"/>
</xsl:template>

<xsl:template match="*[guibutton or guilabel or guimenu or guimenuitem or 
            guisubmenu or interface]"
        mode="ui-d.uicontrol.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates
      select="guibutton|guilabel|guimenu|guimenuitem|guisubmenu|interface"
      mode="ui-d.uicontrol.out"/>
</xsl:template>


<!-- PROGRAMMING LANGUAGE PHRASES  - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="errorcode|errorname" mode="child">
  <xsl:apply-templates select="." mode="sw-d.msgnum.out"/>
</xsl:template>

<xsl:template match="*[errorcode or errorname]" mode="sw-d.msgnum.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="errorcode|errorname" mode="sw-d.msgnum.out"/>
</xsl:template>

<xsl:template match="errortext" mode="child">
  <xsl:apply-templates select="." mode="sw-d.msgph.out"/>
</xsl:template>

<xsl:template match="*[errortext]" mode="sw-d.msgph.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="errortext" mode="sw-d.msgph.out"/>
</xsl:template>

<xsl:template match="replaceable" mode="pr-d.synph.child">
  <xsl:apply-templates select="." mode="pr-d.var.out"/>
</xsl:template>

<xsl:template match="replaceable" mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.var.out"/>
</xsl:template>

<xsl:template match="replaceable" mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.var.out"/>
</xsl:template>

<xsl:template match="replaceable" mode="pr-d.groupcomp.child">
  <xsl:apply-templates select="." mode="pr-d.var.out"/>
</xsl:template>

<xsl:template match="*[replaceable]" mode="pr-d.var.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="replaceable" mode="pr-d.var.out"/>
</xsl:template>

<xsl:template match="classname|constant|exceptionname|funcparams|function|
          interfacename|methodname|modifier|ooclass|ooexception|
          oointerface|structfield|structname|symbol|type"
      mode="child">
  <xsl:apply-templates select="." mode="pr-d.apiname.out"/>
</xsl:template>

<xsl:template match="*" mode="pr-d.apiname.child">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<xsl:template match="*[classname or constant or exceptionname or
          funcparams or function or interfacename or methodname or
          modifier or ooclass or ooexception or oointerface or
          structfield or structname or symbol or type]"
      mode="pr-d.apiname.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="classname|constant|exceptionname|funcparams|
            function|interfacename|methodname|modifier|ooclass|ooexception|
            oointerface|structfield|structname|
            symbol|type"
        mode="pr-d.apiname.out"/>
</xsl:template>

<xsl:template match="application|command" mode="child">
  <xsl:choose>
    <xsl:when test="$replace-cmdname-with-userinput">
      <xsl:apply-templates select="." mode="sw-d.userinput.out"/>
    </xsl:when>
    <xsl:otherwise>
  <xsl:apply-templates select="." mode="sw-d.cmdname.out"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="*[application or command]" mode="sw-d.cmdname.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="application|command" mode="sw-d.cmdname.out"/>
</xsl:template>

<xsl:template match="database|envar|property|replaceable|varname" mode="child">
  <xsl:apply-templates select="." mode="sw-d.varname.out"/>
</xsl:template>

<xsl:template
      match="*[database or envar or property or replaceable or varname]"
      mode="sw-d.varname.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="database|envar|property|replaceable|varname"
        mode="sw-d.varname.out"/>
</xsl:template>

<!-- token -->

<xsl:template match="sgmltag" mode="child">
  <xsl:apply-templates select="." mode="topic.keyword.out"/>
</xsl:template>


<!-- OPERATING SYSTEM PHRASES  - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="filename" mode="child">
  <xsl:apply-templates select="." mode="sw-d.filepath.out"/>
</xsl:template>

<xsl:template match="*[filename]" mode="sw-d.filepath.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="filename" mode="sw-d.filepath.out"/>
</xsl:template>


<!-- LINKING PHRASES - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="citation|citetitle|citerefentry" mode="child">
  <xsl:apply-templates select="." mode="topic.cite.out"/>
</xsl:template>

<xsl:template match="*[citation or citetitle or citerefentry]"
      mode="topic.cite.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="citation|citetitle|citerefentry"
        mode="topic.cite.out"/>
</xsl:template>

<xsl:template match="refentrytitle" mode="xrefph.cnt.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="manvolnum" mode="xrefph.cnt.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="firstterm|glossterm[not(parent::glossentry)]|wordasword"
      mode="child">
  <xsl:apply-templates select="." mode="topic.term.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.term.child">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<xsl:template match="*[firstterm or glossterm[not(parent::glossentry)]]"
        mode="topic.term.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="firstterm|glossterm[not(parent::glossentry)]"
        mode="topic.term.out"/>
</xsl:template>

<xsl:template match="title/footnote" mode="child"/>

<xsl:template match="footnote" mode="child">
  <xsl:apply-templates select="." mode="topic.fn.out"/>
</xsl:template>

<xsl:template match="*[footnote]" mode="topic.fn.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="footnote" mode="topic.fn.out"/>
</xsl:template>

<xsl:template match="indexterm[@zone]" mode="child"/>

<!-- TO DO:  BETTER TO EMIT WITHIN PROLOG KEYWORDS -->
<xsl:template match="indexterm" mode="title.cnt.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="topic.body.child"/>

<!-- TO DO:  BETTER TO EMIT OUTSIDE THE HIGHLIGHTING -->
<xsl:template match="indexterm" mode="hi-d.b.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="hi-d.i.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="hi-d.sub.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="hi-d.sup.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="hi-d.tt.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="hi-d.u.child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="indexterm" mode="topic.ph.atts.in"/>

<xsl:template match="indexterm" mode="ph.cnt.text.in">
  <xsl:apply-templates select="." mode="child"/>
</xsl:template>

<!-- instead, emit within prolog keywords -->
<xsl:template match="
            appendixinfo/indexterm[not(@zone)]|
            articleinfo/indexterm[not(@zone)]|
            bibliographyinfo/indexterm[not(@zone)]|
            blockinfo/indexterm[not(@zone)]|
            bookinfo/indexterm[not(@zone)]|
            chapterinfo/indexterm[not(@zone)]|
            glossaryinfo/indexterm[not(@zone)]|
            partinfo/indexterm[not(@zone)]|
            prefaceinfo/indexterm[not(@zone)]|
            referenceinfo/indexterm[not(@zone)]|
            refentryinfo/indexterm[not(@zone)]|
            refsect1info/indexterm[not(@zone)]|
            refsect2info/indexterm[not(@zone)]|
            refsect3info/indexterm[not(@zone)]|
            refsectioninfo/indexterm[not(@zone)]|
            refsynopsisdivinfo/indexterm[not(@zone)]|
            sect1info/indexterm[not(@zone)]|
            sect2info/indexterm[not(@zone)]|
            sect3info/indexterm[not(@zone)]|
            sect4info/indexterm[not(@zone)]|
            sect5info/indexterm[not(@zone)]|
            sectioninfo/indexterm[not(@zone)]|
            setinfo/indexterm[not(@zone)]"
      mode="child"/>

<xsl:template match="indexterm[not(@zone)]" mode="child">
  <xsl:apply-templates select="primary" mode="topic.indexterm.out"/>
</xsl:template>

<xsl:template match="primary" mode="topic.indexterm.content.in">
  <xsl:apply-templates mode="topic.indexterm.child"/>
  <xsl:apply-templates select="following-sibling::secondary"
        mode="topic.indexterm.out"/>
</xsl:template>

<xsl:template match="secondary" mode="topic.indexterm.content.in">
  <xsl:apply-templates mode="topic.indexterm.child"/>
  <xsl:apply-templates select="following-sibling::tertiary"
        mode="topic.indexterm.out"/>
</xsl:template>

<xsl:template match="*[indexterm[not(@zone)]]" mode="topic.indexterm.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="indexterm[not(@zone)]"
        mode="topic.indexterm.out"/>
</xsl:template>

<xsl:template match="emphasis|phrase" mode="topic.indexterm.child">
  <xsl:apply-templates mode="topic.indexterm.child"/>
</xsl:template>

<xsl:template match="indexterm|footnote" mode="textOnly"/>

<xsl:template match="biblioref|link|ulink|xref" mode="title.cnt.child">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<xsl:template match="biblioref|link|ulink|xref" mode="child">
  <xsl:apply-templates select="." mode="topic.xref.out"/>
</xsl:template>

<!-- TO DO: SHOULD PROCESS id(xref/@endterm) FOR CONTENTS -->

<xsl:template match="biblioref[@linkend]|link[@linkend]|xref[@linkend]"
      mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:call-template name="makeRef"/>
</xsl:template>

<xsl:template match="link[@endterm and not(* or text())]"
      mode="topic.xref.content.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="id(@endterm)" mode="textOnly"/>
</xsl:template>

<!-- TO DO: MUST HANDLE xref TO ELEMENT WITH NO ID IN DITA:
         author, authorgroup, anchor -->

<xsl:template match="ulink[@url]" mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="href.att.out">
    <xsl:with-param name="value" select="@url"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[biblioref or link or ulink or xref]"
      mode="topic.xref.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="biblioreflink|ulink|xref"
        mode="topic.xref.out"/>
</xsl:template>


<!-- FIGURES - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="example|informalexample" mode="topic.body.child">
  <xsl:apply-templates select="." mode="topic.example.out"/>
</xsl:template>

<xsl:template match="example|informalexample" mode="concept.conbody.child">
  <xsl:apply-templates select="." mode="topic.example.out"/>
</xsl:template>

<xsl:template match="example|informalexample" mode="reference.refbody.child">
  <xsl:apply-templates select="." mode="topic.example.out"/>
</xsl:template>

<xsl:template match="informalexample|informalfigure"
      mode="topic.example.child">
  <xsl:apply-templates mode="topic.example.child"/>
</xsl:template>

<xsl:template match="*[example or informalexample]" mode="topic.example.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="example|informalexample"
        mode="topic.example.out"/>
</xsl:template>

<xsl:template match="equation|example|figure|
          informalequation|informalexample|informalfigure|screenshot|sidebar"
      mode="child">
  <xsl:apply-templates select="." mode="topic.fig.out"/>
</xsl:template>

<xsl:template match="equation/title|example/title|figure/title|sidebar/title"
      mode="child">
  <xsl:apply-templates select="." mode="topic.title.out"/>
</xsl:template>

<xsl:template match="
          equation/blockinfo[not(following-sibling::title) and title]|
          example/blockinfo[not(following-sibling::title) and title]|
          figure/blockinfo[not(following-sibling::title) and title]|
          sidebar/sidebarinfo[not(following-sibling::title) and title]"
      mode="child">
  <xsl:apply-templates select="title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="blockinfo[not(title)]|sidebarinfo[not(title)]"
      mode="child"/>

<xsl:template match="equation|example|figure|
          informalequation|informalexample|informalfigure|screenshot"
      mode="topic.fig.child">
  <xsl:apply-templates mode="topic.fig.child"/>
</xsl:template>

<xsl:template match="*[equation or example or figure or informalequation or 
            informalexample or informalfigure or screenshot or sidebar]"
        mode="topic.fig.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="equation|example|figure|
          informalequation|informalexample|informalfigure|screenshot|sidebar"
        mode="topic.fig.out"/>
</xsl:template>

<xsl:template match="classsynopsis|cmdsynopsis|constructorsynopsis|
          destructorsynopsis|fieldsynopsis|funcsynopsis|methodsynopsis"
      mode="term.cnt.child">
  <xsl:variable name="value">
    <xsl:apply-templates mode="textOnly"/>
  </xsl:variable>
  <xsl:value-of select="normalize-space($value)"/>
</xsl:template>

<xsl:template match="classsynopsis|cmdsynopsis|constructorsynopsis|
          destructorsynopsis|fieldsynopsis|funcsynopsis|methodsynopsis"
      mode="child">
  <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.out"/>
</xsl:template>

<xsl:template match="*[classsynopsis or cmdsynopsis or constructorsynopsis or 
            destructorsynopsis or fieldsynopsis or funcsynopsis or 
            methodsynopsis]" 
        mode="pr-d.syntaxdiagram.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="classsynopsis|cmdsynopsis|constructorsynopsis|
            destructorsynopsis|fieldsynopsis|funcsynopsis|methodsynopsis"
        mode="pr-d.syntaxdiagram.out"/>
</xsl:template>

<xsl:template match="cmdsynopsis" mode="pr-d.syntaxdiagram.content.in">
  <xsl:apply-templates select="command"       mode="pr-d.groupchoice.out"/>
  <xsl:apply-templates select="synopfragment" mode="pr-d.fragment.out"/>
</xsl:template>

<xsl:template match="command" mode="pr-d.groupchoice.content.in">
  <xsl:variable name="grouped" select="following-sibling::node()[
                generate-id(preceding-sibling::command[1])=
                generate-id(current())]"/>
  <xsl:variable name="groupNewlines" select="$grouped[local-name()='sbr']"/>
  <xsl:choose>
  <xsl:when test="$groupNewlines">
    <xsl:apply-templates select=".|$groupNewlines" mode="pr-d.groupcomp.out"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select=".|$grouped" mode="pr-d.groupchoice.child"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="command" mode="pr-d.groupcomp.content.in">
  <xsl:variable name="newlineEnd" select="following-sibling::*[
            local-name()='sbr' or local-name()='command'][1]"/>
  <xsl:variable name="newlineEndID" select="generate-id($newlineEnd)"/>
  <xsl:apply-templates select=".|following-sibling::node()[
            generate-id(following-sibling::*[local-name()='sbr' or
                  local-name()='command'][1])=$newlineEndID]"
        mode="pr-d.groupcomp.child"/>
</xsl:template>

<xsl:template match="sbr" mode="pr-d.groupcomp.content.in">
  <xsl:variable name="newlineEnd" select="following-sibling::*[
            local-name()='sbr' or local-name()='command'][1]"/>
  <xsl:choose>
  <xsl:when test="$newlineEnd">
    <xsl:variable name="newlineEndID" select="generate-id($newlineEnd)"/>
    <xsl:apply-templates select="following-sibling::node()[
              generate-id(following-sibling::*[local-name()='sbr' or
                  local-name()='command'][1])=$newlineEndID]"
          mode="pr-d.groupcomp.child"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="following-sibling::node()"
          mode="pr-d.groupcomp.child"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="command|option" mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
</xsl:template>

<xsl:template match="command|option" mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
</xsl:template>

<xsl:template match="command|option" mode="pr-d.groupcomp.child">
  <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
</xsl:template>

<xsl:template match="arg|group" mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="arg|group" mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="arg|group" mode="pr-d.groupcomp.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="arg|group" mode="pr-d.fragment.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="arg[@choice='req']|group[@choice='req']"
      mode="pr-d.groupchoice.importance.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="importance.att.out">
    <xsl:with-param name="value" select="'required'"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="arg" mode="pr-d.groupchoice.content.in">
  <xsl:apply-templates select="." mode="firstTextKwd"/>
  <xsl:for-each select="*">
    <xsl:apply-templates select="." mode="pr-d.groupchoice.child"/>
    <xsl:apply-templates select="." mode="followingTextSep"/>
  </xsl:for-each>
</xsl:template>

<xsl:template match="arg" mode="firstTextKwd">
  <xsl:variable name="firstText" select="text()[not(preceding-sibling::*)]"/>
  <xsl:if test="$firstText and
            string-length(normalize-space($firstText)) &gt; 0">
    <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="arg" mode="pr-d.kwd.atts.in"/>

<xsl:template match="arg" mode="pr-d.kwd.content.in">
  <xsl:variable name="firstText" select="text()[not(preceding-sibling::*)]"/>
  <xsl:apply-templates select="$firstText" mode="pr-d.kwd.child"/>
</xsl:template>

<xsl:template match="arg/*" mode="followingTextSep">
  <xsl:variable name="nextText" select="following-sibling::text()[
            generate-id(preceding-sibling::*[1])=generate-id(current())]"/>
  <xsl:if test="$nextText and
            string-length(normalize-space($nextText)) &gt; 0">
    <xsl:apply-templates select="." mode="pr-d.sep.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="arg/*" mode="pr-d.sep.atts.in"/>

<xsl:template match="arg/*" mode="pr-d.sep.content.in">
  <xsl:variable name="nextText" select="following-sibling::text()[
            generate-id(preceding-sibling::*[1])=generate-id(current())]"/>
  <xsl:apply-templates select="$nextText" mode="words.cnt.text.in"/>
</xsl:template>

<xsl:template match="synopfragmentref" mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.fragref.out"/>
</xsl:template>

<xsl:template match="synopfragmentref" mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.fragref.out"/>
</xsl:template>

<xsl:template match="synopfragmentref" mode="pr-d.groupcomp.child">
  <xsl:apply-templates select="." mode="pr-d.fragref.out"/>
</xsl:template>

<xsl:template match="synopfragmentref[@linkend]" mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:call-template name="makeRef"/>
</xsl:template>

<xsl:template match="classsynopsis" mode="pr-d.syntaxdiagram.content.in">
  <xsl:apply-templates select="ooclass|oointerface|ooexception"
        mode="pr-d.groupchoice.out"/>
  <xsl:apply-templates select="fieldsynopsis|constructorsynopsis|
            destructorsynopsis|methodsynopsis"
        mode="pr-d.groupseq.out"/>
</xsl:template>

<xsl:template match="constructorsynopsis|destructorsynopsis|fieldsynopsis|
          methodsynopsis"
      mode="pr-d.syntaxdiagram.content.in">
  <xsl:apply-templates select="." mode="pr-d.groupseq.out"/>
</xsl:template>

<xsl:template match="funcsynopsis" mode="pr-d.syntaxdiagram.content.in">
  <xsl:apply-templates select="funcprototype" mode="pr-d.groupseq.out"/>
</xsl:template>

<xsl:template match="methodparam" mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="methodparam" mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.groupchoice.out"/>
</xsl:template>

<xsl:template match="classname|exceptionname|funcdef|initializer|interfacename|
          methodname|modifier|package|paramdef|parameter|type|varargs|varname|
          void"
      mode="pr-d.groupchoice.child">
  <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
</xsl:template>

<xsl:template match="classname|exceptionname|funcdef|initializer|interfacename|
          methodname|modifier|package|paramdef|parameter|type|varargs|varname|
          void"
      mode="pr-d.groupseq.child">
  <xsl:apply-templates select="." mode="pr-d.kwd.out"/>
</xsl:template>

<xsl:template match="*" mode="pr-d.kwd.child">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<!-- funcparams -->

<xsl:template match="co[@id]" mode="child">
  <xsl:apply-templates select="." mode="topic.keyword.out"/>
</xsl:template>

<xsl:template match="co[@id and @label]" mode="topic.keyword.content.in">
  <xsl:value-of select="@label"/>
</xsl:template>

<xsl:template match="co[@id and not(@label)]" mode="topic.keyword.content.in">
  <xsl:variable name="containerId"
        select="generate-id(ancestor::*[.//co and count(.//co) &gt; 1][1])"/>
  <xsl:value-of select="count(.|
          preceding::co[ancestor::*[generate-id(.)=$containerId]])"/>
</xsl:template>

<xsl:template match="programlistingco" mode="child">
  <xsl:apply-templates select="programlisting|calloutlist" mode="child"/>
</xsl:template>

<xsl:template match="screenco" mode="child">
  <xsl:apply-templates select="screen|calloutlist" mode="child"/>
</xsl:template>

<xsl:template match="calloutlist" mode="child">
  <xsl:apply-templates select="." mode="topic.ol.out"/>
</xsl:template>

<xsl:template match="*[callout]" mode="topic.li.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="callout" mode="topic.li.out"/>
</xsl:template>

<xsl:template match="callout" mode="listitem.cnt.text.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:variable name="called"  select="id(@arearefs)"/>
  <xsl:if test="$called and local-name($called)='co'">
    <xsl:apply-templates select="$called" mode="topic.xref.out"/>
  </xsl:if>
  <xsl:apply-templates select="*|text()" mode="listitem.cnt.child">
     <xsl:with-param name="container"  select="$container"/>
     <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="co" mode="topic.xref.id.att.in"/>

<xsl:template match="co" mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates select="." mode="targetId"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="href.att.out">
    <xsl:with-param name="value" select="$value"/>
  </xsl:apply-templates>
</xsl:template>


<!-- MULTIMEDIA OBJECTS  - - - - - - - - - - - - - - - - - - - - - - - - -->
<!-- TO DO: graphicco|imageobjectco|mediaobjectco SHOULD BE HANDLED BY
         imagemap AND ol -->

<xsl:template match="graphic[@fileref]|imageobject[imagedata/@fileref]|
            inlinegraphic[@fileref]|
            mediaobject[imageobject/imagedata/@fileref]|
            inlinemediaobject[imageobject/imagedata/@fileref]"
      mode="child">
  <xsl:apply-templates select="." mode="topic.image.out"/>
</xsl:template>

<xsl:template match="graphic[not(@fileref)]|
            imageobject[not(imagedata/@fileref)]|
            inlinegraphic[not(@fileref)]|
            mediaobject[not(imageobject/imagedata/@fileref)]|
            inlinemediaobject[not(imageobject/imagedata/@fileref)]"
      mode="child">
  <!-- TO DO:  SHOULD EMIT EITHER AS A COMMENT OR <required-cleanup>
       BASED ON PARAMETER FOR WHETHER A MIGRATION OR PROCESSING CONVERSION -->
</xsl:template>

<xsl:template match="mediaobject|inlinemediaobject" mode="topic.image.atts.in">
  <xsl:apply-templates select="imageobject" mode="topic.image.atts.in"/>
</xsl:template>

<xsl:template match="imageobject" mode="topic.image.atts.in">
  <xsl:apply-templates select="imagedata" mode="topic.image.atts.in"/>
</xsl:template>

<xsl:template match="graphic|mediaobject/imageobject/imagedata"
        mode="placement.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="placement.att.out">
    <xsl:with-param name="value" select="'break'"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template
        match="graphic[@fileref]|imagedata[@fileref]|inlinegraphic[@fileref]"
        mode="href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="href.att.out">
    <xsl:with-param name="value" select="@fileref"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="graphic[@entityref]|imagedata[@entityref]|
            inlinegraphic[@entityref]"
        mode="keyref.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="keyref.att.out">
    <xsl:with-param name="value" select="@entityref"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[graphic or imageobject or inlinegraphic or 
            mediaobject[imageobject] or inlinemediaobject[imageobject]]"
        mode="topic.image.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="graphic|imageobject|inlinegraphic|
            mediaobject[imageobject]|inlinemediaobject[imageobject]"
        mode="topic.image.out"/>
</xsl:template>

<xsl:template match="mediaobject|inlinemediaobject" mode="topic.alt.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="textobject[1]" mode="topic.alt.out"/>
</xsl:template>

<xsl:template match="imageobject" mode="topic.alt.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired" select="'no'"/>
  <xsl:apply-templates select="following-sibling::textobject[1]"
        mode="topic.alt.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.alt.content.in">
  <xsl:apply-templates mode="topic.alt.content.in"/>
</xsl:template>

<xsl:template match="text()" mode="topic.alt.content.in">
  <xsl:copy-of select="."/>
</xsl:template>

<xsl:template match="audioobject|videoobject|
            mediaobject[audioobject|videoobject]|
            inlinemediaobject[audioobject|videoobject]"
      mode="child">
  <xsl:apply-templates select="." mode="topic.object.out"/>
</xsl:template>

<xsl:template match="mediaobject|inlinemediaobject"
        mode="topic.object.atts.in">
  <xsl:apply-templates select="audioobject|videoobject"
        mode="topic.object.atts.in"/>
</xsl:template>

<xsl:template match="mediaobject[caption]|inlinemediaobject[caption]"
        mode="topic.desc.in">
  <xsl:apply-templates select="caption" mode="topic.desc.out"/>
</xsl:template>

<xsl:template match="mediaobject[not(caption) and not(objectinfo)]|
              inlinemediaobject[not(caption) and not(objectinfo)]"
        mode="topic.desc.in">
  <xsl:apply-templates select="audioobject|imageobject|videoobject"
        mode="topic.desc.in"/>
</xsl:template>

<xsl:template match="audioobject|imageobject|videoobject|
          mediaobject[not(caption) and objectinfo]|
          inlinemediaobject[not(caption) and objectinfo]"
      mode="topic.desc.in">
  <xsl:apply-templates select="objectinfo" mode="topic.desc.in"/>
</xsl:template>

<xsl:template match="objectinfo" mode="topic.desc.in">
  <xsl:apply-templates select="abstract" mode="topic.desc.in"/>
</xsl:template>

<xsl:template match="audioobject" mode="topic.object.atts.in">
  <xsl:apply-templates select="audiodata" mode="topic.object.atts.in"/>
</xsl:template>

<xsl:template match="videoobject" mode="topic.object.atts.in">
  <xsl:apply-templates select="videodata" mode="topic.object.atts.in"/>
</xsl:template>

<xsl:template match="audiodata[@fileref]|videodata[@fileref]"
      mode="data.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="data.att.out">
    <xsl:with-param name="value" select="@fileref"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="audiodata[@format]|videodata[@format]" mode="type.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="type.att.out">
    <xsl:with-param name="value" select="@format"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@depth and not(contains(@depth,'%'))]"
      mode="height.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="height.att.out">
    <xsl:with-param name="value" select="@depth"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@width and not(contains(@width,'%'))]"
      mode="width.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="width.att.out">
    <xsl:with-param name="value" select="@width"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[audioobject or videoobject or 
            mediaobject[audioobject or videoobject] or 
            inlinemediaobject[audioobject or videoobject]]"
        mode="topic.object.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="audioobject|videoobject|
            mediaobject[audioobject or videoobject]|
            inlinemediaobject[audioobject or videoobject]"
        mode="topic.object.out"/>
</xsl:template>


<!-- TABLES - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="table|informaltable" mode="child">
  <xsl:apply-templates select="." mode="topic.table.out"/>
</xsl:template>

<xsl:template match="*[table or informaltable]" mode="topic.table.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="table|informaltable" mode="topic.table.out"/>
</xsl:template>

<xsl:template match="table" mode="topic.title.in">
  <xsl:apply-templates select="title" mode="topic.title.out"/>
</xsl:template>

<xsl:template match="table" mode="topic.desc.in">
  <xsl:apply-templates select="caption|textobject" mode="topic.desc.out"/>
</xsl:template>

<xsl:template match="informaltable" mode="topic.desc.in">
  <xsl:apply-templates select="textobject" mode="topic.desc.out"/>
</xsl:template>

<xsl:template match="*[tgroup]" mode="topic.tgroup.in">
  <xsl:apply-templates select="tgroup" mode="topic.tgroup.out"/>
</xsl:template>

<xsl:template match="table[not(tgroup)]|informaltable[not(tgroup)]"
        mode="topic.tgroup.in">
  <xsl:apply-templates select="." mode="topic.tgroup.out"/>
</xsl:template>

<xsl:template match="table|informaltable" mode="topic.tgroup.id.atts.in"/>

<xsl:template match="table[col or colgroup]|informaltable[col or colgroup]"
        mode="topic.colspec.in">
  <!-- TO DO: SHOULD OUTPUT AS COLSPEC -->
</xsl:template>

<xsl:template match="tbody" mode="child">
  <xsl:apply-templates select="." mode="topic.tbody.out"/>
</xsl:template>

<xsl:template match="*[tbody]" mode="topic.tbody.in">
  <xsl:apply-templates select="tbody" mode="topic.tbody.out"/>
</xsl:template>

<xsl:template match="tbody" mode="topic.tbody.content.in">
  <xsl:apply-templates select="." mode="topic.tbody.topic.row.in"/>
  <xsl:apply-templates select="preceding-sibling::tfoot"
      mode="topic.tbody.topic.row.in"/>
</xsl:template>

<xsl:template match="table[not(tbody)]|informaltable[not(tbody)]"
      mode="topic.tbody.in">
  <xsl:apply-templates select="." mode="topic.tbody.out"/>
</xsl:template>

<xsl:template match="table|informaltable" mode="topic.body.id.atts.in"/>

<xsl:template match="table|informaltable" mode="topic.tbody.content.in">
  <xsl:apply-templates select="."     mode="topic.tbody.topic.row.in"/>
  <xsl:apply-templates select="tfoot" mode="topic.tbody.topic.row.in"/>
</xsl:template>

<xsl:template match="colspec" mode="child">
  <xsl:apply-templates select="." mode="topic.colspec.out"/>
</xsl:template>

<xsl:template match="*[colspec]" mode="topic.colspec.in">
  <xsl:apply-templates select="colspec" mode="topic.colspec.out"/>
</xsl:template>

<xsl:template match="thead" mode="child">
  <xsl:apply-templates select="." mode="topic.thead.out"/>
</xsl:template>

<xsl:template match="*[thead]" mode="topic.thead.in">
  <xsl:apply-templates select="thead" mode="topic.thead.out"/>
</xsl:template>

<xsl:template match="tfoot" mode="child"/>

<xsl:template match="row|tr" mode="child">
  <xsl:apply-templates select="." mode="topic.row.out"/>
</xsl:template>

<xsl:template match="*[row or tr]" mode="topic.row.in">
  <xsl:apply-templates select="row|tr" mode="topic.row.out"/>
</xsl:template>

<xsl:template match="entry|td|th" mode="child">
  <xsl:apply-templates select="." mode="topic.entry.out"/>
</xsl:template>

<xsl:template match="*[entry or td or th]" mode="topic.entry.in">
  <xsl:apply-templates select="entry|td|th" mode="topic.entry.out"/>
</xsl:template>

<xsl:template match="*[@align]" mode="align.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="align.att.out">
    <xsl:with-param name="value" select="@align"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@colname]" mode="colname.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="colname.att.out">
    <xsl:with-param name="value" select="@colname"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@colnum]" mode="colnum.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="colnum.att.out">
    <xsl:with-param name="value" select="@colnum"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@colrowsep]" mode="colrowsep.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="colrowsep.att.out">
    <xsl:with-param name="value" select="@colrowsep"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@cols]" mode="cols.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="cols.att.out">
    <xsl:with-param name="value" select="@cols"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@colsep]" mode="colsep.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="colsep.att.out">
    <xsl:with-param name="value" select="@colsep"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@colwidth]" mode="colwidth.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="colwidth.att.out">
    <xsl:with-param name="value" select="@colwidth"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@frame]" mode="frame.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="frame.att.out">
    <xsl:with-param name="value" select="@frame"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@morerows]" mode="morerows.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="morerows.att.out">
    <xsl:with-param name="value" select="@morerows"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@nameend]" mode="nameend.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="nameend.att.out">
    <xsl:with-param name="value" select="@nameend"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@namest]" mode="namest.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="namest.att.out">
    <xsl:with-param name="value" select="@namest"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@pgwide]" mode="pgwide.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="pgwide.att.out">
    <xsl:with-param name="value" select="@pgwide"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@rowheader]" mode="rowheader.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="rowheader.att.out">
    <xsl:with-param name="value" select="@rowheader"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[@rowsep]" mode="rowsep.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="rowsep.att.out">
    <xsl:with-param name="value" select="@rowsep"/>
  </xsl:apply-templates>
</xsl:template>

<!-- UNCLEAR STATUS - IN DITA SCHEMA BUT NOT DTD -->
<xsl:template match="*[@spanname]" mode="spanname.att.in"/>

<!-- xsl:template match="*[@spanname]" mode="spanname.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="spanname.att.out">
    <xsl:with-param name="value" select="@spanname"/>
  </xsl:apply-templates>
</xsl:template -->

<xsl:template match="*[@valign]" mode="valign.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="valign.att.out">
    <xsl:with-param name="value" select="@valign"/>
  </xsl:apply-templates>
</xsl:template>

<!-- TO DO: FLATTEN entrytbl -->


<!-- META INFORMATION  - - - - - - - - - - - - - - - - - - - - - - - - - -->
<!-- TO DO: 
HANDLE

    mode="topic.source.in"
    mode="topic.permissions.in"
    mode="topic.resourceid.in"
    mode="topic.audience.in"

SHOULD HANDLE ANYTHING UNKNOWN
    mode="topic.othermeta.in"

FOR ANYTHING THAT CAN APPEAR IN *info or *meta

 -->

<xsl:template match="*[authorgroup]" mode="topic.author.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="authorgroup" mode="topic.author.in"/>
</xsl:template>

<xsl:template match="author|corpauthor" mode="child">
  <xsl:apply-templates select="." mode="topic.ph.out"/>
</xsl:template>

<xsl:template match="author|corpauthor" mode="words.cnt.child">
  <xsl:apply-templates mode="words.cnt.child"/>
</xsl:template>

<xsl:template match="*[author or corpauthor]" mode="topic.author.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="author|corpauthor" mode="topic.author.out"/>
</xsl:template>

<xsl:template match="author[email]" mode="topic.author.href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="email" mode="topic.author.href.att.in">
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="author[not(email) and address/email[1]]"
      mode="topic.author.href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="address/email[1]"
        mode="topic.author.href.att.in">
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="author[not(email) and not(address/email) and
          affiliation/address/email[1]]"
      mode="topic.author.href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="affiliation/address/email[1]"
      mode="topic.author.href.att.in">
    <xsl:with-param name="isRequired" select="$isRequired"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="email" mode="topic.author.href.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates mode="textOnly"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="href.att.out">
    <xsl:with-param name="value" select="normalize-space($value)"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="author" mode="words.cnt.text.in">
  <xsl:apply-templates
        select="personname|honorific|firstname|surname|lineage|othername"
        mode="words.cnt.text.in"/>
</xsl:template>

<xsl:template match="personname" mode="words.cnt.text.in">
  <xsl:apply-templates select="honorific|firstname|surname|lineage|othername"
        mode="words.cnt.text.in"/>
</xsl:template>

<xsl:template match="honorific|firstname|surname|lineage|othername"
      mode="words.cnt.text.in">
  <xsl:if test="preceding-sibling::honorific or
                preceding-sibling::firstname or
                preceding-sibling::surname or
                preceding-sibling::lineage or
                preceding-sibling::othername">
      <xsl:text> </xsl:text>
  </xsl:if>
  <xsl:apply-templates mode="words.cnt.text.in"/>
</xsl:template>

<xsl:template match="*[publisher]" mode="topic.publisher.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="publisher/publishername"
        mode="topic.publisher.out"/>
</xsl:template>

<xsl:template match="*[copyright]" mode="topic.copyright.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:variable name="holders" select="copyright/holder"/>
  <xsl:choose>
  <xsl:when test="$holders">
    <xsl:apply-templates select="$holders" mode="topic.copyright.out"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="copyright" mode="topic.copyright.out"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="copyright[not(holder)]" mode="topic.copyright.content.in">
  <xsl:apply-templates select="year" mode="topic.copyryear.out"/>
  <xsl:apply-templates select="." mode="topic.copyrholder.out"/>
</xsl:template>

<xsl:template match="copyright[not(holder)]"
      mode="topic.copyrholder.content.in"/>

<xsl:template match="holder" mode="topic.copyright.content.in">
  <xsl:apply-templates select="../year" mode="topic.copyryear.out"/>
  <xsl:apply-templates select="." mode="topic.copyrholder.out"/>
</xsl:template>

<xsl:template match="year" mode="year.att.in">
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="year.att.out">
    <xsl:with-param name="value" select="text()"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[revhistory]" mode="topic.critdates.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="revhistory" mode="topic.critdates.out"/>
</xsl:template>

<xsl:template match="revhistory" mode="topic.critdates.content.in">
  <xsl:apply-templates select="revision[1]/date" mode="topic.created.out"/>
  <xsl:apply-templates select="revision[position() &gt; 1]/date"
        mode="topic.revised.out"/>
</xsl:template>

<xsl:template match="revision/date" mode="topic.created.date.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates mode="textOnly"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="date.att.out">
    <xsl:with-param name="value" select="normalize-space($value)"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="revision/date" mode="topic.revised.modified.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates mode="textOnly"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="modified.att.out">
    <xsl:with-param name="value" select="normalize-space($value)"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[subjectset]" mode="topic.category.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="subjectset" mode="topic.category.out"/>
</xsl:template>

<xsl:template match="subjectset" mode="topic.category.content.in">
  <xsl:if test="@scheme">
    <xsl:apply-templates select="." mode="topic.keyword.out"/>
  </xsl:if>
  <xsl:apply-templates select="subject/subjectterm" mode="topic.term.out"/>
</xsl:template>

<xsl:template match="subjectset[@scheme]" mode="topic.keyword.content.in">
  <xsl:value-of select="@scheme"/>
</xsl:template>

<xsl:template match="*[not(itermset) and not(keywordset)]"
      mode="topic.keywords.in">
  <xsl:if test="indexterm or (position()=1 and
            contains(local-name(),'info') and parent::indexterm)">
    <xsl:apply-templates select="." mode="topic.keywords.out"/>
  </xsl:if>
</xsl:template>

<xsl:template match="*[itermset or keywordset]" mode="topic.keywords.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:if test="indexterm or (position()=1 and
            contains(local-name(),'info') and parent::indexterm)">
    <xsl:apply-templates select="." mode="topic.keywords.out"/>
  </xsl:if>
  <xsl:apply-templates select="itermset|keywordset" mode="topic.keywords.out"/>
</xsl:template>

<xsl:template
      match="*[local-name()!='itermset' and local-name()!='keywordset']"
      mode="topic.keywords.content.in">
  <xsl:apply-templates select="indexterm" mode="child"/>
  <xsl:if test="position()=1 and contains(local-name(),'info')">
    <xsl:apply-templates select="../indexterm" mode="child"/>
  </xsl:if>
</xsl:template>

<xsl:template match="keyword|nonterminal|token" mode="child">
  <xsl:apply-templates select="." mode="topic.keyword.out"/>
</xsl:template>

<xsl:template match="*" mode="topic.keyword.child">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<xsl:template match="*[productname or productnumber]" mode="topic.prodinfo.in">
  <xsl:param name="container"/>
  <xsl:param name="isRequired"/>
  <xsl:apply-templates select="." mode="topic.prodinfo.out"/>
</xsl:template>

<xsl:template match="*[productname or productnumber]"
      mode="topic.prodinfo.content.in">
  <xsl:choose>
  <xsl:when test="productname">
    <xsl:apply-templates select="productname" mode="topic.prodname.out"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="." mode="topic.prodname.out"/>
  </xsl:otherwise>
  </xsl:choose>
  <xsl:choose>
  <xsl:when test="productnumber">
    <xsl:apply-templates select="productnumber" mode="topic.vrmlist.out"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="." mode="topic.vrmlist.out"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="*" mode="topic.vrm.in">
  <xsl:apply-templates select="." mode="topic.vrm.out"/>
</xsl:template>

<xsl:template match="*[productnumber]" mode="topic.prodname.content.in"/>

<xsl:template match="productnumber" mode="topic.vrm.version.att.in">
  <xsl:param name="isRequired"/>
  <xsl:variable name="value">
    <xsl:apply-templates mode="textOnly"/>
  </xsl:variable>
  <xsl:apply-templates select="." mode="version.att.out">
    <xsl:with-param name="value" select="normalize-space($value)"/>
  </xsl:apply-templates>
</xsl:template>


<!-- MISCELLANEOUS - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="comment|remark" mode="topic.body.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="comment|remark" mode="topic.fig.child">
  <xsl:apply-templates select="." mode="topic.p.out"/>
</xsl:template>

<xsl:template match="comment|remark" mode="topic.p.atts.in"/>

<xsl:template match="comment|remark" mode="para.cnt.text.in">
  <xsl:apply-templates select="." mode="child"/>
</xsl:template>

<xsl:template match="comment|remark" mode="child">
  <xsl:apply-templates select="." mode="topic.draft-comment.out"/>
</xsl:template>

<xsl:template match="*[comment or remark]" mode="topic.draft-comment.in">
  <xsl:apply-templates select="comment|remark" mode="topic.draft-comment.out"/>
</xsl:template>


<!-- ELEMENTS WITH NO EQUIVALENT - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="titleabbrev|index|setindex|lot|toc|tocchap|beginpage"
    mode="child"/>


<!-- UTILITY FUNCTIONS - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*" mode="textOnly">
  <xsl:apply-templates mode="textOnly"/>
</xsl:template>

<xsl:template match="text()" mode="textOnly">
  <xsl:copy-of select="."/>
</xsl:template>

<xsl:template name="makeRef">
  <xsl:param name="target" select="."/>
  <xsl:param name="targetId">
    <xsl:apply-templates select="$target" mode="targetId"/>
  </xsl:param>
  <xsl:choose>
  <xsl:when test="string-length($targetId) > 0">
    <xsl:apply-templates select="$target" mode="href.att.out">
      <xsl:with-param name="value" select="$targetId"/>
    </xsl:apply-templates>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="$target" mode="href.att.out">
      <xsl:with-param name="value" select="@linkend|@otherterm"/>
    </xsl:apply-templates>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="*[@linkend]" mode="targetId">
  <xsl:variable name="targetElement" select="id(@linkend)"/>
  <xsl:if test="$targetElement">
    <xsl:apply-templates select="$targetElement" mode="targetId"/>
  </xsl:if>
</xsl:template>

<xsl:template match="*[@otherterm]" mode="targetId">
  <xsl:variable name="targetElement" select="id(@otherterm)"/>
  <xsl:if test="$targetElement">
    <xsl:apply-templates select="$targetElement" mode="targetId"/>
  </xsl:if>
</xsl:template>

<xsl:template match="*[@id]" mode="targetId">
  <xsl:variable name="isTopic">
    <xsl:apply-templates select="." mode="isTopic"/>
  </xsl:variable>
  <xsl:text>#</xsl:text>
  <xsl:choose>
  <xsl:when test="$isTopic='yes'">
    <xsl:value-of select="@id"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select=".." mode="findTopicId"/>
    <xsl:text>/</xsl:text>
    <xsl:value-of select="@id"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="*" mode="findTopicId">
  <xsl:variable name="isTopic">
    <xsl:apply-templates select="." mode="isTopic"/>
  </xsl:variable>
  <xsl:choose>
  <xsl:when test="$isTopic='yes'">
    <xsl:apply-templates select="." mode="topicId"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select=".." mode="findTopicId"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="makeTopicId">
  <xsl:param name="titleElement"/>
  <xsl:choose>
  <xsl:when test="@id">
    <xsl:value-of select="@id"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:variable name="topicName">
      <xsl:choose>
      <xsl:when test="$titleElement">
        <xsl:apply-templates select="$titleElement" mode="textOnly"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="local-name(.)"/>
      </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:if test="string(number(substring($topicName,1,1)))!='NaN'">
      <xsl:text>a</xsl:text>
    </xsl:if>
    <xsl:call-template name="camelCase">
      <xsl:with-param name="input" select="$topicName"/>
    </xsl:call-template>
    <xsl:text>.</xsl:text>
    <xsl:value-of select="generate-id(.)"/>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- TO DO: NOT APPROPRIATE FOR EVERY LOCALE -->
<xsl:template name="camelCase">
  <xsl:param name="input"/>
  <xsl:param name="delimiters">
    <xsl:text> !@#$%^*()-_=+[]{}|;:'",./?&amp;&lt;&gt;</xsl:text>
  </xsl:param>
  <xsl:param name="lowerFirst" select="'yes'"/>
  <xsl:variable name="delimiter" select="substring($delimiters,1,1)"/>
  <xsl:call-template name="camelCasePart">
    <xsl:with-param name="input"     select="translate(normalize-space($input),
          substring($delimiters,2), $delimiter)"/>
    <xsl:with-param name="delimiter"  select="$delimiter"/>
    <xsl:with-param name="lowerFirst" select="$lowerFirst"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="camelCasePart">
  <xsl:param name="input"/>
  <xsl:param name="delimiter"  select="' '"/>
  <xsl:param name="lowerFirst" select="'yes'"/>
  <xsl:param name="isFirst"    select="'yes'"/>
  <xsl:choose>
  <xsl:when test="contains($input,$delimiter)">
    <xsl:variable name="beforePart"
          select="substring-before($input,$delimiter)"/>
    <xsl:variable name="afterPart"
          select="substring-after($input,$delimiter)"/>
    <xsl:call-template name="initialCapital">
      <xsl:with-param name="input"      select="$beforePart"/>
      <xsl:with-param name="lowerFirst" select="$lowerFirst"/>
      <xsl:with-param name="isFirst"    select="$isFirst"/>
    </xsl:call-template>
    <xsl:call-template name="camelCasePart">
      <xsl:with-param name="input"      select="$afterPart"/>
      <xsl:with-param name="delimiter"  select="$delimiter"/>
      <xsl:with-param name="lowerFirst" select="$lowerFirst"/>
      <xsl:with-param name="isFirst"    select="'no'"/>
    </xsl:call-template>
  </xsl:when>
  <xsl:otherwise>
    <xsl:call-template name="initialCapital">
      <xsl:with-param name="input"      select="$input"/>
      <xsl:with-param name="lowerFirst" select="$lowerFirst"/>
      <xsl:with-param name="isFirst"    select="$isFirst"/>
    </xsl:call-template>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="initialCapital">
  <xsl:param name="input"/>
  <xsl:param name="lowerFirst" select="'yes'"/>
  <xsl:param name="isFirst"    select="'yes'"/>
  <xsl:if test="string-length($input) &gt; 0">
    <xsl:variable name="initial" select="substring($input,1,1)"/>
    <xsl:choose>
    <xsl:when test="$isFirst='yes' and $lowerFirst='yes'">
      <xsl:value-of select="translate($initial,
                'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
                'abcdefghijklmnopqrstuvwxyz')"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="translate($initial,
                'abcdefghijklmnopqrstuvwxyz',
                'ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
    </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="string-length($input) &gt; 1">
      <xsl:value-of select="translate(substring($input,2),
                'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
                'abcdefghijklmnopqrstuvwxyz')"/>
    </xsl:if>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
