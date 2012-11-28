<?xml version="1.0"?>

<!--
    docbook2latex.xslt:
        translates a DocBook XML source into a LaTeX source file,
        which can be processed with pdflatex to produce a
        pretty PDF file.

        Note: In the LaTeX output, this XSLT encodes all quotes
        with \QUOTE{} commands, which are not defined in this
        file. This is because XSLT does not support regular
        expressions natively and therefore it is rather difficult
        to implement proper "pretty quotes" (different glyphs for
        opening and closing quotes) in XSLT. The doc/manual/
        makefile solves this by running sed over the LaTeX source
        once more, replacing all \QUOTE{} commands with
        \OQ{} and \CQ{} commands, which _are_ defined to the
        pretty quotes for English in the LaTeX output generated
        by this XSLT (see below).

    Copyright (C) 2006-2012 Oracle Corporation

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
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:str="http://xsltsl.org/string"
>

  <xsl:import href="string.xsl"/>

  <xsl:variable name="g_nlsChapter">
    <xsl:choose>
      <xsl:when test="$TARGETLANG='de_DE'">Kapitel</xsl:when>
      <xsl:when test="$TARGETLANG='fr_FR'">chapitre</xsl:when>
      <xsl:when test="$TARGETLANG='en_US'">chapter</xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes"><xsl:value-of select="concat('Invalid language ', $TARGETLANG)" /></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="g_nlsPage">
    <xsl:choose>
      <xsl:when test="$TARGETLANG='de_DE'">auf Seite</xsl:when>
      <xsl:when test="$TARGETLANG='fr_FR'">page</xsl:when>
      <xsl:when test="$TARGETLANG='en_US'">page</xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes"><xsl:value-of select="concat('Invalid language ', $TARGETLANG)" /></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="g_nlsNote">
    <xsl:choose>
      <xsl:when test="$TARGETLANG='de_DE'">Hinweis</xsl:when>
      <xsl:when test="$TARGETLANG='fr_FR'">Note</xsl:when>
      <xsl:when test="$TARGETLANG='en_US'">Note</xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes"><xsl:value-of select="concat('Invalid language ', $TARGETLANG)" /></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

    <xsl:variable name="g_nlsWarning">
    <xsl:choose>
      <xsl:when test="$TARGETLANG='de_DE'">Warnung</xsl:when>
      <xsl:when test="$TARGETLANG='fr_FR'">Avertissement</xsl:when>
      <xsl:when test="$TARGETLANG='en_US'">Warning</xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes"><xsl:value-of select="concat('Invalid language ', $TARGETLANG)" /></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:output method="text"/>

  <xsl:strip-space elements="*"/>
  <xsl:preserve-space elements="para"/>

  <xsl:template match="/book">
    <xsl:text>
\documentclass[oneside,a4paper,10pt,DIV10]{scrbook}
\usepackage{geometry}
\geometry{top=3cm,bottom=4cm}
\usepackage{ucs}
\usepackage[utf8x]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{tabulary}
\usepackage[pdftex,
            a4paper,
            colorlinks=true,
            linkcolor=blue,
            bookmarksnumbered,
            bookmarksopen=true,
            bookmarksopenlevel=0,
            hyperfootnotes=false,
            plainpages=false,
            pdfpagelabels
  ]{hyperref}

\usepackage{nameref}
\usepackage{graphicx}
\usepackage{fancybox}
\usepackage{fancyvrb}
\usepackage{alltt}
\usepackage{color}

</xsl:text>
  <xsl:if test="$TARGETLANG='de_DE'">\usepackage[ngerman]{babel}&#10;\PrerenderUnicode{ü}</xsl:if>
<!--   <xsl:if test="$TARGETLANG='fr_FR'">\usepackage[french]{babel}&#10;\FrenchItemizeSpacingfalse&#10;\renewcommand{\FrenchLabelItem}{\textbullet}</xsl:if>
    this command is no longer understood by TexLive2008
    -->
    <xsl:text>

