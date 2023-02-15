<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<!ENTITY % articleref  "articleref">

<!ELEMENT articleref ((%topicmeta;)?, (%topicref;)*)>

<!ATTLIST articleref
             navtitle   CDATA                             #IMPLIED
             href       CDATA                             #IMPLIED
             keyref     CDATA                             #IMPLIED
             query      CDATA                             #IMPLIED
             copy-to    CDATA                             #IMPLIED

             format     CDATA     'docbook'
             type       CDATA     'article'
             collection-type (choice | unordered | sequence | family)
                                                          #IMPLIED
             scope      (local | peer | external)         #IMPLIED
             locktitle  (yes|no)                          #IMPLIED
             linking    (targetonly | sourceonly | normal | none)
                                                          #IMPLIED
             toc        (yes | no)                        #IMPLIED
             print      (yes | no)                        #IMPLIED
             search     (yes | no)                        #IMPLIED
             chunk      CDATA                             #IMPLIED

             %id-atts;
             %select-atts;
             translate  (yes | no)                        #IMPLIED
             xml:lang   NMTOKEN                           #IMPLIED   >

<!ATTLIST articleref %global-atts;
    class CDATA "+ map/topicref articleDomain/articleref ">
