<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<!ENTITY % docbookMap        "docbookMap">
<!ENTITY % sethead           "sethead">
<!ENTITY % bookhead          "bookhead">
<!ENTITY % dedicationref     "dedicationref">
<!ENTITY % prefaceref        "prefaceref">
<!ENTITY % chapterref        "chapterref">
<!ENTITY % sectionref        "sectionref">
<!-- no sect1 and so on because a reused section should be able to
     change levels -->
<!ENTITY % referencehead     "referencehead">
<!ENTITY % refentryref       "refentryref">
<!ENTITY % refsynopsisdivref "refsynopsisdivref">
<!ENTITY % refsectionref     "refsectionref">
<!-- no refsect1 and so on because a reused refsection should be able to
     change levels -->
<!ENTITY % parthead          "parthead">
<!ENTITY % partintroref      "partintroref">
<!ENTITY % appendixref       "appendixref">
<!ENTITY % glossaryref       "glossaryref">
<!ENTITY % glossdivref       "glossdivref">
<!ENTITY % glossentryref     "glossentryref">
<!ENTITY % bibliographyref   "bibliographyref">
<!ENTITY % bibliodivref      "bibliodivref">
<!ENTITY % biblioentryref    "biblioentryref">
<!ENTITY % colophonref       "colophonref">

<!ELEMENT docbookMap ((%topicmeta;)?, ((%sethead;|%bookhead;) | (%prefaceref;|%chapterref;|%referencehead;|%parthead;|%appendixref;|%bibliographyref;|%glossaryref;|%topicref;)+))>
<!ATTLIST docbookMap      id        ID    #IMPLIED
                          anchorref CDATA #IMPLIED
                          %topicref-atts;
                          %select-atts;
                          %arch-atts;
                          domains    CDATA "&included-domains;"
>

<!ELEMENT sethead ((%topicmeta;)?, (%sethead;|%bookhead;)*)>
<!ATTLIST sethead
  navtitle     CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'set'
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT bookhead ((%topicmeta;)?, (%dedicationref;|%prefaceref;|%chapterref;|%referencehead;|%parthead;|%appendixref;|%bibliographyref;|%glossaryref;|%colophonref;|%topicref;)*)>
<!ATTLIST bookhead
  navtitle     CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'book'
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT dedicationref ((%topicmeta;)?)>
<!ATTLIST dedicationref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'dedication'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT prefaceref ((%topicmeta;)?, ((%sectionref;|%topicref;)*|(%refentryref;|%topicref;)*), (%bibliographyref;|%glossaryref;|%topicref;)*)>
<!ATTLIST prefaceref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'preface'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT chapterref ((%topicmeta;)?, ((%sectionref;|%topicref;)*|(%refentryref;|%topicref;)*), (%bibliographyref;|%glossaryref;|%topicref;)*)>
<!ATTLIST chapterref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'chapter'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT sectionref ((%topicmeta;)?, ((%sectionref;|%topicref;)*|(%refentryref;|%topicref;)*), (%bibliographyref;|%glossaryref;|%topicref;)*)>
<!ATTLIST sectionref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'chapter'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT referencehead ((%topicmeta;)?, (%partintroref;|%topicref;)?, (%refentryref;|%topicref;)+)>
<!ATTLIST referencehead
  navtitle     CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'reference'
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT refentryref ((%topicmeta;)?, (%refsynopsisdivref;|%topicref;)?, (%refsectionref;|%topicref;)+)>
<!ATTLIST refentryref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'refentry'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!-- if refsynopsisdiv ever allows refsection instead of refsect2, 
     add refsectionref -->