% use Palatino as serif font:
% \usepackage{mathpazo}
\usepackage{charter}
% use Helvetica as sans-serif font:
\usepackage{helvet}

% use Bera Mono (a variant of Bitstream Vera Mono) as typewriter font
% (requires texlive-fontsextra)
\usepackage[scaled]{beramono}
% previously: use Courier as typewriter font:
% \usepackage{courier}

\definecolor{colNote}{rgb}{0,0,0}
\definecolor{colWarning}{rgb}{0,0,0}
\definecolor{colScreenFrame}{rgb}{0,0,0}
\definecolor{colScreenText}{rgb}{0,0,0}

% number headings down to this level
\setcounter{secnumdepth}{3}
% more space for the section numbers
\makeatletter
\renewcommand*\l@section{\@dottedtocline{1}{1.5em}{2.9em}}
\renewcommand*\l@subsection{\@dottedtocline{2}{4.4em}{3.8em}}
\renewcommand*\l@subsubsection{\@dottedtocline{3}{8.2em}{3.8em}}
\renewcommand*\@pnumwidth{1.7em}
\renewcommand*\@tocrmarg{5.0em}
\makeatother

% more tolerance at 2nd wrap stage:
\tolerance = 1000
% allow 3rd wrap stage:
\emergencystretch = 10pt
% no Schusterjungen:
\clubpenalty = 10000
% no Hurenkinder:
\widowpenalty = 10000
\displaywidowpenalty = 10000
% max pdf compression:
\pdfcompresslevel9

% opening and closing quotes: the OQ and CQ macros define this (and the makefile employs some sed magic also)
</xsl:text>
  <xsl:choose>
    <xsl:when test="$TARGETLANG='de_DE'">
      <xsl:text>\newcommand\OQ{\texorpdfstring{\glqq}{"}}&#10;\newcommand\CQ{\texorpdfstring{\grqq}{"}}&#10;</xsl:text>
    </xsl:when>
    <xsl:when test="$TARGETLANG='fr_FR'">
      <xsl:text>\newcommand\OQ{\texorpdfstring{``}{"}}&#10;\newcommand\CQ{\texorpdfstring{''}{"}}&#10;</xsl:text>
    </xsl:when>
    <xsl:when test="$TARGETLANG='en_US'">
      <xsl:text>\newcommand\OQ{\texorpdfstring{``}{"}}&#10;\newcommand\CQ{\texorpdfstring{''}{"}}&#10;</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:message terminate="yes"><xsl:value-of select="concat('Invalid language ', $TARGETLANG)" /></xsl:message>
    </xsl:otherwise>
  </xsl:choose>

  <xsl:apply-templates />

  <xsl:text>
\end{document}
  </xsl:text>

  </xsl:template>

  <xsl:template match="bookinfo">
    <xsl:apply-templates />
    <xsl:text>&#x0a;\newcommand\docbookbookinfocopyright{\copyright{} \docbookbookinfocopyrightyear{} \docbookbookinfocopyrightholder{}}&#x0a;
\author{ \docbooktitleedition \\ %
\\ %
</xsl:text>
    <xsl:if test="//bookinfo/address">
      <xsl:text>\docbookbookinfoaddress \\ %
\\ %
</xsl:text>
    </xsl:if>
    <xsl:text>\docbookbookinfocopyright \\ %
}

\title{\docbooktitle \\
\docbooksubtitle}
% \subtitle{\docbooksubtitle}
\hypersetup{pdfauthor=\docbookcorpauthor}
\hypersetup{pdftitle=\docbooktitle{} \docbooksubtitle{}}

\hyphenation{da-ta-ba-ses}
\hyphenation{deb-conf}
\hyphenation{VirtualBox}

\begin{document}
% \maketitle
%\begin{titlepage}
\thispagestyle{empty}
\begin{minipage}{\textwidth}
\begin{center}
\includegraphics[width=4cm]{images/vboxlogo.png}
\end{center}%
\vspace{10mm}

{\fontsize{40pt}{40pt}\selectfont\rmfamily\bfseries%
\begin{center}
\docbooktitle
\end{center}%
\vspace{10mm}
}

{\fontsize{30pt}{30pt}\selectfont\rmfamily\bfseries%
\begin{center}
\docbooksubtitle
\end{center}%
\vspace{10mm}
}

{\fontsize{16pt}{20pt}\selectfont\rmfamily%
\begin{center}
\docbooktitleedition

\vspace{2mm}

\docbookbookinfocopyright

\vspace{2mm}

\docbookbookinfoaddress
\end{center}%
}

%\end{titlepage}
\end{minipage}

\tableofcontents
  </xsl:text>
  </xsl:template>

  <xsl:template match="subtitle">
    <xsl:choose>
      <xsl:when test="name(..)='bookinfo'">
        <xsl:text>\newcommand\docbooksubtitle{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="title">
    <xsl:choose>
      <xsl:when test="name(..)='bookinfo'">
        <xsl:text>\newcommand\docbooktitle{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='chapter'">
        <xsl:text>&#x0a;&#x0a;\chapter{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='sect1'">
        <xsl:text>&#x0a;&#x0a;\section{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='sect2'">
        <xsl:text>&#x0a;&#x0a;\subsection{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='sect3'">
        <xsl:text>&#x0a;&#x0a;\subsubsection{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='sect4'">
        <xsl:text>&#x0a;&#x0a;\paragraph{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='sect5'">
        <xsl:text>&#x0a;&#x0a;\subparagraph{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='appendix'">
        <xsl:text>&#x0a;&#x0a;\chapter{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="name(..)='glossdiv'">
        <xsl:text>&#x0a;&#x0a;\section*{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}</xsl:text>
      </xsl:when>
    </xsl:choose>
    <xsl:variable name="refid" select="(@id) | (../@id)" />
    <xsl:if test="$refid">
      <xsl:value-of select="concat('&#x0a;\label{', $refid, '}')" />
    </xsl:if>
    <xsl:text>&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="edition">
    <xsl:choose>
      <xsl:when test="name(..)='bookinfo'">
        <xsl:text>\newcommand\docbooktitleedition{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}&#x0a;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="corpauthor">
    <xsl:choose>
      <xsl:when test="name(..)='bookinfo'">
        <xsl:text>\newcommand\docbookcorpauthor{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}&#x0a;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="address">
    <xsl:choose>
      <xsl:when test="name(..)='bookinfo'">
        <xsl:text>\newcommand\docbookbookinfoaddress{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}&#x0a;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="year">
    <xsl:choose>
      <xsl:when test="name(..)='copyright'">
        <xsl:text>\newcommand\docbookbookinfocopyrightyear{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}&#x0a;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="holder">
    <xsl:choose>
      <xsl:when test="name(..)='copyright'">
        <xsl:text>\newcommand\docbookbookinfocopyrightholder{</xsl:text>
        <xsl:apply-templates />
        <xsl:text>}&#x0a;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="glossary">
    <xsl:text>&#x0a;&#x0a;\backmatter&#x0a;\chapter{Glossary}&#x0a;</xsl:text>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="para">
    <xsl:if test="not(name(..)='footnote' or name(..)='note' or name(..)='warning')">
      <xsl:text>&#x0a;&#x0a;</xsl:text>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="note">
    <xsl:value-of select="concat('&#x0a;&#x0a;\vspace{.2cm}&#x0a;&#x0a;\begin{center}\fbox{\begin{minipage}[c]{0.9\textwidth}\color{colNote}\textbf{', $g_nlsNote, ':} ')" />
    <xsl:apply-templates />
    <xsl:text>\end{minipage}}\end{center}&#x0a;&#x0a;\vspace{.2cm}&#x0a;&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="warning">
    <xsl:value-of select="concat('&#x0a;&#x0a;\vspace{.2cm}&#x0a;&#x0a;\begin{center}\fbox{\begin{minipage}[c]{0.9\textwidth}\color{colWarning}\textbf{', $g_nlsWarning, ':} ')" />
    <xsl:apply-templates />
    <xsl:text>\end{minipage}}\end{center}&#x0a;&#x0a;\vspace{.2cm}&#x0a;&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="screen">
    <xsl:text>&#x0a;&#x0a;\begin{Verbatim}[fontsize=\footnotesize]&#x0a;</xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{Verbatim}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="programlisting">
    <xsl:text>&#x0a;&#x0a;{\small\begin{alltt}&#x0a;</xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{alltt}}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="footnote">
    <xsl:text>\footnote{</xsl:text>
    <xsl:apply-templates />
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="tgroup">
    <xsl:choose>
      <xsl:when test="@style='verywide'">
        <xsl:text>&#x0a;&#x0a;{\small\begin{center}&#x0a;\begin{tabulary}{1.1\textwidth}[]{|L|L|L|}&#x0a;\hline&#x0a;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>&#x0a;&#x0a;{\small\begin{center}&#x0a;\begin{tabulary}{.9\textwidth}[]{|L|L|L|}&#x0a;\hline&#x0a;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{tabulary}&#x0a;\end{center}}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="row">
    <xsl:apply-templates />
    <xsl:text>&#x0a;\\ \hline&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="entry">
    <xsl:if test="not(position()=1)">
      <xsl:text> &amp; </xsl:text>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="itemizedlist">
    <xsl:text>&#x0a;&#x0a;\begin{itemize}&#x0a;</xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{itemize}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="orderedlist">
    <xsl:text>&#x0a;&#x0a;\begin{enumerate}&#x0a;</xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{enumerate}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="listitem">
    <xsl:text>&#x0a;&#x0a;\item </xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="glossterm">
    <xsl:variable name="refid" select="(@id)" />
    <xsl:if test="$refid">
      <xsl:value-of select="concat('&#x0a;\label{', $refid, '}')" />
    </xsl:if>
    <xsl:text>&#x0a;&#x0a;\item[</xsl:text>
    <xsl:apply-templates />
    <xsl:text>]</xsl:text>
  </xsl:template>

  <xsl:template match="glosslist | glossdiv">
    <xsl:text>&#x0a;&#x0a;\begin{description}&#x0a;</xsl:text>
    <xsl:apply-templates />
    <xsl:text>&#x0a;\end{description}&#x0a;</xsl:text>
  </xsl:template>

  <xsl:template match="superscript">
    <xsl:variable name="contents">
      <xsl:apply-templates />
    </xsl:variable>
    <xsl:value-of select="concat('\texorpdfstring{\textsuperscript{', $contents, '}}{', $contents, '}')" />
  </xsl:template>

  <xsl:template match="emphasis">
    <xsl:choose>
      <xsl:when test="@role='bold'">
        <xsl:text>\textbf{</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>\textit{</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates />
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="computeroutput | code">
    <xsl:text>\texttt{</xsl:text>
    <xsl:apply-templates />
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="ulink">
    <xsl:text>\url{</xsl:text>
    <xsl:apply-templates />
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="xref">
    <xsl:choose>
      <xsl:when test="@xreflabel">
        <xsl:value-of select="concat('\hyperref[', @linkend, ']{\mbox{', @xreflabel, '}}')" />
      </xsl:when>
      <xsl:when test="@apiref='yes'">
        <xsl:value-of select="concat('\hyperref[', @linkend, ']{\mbox{')" />
        <xsl:apply-templates />
        <xsl:value-of select="'}}'" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="concat($g_nlsChapter, ' \ref{', @linkend, '}, \textit{\nameref{', @linkend, '}}, ', $g_nlsPage, ' \pageref{', @linkend, '}')" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- for some reason, DocBook insists of having image data nested this way always:
       mediaobject -> imageobject -> imagedata
       but only imagedata is interesting  -->
  <xsl:template match="imagedata">
    <xsl:if test="@align='center'">
      <xsl:text>\begin{center}</xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('&#x0a;\includegraphics[width=', @width, ']{', @fileref, '}&#x0a;')" />
    <xsl:apply-templates />
    <xsl:if test="@align='center'">
      <xsl:text>\end{center}</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="//text()">
    <xsl:variable name="subst1">
      <xsl:call-template name="str:subst">
        <xsl:with-param name="text" select="." />
        <xsl:with-param name="replace" select="'\'" />
        <xsl:with-param name="with" select="'\textbackslash{}'" />
        <xsl:with-param name="disable-output-escaping" select="no" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="(name(..)='screen') or (name(../..)='screen')">
        <xsl:value-of select="." />
      </xsl:when>
      <xsl:when test="(name(..)='computeroutput') or (name(../..)='computeroutput') or (name(..)='code') or (name(../..)='code')">
        <xsl:variable name="subst2">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst1" />
            <xsl:with-param name="replace" select="'--'" />
            <xsl:with-param name="with" select="'-{}-'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst3">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst2" />
            <xsl:with-param name="replace" select="'_'" />
            <xsl:with-param name="with" select="'\_'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst4">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst3" />
            <xsl:with-param name="replace" select="'$'" />
            <xsl:with-param name="with" select="'\$'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst5">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst4" />
            <xsl:with-param name="replace" select="'%'" />
            <xsl:with-param name="with" select="'\%'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst6">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst5" />
            <xsl:with-param name="replace" select="'#'" />
            <xsl:with-param name="with" select="'\#'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst7">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst6" />
            <xsl:with-param name="replace" select="'~'" />
            <xsl:with-param name="with" select="'\textasciitilde '" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst8">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst7" />
            <xsl:with-param name="replace" select="'&amp;'" />
            <xsl:with-param name="with" select="'\&amp;'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="$subst8" />
      </xsl:when>
      <xsl:when test="(name(..)='address') or (name(../..)='address')">
        <xsl:variable name="subst2">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst1" />
            <xsl:with-param name="replace" select="'&#x0a;'" />
            <xsl:with-param name="with" select="' \\'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="$subst2" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="subst2">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst1" />
            <xsl:with-param name="replace" select="'_'" />
            <xsl:with-param name="with" select="'\_'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst3">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst2" />
            <xsl:with-param name="replace" select="'$'" />
            <xsl:with-param name="with" select="'\$'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst4">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst3" />
            <xsl:with-param name="replace" select="'%'" />
            <xsl:with-param name="with" select="'\%'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst5">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst4" />
            <xsl:with-param name="replace" select="'#'" />
            <xsl:with-param name="with" select="'\#'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst6">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst5" />
            <xsl:with-param name="replace" select="'µ'" />
            <xsl:with-param name="with" select="'$\mu$'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst7">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst6" />
            <xsl:with-param name="replace" select="'®'" />
            <xsl:with-param name="with" select="'\texorpdfstring{\textregistered}{}'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="quote">"</xsl:variable>
        <!-- preparation for pretty quotes: replace all double quotes _outside_ screen
             sections with "\QUOTE{}" strings, which the makefile will then replace
             with pretty quotes by invoking sed a few times. Unfortunately there are
             no regular expressions in XSLT so there's no other way. -->
        <xsl:variable name="subst8">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst7" />
            <xsl:with-param name="replace" select="$quote" />
            <xsl:with-param name="with" select="'\QUOTE{}'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst9">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst8" />
            <xsl:with-param name="replace" select="'~'" />
            <xsl:with-param name="with" select="'\textasciitilde '" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="subst10">
          <xsl:call-template name="str:subst">
            <xsl:with-param name="text" select="$subst9" />
            <xsl:with-param name="replace" select="'&amp;'" />
            <xsl:with-param name="with" select="'\&amp;'" />
            <xsl:with-param name="disable-output-escaping" select="no" />
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="$subst10" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>