<!ELEMENT refsynopsisdivref ((%topicmeta;)?, (%topicref;)*)>
<!ATTLIST refsynopsisdivref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'refsynopsisdiv'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT refsectionref ((%topicmeta;)?, (%refsectionref;|%topicref;)*)>
<!ATTLIST refsectionref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'refsection'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT parthead ((%topicmeta;)?, (%partintroref;|%topicref;)?, (%appendixref;|%chapterref;|%glossaryref;|%bibliographyref;|%prefaceref;|%refentryref;|%referencehead;|%topicref;)+)>
<!ATTLIST parthead
  navtitle     CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'part'
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT partintroref ((%topicmeta;)?, ((%sectionref;|%topicref;)*|(%refentryref;|%topicref;)*))>
<!ATTLIST partintroref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'partintro'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT appendixref ((%topicmeta;)?, ((%sectionref;|%topicref;)*|(%refentryref;|%topicref;)*), (%bibliographyref;|%glossaryref;|%topicref;)*)>
<!ATTLIST appendixref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'appendix'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT glossaryref ((%topicmeta;)?, ((%glossdivref;|%topicref;)+ | (%glossentryref;|%topicref;)+), (%bibliographyref;|%topicref;)?)>
<!ATTLIST glossaryref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'glossary'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT glossdivref ((%topicmeta;)?, (%glossentryref;|%topicref;)+)>
<!ATTLIST glossdivref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'glossdiv'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT glossentryref ((%topicmeta;)?)>
<!ATTLIST glossentryref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'glossentry'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT bibliographyref ((%topicmeta;)?, ((%bibliodivref;|%topicref;)+ | (%biblioentryref;|%topicref;)+))>
<!ATTLIST bibliographyref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'bibliography'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT bibliodivref ((%topicmeta;)?, (%biblioentryref;|%topicref;)+)>
<!ATTLIST bibliodivref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'bibliodiv'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT biblioentryref ((%topicmeta;)?)>
<!ATTLIST biblioentryref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'biblioentry'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ELEMENT colophonref ((%topicmeta;)?)>
<!ATTLIST colophonref
  navtitle     CDATA     #IMPLIED
  href         CDATA     #IMPLIED
  keyref       CDATA     #IMPLIED
  query        CDATA     #IMPLIED
  format       CDATA     'docbook'
  type         CDATA     'colophon'
  locktitle    (yes|no)  #IMPLIED
  %id-atts;
  %select-atts;
  translate    (yes | no) #IMPLIED
  xml:lang     NMTOKEN    #IMPLIED
>

<!ATTLIST docbookMap %global-atts;
    class CDATA "- map/map docbookMap/docbookMap ">
<!ATTLIST sethead %global-atts;
    class CDATA "- map/topicref docbookMap/sethead ">
<!ATTLIST bookhead %global-atts;
    class CDATA "- map/topicref docbookMap/bookhead ">
<!ATTLIST dedicationref %global-atts;
    class CDATA "- map/topicref docbookMap/dedicationref ">
<!ATTLIST prefaceref %global-atts;
    class CDATA "- map/topicref docbookMap/prefaceref ">
<!ATTLIST chapterref %global-atts;
    class CDATA "- map/topicref docbookMap/chapterref ">
<!ATTLIST sectionref %global-atts;
    class CDATA "- map/topicref docbookMap/sectionref ">
<!ATTLIST referencehead %global-atts;
    class CDATA "- map/topicref docbookMap/referencehead ">
<!ATTLIST refentryref %global-atts;
    class CDATA "- map/topicref docbookMap/refentryref ">
<!ATTLIST refsynopsisdivref %global-atts;
    class CDATA "- map/topicref docbookMap/refsynopsisdivref ">
<!ATTLIST refsectionref %global-atts;
    class CDATA "- map/topicref docbookMap/refsectionref ">
<!ATTLIST parthead %global-atts;
    class CDATA "- map/topicref docbookMap/parthead ">
<!ATTLIST partintroref %global-atts;
    class CDATA "- map/topicref docbookMap/partintroref ">
<!ATTLIST appendixref %global-atts;
    class CDATA "- map/topicref docbookMap/appendixref ">
<!ATTLIST bibliographyref %global-atts;
    class CDATA "- map/topicref docbookMap/bibliographyref ">
<!ATTLIST bibliodivref %global-atts;
    class CDATA "- map/topicref docbookMap/bibliodivref ">
<!ATTLIST biblioentryref %global-atts;
    class CDATA "- map/topicref docbookMap/biblioentryref ">
<!ATTLIST glossaryref %global-atts;
    class CDATA "- map/topicref docbookMap/glossaryref ">
<!ATTLIST glossdivref %global-atts;
    class CDATA "- map/topicref docbookMap/glossdivref ">
<!ATTLIST glossentryref %global-atts;
    class CDATA "- map/topicref docbookMap/glossentryref ">
<!ATTLIST colophonref %global-atts;
    class CDATA "- map/topicref docbookMap/colophonref ">
