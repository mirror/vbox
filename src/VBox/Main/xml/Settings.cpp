/** @file
 * Settings File Manipulation API.
 *
 * Two classes, MainConfigFile and MachineConfigFile, represent the VirtualBox.xml and
 * machine XML files. They share a common ancestor class, ConfigFileBase, which shares
 * functionality such as talking to the XML back-end classes and settings version management.
 *
 * The code can read all VirtualBox settings files version 1.3 and higher. That version was
 * written by VirtualBox 2.0. It can write settings version 1.7 (used by VirtualBox 2.2 and
 * 3.0) and 1.9 (used by VirtualBox 3.1).
 *
 * Rules for introducing new settings: If an element or attribute is introduced that was not
 * present before VirtualBox 3.1, then settings version checks need to be introduced. The
 * settings version for VirtualBox 3.1 is 1.9; see the SettingsVersion enumeration in
 * src/VBox/Main/idl/VirtualBox.xidl for details about which version was used when.
 *
 * The settings versions checks are necessary because VirtualBox 3.1 no longer automatically
 * converts XML settings files but only if necessary, that is, if settings are present that
 * the old format does not support. If we write an element or attribute to a settings file
 * of an older version, then an old VirtualBox (before 3.1) will attempt to validate it
 * with XML schema, and that will certainly fail.
 *
 * So, to introduce a new setting:
 *
 *   1) Make sure the constructor of corresponding settings structure has a proper default.
 *
 *   2) In the settings reader method, try to read the setting; if it's there, great, if not,
 *      the default value will have been set by the constructor.
 *
 *   3) In the settings writer method, write the setting _only_ if the current settings
 *      version (stored in m->sv) is high enough. That is, for VirtualBox 3.1, write it
 *      only if (m->sv >= SettingsVersion_v1_9).
 *
 *   4) In MachineConfigFile::bumpSettingsVersionIfNeeded(), check if the new setting has
 *      a non-default value (i.e. that differs from the constructor). If so, bump the
 *      settings version to the current version so the settings writer (3) can write out
 *      the non-default value properly.
 *
 *      So far a corresponding method for MainConfigFile has not been necessary since there
 *      have been no incompatible changes yet.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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
 */

#include "VBox/com/string.h"
#include "VBox/settings.h"
#include <iprt/xml_cpp.h>
#include <iprt/stream.h>
#include <iprt/ctype.h>
#include <iprt/file.h>

// generated header
#include "SchemaDefs.h"

#include "Logging.h"

using namespace com;
using namespace settings;

////////////////////////////////////////////////////////////////////////////////
//
// Defines
//
////////////////////////////////////////////////////////////////////////////////

/** VirtualBox XML settings namespace */
#define VBOX_XML_NAMESPACE      "http://www.innotek.de/VirtualBox-settings"

/** VirtualBox XML settings version number substring ("x.y")  */
#define VBOX_XML_VERSION        "1.9"

/** VirtualBox XML settings version platform substring */
#if defined (RT_OS_DARWIN)
#   define VBOX_XML_PLATFORM     "macosx"
#elif defined (RT_OS_FREEBSD)
#   define VBOX_XML_PLATFORM     "freebsd"
#elif defined (RT_OS_LINUX)
#   define VBOX_XML_PLATFORM     "linux"
#elif defined (RT_OS_NETBSD)
#   define VBOX_XML_PLATFORM     "netbsd"
#elif defined (RT_OS_OPENBSD)
#   define VBOX_XML_PLATFORM     "openbsd"
#elif defined (RT_OS_OS2)
#   define VBOX_XML_PLATFORM     "os2"
#elif defined (RT_OS_SOLARIS)
#   define VBOX_XML_PLATFORM     "solaris"
#elif defined (RT_OS_WINDOWS)
#   define VBOX_XML_PLATFORM     "windows"
#else
#   error Unsupported platform!
#endif

/** VirtualBox XML settings full version string ("x.y-platform") */
#define VBOX_XML_VERSION_FULL   VBOX_XML_VERSION "-" VBOX_XML_PLATFORM

////////////////////////////////////////////////////////////////////////////////
//
// Internal data
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Opaque data structore for ConfigFileBase (only declared
 * in header, defined only here).
 */

struct ConfigFileBase::Data
{
    Data()
        : pParser(NULL),
          pDoc(NULL),
          pelmRoot(NULL),
          sv(SettingsVersion_Null),
          svRead(SettingsVersion_Null)
    {}

    ~Data()
    {
        cleanup();
    }

    iprt::MiniString        strFilename;
    bool                    fFileExists;

    xml::XmlFileParser      *pParser;
    xml::Document           *pDoc;
    xml::ElementNode        *pelmRoot;

    com::Utf8Str            strSettingsVersionFull;     // e.g. "1.7-linux"
    SettingsVersion_T       sv;                         // e.g. SettingsVersion_v1_7

    SettingsVersion_T       svRead;                     // settings version that the original file had when it was read,
                                                        // or SettingsVersion_Null if none

    void cleanup()
    {
        if (pDoc)
        {
            delete pDoc;
            pDoc = NULL;
            pelmRoot = NULL;
        }

        if (pParser)
        {
            delete pParser;
            pParser = NULL;
        }
    }
};

/**
 * Private exception class (not in the header file) that makes
 * throwing xml::LogicError instances easier. That class is public
 * and should be caught by client code.
 */
class settings::ConfigFileError : public xml::LogicError
{
public:
    ConfigFileError(const ConfigFileBase *file,
                    const xml::Node *pNode,
                    const char *pcszFormat, ...)
        : xml::LogicError()
    {
        va_list args;
        va_start(args, pcszFormat);
        Utf8StrFmtVA what(pcszFormat, args);
        va_end(args);

        Utf8Str strLine;
        if (pNode)
            strLine = Utf8StrFmt(" (line %RU32)", pNode->getLineNumber());

        const char *pcsz = strLine.c_str();
        Utf8StrFmt str(N_("Error in %s%s -- %s"),
                       file->m->strFilename.c_str(),
                       (pcsz) ? pcsz : "",
                       what.c_str());

        setWhat(str.c_str());
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// ConfigFileBase
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Constructor. Allocates the XML internals.
 * @param strFilename
 */
ConfigFileBase::ConfigFileBase(const com::Utf8Str *pstrFilename)
    : m(new Data)
{
    Utf8Str strMajor;
    Utf8Str strMinor;

    m->fFileExists = false;

    if (pstrFilename)
    {
        m->strFilename = *pstrFilename;

        m->pParser = new xml::XmlFileParser;
        m->pDoc = new xml::Document;
        m->pParser->read(*pstrFilename,
                        *m->pDoc);

        m->fFileExists = true;

        m->pelmRoot = m->pDoc->getRootElement();
        if (!m->pelmRoot || !m->pelmRoot->nameEquals("VirtualBox"))
            throw ConfigFileError(this, NULL, N_("Root element in VirtualBox settings files must be \"VirtualBox\"."));

        if (!(m->pelmRoot->getAttributeValue("version", m->strSettingsVersionFull)))
            throw ConfigFileError(this, m->pelmRoot, N_("Required VirtualBox/@version attribute is missing"));

        LogRel(("Loading settings file \"%s\" with version \"%s\"\n", m->strFilename.c_str(), m->strSettingsVersionFull.c_str()));

        // parse settings version; allow future versions but fail if file is older than 1.6
        m->sv = SettingsVersion_Null;
        if (m->strSettingsVersionFull.length() > 3)
        {
            const char *pcsz = m->strSettingsVersionFull.c_str();
            char c;

            while (    (c = *pcsz)
                    && RT_C_IS_DIGIT(c)
                  )
            {
                strMajor.append(c);
                ++pcsz;
            }

            if (*pcsz++ == '.')
            {
                while (    (c = *pcsz)
                        && RT_C_IS_DIGIT(c)
                      )
                {
                    strMinor.append(c);
                    ++pcsz;
                }
            }

            uint32_t ulMajor = RTStrToUInt32(strMajor.c_str());
            uint32_t ulMinor = RTStrToUInt32(strMinor.c_str());

            if (ulMajor == 1)
            {
                if (ulMinor == 3)
                    m->sv = SettingsVersion_v1_3;
                else if (ulMinor == 4)
                    m->sv = SettingsVersion_v1_4;
                else if (ulMinor == 5)
                    m->sv = SettingsVersion_v1_5;
                else if (ulMinor == 6)
                    m->sv = SettingsVersion_v1_6;
                else if (ulMinor == 7)
                    m->sv = SettingsVersion_v1_7;
                else if (ulMinor == 8)
                    m->sv = SettingsVersion_v1_8;
                else if (ulMinor == 9)
                    m->sv = SettingsVersion_v1_9;
                else if (ulMinor > 9)
                    m->sv = SettingsVersion_Future;
            }
            else if (ulMajor > 1)
                m->sv = SettingsVersion_Future;

            LogRel(("Parsed settings version %d.%d to enum value %d\n", ulMajor, ulMinor, m->sv));
        }

        if (m->sv == SettingsVersion_Null)
            throw ConfigFileError(this, m->pelmRoot, N_("Cannot handle settings version '%s'"), m->strSettingsVersionFull.c_str());

        // remember the settings version we read in case it gets upgraded later,
        // so we know when to make backups
        m->svRead = m->sv;
    }
    else
    {
        m->strSettingsVersionFull = VBOX_XML_VERSION_FULL;
        m->sv = SettingsVersion_v1_9;
    }
}

/**
 * Clean up.
 */
ConfigFileBase::~ConfigFileBase()
{
    if (m)
    {
        delete m;
        m = NULL;
    }
}

/**
 * Helper function that parses a UUID in string form into
 * a com::Guid item. Since that uses an IPRT function which
 * does not accept "{}" characters around the UUID string,
 * we handle that here. Throws on errors.
 * @param guid
 * @param strUUID
 */
void ConfigFileBase::parseUUID(Guid &guid,
                               const Utf8Str &strUUID) const
{
    // {5f102a55-a51b-48e3-b45a-b28d33469488}
    // 01234567890123456789012345678901234567
    //           1         2         3
    if (    (strUUID[0] == '{')
         && (strUUID[37] == '}')
       )
        guid = strUUID.substr(1, 36).c_str();
    else
        guid = strUUID.c_str();

    if (guid.isEmpty())
        throw ConfigFileError(this, NULL, N_("UUID \"%s\" has invalid format"), strUUID.c_str());
}

/**
 * Parses the given string in str and attempts to treat it as an ISO
 * date/time stamp to put into timestamp. Throws on errors.
 * @param timestamp
 * @param str
 */
void ConfigFileBase::parseTimestamp(RTTIMESPEC &timestamp,
                                    const com::Utf8Str &str) const
{
    const char *pcsz = str.c_str();
        //  yyyy-mm-ddThh:mm:ss
        // "2009-07-10T11:54:03Z"
        //  01234567890123456789
        //            1
    if (str.length() > 19)
    {
        // timezone must either be unspecified or 'Z' for UTC
        if (    (pcsz[19])
             && (pcsz[19] != 'Z')
           )
            throw ConfigFileError(this, NULL, N_("Cannot handle ISO timestamp '%s': is not UTC date"), str.c_str());

        int32_t yyyy;
        uint32_t mm, dd, hh, min, secs;
        if (    (pcsz[4] == '-')
             && (pcsz[7] == '-')
             && (pcsz[10] == 'T')
             && (pcsz[13] == ':')
             && (pcsz[16] == ':')
           )
        {
            int rc;
            if (    (RT_SUCCESS(rc = RTStrToInt32Ex(pcsz, NULL, 0, &yyyy)))
                        // could theoretically be negative but let's assume that nobody
                        // created virtual machines before the Christian era
                 && (RT_SUCCESS(rc = RTStrToUInt32Ex(pcsz + 5, NULL, 0, &mm)))
                 && (RT_SUCCESS(rc = RTStrToUInt32Ex(pcsz + 8, NULL, 0, &dd)))
                 && (RT_SUCCESS(rc = RTStrToUInt32Ex(pcsz + 11, NULL, 0, &hh)))
                 && (RT_SUCCESS(rc = RTStrToUInt32Ex(pcsz + 14, NULL, 0, &min)))
                 && (RT_SUCCESS(rc = RTStrToUInt32Ex(pcsz + 17, NULL, 0, &secs)))
               )
            {
                RTTIME time = { yyyy,
                                (uint8_t)mm,
                                0,
                                0,
                                (uint8_t)dd,
                                (uint8_t)hh,
                                (uint8_t)min,
                                (uint8_t)secs,
                                0,
                                RTTIME_FLAGS_TYPE_UTC };
                if (RTTimeNormalize(&time))
                    if (RTTimeImplode(&timestamp, &time))
                        return;
            }

            throw ConfigFileError(this, NULL, N_("Cannot parse ISO timestamp '%s': runtime error, %Rra"), str.c_str(), rc);
        }

        throw ConfigFileError(this, NULL, N_("Cannot parse ISO timestamp '%s': invalid format"), str.c_str());
    }
}

/**
 * Helper to create a string for a RTTIMESPEC for writing out ISO timestamps.
 * @param stamp
 * @return
 */
com::Utf8Str ConfigFileBase::makeString(const RTTIMESPEC &stamp)
{
    RTTIME time;
    if (!RTTimeExplode(&time, &stamp))
        throw ConfigFileError(this, NULL, N_("Timespec %lld ms is invalid"), RTTimeSpecGetMilli(&stamp));

    return Utf8StrFmt("%04ld-%02hd-%02hdT%02hd:%02hd:%02hdZ",
                      time.i32Year,
                      (uint16_t)time.u8Month,
                      (uint16_t)time.u8MonthDay,
                      (uint16_t)time.u8Hour,
                      (uint16_t)time.u8Minute,
                      (uint16_t)time.u8Second);
}

/**
 * Helper to create a string for a GUID.
 * @param guid
 * @return
 */
com::Utf8Str ConfigFileBase::makeString(const Guid &guid)
{
    Utf8Str str("{");
    str.append(guid.toString());
    str.append("}");
    return str;
}

/**
 * Helper method to read in an ExtraData subtree and stores its contents
 * in the given map of extradata items. Used for both main and machine
 * extradata (MainConfigFile and MachineConfigFile).
 * @param elmExtraData
 * @param map
 */
void ConfigFileBase::readExtraData(const xml::ElementNode &elmExtraData,
                                   ExtraDataItemsMap &map)
{
    xml::NodesLoop nlLevel4(elmExtraData);
    const xml::ElementNode *pelmExtraDataItem;
    while ((pelmExtraDataItem = nlLevel4.forAllNodes()))
    {
        if (pelmExtraDataItem->nameEquals("ExtraDataItem"))
        {
            // <ExtraDataItem name="GUI/LastWindowPostion" value="97,88,981,858"/>
            Utf8Str strName, strValue;
            if (    ((pelmExtraDataItem->getAttributeValue("name", strName)))
                 && ((pelmExtraDataItem->getAttributeValue("value", strValue)))
               )
                map[strName] = strValue;
            else
                throw ConfigFileError(this, pelmExtraDataItem, N_("Required ExtraDataItem/@name or @value attribute is missing"));
        }
    }
}

/**
 * Reads <USBDeviceFilter> entries from under the given elmDeviceFilters node and
 * stores them in the given linklist. This is in ConfigFileBase because it's used
 * from both MainConfigFile (for host filters) and MachineConfigFile (for machine
 * filters).
 * @param elmDeviceFilters
 * @param ll
 */
void ConfigFileBase::readUSBDeviceFilters(const xml::ElementNode &elmDeviceFilters,
                                          USBDeviceFiltersList &ll)
{
    xml::NodesLoop nl1(elmDeviceFilters, "DeviceFilter");
    const xml::ElementNode *pelmLevel4Child;
    while ((pelmLevel4Child = nl1.forAllNodes()))
    {
        USBDeviceFilter flt;
        flt.action = USBDeviceFilterAction_Ignore;
        Utf8Str strAction;
        if (    (pelmLevel4Child->getAttributeValue("name", flt.strName))
             && (pelmLevel4Child->getAttributeValue("active", flt.fActive))
           )
        {
            if (!pelmLevel4Child->getAttributeValue("vendorId", flt.strVendorId))
                pelmLevel4Child->getAttributeValue("vendorid", flt.strVendorId);            // used before 1.3
            if (!pelmLevel4Child->getAttributeValue("productId", flt.strProductId))
                pelmLevel4Child->getAttributeValue("productid", flt.strProductId);          // used before 1.3
            pelmLevel4Child->getAttributeValue("revision", flt.strRevision);
            pelmLevel4Child->getAttributeValue("manufacturer", flt.strManufacturer);
            pelmLevel4Child->getAttributeValue("product", flt.strProduct);
            if (!pelmLevel4Child->getAttributeValue("serialNumber", flt.strSerialNumber))
                pelmLevel4Child->getAttributeValue("serialnumber", flt.strSerialNumber);    // used before 1.3
            pelmLevel4Child->getAttributeValue("port", flt.strPort);

            // the next 2 are irrelevant for host USB objects
            pelmLevel4Child->getAttributeValue("remote", flt.strRemote);
            pelmLevel4Child->getAttributeValue("maskedInterfaces", flt.ulMaskedInterfaces);

            // action is only used with host USB objects
            if (pelmLevel4Child->getAttributeValue("action", strAction))
            {
                if (strAction == "Ignore")
                    flt.action = USBDeviceFilterAction_Ignore;
                else if (strAction == "Hold")
                    flt.action = USBDeviceFilterAction_Hold;
                else
                    throw ConfigFileError(this, pelmLevel4Child, N_("Invalid value '%s' in DeviceFilter/@action attribute"), strAction.c_str());
            }

            ll.push_back(flt);
        }
    }
}

/**
 * Creates a new stub xml::Document in the m->pDoc member with the
 * root "VirtualBox" element set up. This is used by both
 * MainConfigFile and MachineConfigFile at the beginning of writing
 * out their XML.
 *
 * Before calling this, it is the responsibility of the caller to
 * set the "sv" member to the required settings version that is to
 * be written. For newly created files, the settings version will be
 * the latest (1.9); for files read in from disk earlier, it will be
 * the settings version indicated in the file. However, this method
 * will silently make sure that the settings version is always
 * at least 1.7 and change it if necessary, since there is no write
 * support for earlier settings versions.
 */
void ConfigFileBase::createStubDocument()
{
    Assert(m->pDoc == NULL);
    m->pDoc = new xml::Document;

    m->pelmRoot = m->pDoc->createRootElement("VirtualBox");
    m->pelmRoot->setAttribute("xmlns", VBOX_XML_NAMESPACE);

    const char *pcszVersion = NULL;
    switch (m->sv)
    {
        case SettingsVersion_v1_8:
            pcszVersion = "1.8";
        break;

        case SettingsVersion_v1_9:
        case SettingsVersion_Future:                // can be set if this code runs on XML files that were created by a future version of VBox;
                                                    // in that case, downgrade to current version when writing since we can't write future versions...
            pcszVersion = "1.9";
            m->sv = SettingsVersion_v1_9;
        break;

        default:
            // silently upgrade if this is less than 1.7 because that's the oldest we can write
            pcszVersion = "1.7";
            m->sv = SettingsVersion_v1_7;
        break;
    }

    m->pelmRoot->setAttribute("version", Utf8StrFmt("%s-%s",
                                                    pcszVersion,
                                                    VBOX_XML_PLATFORM));       // e.g. "linux"

    // since this gets called before the XML document is actually written out
    // do this, this is where we must check whether we're upgrading the settings
    // version and need to make a backup, so the user can go back to an earlier
    // VirtualBox version and recover his old settings files.
    if (    (m->svRead != SettingsVersion_Null)     // old file exists?
         && (m->svRead < m->sv)                     // we're upgrading?
       )
    {
        // compose new filename: strip off trailing ".xml"
        Utf8Str strFilenameNew = m->strFilename.substr(0, m->strFilename.length() - 4);
        // and append something likd "-1.3-linux.xml"
        strFilenameNew.append("-");
        strFilenameNew.append(m->strSettingsVersionFull);       // e.g. "1.3-linux"
        strFilenameNew.append(".xml");

        RTFileMove(m->strFilename.c_str(),
                   strFilenameNew.c_str(),
                   0);      // no RTFILEMOVE_FLAGS_REPLACE

        // do this only once
        m->svRead = SettingsVersion_Null;
    }
}

/**
 * Creates an <ExtraData> node under the given parent element with
 * <ExtraDataItem> childern according to the contents of the given
 * map.
 * This is in ConfigFileBase because it's used in both MainConfigFile
 * MachineConfigFile, which both can have extradata.
 *
 * @param elmParent
 * @param me
 */
void ConfigFileBase::writeExtraData(xml::ElementNode &elmParent,
                                    const ExtraDataItemsMap &me)
{
    if (me.size())
    {
        xml::ElementNode *pelmExtraData = elmParent.createChild("ExtraData");
        for (ExtraDataItemsMap::const_iterator it = me.begin();
             it != me.end();
             ++it)
        {
            const Utf8Str &strName = it->first;
            const Utf8Str &strValue = it->second;
            xml::ElementNode *pelmThis = pelmExtraData->createChild("ExtraDataItem");
            pelmThis->setAttribute("name", strName);
            pelmThis->setAttribute("value", strValue);
        }
    }
}

/**
 * Creates <DeviceFilter> nodes under the given parent element according to
 * the contents of the given USBDeviceFiltersList. This is in ConfigFileBase
 * because it's used in both MainConfigFile (for host filters) and
 * MachineConfigFile (for machine filters).
 *
 * If fHostMode is true, this means that we're supposed to write filters
 * for the IHost interface (respect "action", omit "strRemote" and
 * "ulMaskedInterfaces" in struct USBDeviceFilter).
 *
 * @param elmParent
 * @param ll
 * @param fHostMode
 */
void ConfigFileBase::writeUSBDeviceFilters(xml::ElementNode &elmParent,
                                           const USBDeviceFiltersList &ll,
                                           bool fHostMode)
{
    for (USBDeviceFiltersList::const_iterator it = ll.begin();
         it != ll.end();
         ++it)
    {
        const USBDeviceFilter &flt = *it;
        xml::ElementNode *pelmFilter = elmParent.createChild("DeviceFilter");
        pelmFilter->setAttribute("name", flt.strName);
        pelmFilter->setAttribute("active", flt.fActive);
        if (flt.strVendorId.length())
            pelmFilter->setAttribute("vendorId", flt.strVendorId);
        if (flt.strProductId.length())
            pelmFilter->setAttribute("productId", flt.strProductId);
        if (flt.strRevision.length())
            pelmFilter->setAttribute("revision", flt.strRevision);
        if (flt.strManufacturer.length())
            pelmFilter->setAttribute("manufacturer", flt.strManufacturer);
        if (flt.strProduct.length())
            pelmFilter->setAttribute("product", flt.strProduct);
        if (flt.strSerialNumber.length())
            pelmFilter->setAttribute("serialNumber", flt.strSerialNumber);
        if (flt.strPort.length())
            pelmFilter->setAttribute("port", flt.strPort);

        if (fHostMode)
        {
            const char *pcsz =
                (flt.action == USBDeviceFilterAction_Ignore) ? "Ignore"
                : /*(flt.action == USBDeviceFilterAction_Hold) ?*/ "Hold";
            pelmFilter->setAttribute("action", pcsz);
        }
        else
        {
            if (flt.strRemote.length())
                pelmFilter->setAttribute("remote", flt.strRemote);
            if (flt.ulMaskedInterfaces)
                pelmFilter->setAttribute("maskedInterfaces", flt.ulMaskedInterfaces);
        }
    }
}

/**
 * Cleans up memory allocated by the internal XML parser. To be called by
 * descendant classes when they're done analyzing the DOM tree to discard it.
 */
void ConfigFileBase::clearDocument()
{
    m->cleanup();
}

/**
 * Returns true only if the underlying config file exists on disk;
 * either because the file has been loaded from disk, or it's been written
 * to disk, or both.
 * @return
 */
bool ConfigFileBase::fileExists()
{
    return m->fFileExists;
}


////////////////////////////////////////////////////////////////////////////////
//
// MainConfigFile
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Reads one <MachineEntry> from the main VirtualBox.xml file.
 * @param elmMachineRegistry
 */
void MainConfigFile::readMachineRegistry(const xml::ElementNode &elmMachineRegistry)
{
    // <MachineEntry uuid="{ xxx }" src="   xxx "/>
    xml::NodesLoop nl1(elmMachineRegistry);
    const xml::ElementNode *pelmChild1;
    while ((pelmChild1 = nl1.forAllNodes()))
    {
        if (pelmChild1->nameEquals("MachineEntry"))
        {
            MachineRegistryEntry mre;
            Utf8Str strUUID;
            if (    ((pelmChild1->getAttributeValue("uuid", strUUID)))
                 && ((pelmChild1->getAttributeValue("src", mre.strSettingsFile)))
               )
            {
                parseUUID(mre.uuid, strUUID);
                llMachines.push_back(mre);
            }
            else
                throw ConfigFileError(this, pelmChild1, N_("Required MachineEntry/@uuid or @src attribute is missing"));
        }
    }
}

/**
 * Reads a media registry entry from the main VirtualBox.xml file.
 *
 * Whereas the current media registry code is fairly straightforward, it was quite a mess
 * with settings format before 1.4 (VirtualBox 2.0 used settings format 1.3). The elements
 * in the media registry were much more inconsistent, and different elements were used
 * depending on the type of device and image.
 *
 * @param t
 * @param elmMedium
 * @param llMedia
 */
void MainConfigFile::readMedium(MediaType t,
                                const xml::ElementNode &elmMedium,  // HardDisk node if root; if recursing,
                                                                    // child HardDisk node or DiffHardDisk node for pre-1.4
                                MediaList &llMedia)     // list to append medium to (root disk or child list)
{
    // <HardDisk uuid="{5471ecdb-1ddb-4012-a801-6d98e226868b}" location="/mnt/innotek-unix/vdis/Windows XP.vdi" format="VDI" type="Normal">
    settings::Medium med;
    Utf8Str strUUID;
    if (!(elmMedium.getAttributeValue("uuid", strUUID)))
        throw ConfigFileError(this, &elmMedium, N_("Required %s/@uuid attribute is missing"), elmMedium.getName());

    parseUUID(med.uuid, strUUID);

    bool fNeedsLocation = true;

    if (t == HardDisk)
    {
        if (m->sv < SettingsVersion_v1_4)
        {
            // here the system is:
            //         <HardDisk uuid="{....}" type="normal">
            //           <VirtualDiskImage filePath="/path/to/xxx.vdi"/>
            //         </HardDisk>

            fNeedsLocation = false;
            bool fNeedsFilePath = true;
            const xml::ElementNode *pelmImage;
            if ((pelmImage = elmMedium.findChildElement("VirtualDiskImage")))
                med.strFormat = "VDI";
            else if ((pelmImage = elmMedium.findChildElement("VMDKImage")))
                med.strFormat = "VMDK";
            else if ((pelmImage = elmMedium.findChildElement("VHDImage")))
                med.strFormat = "VHD";
            else if ((pelmImage = elmMedium.findChildElement("ISCSIHardDisk")))
            {
                med.strFormat = "iSCSI";

                fNeedsFilePath = false;
                // location is special here: current settings specify an "iscsi://user@server:port/target/lun"
                // string for the location and also have several disk properties for these, whereas this used
                // to be hidden in several sub-elements before 1.4, so compose a location string and set up
                // the properties:
                med.strLocation = "iscsi://";
                Utf8Str strUser, strServer, strPort, strTarget, strLun;
                if (pelmImage->getAttributeValue("userName", strUser))
                {
                    med.strLocation.append(strUser);
                    med.strLocation.append("@");
                }
                Utf8Str strServerAndPort;
                if (pelmImage->getAttributeValue("server", strServer))
                {
                    strServerAndPort = strServer;
                }
                if (pelmImage->getAttributeValue("port", strPort))
                {
                    if (strServerAndPort.length())
                        strServerAndPort.append(":");
                    strServerAndPort.append(strPort);
                }
                med.strLocation.append(strServerAndPort);
                if (pelmImage->getAttributeValue("target", strTarget))
                {
                    med.strLocation.append("/");
                    med.strLocation.append(strTarget);
                }
                if (pelmImage->getAttributeValue("lun", strLun))
                {
                    med.strLocation.append("/");
                    med.strLocation.append(strLun);
                }

                if (strServer.length() && strPort.length())
                    med.properties["TargetAddress"] = strServerAndPort;
                if (strTarget.length())
                    med.properties["TargetName"] = strTarget;
                if (strUser.length())
                    med.properties["InitiatorUsername"] = strUser;
                Utf8Str strPassword;
                if (pelmImage->getAttributeValue("password", strPassword))
                    med.properties["InitiatorSecret"] = strPassword;
                if (strLun.length())
                    med.properties["LUN"] = strLun;
            }
            else if ((pelmImage = elmMedium.findChildElement("CustomHardDisk")))
            {
                fNeedsFilePath = false;
                fNeedsLocation = true;
                    // also requires @format attribute, which will be queried below
            }
            else
                throw ConfigFileError(this, &elmMedium, N_("Required %s/VirtualDiskImage element is missing"), elmMedium.getName());

            if (fNeedsFilePath)
                if (!(pelmImage->getAttributeValue("filePath", med.strLocation)))
                    throw ConfigFileError(this, &elmMedium, N_("Required %s/@filePath attribute is missing"), elmMedium.getName());
        }

        if (med.strFormat.isEmpty())        // not set with 1.4 format above, or 1.4 Custom format?
            if (!(elmMedium.getAttributeValue("format", med.strFormat)))
                throw ConfigFileError(this, &elmMedium, N_("Required %s/@format attribute is missing"), elmMedium.getName());

        if (!(elmMedium.getAttributeValue("autoReset", med.fAutoReset)))
            med.fAutoReset = false;

        Utf8Str strType;
        if ((elmMedium.getAttributeValue("type", strType)))
        {
            // pre-1.4 used lower case, so make this case-insensitive
            strType.toUpper();
            if (strType == "NORMAL")
                med.hdType = MediumType_Normal;
            else if (strType == "IMMUTABLE")
                med.hdType = MediumType_Immutable;
            else if (strType == "WRITETHROUGH")
                med.hdType = MediumType_Writethrough;
            else
                throw ConfigFileError(this, &elmMedium, N_("HardDisk/@type attribute must be one of Normal, Immutable or Writethrough"));
        }
    }
    else if (m->sv < SettingsVersion_v1_4)
    {
        // DVD and floppy images before 1.4 had "src" attribute instead of "location"
        if (!(elmMedium.getAttributeValue("src", med.strLocation)))
            throw ConfigFileError(this, &elmMedium, N_("Required %s/@src attribute is missing"), elmMedium.getName());

        fNeedsLocation = false;
    }

    if (fNeedsLocation)
        // current files and 1.4 CustomHardDisk elements must have a location attribute
        if (!(elmMedium.getAttributeValue("location", med.strLocation)))
            throw ConfigFileError(this, &elmMedium, N_("Required %s/@location attribute is missing"), elmMedium.getName());

    elmMedium.getAttributeValue("Description", med.strDescription);       // optional

    // recurse to handle children
    xml::NodesLoop nl2(elmMedium);
    const xml::ElementNode *pelmHDChild;
    while ((pelmHDChild = nl2.forAllNodes()))
    {
        if (    t == HardDisk
             && (    pelmHDChild->nameEquals("HardDisk")
                  || (    (m->sv < SettingsVersion_v1_4)
                       && (pelmHDChild->nameEquals("DiffHardDisk"))
                     )
                )
           )
            // recurse with this element and push the child onto our current children list
            readMedium(t,
                        *pelmHDChild,
                        med.llChildren);
        else if (pelmHDChild->nameEquals("Property"))
        {
            Utf8Str strPropName, strPropValue;
            if (    (pelmHDChild->getAttributeValue("name", strPropName))
                 && (pelmHDChild->getAttributeValue("value", strPropValue))
               )
                med.properties[strPropName] = strPropValue;
            else
                throw ConfigFileError(this, pelmHDChild, N_("Required HardDisk/Property/@name or @value attribute is missing"));
        }
    }

    llMedia.push_back(med);
}

/**
 * Reads in the entire <MediaRegistry> chunk. For pre-1.4 files, this gets called
 * with the <DiskRegistry> chunk instead.
 * @param elmMediaRegistry
 */
void MainConfigFile::readMediaRegistry(const xml::ElementNode &elmMediaRegistry)
{
    xml::NodesLoop nl1(elmMediaRegistry);
    const xml::ElementNode *pelmChild1;
    while ((pelmChild1 = nl1.forAllNodes()))
    {
        MediaType t = Error;
        if (pelmChild1->nameEquals("HardDisks"))
            t = HardDisk;
        else if (pelmChild1->nameEquals("DVDImages"))
            t = DVDImage;
        else if (pelmChild1->nameEquals("FloppyImages"))
            t = FloppyImage;
        else
            continue;

        xml::NodesLoop nl2(*pelmChild1);
        const xml::ElementNode *pelmMedium;
        while ((pelmMedium = nl2.forAllNodes()))
        {
            if (    t == HardDisk
                 && (pelmMedium->nameEquals("HardDisk"))
               )
                readMedium(t,
                           *pelmMedium,
                           llHardDisks);      // list to append hard disk data to: the root list
            else if (    t == DVDImage
                      && (pelmMedium->nameEquals("Image"))
                    )
                readMedium(t,
                           *pelmMedium,
                           llDvdImages);      // list to append dvd images to: the root list
            else if (    t == FloppyImage
                      && (pelmMedium->nameEquals("Image"))
                    )
                readMedium(t,
                           *pelmMedium,
                           llFloppyImages);      // list to append floppy images to: the root list
        }
    }
}

/**
 * Reads in the <DHCPServers> chunk.
 * @param elmDHCPServers
 */
void MainConfigFile::readDHCPServers(const xml::ElementNode &elmDHCPServers)
{
    xml::NodesLoop nl1(elmDHCPServers);
    const xml::ElementNode *pelmServer;
    while ((pelmServer = nl1.forAllNodes()))
    {
        if (pelmServer->nameEquals("DHCPServer"))
        {
            DHCPServer srv;
            if (    (pelmServer->getAttributeValue("networkName", srv.strNetworkName))
                 && (pelmServer->getAttributeValue("IPAddress", srv.strIPAddress))
                 && (pelmServer->getAttributeValue("networkMask", srv.strIPNetworkMask))
                 && (pelmServer->getAttributeValue("lowerIP", srv.strIPLower))
                 && (pelmServer->getAttributeValue("upperIP", srv.strIPUpper))
                 && (pelmServer->getAttributeValue("enabled", srv.fEnabled))
               )
                llDhcpServers.push_back(srv);
            else
                throw ConfigFileError(this, pelmServer, N_("Required DHCPServer/@networkName, @IPAddress, @networkMask, @lowerIP, @upperIP or @enabled attribute is missing"));
        }
    }
}

/**
 * Constructor.
 *
 * If pstrFilename is != NULL, this reads the given settings file into the member
 * variables and various substructures and lists. Otherwise, the member variables
 * are initialized with default values.
 *
 * Throws variants of xml::Error for I/O, XML and logical content errors, which
 * the caller should catch; if this constructor does not throw, then the member
 * variables contain meaningful values (either from the file or defaults).
 *
 * @param strFilename
 */
MainConfigFile::MainConfigFile(const Utf8Str *pstrFilename)
    : ConfigFileBase(pstrFilename)
{
    if (pstrFilename)
    {
        // the ConfigFileBase constructor has loaded the XML file, so now
        // we need only analyze what is in there
        xml::NodesLoop nlRootChildren(*m->pelmRoot);
        const xml::ElementNode *pelmRootChild;
        while ((pelmRootChild = nlRootChildren.forAllNodes()))
        {
            if (pelmRootChild->nameEquals("Global"))
            {
                xml::NodesLoop nlGlobalChildren(*pelmRootChild);
                const xml::ElementNode *pelmGlobalChild;
                while ((pelmGlobalChild = nlGlobalChildren.forAllNodes()))
                {
                    if (pelmGlobalChild->nameEquals("SystemProperties"))
                    {
                        pelmGlobalChild->getAttributeValue("defaultMachineFolder", systemProperties.strDefaultMachineFolder);
                        if (!pelmGlobalChild->getAttributeValue("defaultHardDiskFolder", systemProperties.strDefaultHardDiskFolder))
                            // pre-1.4 used @defaultVDIFolder instead
                            pelmGlobalChild->getAttributeValue("defaultVDIFolder", systemProperties.strDefaultHardDiskFolder);
                        pelmGlobalChild->getAttributeValue("defaultHardDiskFormat", systemProperties.strDefaultHardDiskFormat);
                        pelmGlobalChild->getAttributeValue("remoteDisplayAuthLibrary", systemProperties.strRemoteDisplayAuthLibrary);
                        pelmGlobalChild->getAttributeValue("webServiceAuthLibrary", systemProperties.strWebServiceAuthLibrary);
                        pelmGlobalChild->getAttributeValue("LogHistoryCount", systemProperties.ulLogHistoryCount);
                    }
                    else if (pelmGlobalChild->nameEquals("ExtraData"))
                        readExtraData(*pelmGlobalChild, mapExtraDataItems);
                    else if (pelmGlobalChild->nameEquals("MachineRegistry"))
                        readMachineRegistry(*pelmGlobalChild);
                    else if (    (pelmGlobalChild->nameEquals("MediaRegistry"))
                              || (    (m->sv < SettingsVersion_v1_4)
                                   && (pelmGlobalChild->nameEquals("DiskRegistry"))
                                 )
                            )
                        readMediaRegistry(*pelmGlobalChild);
                    else if (pelmGlobalChild->nameEquals("NetserviceRegistry"))
                    {
                        xml::NodesLoop nlLevel4(*pelmGlobalChild);
                        const xml::ElementNode *pelmLevel4Child;
                        while ((pelmLevel4Child = nlLevel4.forAllNodes()))
                        {
                            if (pelmLevel4Child->nameEquals("DHCPServers"))
                                readDHCPServers(*pelmLevel4Child);
                        }
                    }
                    else if (pelmGlobalChild->nameEquals("USBDeviceFilters"))
                        readUSBDeviceFilters(*pelmGlobalChild, host.llUSBDeviceFilters);
                }
            } // end if (pelmRootChild->nameEquals("Global"))
        }

        clearDocument();
    }

    // DHCP servers were introduced with settings version 1.7; if we're loading
    // from an older version OR this is a fresh install, then add one DHCP server
    // with default settings
    if (    (!llDhcpServers.size())
         && (    (!pstrFilename)                    // empty VirtualBox.xml file
              || (m->sv < SettingsVersion_v1_7)     // upgrading from before 1.7
            )
       )
    {
        DHCPServer srv;
        srv.strNetworkName =
#ifdef RT_OS_WINDOWS
            "HostInterfaceNetworking-VirtualBox Host-Only Ethernet Adapter";
#else
            "HostInterfaceNetworking-vboxnet0";
#endif
        srv.strIPAddress = "192.168.56.100";
        srv.strIPNetworkMask = "255.255.255.0";
        srv.strIPLower = "192.168.56.101";
        srv.strIPUpper = "192.168.56.254";
        srv.fEnabled = true;
        llDhcpServers.push_back(srv);
    }
}

/**
 * Creates a single <HardDisk> element for the given Medium structure
 * and recurses to write the child hard disks underneath. Called from
 * MainConfigFile::write().
 *
 * @param elmMedium
 * @param m
 * @param level
 */
void MainConfigFile::writeHardDisk(xml::ElementNode &elmMedium,
                                   const Medium &m,
                                   uint32_t level)          // 0 for "root" call, incremented with each recursion
{
    xml::ElementNode *pelmHardDisk = elmMedium.createChild("HardDisk");
    pelmHardDisk->setAttribute("uuid", makeString(m.uuid));
    pelmHardDisk->setAttribute("location", m.strLocation);
    pelmHardDisk->setAttribute("format", m.strFormat);
    if (m.fAutoReset)
        pelmHardDisk->setAttribute("autoReset", m.fAutoReset);
    if (m.strDescription.length())
        pelmHardDisk->setAttribute("Description", m.strDescription);

    for (PropertiesMap::const_iterator it = m.properties.begin();
         it != m.properties.end();
         ++it)
    {
        xml::ElementNode *pelmProp = pelmHardDisk->createChild("Property");
        pelmProp->setAttribute("name", it->first);
        pelmProp->setAttribute("value", it->second);
    }

    // only for base hard disks, save the type
    if (level == 0)
    {
        const char *pcszType =
            m.hdType == MediumType_Normal ? "Normal" :
            m.hdType == MediumType_Immutable ? "Immutable" :
            /*m.hdType == MediumType_Writethrough ?*/ "Writethrough";
        pelmHardDisk->setAttribute("type", pcszType);
    }

    for (MediaList::const_iterator it = m.llChildren.begin();
         it != m.llChildren.end();
         ++it)
    {
        // recurse for children
        writeHardDisk(*pelmHardDisk, // parent
                      *it,           // settings::Medium
                      ++level);      // recursion level
    }
}

/**
 * Called from the IVirtualBox interface to write out VirtualBox.xml. This
 * builds an XML DOM tree and writes it out to disk.
 */
void MainConfigFile::write(const com::Utf8Str strFilename)
{
    m->strFilename = strFilename;
    createStubDocument();

    xml::ElementNode *pelmGlobal = m->pelmRoot->createChild("Global");

    writeExtraData(*pelmGlobal, mapExtraDataItems);

    xml::ElementNode *pelmMachineRegistry = pelmGlobal->createChild("MachineRegistry");
    for (MachinesRegistry::const_iterator it = llMachines.begin();
         it != llMachines.end();
         ++it)
    {
        // <MachineEntry uuid="{5f102a55-a51b-48e3-b45a-b28d33469488}" src="/mnt/innotek-unix/vbox-machines/Windows 5.1 XP 1 (Office 2003)/Windows 5.1 XP 1 (Office 2003).xml"/>
        const MachineRegistryEntry &mre = *it;
        xml::ElementNode *pelmMachineEntry = pelmMachineRegistry->createChild("MachineEntry");
        pelmMachineEntry->setAttribute("uuid", makeString(mre.uuid));
        pelmMachineEntry->setAttribute("src", mre.strSettingsFile);
    }

    xml::ElementNode *pelmMediaRegistry = pelmGlobal->createChild("MediaRegistry");

    xml::ElementNode *pelmHardDisks = pelmMediaRegistry->createChild("HardDisks");
    for (MediaList::const_iterator it = llHardDisks.begin();
         it != llHardDisks.end();
         ++it)
    {
        writeHardDisk(*pelmHardDisks, *it, 0);
    }

    xml::ElementNode *pelmDVDImages = pelmMediaRegistry->createChild("DVDImages");
    for (MediaList::const_iterator it = llDvdImages.begin();
         it != llDvdImages.end();
         ++it)
    {
        const Medium &m = *it;
        xml::ElementNode *pelmMedium = pelmDVDImages->createChild("Image");
        pelmMedium->setAttribute("uuid", makeString(m.uuid));
        pelmMedium->setAttribute("location", m.strLocation);
        if (m.strDescription.length())
            pelmMedium->setAttribute("Description", m.strDescription);
    }

    xml::ElementNode *pelmFloppyImages = pelmMediaRegistry->createChild("FloppyImages");
    for (MediaList::const_iterator it = llFloppyImages.begin();
         it != llFloppyImages.end();
         ++it)
    {
        const Medium &m = *it;
        xml::ElementNode *pelmMedium = pelmFloppyImages->createChild("Image");
        pelmMedium->setAttribute("uuid", makeString(m.uuid));
        pelmMedium->setAttribute("location", m.strLocation);
        if (m.strDescription.length())
            pelmMedium->setAttribute("Description", m.strDescription);
    }

    xml::ElementNode *pelmNetserviceRegistry = pelmGlobal->createChild("NetserviceRegistry");
    xml::ElementNode *pelmDHCPServers = pelmNetserviceRegistry->createChild("DHCPServers");
    for (DHCPServersList::const_iterator it = llDhcpServers.begin();
         it != llDhcpServers.end();
         ++it)
    {
        const DHCPServer &d = *it;
        xml::ElementNode *pelmThis = pelmDHCPServers->createChild("DHCPServer");
        pelmThis->setAttribute("networkName", d.strNetworkName);
        pelmThis->setAttribute("IPAddress", d.strIPAddress);
        pelmThis->setAttribute("networkMask", d.strIPNetworkMask);
        pelmThis->setAttribute("lowerIP", d.strIPLower);
        pelmThis->setAttribute("upperIP", d.strIPUpper);
        pelmThis->setAttribute("enabled", (d.fEnabled) ? 1 : 0);        // too bad we chose 1 vs. 0 here
    }

    xml::ElementNode *pelmSysProps = pelmGlobal->createChild("SystemProperties");
    if (systemProperties.strDefaultMachineFolder.length())
        pelmSysProps->setAttribute("defaultMachineFolder", systemProperties.strDefaultMachineFolder);
    if (systemProperties.strDefaultHardDiskFolder.length())
        pelmSysProps->setAttribute("defaultHardDiskFolder", systemProperties.strDefaultHardDiskFolder);
    if (systemProperties.strDefaultHardDiskFormat.length())
        pelmSysProps->setAttribute("defaultHardDiskFormat", systemProperties.strDefaultHardDiskFormat);
    if (systemProperties.strRemoteDisplayAuthLibrary.length())
        pelmSysProps->setAttribute("remoteDisplayAuthLibrary", systemProperties.strRemoteDisplayAuthLibrary);
    if (systemProperties.strWebServiceAuthLibrary.length())
        pelmSysProps->setAttribute("webServiceAuthLibrary", systemProperties.strWebServiceAuthLibrary);
    pelmSysProps->setAttribute("LogHistoryCount", systemProperties.ulLogHistoryCount);

    writeUSBDeviceFilters(*pelmGlobal->createChild("USBDeviceFilters"),
                          host.llUSBDeviceFilters,
                          true);               // fHostMode

    // now go write the XML
    xml::XmlFileWriter writer(*m->pDoc);
    writer.write(m->strFilename.c_str());

    m->fFileExists = true;

    clearDocument();
}

// use a define for the platform-dependent default value of
// hwvirt exclusivity, since we'll need to check that value
// in bumpSettingsVersionIfNeeded()
#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
    #define HWVIRTEXCLUSIVEDEFAULT false
#else
    #define HWVIRTEXCLUSIVEDEFAULT true
#endif

/**
 * Hardware struct constructor.
 */
Hardware::Hardware()
        : strVersion("1"),
          fHardwareVirt(true),
          fHardwareVirtExclusive(HWVIRTEXCLUSIVEDEFAULT),
          fNestedPaging(false),
          fVPID(false),
          fSyntheticCpu(false),
          fPAE(false),
          cCPUs(1),
          ulMemorySizeMB((uint32_t)-1),
          ulVRAMSizeMB(8),
          cMonitors(1),
          fAccelerate3D(false),
          fAccelerate2DVideo(false),
          firmwareType(FirmwareType_BIOS),
          clipboardMode(ClipboardMode_Bidirectional),
          ulMemoryBalloonSize(0),
          ulStatisticsUpdateInterval(0)
{
    mapBootOrder[0] = DeviceType_Floppy;
    mapBootOrder[1] = DeviceType_DVD;
    mapBootOrder[2] = DeviceType_HardDisk;

    /* The default value for PAE depends on the host:
     * - 64 bits host -> always true
     * - 32 bits host -> true for Windows & Darwin (masked off if the host cpu doesn't support it anyway)
     */
#if HC_ARCH_BITS == 64 || defined(RT_OS_WINDOWS) || defined(RT_OS_DARWIN)
    fPAE = true;
#endif
}

/**
 * Called from MachineConfigFile::readHardware() to cpuid information.
 * @param elmCpuid
 * @param ll
 */
void MachineConfigFile::readCpuIdTree(const xml::ElementNode &elmCpuid,
                                      CpuIdLeafsList &ll)
{
    xml::NodesLoop nl1(elmCpuid, "CpuIdLeaf");
    const xml::ElementNode *pelmCpuIdLeaf;
    while ((pelmCpuIdLeaf = nl1.forAllNodes()))
    {
        CpuIdLeaf leaf;

        if (!pelmCpuIdLeaf->getAttributeValue("id", leaf.ulId))
            throw ConfigFileError(this, pelmCpuIdLeaf, N_("Required CpuId/@id attribute is missing"));

        pelmCpuIdLeaf->getAttributeValue("eax", leaf.ulEax);
        pelmCpuIdLeaf->getAttributeValue("ebx", leaf.ulEbx);
        pelmCpuIdLeaf->getAttributeValue("ecx", leaf.ulEcx);
        pelmCpuIdLeaf->getAttributeValue("edx", leaf.ulEdx);

        ll.push_back(leaf);
    }
}

/**
 * Called from MachineConfigFile::readHardware() to network information.
 * @param elmNetwork
 * @param ll
 */
void MachineConfigFile::readNetworkAdapters(const xml::ElementNode &elmNetwork,
                                            NetworkAdaptersList &ll)
{
    xml::NodesLoop nl1(elmNetwork, "Adapter");
    const xml::ElementNode *pelmAdapter;
    while ((pelmAdapter = nl1.forAllNodes()))
    {
        NetworkAdapter nic;

        if (!pelmAdapter->getAttributeValue("slot", nic.ulSlot))
            throw ConfigFileError(this, pelmAdapter, N_("Required Adapter/@slot attribute is missing"));

        Utf8Str strTemp;
        if (pelmAdapter->getAttributeValue("type", strTemp))
        {
            if (strTemp == "Am79C970A")
                nic.type = NetworkAdapterType_Am79C970A;
            else if (strTemp == "Am79C973")
                nic.type = NetworkAdapterType_Am79C973;
            else if (strTemp == "82540EM")
                nic.type = NetworkAdapterType_I82540EM;
            else if (strTemp == "82543GC")
                nic.type = NetworkAdapterType_I82543GC;
            else if (strTemp == "82545EM")
                nic.type = NetworkAdapterType_I82545EM;
            else if (strTemp == "virtio")
                nic.type = NetworkAdapterType_Virtio;
            else
                throw ConfigFileError(this, pelmAdapter, N_("Invalid value '%s' in Adapter/@type attribute"), strTemp.c_str());
        }

        pelmAdapter->getAttributeValue("enabled", nic.fEnabled);
        pelmAdapter->getAttributeValue("MACAddress", nic.strMACAddress);
        pelmAdapter->getAttributeValue("cable", nic.fCableConnected);
        pelmAdapter->getAttributeValue("speed", nic.ulLineSpeed);
        pelmAdapter->getAttributeValue("trace", nic.fTraceEnabled);
        pelmAdapter->getAttributeValue("tracefile", nic.strTraceFile);

        const xml::ElementNode *pelmAdapterChild;
        if ((pelmAdapterChild = pelmAdapter->findChildElement("NAT")))
        {
            nic.mode = NetworkAttachmentType_NAT;
            pelmAdapterChild->getAttributeValue("name", nic.strName);    // optional network name
        }
        else if (    ((pelmAdapterChild = pelmAdapter->findChildElement("HostInterface")))
                  || ((pelmAdapterChild = pelmAdapter->findChildElement("BridgedInterface")))
                )
        {
            nic.mode = NetworkAttachmentType_Bridged;
            pelmAdapterChild->getAttributeValue("name", nic.strName);    // optional host interface name
        }
        else if ((pelmAdapterChild = pelmAdapter->findChildElement("InternalNetwork")))
        {
            nic.mode = NetworkAttachmentType_Internal;
            if (!pelmAdapterChild->getAttributeValue("name", nic.strName))    // required network name
                throw ConfigFileError(this, pelmAdapterChild, N_("Required InternalNetwork/@name element is missing"));
        }
        else if ((pelmAdapterChild = pelmAdapter->findChildElement("HostOnlyInterface")))
        {
            nic.mode = NetworkAttachmentType_HostOnly;
            if (!pelmAdapterChild->getAttributeValue("name", nic.strName))    // required network name
                throw ConfigFileError(this, pelmAdapterChild, N_("Required HostOnlyInterface/@name element is missing"));
        }
        // else: default is NetworkAttachmentType_Null

        ll.push_back(nic);
    }
}

/**
 * Called from MachineConfigFile::readHardware() to read serial port information.
 * @param elmUART
 * @param ll
 */
void MachineConfigFile::readSerialPorts(const xml::ElementNode &elmUART,
                                        SerialPortsList &ll)
{
    xml::NodesLoop nl1(elmUART, "Port");
    const xml::ElementNode *pelmPort;
    while ((pelmPort = nl1.forAllNodes()))
    {
        SerialPort port;
        if (!pelmPort->getAttributeValue("slot", port.ulSlot))
            throw ConfigFileError(this, pelmPort, N_("Required UART/Port/@slot attribute is missing"));

        // slot must be unique
        for (SerialPortsList::const_iterator it = ll.begin();
             it != ll.end();
             ++it)
            if ((*it).ulSlot == port.ulSlot)
                throw ConfigFileError(this, pelmPort, N_("Invalid value %RU32 in UART/Port/@slot attribute: value is not unique"), port.ulSlot);

        if (!pelmPort->getAttributeValue("enabled", port.fEnabled))
            throw ConfigFileError(this, pelmPort, N_("Required UART/Port/@enabled attribute is missing"));
        if (!pelmPort->getAttributeValue("IOBase", port.ulIOBase))
            throw ConfigFileError(this, pelmPort, N_("Required UART/Port/@IOBase attribute is missing"));
        if (!pelmPort->getAttributeValue("IRQ", port.ulIRQ))
            throw ConfigFileError(this, pelmPort, N_("Required UART/Port/@IRQ attribute is missing"));

        Utf8Str strPortMode;
        if (!pelmPort->getAttributeValue("hostMode", strPortMode))
            throw ConfigFileError(this, pelmPort, N_("Required UART/Port/@hostMode attribute is missing"));
        if (strPortMode == "RawFile")
            port.portMode = PortMode_RawFile;
        else if (strPortMode == "HostPipe")
            port.portMode = PortMode_HostPipe;
        else if (strPortMode == "HostDevice")
            port.portMode = PortMode_HostDevice;
        else if (strPortMode == "Disconnected")
            port.portMode = PortMode_Disconnected;
        else
            throw ConfigFileError(this, pelmPort, N_("Invalid value '%s' in UART/Port/@hostMode attribute"), strPortMode.c_str());

        pelmPort->getAttributeValue("path", port.strPath);
        pelmPort->getAttributeValue("server", port.fServer);

        ll.push_back(port);
    }
}

/**
 * Called from MachineConfigFile::readHardware() to read parallel port information.
 * @param elmLPT
 * @param ll
 */
void MachineConfigFile::readParallelPorts(const xml::ElementNode &elmLPT,
                                          ParallelPortsList &ll)
{
    xml::NodesLoop nl1(elmLPT, "Port");
    const xml::ElementNode *pelmPort;
    while ((pelmPort = nl1.forAllNodes()))
    {
        ParallelPort port;
        if (!pelmPort->getAttributeValue("slot", port.ulSlot))
            throw ConfigFileError(this, pelmPort, N_("Required LPT/Port/@slot attribute is missing"));

        // slot must be unique
        for (ParallelPortsList::const_iterator it = ll.begin();
             it != ll.end();
             ++it)
            if ((*it).ulSlot == port.ulSlot)
                throw ConfigFileError(this, pelmPort, N_("Invalid value %RU32 in LPT/Port/@slot attribute: value is not unique"), port.ulSlot);

        if (!pelmPort->getAttributeValue("enabled", port.fEnabled))
            throw ConfigFileError(this, pelmPort, N_("Required LPT/Port/@enabled attribute is missing"));
        if (!pelmPort->getAttributeValue("IOBase", port.ulIOBase))
            throw ConfigFileError(this, pelmPort, N_("Required LPT/Port/@IOBase attribute is missing"));
        if (!pelmPort->getAttributeValue("IRQ", port.ulIRQ))
            throw ConfigFileError(this, pelmPort, N_("Required LPT/Port/@IRQ attribute is missing"));

        pelmPort->getAttributeValue("path", port.strPath);

        ll.push_back(port);
    }
}

/**
 * Called from MachineConfigFile::readHardware() to read guest property information.
 * @param elmGuestProperties
 * @param hw
 */
void MachineConfigFile::readGuestProperties(const xml::ElementNode &elmGuestProperties,
                                            Hardware &hw)
{
    xml::NodesLoop nl1(elmGuestProperties, "GuestProperty");
    const xml::ElementNode *pelmProp;
    while ((pelmProp = nl1.forAllNodes()))
    {
        GuestProperty prop;
        pelmProp->getAttributeValue("name", prop.strName);
        pelmProp->getAttributeValue("value", prop.strValue);

        pelmProp->getAttributeValue("timestamp", prop.timestamp);
        pelmProp->getAttributeValue("flags", prop.strFlags);
        hw.llGuestProperties.push_back(prop);
    }

    elmGuestProperties.getAttributeValue("notificationPatterns", hw.strNotificationPatterns);
}

/**
 * Helper function to read attributes that are common to <SATAController> (pre-1.7)
 * and <StorageController>.
 * @param elmStorageController
 * @param strg
 */
void MachineConfigFile::readStorageControllerAttributes(const xml::ElementNode &elmStorageController,
                                                        StorageController &sctl)
{
    elmStorageController.getAttributeValue("PortCount", sctl.ulPortCount);
    elmStorageController.getAttributeValue("IDE0MasterEmulationPort", sctl.lIDE0MasterEmulationPort);
    elmStorageController.getAttributeValue("IDE0SlaveEmulationPort", sctl.lIDE0SlaveEmulationPort);
    elmStorageController.getAttributeValue("IDE1MasterEmulationPort", sctl.lIDE1MasterEmulationPort);
    elmStorageController.getAttributeValue("IDE1SlaveEmulationPort", sctl.lIDE1SlaveEmulationPort);
}

/**
 * Reads in a <Hardware> block and stores it in the given structure. Used
 * both directly from readMachine and from readSnapshot, since snapshots
 * have their own hardware sections.
 *
 * For legacy pre-1.7 settings we also need a storage structure because
 * the IDE and SATA controllers used to be defined under <Hardware>.
 *
 * @param elmHardware
 * @param hw
 */
void MachineConfigFile::readHardware(const xml::ElementNode &elmHardware,
                                     Hardware &hw,
                                     Storage &strg)
{
    if (!elmHardware.getAttributeValue("version", hw.strVersion))
    {
        /* KLUDGE ALERT!  For a while during the 3.1 development this was not
           written because it was thought to have a default value of "2".  For
           sv <= 1.3 it defaults to "1" because the attribute didn't exist,
           while for 1.4+ it is sort of mandatory.  Now, the buggy XML writer
           code only wrote 1.7 and later.  So, if it's a 1.7+ XML file and it's
           missing the hardware version, then it probably should be "2" instead
           of "1". */
        if (m->sv < SettingsVersion_v1_7)
            hw.strVersion = "1";
        else
            hw.strVersion = "2";
    }
    Utf8Str strUUID;
    if (elmHardware.getAttributeValue("uuid", strUUID))
        parseUUID(hw.uuid, strUUID);

    xml::NodesLoop nl1(elmHardware);
    const xml::ElementNode *pelmHwChild;
    while ((pelmHwChild = nl1.forAllNodes()))
    {
        if (pelmHwChild->nameEquals("CPU"))
        {
            if (!pelmHwChild->getAttributeValue("count", hw.cCPUs))
            {
                // pre-1.5 variant; not sure if this actually exists in the wild anywhere
                const xml::ElementNode *pelmCPUChild;
                if ((pelmCPUChild = pelmHwChild->findChildElement("CPUCount")))
                    pelmCPUChild->getAttributeValue("count", hw.cCPUs);
            }

            const xml::ElementNode *pelmCPUChild;
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtEx")))
            {
                pelmCPUChild->getAttributeValue("enabled", hw.fHardwareVirt);
                pelmCPUChild->getAttributeValue("exclusive", hw.fHardwareVirtExclusive);        // settings version 1.9
            }
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtExNestedPaging")))
                pelmCPUChild->getAttributeValue("enabled", hw.fNestedPaging);
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtExVPID")))
                pelmCPUChild->getAttributeValue("enabled", hw.fVPID);

            if (!(pelmCPUChild = pelmHwChild->findChildElement("PAE")))
            {
                /* The default for pre 3.1 was false, so we must respect that. */
                if (m->sv < SettingsVersion_v1_9)
                    hw.fPAE = false;
            }
            else
                pelmCPUChild->getAttributeValue("enabled", hw.fPAE);

            if ((pelmCPUChild = pelmHwChild->findChildElement("SyntheticCpu")))
                pelmCPUChild->getAttributeValue("enabled", hw.fSyntheticCpu);
            if ((pelmCPUChild = pelmHwChild->findChildElement("CpuIdTree")))
                readCpuIdTree(*pelmCPUChild, hw.llCpuIdLeafs);
        }
        else if (pelmHwChild->nameEquals("Memory"))
            pelmHwChild->getAttributeValue("RAMSize", hw.ulMemorySizeMB);
        else if (pelmHwChild->nameEquals("Firmware"))
        {
            Utf8Str strFirmwareType;
            if (pelmHwChild->getAttributeValue("type", strFirmwareType))
            {
                if (    (strFirmwareType == "BIOS")
                     || (strFirmwareType == "1")                // some trunk builds used the number here
                   )
                    hw.firmwareType = FirmwareType_BIOS;
                else if (    (strFirmwareType == "EFI")
                          || (strFirmwareType == "2")           // some trunk builds used the number here
                        )
                    hw.firmwareType = FirmwareType_EFI;
                else if (    strFirmwareType == "EFI64")
                    hw.firmwareType = FirmwareType_EFI64;
                else if (    strFirmwareType == "EFIDUAL")
                    hw.firmwareType = FirmwareType_EFIDUAL;
                else
                    throw ConfigFileError(this,
                                          pelmHwChild,
                                          N_("Invalid value '%s' in Boot/Firmware/@type"),
                                          strFirmwareType.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("Boot"))
        {
            hw.mapBootOrder.clear();

            xml::NodesLoop nl2(*pelmHwChild, "Order");
            const xml::ElementNode *pelmOrder;
            while ((pelmOrder = nl2.forAllNodes()))
            {
                uint32_t ulPos;
                Utf8Str strDevice;
                if (!pelmOrder->getAttributeValue("position", ulPos))
                    throw ConfigFileError(this, pelmOrder, N_("Required Boot/Order/@position attribute is missing"));

                if (    ulPos < 1
                     || ulPos > SchemaDefs::MaxBootPosition
                   )
                    throw ConfigFileError(this,
                                          pelmOrder,
                                          N_("Invalid value '%RU32' in Boot/Order/@position: must be greater than 0 and less than %RU32"),
                                          ulPos,
                                          SchemaDefs::MaxBootPosition + 1);
                // XML is 1-based but internal data is 0-based
                --ulPos;

                if (hw.mapBootOrder.find(ulPos) != hw.mapBootOrder.end())
                    throw ConfigFileError(this, pelmOrder, N_("Invalid value '%RU32' in Boot/Order/@position: value is not unique"), ulPos);

                if (!pelmOrder->getAttributeValue("device", strDevice))
                    throw ConfigFileError(this, pelmOrder, N_("Required Boot/Order/@device attribute is missing"));

                DeviceType_T type;
                if (strDevice == "None")
                    type = DeviceType_Null;
                else if (strDevice == "Floppy")
                    type = DeviceType_Floppy;
                else if (strDevice == "DVD")
                    type = DeviceType_DVD;
                else if (strDevice == "HardDisk")
                    type = DeviceType_HardDisk;
                else if (strDevice == "Network")
                    type = DeviceType_Network;
                else
                    throw ConfigFileError(this, pelmOrder, N_("Invalid value '%s' in Boot/Order/@device attribute"), strDevice.c_str());
                hw.mapBootOrder[ulPos] = type;
            }
        }
        else if (pelmHwChild->nameEquals("Display"))
        {
            pelmHwChild->getAttributeValue("VRAMSize", hw.ulVRAMSizeMB);
            if (!pelmHwChild->getAttributeValue("monitorCount", hw.cMonitors))
                pelmHwChild->getAttributeValue("MonitorCount", hw.cMonitors);       // pre-v1.5 variant
            if (!pelmHwChild->getAttributeValue("accelerate3D", hw.fAccelerate3D))
                pelmHwChild->getAttributeValue("Accelerate3D", hw.fAccelerate3D);   // pre-v1.5 variant
            pelmHwChild->getAttributeValue("accelerate2DVideo", hw.fAccelerate2DVideo);
        }
        else if (pelmHwChild->nameEquals("RemoteDisplay"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.vrdpSettings.fEnabled);
            pelmHwChild->getAttributeValue("port", hw.vrdpSettings.strPort);
            pelmHwChild->getAttributeValue("netAddress", hw.vrdpSettings.strNetAddress);

            Utf8Str strAuthType;
            if (pelmHwChild->getAttributeValue("authType", strAuthType))
            {
                // settings before 1.3 used lower case so make sure this is case-insensitive
                strAuthType.toUpper();
                if (strAuthType == "NULL")
                    hw.vrdpSettings.authType = VRDPAuthType_Null;
                else if (strAuthType == "GUEST")
                    hw.vrdpSettings.authType = VRDPAuthType_Guest;
                else if (strAuthType == "EXTERNAL")
                    hw.vrdpSettings.authType = VRDPAuthType_External;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in RemoteDisplay/@authType attribute"), strAuthType.c_str());
            }

            pelmHwChild->getAttributeValue("authTimeout", hw.vrdpSettings.ulAuthTimeout);
            pelmHwChild->getAttributeValue("allowMultiConnection", hw.vrdpSettings.fAllowMultiConnection);
            pelmHwChild->getAttributeValue("reuseSingleConnection", hw.vrdpSettings.fReuseSingleConnection);
        }
        else if (pelmHwChild->nameEquals("BIOS"))
        {
            const xml::ElementNode *pelmBIOSChild;
            if ((pelmBIOSChild = pelmHwChild->findChildElement("ACPI")))
                pelmBIOSChild->getAttributeValue("enabled", hw.biosSettings.fACPIEnabled);
            if ((pelmBIOSChild = pelmHwChild->findChildElement("IOAPIC")))
                pelmBIOSChild->getAttributeValue("enabled", hw.biosSettings.fIOAPICEnabled);
            if ((pelmBIOSChild = pelmHwChild->findChildElement("Logo")))
            {
                pelmBIOSChild->getAttributeValue("fadeIn", hw.biosSettings.fLogoFadeIn);
                pelmBIOSChild->getAttributeValue("fadeOut", hw.biosSettings.fLogoFadeOut);
                pelmBIOSChild->getAttributeValue("displayTime", hw.biosSettings.ulLogoDisplayTime);
                pelmBIOSChild->getAttributeValue("imagePath", hw.biosSettings.strLogoImagePath);
            }
            if ((pelmBIOSChild = pelmHwChild->findChildElement("BootMenu")))
            {
                Utf8Str strBootMenuMode;
                if (pelmBIOSChild->getAttributeValue("mode", strBootMenuMode))
                {
                    // settings before 1.3 used lower case so make sure this is case-insensitive
                    strBootMenuMode.toUpper();
                    if (strBootMenuMode == "DISABLED")
                        hw.biosSettings.biosBootMenuMode = BIOSBootMenuMode_Disabled;
                    else if (strBootMenuMode == "MENUONLY")
                        hw.biosSettings.biosBootMenuMode = BIOSBootMenuMode_MenuOnly;
                    else if (strBootMenuMode == "MESSAGEANDMENU")
                        hw.biosSettings.biosBootMenuMode = BIOSBootMenuMode_MessageAndMenu;
                    else
                        throw ConfigFileError(this, pelmBIOSChild, N_("Invalid value '%s' in BootMenu/@mode attribute"), strBootMenuMode.c_str());
                }
            }
            if ((pelmBIOSChild = pelmHwChild->findChildElement("PXEDebug")))
                pelmBIOSChild->getAttributeValue("enabled", hw.biosSettings.fPXEDebugEnabled);
            if ((pelmBIOSChild = pelmHwChild->findChildElement("TimeOffset")))
                pelmBIOSChild->getAttributeValue("value", hw.biosSettings.llTimeOffset);

            // legacy BIOS/IDEController (pre 1.7)
            if (    (m->sv < SettingsVersion_v1_7)
                 && ((pelmBIOSChild = pelmHwChild->findChildElement("IDEController")))
               )
            {
                StorageController sctl;
                sctl.strName = "IDE Controller";
                sctl.storageBus = StorageBus_IDE;

                Utf8Str strType;
                if (pelmBIOSChild->getAttributeValue("type", strType))
                {
                    if (strType == "PIIX3")
                        sctl.controllerType = StorageControllerType_PIIX3;
                    else if (strType == "PIIX4")
                        sctl.controllerType = StorageControllerType_PIIX4;
                    else if (strType == "ICH6")
                        sctl.controllerType = StorageControllerType_ICH6;
                    else
                        throw ConfigFileError(this, pelmBIOSChild, N_("Invalid value '%s' for IDEController/@type attribute"), strType.c_str());
                }
                sctl.ulPortCount = 2;
                strg.llStorageControllers.push_back(sctl);
            }
        }
        else if (pelmHwChild->nameEquals("USBController"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.usbController.fEnabled);
            pelmHwChild->getAttributeValue("enabledEhci", hw.usbController.fEnabledEHCI);

            readUSBDeviceFilters(*pelmHwChild,
                                 hw.usbController.llDeviceFilters);
        }
        else if (    (m->sv < SettingsVersion_v1_7)
                  && (pelmHwChild->nameEquals("SATAController"))
                )
        {
            bool f;
            if (    (pelmHwChild->getAttributeValue("enabled", f))
                 && (f)
               )
            {
                StorageController sctl;
                sctl.strName = "SATA Controller";
                sctl.storageBus = StorageBus_SATA;
                sctl.controllerType = StorageControllerType_IntelAhci;

                readStorageControllerAttributes(*pelmHwChild, sctl);

                strg.llStorageControllers.push_back(sctl);
            }
        }
        else if (pelmHwChild->nameEquals("Network"))
            readNetworkAdapters(*pelmHwChild, hw.llNetworkAdapters);
        else if (    (pelmHwChild->nameEquals("UART"))
                  || (pelmHwChild->nameEquals("Uart"))      // used before 1.3
                )
            readSerialPorts(*pelmHwChild, hw.llSerialPorts);
        else if (    (pelmHwChild->nameEquals("LPT"))
                  ||  (pelmHwChild->nameEquals("Lpt"))      // used before 1.3
                )
            readParallelPorts(*pelmHwChild, hw.llParallelPorts);
        else if (pelmHwChild->nameEquals("AudioAdapter"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.audioAdapter.fEnabled);

            Utf8Str strTemp;
            if (pelmHwChild->getAttributeValue("controller", strTemp))
            {
                if (strTemp == "SB16")
                    hw.audioAdapter.controllerType = AudioControllerType_SB16;
                else if (strTemp == "AC97")
                    hw.audioAdapter.controllerType = AudioControllerType_AC97;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in AudioAdapter/@controller attribute"), strTemp.c_str());
            }
            if (pelmHwChild->getAttributeValue("driver", strTemp))
            {
                // settings before 1.3 used lower case so make sure this is case-insensitive
                strTemp.toUpper();
                if (strTemp == "NULL")
                    hw.audioAdapter.driverType = AudioDriverType_Null;
                else if (strTemp == "WINMM")
                    hw.audioAdapter.driverType = AudioDriverType_WinMM;
                else if ( (strTemp == "DIRECTSOUND") || (strTemp == "DSOUND") )
                    hw.audioAdapter.driverType = AudioDriverType_DirectSound;
                else if (strTemp == "SOLAUDIO")
                    hw.audioAdapter.driverType = AudioDriverType_SolAudio;
                else if (strTemp == "ALSA")
                    hw.audioAdapter.driverType = AudioDriverType_ALSA;
                else if (strTemp == "PULSE")
                    hw.audioAdapter.driverType = AudioDriverType_Pulse;
                else if (strTemp == "OSS")
                    hw.audioAdapter.driverType = AudioDriverType_OSS;
                else if (strTemp == "COREAUDIO")
                    hw.audioAdapter.driverType = AudioDriverType_CoreAudio;
                else if (strTemp == "MMPM")
                    hw.audioAdapter.driverType = AudioDriverType_MMPM;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in AudioAdapter/@driver attribute"), strTemp.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("SharedFolders"))
        {
            xml::NodesLoop nl2(*pelmHwChild, "SharedFolder");
            const xml::ElementNode *pelmFolder;
            while ((pelmFolder = nl2.forAllNodes()))
            {
                SharedFolder sf;
                pelmFolder->getAttributeValue("name", sf.strName);
                pelmFolder->getAttributeValue("hostPath", sf.strHostPath);
                pelmFolder->getAttributeValue("writable", sf.fWritable);
                hw.llSharedFolders.push_back(sf);
            }
        }
        else if (pelmHwChild->nameEquals("Clipboard"))
        {
            Utf8Str strTemp;
            if (pelmHwChild->getAttributeValue("mode", strTemp))
            {
                if (strTemp == "Disabled")
                    hw.clipboardMode = ClipboardMode_Disabled;
                else if (strTemp == "HostToGuest")
                    hw.clipboardMode = ClipboardMode_HostToGuest;
                else if (strTemp == "GuestToHost")
                    hw.clipboardMode = ClipboardMode_GuestToHost;
                else if (strTemp == "Bidirectional")
                    hw.clipboardMode = ClipboardMode_Bidirectional;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in Clipbord/@mode attribute"), strTemp.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("Guest"))
        {
            if (!pelmHwChild->getAttributeValue("memoryBalloonSize", hw.ulMemoryBalloonSize))
                pelmHwChild->getAttributeValue("MemoryBalloonSize", hw.ulMemoryBalloonSize);            // used before 1.3
            if (!pelmHwChild->getAttributeValue("statisticsUpdateInterval", hw.ulStatisticsUpdateInterval))
                pelmHwChild->getAttributeValue("StatisticsUpdateInterval", hw.ulStatisticsUpdateInterval);
        }
        else if (pelmHwChild->nameEquals("GuestProperties"))
            readGuestProperties(*pelmHwChild, hw);
    }

    if (hw.ulMemorySizeMB == (uint32_t)-1)
        throw ConfigFileError(this, &elmHardware, N_("Required Memory/@RAMSize element/attribute is missing"));
}

/**
 * This gets called instead of readStorageControllers() for legacy pre-1.7 settings
 * files which have a <HardDiskAttachments> node and storage controller settings
 * hidden in the <Hardware> settings. We set the StorageControllers fields just the
 * same, just from different sources.
 * @param elmHardware <Hardware> XML node.
 * @param elmHardDiskAttachments  <HardDiskAttachments> XML node.
 * @param strg
 */
void MachineConfigFile::readHardDiskAttachments_pre1_7(const xml::ElementNode &elmHardDiskAttachments,
                                                       Storage &strg)
{
    StorageController *pIDEController = NULL;
    StorageController *pSATAController = NULL;

    for (StorageControllersList::iterator it = strg.llStorageControllers.begin();
         it != strg.llStorageControllers.end();
         ++it)
    {
        StorageController &s = *it;
        if (s.storageBus == StorageBus_IDE)
            pIDEController = &s;
        else if (s.storageBus == StorageBus_SATA)
            pSATAController = &s;
    }

    xml::NodesLoop nl1(elmHardDiskAttachments, "HardDiskAttachment");
    const xml::ElementNode *pelmAttachment;
    while ((pelmAttachment = nl1.forAllNodes()))
    {
        AttachedDevice att;
        Utf8Str strUUID, strBus;

        if (!pelmAttachment->getAttributeValue("hardDisk", strUUID))
            throw ConfigFileError(this, pelmAttachment, N_("Required HardDiskAttachment/@hardDisk attribute is missing"));
        parseUUID(att.uuid, strUUID);

        if (!pelmAttachment->getAttributeValue("bus", strBus))
            throw ConfigFileError(this, pelmAttachment, N_("Required HardDiskAttachment/@bus attribute is missing"));
        // pre-1.7 'channel' is now port
        if (!pelmAttachment->getAttributeValue("channel", att.lPort))
            throw ConfigFileError(this, pelmAttachment, N_("Required HardDiskAttachment/@channel attribute is missing"));
        // pre-1.7 'device' is still device
        if (!pelmAttachment->getAttributeValue("device", att.lDevice))
            throw ConfigFileError(this, pelmAttachment, N_("Required HardDiskAttachment/@device attribute is missing"));

        att.deviceType = DeviceType_HardDisk;

        if (strBus == "IDE")
        {
            if (!pIDEController)
                throw ConfigFileError(this, pelmAttachment, N_("HardDiskAttachment/@bus is 'IDE' but cannot find IDE controller"));
            pIDEController->llAttachedDevices.push_back(att);
        }
        else if (strBus == "SATA")
        {
            if (!pSATAController)
                throw ConfigFileError(this, pelmAttachment, N_("HardDiskAttachment/@bus is 'SATA' but cannot find SATA controller"));
            pSATAController->llAttachedDevices.push_back(att);
        }
        else
            throw ConfigFileError(this, pelmAttachment, N_("HardDiskAttachment/@bus attribute has illegal value '%s'"), strBus.c_str());
    }
}

/**
 * Reads in a <StorageControllers> block and stores it in the given Storage structure.
 * Used both directly from readMachine and from readSnapshot, since snapshots
 * have their own storage controllers sections.
 *
 * This is only called for settings version 1.7 and above; see readHardDiskAttachments_pre1_7()
 * for earlier versions.
 *
 * @param elmStorageControllers
 */
void MachineConfigFile::readStorageControllers(const xml::ElementNode &elmStorageControllers,
                                               Storage &strg)
{
    xml::NodesLoop nlStorageControllers(elmStorageControllers, "StorageController");
    const xml::ElementNode *pelmController;
    while ((pelmController = nlStorageControllers.forAllNodes()))
    {
        StorageController sctl;

        if (!pelmController->getAttributeValue("name", sctl.strName))
            throw ConfigFileError(this, pelmController, N_("Required StorageController/@name attribute is missing"));
        //  canonicalize storage controller names for configs in the switchover
        //  period.
        if (m->sv <= SettingsVersion_v1_9)
        {
            if (sctl.strName == "IDE")
                sctl.strName = "IDE Controller";
            else if (sctl.strName == "SATA")
                sctl.strName = "SATA Controller";
            else if (sctl.strName == "SCSI")
                sctl.strName = "SCSI Controller";
        }

        pelmController->getAttributeValue("Instance", sctl.ulInstance);
            // default from constructor is 0

        Utf8Str strType;
        if (!pelmController->getAttributeValue("type", strType))
            throw ConfigFileError(this, pelmController, N_("Required StorageController/@type attribute is missing"));

        if (strType == "AHCI")
        {
            sctl.storageBus = StorageBus_SATA;
            sctl.controllerType = StorageControllerType_IntelAhci;
        }
        else if (strType == "LsiLogic")
        {
            sctl.storageBus = StorageBus_SCSI;
            sctl.controllerType = StorageControllerType_LsiLogic;
        }
        else if (strType == "BusLogic")
        {
            sctl.storageBus = StorageBus_SCSI;
            sctl.controllerType = StorageControllerType_BusLogic;
        }
        else if (strType == "PIIX3")
        {
            sctl.storageBus = StorageBus_IDE;
            sctl.controllerType = StorageControllerType_PIIX3;
        }
        else if (strType == "PIIX4")
        {
            sctl.storageBus = StorageBus_IDE;
            sctl.controllerType = StorageControllerType_PIIX4;
        }
        else if (strType == "ICH6")
        {
            sctl.storageBus = StorageBus_IDE;
            sctl.controllerType = StorageControllerType_ICH6;
        }
        else if (   (m->sv >= SettingsVersion_v1_9)
                 && (strType == "I82078")
                )
        {
            sctl.storageBus = StorageBus_Floppy;
            sctl.controllerType = StorageControllerType_I82078;
        }
        else
            throw ConfigFileError(this, pelmController, N_("Invalid value '%s' for StorageController/@type attribute"), strType.c_str());

        readStorageControllerAttributes(*pelmController, sctl);

        xml::NodesLoop nlAttached(*pelmController, "AttachedDevice");
        const xml::ElementNode *pelmAttached;
        while ((pelmAttached = nlAttached.forAllNodes()))
        {
            AttachedDevice att;
            Utf8Str strTemp;
            pelmAttached->getAttributeValue("type", strTemp);

            if (strTemp == "HardDisk")
                att.deviceType = DeviceType_HardDisk;
            else if (m->sv >= SettingsVersion_v1_9)
            {
                // starting with 1.9 we list DVD and floppy drive info + attachments under <StorageControllers>
                if (strTemp == "DVD")
                {
                    att.deviceType = DeviceType_DVD;
                    pelmAttached->getAttributeValue("passthrough", att.fPassThrough);
                }
                else if (strTemp == "Floppy")
                    att.deviceType = DeviceType_Floppy;
            }

            if (att.deviceType != DeviceType_Null)
            {
                const xml::ElementNode *pelmImage;
                // all types can have images attached, but for HardDisk it's required
                if (!(pelmImage = pelmAttached->findChildElement("Image")))
                {
                    if (att.deviceType == DeviceType_HardDisk)
                        throw ConfigFileError(this, pelmImage, N_("Required AttachedDevice/Image element is missing"));
                    else
                    {
                        // DVDs and floppies can also have <HostDrive> instead of <Image>
                        const xml::ElementNode *pelmHostDrive;
                        if ((pelmHostDrive = pelmAttached->findChildElement("HostDrive")))
                            if (!pelmHostDrive->getAttributeValue("src", att.strHostDriveSrc))
                                throw ConfigFileError(this, pelmHostDrive, N_("Required AttachedDevice/HostDrive/@src attribute is missing"));
                    }
                }
                else
                {
                    if (!pelmImage->getAttributeValue("uuid", strTemp))
                        throw ConfigFileError(this, pelmImage, N_("Required AttachedDevice/Image/@uuid attribute is missing"));
                    parseUUID(att.uuid, strTemp);
                }

                if (!pelmAttached->getAttributeValue("port", att.lPort))
                    throw ConfigFileError(this, pelmImage, N_("Required AttachedDevice/@port attribute is missing"));
                if (!pelmAttached->getAttributeValue("device", att.lDevice))
                    throw ConfigFileError(this, pelmImage, N_("Required AttachedDevice/@device attribute is missing"));

                sctl.llAttachedDevices.push_back(att);
            }
        }

        strg.llStorageControllers.push_back(sctl);
    }
}

/**
 * This gets called for legacy pre-1.9 settings files after having parsed the
 * <Hardware> and <StorageControllers> sections to parse <Hardware> once more
 * for the <DVDDrive> and <FloppyDrive> sections.
 *
 * Before settings version 1.9, DVD and floppy drives were specified separately
 * under <Hardware>; we then need this extra loop to make sure the storage
 * controller structs are already set up so we can add stuff to them.
 *
 * @param elmHardware
 * @param strg
 */
void MachineConfigFile::readDVDAndFloppies_pre1_9(const xml::ElementNode &elmHardware,
                                                  Storage &strg)
{
    xml::NodesLoop nl1(elmHardware);
    const xml::ElementNode *pelmHwChild;
    while ((pelmHwChild = nl1.forAllNodes()))
    {
        if (pelmHwChild->nameEquals("DVDDrive"))
        {
            // create a DVD "attached device" and attach it to the existing IDE controller
            AttachedDevice att;
            att.deviceType = DeviceType_DVD;
            // legacy DVD drive is always secondary master (port 1, device 0)
            att.lPort = 1;
            att.lDevice = 0;
            pelmHwChild->getAttributeValue("passthrough", att.fPassThrough);

            const xml::ElementNode *pDriveChild;
            Utf8Str strTmp;
            if (    ((pDriveChild = pelmHwChild->findChildElement("Image")))
                 && (pDriveChild->getAttributeValue("uuid", strTmp))
               )
                parseUUID(att.uuid, strTmp);
            else if ((pDriveChild = pelmHwChild->findChildElement("HostDrive")))
                pDriveChild->getAttributeValue("src", att.strHostDriveSrc);

            // find the IDE controller and attach the DVD drive
            bool fFound = false;
            for (StorageControllersList::iterator it = strg.llStorageControllers.begin();
                 it != strg.llStorageControllers.end();
                 ++it)
            {
                StorageController &sctl = *it;
                if (sctl.storageBus == StorageBus_IDE)
                {
                    sctl.llAttachedDevices.push_back(att);
                    fFound = true;
                    break;
                }
            }

            if (!fFound)
                throw ConfigFileError(this, pelmHwChild, N_("Internal error: found DVD drive but IDE controller does not exist"));
                        // shouldn't happen because pre-1.9 settings files always had at least one IDE controller in the settings
                        // which should have gotten parsed in <StorageControllers> before this got called
        }
        else if (pelmHwChild->nameEquals("FloppyDrive"))
        {
            bool fEnabled;
            if (    (pelmHwChild->getAttributeValue("enabled", fEnabled))
                 && (fEnabled)
               )
            {
                // create a new floppy controller and attach a floppy "attached device"
                StorageController sctl;
                sctl.strName = "Floppy Controller";
                sctl.storageBus = StorageBus_Floppy;
                sctl.controllerType = StorageControllerType_I82078;
                sctl.ulPortCount = 1;

                AttachedDevice att;
                att.deviceType = DeviceType_Floppy;
                att.lPort = 0;
                att.lDevice = 0;

                const xml::ElementNode *pDriveChild;
                Utf8Str strTmp;
                if (    ((pDriveChild = pelmHwChild->findChildElement("Image")))
                     && (pDriveChild->getAttributeValue("uuid", strTmp))
                   )
                    parseUUID(att.uuid, strTmp);
                else if ((pDriveChild = pelmHwChild->findChildElement("HostDrive")))
                    pDriveChild->getAttributeValue("src", att.strHostDriveSrc);

                // store attachment with controller
                sctl.llAttachedDevices.push_back(att);
                // store controller with storage
                strg.llStorageControllers.push_back(sctl);
            }
        }
    }
}

/**
 * Called initially for the <Snapshot> element under <Machine>, if present,
 * to store the snapshot's data into the given Snapshot structure (which is
 * then the one in the Machine struct). This might then recurse if
 * a <Snapshots> (plural) element is found in the snapshot, which should
 * contain a list of child snapshots; such lists are maintained in the
 * Snapshot structure.
 *
 * @param elmSnapshot
 * @param snap
 */
void MachineConfigFile::readSnapshot(const xml::ElementNode &elmSnapshot,
                                     Snapshot &snap)
{
    Utf8Str strTemp;

    if (!elmSnapshot.getAttributeValue("uuid", strTemp))
        throw ConfigFileError(this, &elmSnapshot, N_("Required Snapshot/@uuid attribute is missing"));
    parseUUID(snap.uuid, strTemp);

    if (!elmSnapshot.getAttributeValue("name", snap.strName))
        throw ConfigFileError(this, &elmSnapshot, N_("Required Snapshot/@name attribute is missing"));

    // earlier 3.1 trunk builds had a bug and added Description as an attribute, read it silently and write it back as an element
    elmSnapshot.getAttributeValue("Description", snap.strDescription);

    if (!elmSnapshot.getAttributeValue("timeStamp", strTemp))
        throw ConfigFileError(this, &elmSnapshot, N_("Required Snapshot/@timeStamp attribute is missing"));
    parseTimestamp(snap.timestamp, strTemp);

    elmSnapshot.getAttributeValue("stateFile", snap.strStateFile);      // online snapshots only

    // parse Hardware before the other elements because other things depend on it
    const xml::ElementNode *pelmHardware;
    if (!(pelmHardware = elmSnapshot.findChildElement("Hardware")))
        throw ConfigFileError(this, &elmSnapshot, N_("Required Snapshot/@Hardware element is missing"));
    readHardware(*pelmHardware, snap.hardware, snap.storage);

    xml::NodesLoop nlSnapshotChildren(elmSnapshot);
    const xml::ElementNode *pelmSnapshotChild;
    while ((pelmSnapshotChild = nlSnapshotChildren.forAllNodes()))
    {
        if (pelmSnapshotChild->nameEquals("Description"))
            snap.strDescription = pelmSnapshotChild->getValue();
        else if (    (m->sv < SettingsVersion_v1_7)
                  && (pelmSnapshotChild->nameEquals("HardDiskAttachments"))
                )
            readHardDiskAttachments_pre1_7(*pelmSnapshotChild, snap.storage);
        else if (    (m->sv >= SettingsVersion_v1_7)
                  && (pelmSnapshotChild->nameEquals("StorageControllers"))
                )
            readStorageControllers(*pelmSnapshotChild, snap.storage);
        else if (pelmSnapshotChild->nameEquals("Snapshots"))
        {
            xml::NodesLoop nlChildSnapshots(*pelmSnapshotChild);
            const xml::ElementNode *pelmChildSnapshot;
            while ((pelmChildSnapshot = nlChildSnapshots.forAllNodes()))
            {
                if (pelmChildSnapshot->nameEquals("Snapshot"))
                {
                    Snapshot child;
                    readSnapshot(*pelmChildSnapshot, child);
                    snap.llChildSnapshots.push_back(child);
                }
            }
        }
    }

    if (m->sv < SettingsVersion_v1_9)
        // go through Hardware once more to repair the settings controller structures
        // with data from old DVDDrive and FloppyDrive elements
        readDVDAndFloppies_pre1_9(*pelmHardware, snap.storage);
}

void MachineConfigFile::convertOldOSType_pre1_5(Utf8Str &str)
{
    if (str == "unknown") str = "Other";
    else if (str == "dos") str = "DOS";
    else if (str == "win31") str = "Windows31";
    else if (str == "win95") str = "Windows95";
    else if (str == "win98") str = "Windows98";
    else if (str == "winme") str = "WindowsMe";
    else if (str == "winnt4") str = "WindowsNT4";
    else if (str == "win2k") str = "Windows2000";
    else if (str == "winxp") str = "WindowsXP";
    else if (str == "win2k3") str = "Windows2003";
    else if (str == "winvista") str = "WindowsVista";
    else if (str == "win2k8") str = "Windows2008";
    else if (str == "os2warp3") str = "OS2Warp3";
    else if (str == "os2warp4") str = "OS2Warp4";
    else if (str == "os2warp45") str = "OS2Warp45";
    else if (str == "ecs") str = "OS2eCS";
    else if (str == "linux22") str = "Linux22";
    else if (str == "linux24") str = "Linux24";
    else if (str == "linux26") str = "Linux26";
    else if (str == "archlinux") str = "ArchLinux";
    else if (str == "debian") str = "Debian";
    else if (str == "opensuse") str = "OpenSUSE";
    else if (str == "fedoracore") str = "Fedora";
    else if (str == "gentoo") str = "Gentoo";
    else if (str == "mandriva") str = "Mandriva";
    else if (str == "redhat") str = "RedHat";
    else if (str == "ubuntu") str = "Ubuntu";
    else if (str == "xandros") str = "Xandros";
    else if (str == "freebsd") str = "FreeBSD";
    else if (str == "openbsd") str = "OpenBSD";
    else if (str == "netbsd") str = "NetBSD";
    else if (str == "netware") str = "Netware";
    else if (str == "solaris") str = "Solaris";
    else if (str == "opensolaris") str = "OpenSolaris";
    else if (str == "l4") str = "L4";
}

/**
 * Called from the constructor to actually read in the <Machine> element
 * of a machine config file.
 * @param elmMachine
 */
void MachineConfigFile::readMachine(const xml::ElementNode &elmMachine)
{
    Utf8Str strUUID;
    if (    (elmMachine.getAttributeValue("uuid", strUUID))
         && (elmMachine.getAttributeValue("name", strName))
       )
    {
        parseUUID(uuid, strUUID);

        if (!elmMachine.getAttributeValue("nameSync", fNameSync))
            fNameSync = true;

        Utf8Str str;
        elmMachine.getAttributeValue("Description", strDescription);

        elmMachine.getAttributeValue("OSType", strOsType);
        if (m->sv < SettingsVersion_v1_5)
            convertOldOSType_pre1_5(strOsType);

        elmMachine.getAttributeValue("stateFile", strStateFile);
        if (elmMachine.getAttributeValue("currentSnapshot", str))
            parseUUID(uuidCurrentSnapshot, str);
        elmMachine.getAttributeValue("snapshotFolder", strSnapshotFolder);
        if (!elmMachine.getAttributeValue("currentStateModified", fCurrentStateModified))
            fCurrentStateModified = true;
        if (elmMachine.getAttributeValue("lastStateChange", str))
            parseTimestamp(timeLastStateChange, str);
            // constructor has called RTTimeNow(&timeLastStateChange) before

#if 1 /** @todo Teleportation: Obsolete. Remove in a couple of days. */
        if (!elmMachine.getAttributeValue("teleporterEnabled", fTeleporterEnabled)
         && !elmMachine.getAttributeValue("liveMigrationTarget", fTeleporterEnabled))
            fTeleporterEnabled = false;
        if (!elmMachine.getAttributeValue("teleporterPort", uTeleporterPort)
         && !elmMachine.getAttributeValue("liveMigrationPort", uTeleporterPort))
            uTeleporterPort = 0;
        if (!elmMachine.getAttributeValue("teleporterAddress", strTeleporterAddress))
            strTeleporterAddress = "";
        if (!elmMachine.getAttributeValue("teleporterPassword", strTeleporterPassword)
         && !elmMachine.getAttributeValue("liveMigrationPassword", strTeleporterPassword))
            strTeleporterPassword = "";
#endif

        // parse Hardware before the other elements because other things depend on it
        const xml::ElementNode *pelmHardware;
        if (!(pelmHardware = elmMachine.findChildElement("Hardware")))
            throw ConfigFileError(this, &elmMachine, N_("Required Machine/Hardware element is missing"));
        readHardware(*pelmHardware, hardwareMachine, storageMachine);

        xml::NodesLoop nlRootChildren(elmMachine);
        const xml::ElementNode *pelmMachineChild;
        while ((pelmMachineChild = nlRootChildren.forAllNodes()))
        {
            if (pelmMachineChild->nameEquals("ExtraData"))
                readExtraData(*pelmMachineChild,
                              mapExtraDataItems);
            else if (    (m->sv < SettingsVersion_v1_7)
                      && (pelmMachineChild->nameEquals("HardDiskAttachments"))
                    )
                readHardDiskAttachments_pre1_7(*pelmMachineChild, storageMachine);
            else if (    (m->sv >= SettingsVersion_v1_7)
                      && (pelmMachineChild->nameEquals("StorageControllers"))
                    )
                readStorageControllers(*pelmMachineChild, storageMachine);
            else if (pelmMachineChild->nameEquals("Snapshot"))
            {
                Snapshot snap;
                // this will recurse into child snapshots, if necessary
                readSnapshot(*pelmMachineChild, snap);
                llFirstSnapshot.push_back(snap);
            }
            else if (pelmMachineChild->nameEquals("Description"))
                strDescription = pelmMachineChild->getValue();
            else if (pelmMachineChild->nameEquals("Teleporter"))
            {
                if (!pelmMachineChild->getAttributeValue("enabled", fTeleporterEnabled))
                    fTeleporterEnabled = false;
                if (!pelmMachineChild->getAttributeValue("port", uTeleporterPort))
                    uTeleporterPort = 0;
                if (!pelmMachineChild->getAttributeValue("address", strTeleporterAddress))
                    strTeleporterAddress = "";
                if (!pelmMachineChild->getAttributeValue("password", strTeleporterPassword))
                    strTeleporterPassword = "";
            }
        }

        if (m->sv < SettingsVersion_v1_9)
            // go through Hardware once more to repair the settings controller structures
            // with data from old DVDDrive and FloppyDrive elements
            readDVDAndFloppies_pre1_9(*pelmHardware, storageMachine);
    }
    else
        throw ConfigFileError(this, &elmMachine, N_("Required Machine/@uuid or @name attributes is missing"));
}

////////////////////////////////////////////////////////////////////////////////
//
// MachineConfigFile
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Constructor.
 *
 * If pstrFilename is != NULL, this reads the given settings file into the member
 * variables and various substructures and lists. Otherwise, the member variables
 * are initialized with default values.
 *
 * Throws variants of xml::Error for I/O, XML and logical content errors, which
 * the caller should catch; if this constructor does not throw, then the member
 * variables contain meaningful values (either from the file or defaults).
 *
 * @param strFilename
 */
MachineConfigFile::MachineConfigFile(const Utf8Str *pstrFilename)
    : ConfigFileBase(pstrFilename),
      fNameSync(true),
      fTeleporterEnabled(false),
      uTeleporterPort(0),
      fCurrentStateModified(true),
      fAborted(false)
{
    RTTimeNow(&timeLastStateChange);

    if (pstrFilename)
    {
        // the ConfigFileBase constructor has loaded the XML file, so now
        // we need only analyze what is in there

        xml::NodesLoop nlRootChildren(*m->pelmRoot);
        const xml::ElementNode *pelmRootChild;
        while ((pelmRootChild = nlRootChildren.forAllNodes()))
        {
            if (pelmRootChild->nameEquals("Machine"))
                readMachine(*pelmRootChild);
        }

        // clean up memory allocated by XML engine
        clearDocument();
    }
}

/**
 * Creates a <Hardware> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 * @param elmParent
 * @param st
 */
void MachineConfigFile::writeHardware(xml::ElementNode &elmParent,
                                      const Hardware &hw,
                                      const Storage &strg)
{
    xml::ElementNode *pelmHardware = elmParent.createChild("Hardware");

    if (m->sv >= SettingsVersion_v1_4)
        pelmHardware->setAttribute("version", hw.strVersion);
    if (    (m->sv >= SettingsVersion_v1_9)
         && (!hw.uuid.isEmpty())
       )
        pelmHardware->setAttribute("uuid", makeString(hw.uuid));

    xml::ElementNode *pelmCPU      = pelmHardware->createChild("CPU");

    xml::ElementNode *pelmHwVirtEx = pelmCPU->createChild("HardwareVirtEx");
    pelmHwVirtEx->setAttribute("enabled", hw.fHardwareVirt);
    if (m->sv >= SettingsVersion_v1_9)
        pelmHwVirtEx->setAttribute("exclusive", hw.fHardwareVirtExclusive);

    pelmCPU->createChild("HardwareVirtExNestedPaging")->setAttribute("enabled", hw.fNestedPaging);
    pelmCPU->createChild("HardwareVirtExVPID")->setAttribute("enabled", hw.fVPID);
    pelmCPU->createChild("PAE")->setAttribute("enabled", hw.fPAE);

    if (hw.fSyntheticCpu)
        pelmCPU->createChild("SyntheticCpu")->setAttribute("enabled", hw.fSyntheticCpu);
    pelmCPU->setAttribute("count", hw.cCPUs);
    xml::ElementNode *pelmCpuIdTree = pelmCPU->createChild("CpuIdTree");
    for (CpuIdLeafsList::const_iterator it = hw.llCpuIdLeafs.begin();
         it != hw.llCpuIdLeafs.end();
         ++it)
    {
        const CpuIdLeaf &leaf = *it;

        xml::ElementNode *pelmCpuIdLeaf = pelmCpuIdTree->createChild("CpuIdLeaf");
        pelmCpuIdLeaf->setAttribute("id",  leaf.ulId);
        pelmCpuIdLeaf->setAttribute("eax", leaf.ulEax);
        pelmCpuIdLeaf->setAttribute("ebx", leaf.ulEbx);
        pelmCpuIdLeaf->setAttribute("ecx", leaf.ulEcx);
        pelmCpuIdLeaf->setAttribute("edx", leaf.ulEdx);
    }

    xml::ElementNode *pelmMemory = pelmHardware->createChild("Memory");
    pelmMemory->setAttribute("RAMSize", hw.ulMemorySizeMB);

    if (    (m->sv >= SettingsVersion_v1_9)
         && (hw.firmwareType >= FirmwareType_EFI)
       )
    {
         xml::ElementNode *pelmFirmware = pelmHardware->createChild("Firmware");
         const char *pcszFirmware;

         switch (hw.firmwareType)
         {
            case FirmwareType_EFI:      pcszFirmware = "EFI";   break;
            case FirmwareType_EFI64:    pcszFirmware = "EFI64"; break;
            case FirmwareType_EFIDUAL:  pcszFirmware = "EFIDUAL"; break;
            default:                    pcszFirmware = "None"; break;
         }
         pelmFirmware->setAttribute("type", pcszFirmware);
    }

    xml::ElementNode *pelmBoot = pelmHardware->createChild("Boot");
    for (BootOrderMap::const_iterator it = hw.mapBootOrder.begin();
         it != hw.mapBootOrder.end();
         ++it)
    {
        uint32_t i = it->first;
        DeviceType_T type = it->second;
        const char *pcszDevice;

        switch (type)
        {
            case DeviceType_Floppy:     pcszDevice = "Floppy"; break;
            case DeviceType_DVD:        pcszDevice = "DVD"; break;
            case DeviceType_HardDisk:   pcszDevice = "HardDisk"; break;
            case DeviceType_Network:    pcszDevice = "Network"; break;
            default: /*case DeviceType_Null:*/      pcszDevice = "None"; break;
        }

        xml::ElementNode *pelmOrder = pelmBoot->createChild("Order");
        pelmOrder->setAttribute("position",
                                i + 1);   // XML is 1-based but internal data is 0-based
        pelmOrder->setAttribute("device", pcszDevice);
    }

    xml::ElementNode *pelmDisplay = pelmHardware->createChild("Display");
    pelmDisplay->setAttribute("VRAMSize", hw.ulVRAMSizeMB);
    pelmDisplay->setAttribute("monitorCount", hw.cMonitors);
    pelmDisplay->setAttribute("accelerate3D", hw.fAccelerate3D);

    if (m->sv >= SettingsVersion_v1_8)
        pelmDisplay->setAttribute("accelerate2DVideo", hw.fAccelerate2DVideo);

    xml::ElementNode *pelmVRDP = pelmHardware->createChild("RemoteDisplay");
    pelmVRDP->setAttribute("enabled", hw.vrdpSettings.fEnabled);
    pelmVRDP->setAttribute("port", hw.vrdpSettings.strPort);
    if (hw.vrdpSettings.strNetAddress.length())
        pelmVRDP->setAttribute("netAddress", hw.vrdpSettings.strNetAddress);
    const char *pcszAuthType;
    switch (hw.vrdpSettings.authType)
    {
        case VRDPAuthType_Guest:    pcszAuthType = "Guest";    break;
        case VRDPAuthType_External: pcszAuthType = "External"; break;
        default: /*case VRDPAuthType_Null:*/ pcszAuthType = "Null"; break;
    }
    pelmVRDP->setAttribute("authType", pcszAuthType);

    if (hw.vrdpSettings.ulAuthTimeout != 0)
        pelmVRDP->setAttribute("authTimeout", hw.vrdpSettings.ulAuthTimeout);
    if (hw.vrdpSettings.fAllowMultiConnection)
        pelmVRDP->setAttribute("allowMultiConnection", hw.vrdpSettings.fAllowMultiConnection);
    if (hw.vrdpSettings.fReuseSingleConnection)
        pelmVRDP->setAttribute("reuseSingleConnection", hw.vrdpSettings.fReuseSingleConnection);

    xml::ElementNode *pelmBIOS = pelmHardware->createChild("BIOS");
    pelmBIOS->createChild("ACPI")->setAttribute("enabled", hw.biosSettings.fACPIEnabled);
    pelmBIOS->createChild("IOAPIC")->setAttribute("enabled", hw.biosSettings.fIOAPICEnabled);

    xml::ElementNode *pelmLogo = pelmBIOS->createChild("Logo");
    pelmLogo->setAttribute("fadeIn", hw.biosSettings.fLogoFadeIn);
    pelmLogo->setAttribute("fadeOut", hw.biosSettings.fLogoFadeOut);
    pelmLogo->setAttribute("displayTime", hw.biosSettings.ulLogoDisplayTime);
    if (hw.biosSettings.strLogoImagePath.length())
        pelmLogo->setAttribute("imagePath", hw.biosSettings.strLogoImagePath);

    const char *pcszBootMenu;
    switch (hw.biosSettings.biosBootMenuMode)
    {
        case BIOSBootMenuMode_Disabled: pcszBootMenu = "Disabled"; break;
        case BIOSBootMenuMode_MenuOnly: pcszBootMenu = "MenuOnly"; break;
        default: /*BIOSBootMenuMode_MessageAndMenu*/ pcszBootMenu = "MessageAndMenu"; break;
    }
    pelmBIOS->createChild("BootMenu")->setAttribute("mode", pcszBootMenu);
    pelmBIOS->createChild("TimeOffset")->setAttribute("value", hw.biosSettings.llTimeOffset);
    pelmBIOS->createChild("PXEDebug")->setAttribute("enabled", hw.biosSettings.fPXEDebugEnabled);

    if (m->sv < SettingsVersion_v1_9)
    {
        // settings formats before 1.9 had separate DVDDrive and FloppyDrive items under Hardware;
        // run thru the storage controllers to see if we have a DVD or floppy drives
        size_t cDVDs = 0;
        size_t cFloppies = 0;

        xml::ElementNode *pelmDVD = pelmHardware->createChild("DVDDrive");
        xml::ElementNode *pelmFloppy = pelmHardware->createChild("FloppyDrive");

        for (StorageControllersList::const_iterator it = strg.llStorageControllers.begin();
             it != strg.llStorageControllers.end();
             ++it)
        {
            const StorageController &sctl = *it;
            // in old settings format, the DVD drive could only have been under the IDE controller
            if (sctl.storageBus == StorageBus_IDE)
            {
                for (AttachedDevicesList::const_iterator it2 = sctl.llAttachedDevices.begin();
                     it2 != sctl.llAttachedDevices.end();
                     ++it2)
                {
                    const AttachedDevice &att = *it2;
                    if (att.deviceType == DeviceType_DVD)
                    {
                        if (cDVDs > 0)
                            throw ConfigFileError(this, NULL, N_("Internal error: cannot save more than one DVD drive with old settings format"));

                        ++cDVDs;

                        pelmDVD->setAttribute("passthrough", att.fPassThrough);
                        if (!att.uuid.isEmpty())
                            pelmDVD->createChild("Image")->setAttribute("uuid", makeString(att.uuid));
                        else if (att.strHostDriveSrc.length())
                            pelmDVD->createChild("HostDrive")->setAttribute("src", att.strHostDriveSrc);
                    }
                }
            }
            else if (sctl.storageBus == StorageBus_Floppy)
            {
                size_t cFloppiesHere = sctl.llAttachedDevices.size();
                if (cFloppiesHere > 1)
                    throw ConfigFileError(this, NULL, N_("Internal error: floppy controller cannot have more than one device attachment"));
                if (cFloppiesHere)
                {
                    const AttachedDevice &att = sctl.llAttachedDevices.front();
                    pelmFloppy->setAttribute("enabled", true);
                    if (!att.uuid.isEmpty())
                        pelmFloppy->createChild("Image")->setAttribute("uuid", makeString(att.uuid));
                    else if (att.strHostDriveSrc.length())
                        pelmFloppy->createChild("HostDrive")->setAttribute("src", att.strHostDriveSrc);
                }

                cFloppies += cFloppiesHere;
            }
        }

        if (cFloppies == 0)
            pelmFloppy->setAttribute("enabled", false);
        else if (cFloppies > 1)
            throw ConfigFileError(this, NULL, N_("Internal error: cannot save more than one floppy drive with old settings format"));
    }

    xml::ElementNode *pelmUSB = pelmHardware->createChild("USBController");
    pelmUSB->setAttribute("enabled", hw.usbController.fEnabled);
    pelmUSB->setAttribute("enabledEhci", hw.usbController.fEnabledEHCI);

    writeUSBDeviceFilters(*pelmUSB,
                          hw.usbController.llDeviceFilters,
                          false);               // fHostMode

    xml::ElementNode *pelmNetwork = pelmHardware->createChild("Network");
    for (NetworkAdaptersList::const_iterator it = hw.llNetworkAdapters.begin();
         it != hw.llNetworkAdapters.end();
         ++it)
    {
        const NetworkAdapter &nic = *it;

        xml::ElementNode *pelmAdapter = pelmNetwork->createChild("Adapter");
        pelmAdapter->setAttribute("slot", nic.ulSlot);
        pelmAdapter->setAttribute("enabled", nic.fEnabled);
        pelmAdapter->setAttribute("MACAddress", nic.strMACAddress);
        pelmAdapter->setAttribute("cable", nic.fCableConnected);
        pelmAdapter->setAttribute("speed", nic.ulLineSpeed);
        if (nic.fTraceEnabled)
        {
            pelmAdapter->setAttribute("trace", nic.fTraceEnabled);
            pelmAdapter->setAttribute("tracefile", nic.strTraceFile);
        }

        const char *pcszType;
        switch (nic.type)
        {
            case NetworkAdapterType_Am79C973:   pcszType = "Am79C973"; break;
            case NetworkAdapterType_I82540EM:   pcszType = "82540EM"; break;
            case NetworkAdapterType_I82543GC:   pcszType = "82543GC"; break;
            case NetworkAdapterType_I82545EM:   pcszType = "82545EM"; break;
            case NetworkAdapterType_Virtio:     pcszType = "virtio"; break;
            default: /*case NetworkAdapterType_Am79C970A:*/  pcszType = "Am79C970A"; break;
        }
        pelmAdapter->setAttribute("type", pcszType);

        xml::ElementNode *pelmNAT;
        switch (nic.mode)
        {
            case NetworkAttachmentType_NAT:
                pelmNAT = pelmAdapter->createChild("NAT");
                if (nic.strName.length())
                    pelmNAT->setAttribute("network", nic.strName);
            break;

            case NetworkAttachmentType_Bridged:
                pelmAdapter->createChild("BridgedInterface")->setAttribute("name", nic.strName);
            break;

            case NetworkAttachmentType_Internal:
                pelmAdapter->createChild("InternalNetwork")->setAttribute("name", nic.strName);
            break;

            case NetworkAttachmentType_HostOnly:
                pelmAdapter->createChild("HostOnlyInterface")->setAttribute("name", nic.strName);
            break;

            default: /*case NetworkAttachmentType_Null:*/
            break;
        }
    }

    xml::ElementNode *pelmPorts = pelmHardware->createChild("UART");
    for (SerialPortsList::const_iterator it = hw.llSerialPorts.begin();
         it != hw.llSerialPorts.end();
         ++it)
    {
        const SerialPort &port = *it;
        xml::ElementNode *pelmPort = pelmPorts->createChild("Port");
        pelmPort->setAttribute("slot", port.ulSlot);
        pelmPort->setAttribute("enabled", port.fEnabled);
        pelmPort->setAttributeHex("IOBase", port.ulIOBase);
        pelmPort->setAttribute("IRQ", port.ulIRQ);

        const char *pcszHostMode;
        switch (port.portMode)
        {
            case PortMode_HostPipe: pcszHostMode = "HostPipe"; break;
            case PortMode_HostDevice: pcszHostMode = "HostDevice"; break;
            case PortMode_RawFile: pcszHostMode = "RawFile"; break;
            default: /*case PortMode_Disconnected:*/ pcszHostMode = "Disconnected"; break;
        }
        switch (port.portMode)
        {
            case PortMode_HostPipe:
                pelmPort->setAttribute("server", port.fServer);
                /* no break */
            case PortMode_HostDevice:
            case PortMode_RawFile:
                pelmPort->setAttribute("path", port.strPath);
                break;

            default:
                break;
        }
        pelmPort->setAttribute("hostMode", pcszHostMode);
    }

    pelmPorts = pelmHardware->createChild("LPT");
    for (ParallelPortsList::const_iterator it = hw.llParallelPorts.begin();
         it != hw.llParallelPorts.end();
         ++it)
    {
        const ParallelPort &port = *it;
        xml::ElementNode *pelmPort = pelmPorts->createChild("Port");
        pelmPort->setAttribute("slot", port.ulSlot);
        pelmPort->setAttribute("enabled", port.fEnabled);
        pelmPort->setAttributeHex("IOBase", port.ulIOBase);
        pelmPort->setAttribute("IRQ", port.ulIRQ);
        if (port.strPath.length())
            pelmPort->setAttribute("path", port.strPath);
    }

    xml::ElementNode *pelmAudio = pelmHardware->createChild("AudioAdapter");
    pelmAudio->setAttribute("controller", (hw.audioAdapter.controllerType == AudioControllerType_SB16) ? "SB16" : "AC97");

    const char *pcszDriver;
    switch (hw.audioAdapter.driverType)
    {
        case AudioDriverType_WinMM: pcszDriver = "WinMM"; break;
        case AudioDriverType_DirectSound: pcszDriver = "DirectSound"; break;
        case AudioDriverType_SolAudio: pcszDriver = "SolAudio"; break;
        case AudioDriverType_ALSA: pcszDriver = "ALSA"; break;
        case AudioDriverType_Pulse: pcszDriver = "Pulse"; break;
        case AudioDriverType_OSS: pcszDriver = "OSS"; break;
        case AudioDriverType_CoreAudio: pcszDriver = "CoreAudio"; break;
        case AudioDriverType_MMPM: pcszDriver = "MMPM"; break;
        default: /*case AudioDriverType_Null:*/ pcszDriver = "Null"; break;
    }
    pelmAudio->setAttribute("driver", pcszDriver);

    pelmAudio->setAttribute("enabled", hw.audioAdapter.fEnabled);

    xml::ElementNode *pelmSharedFolders = pelmHardware->createChild("SharedFolders");
    for (SharedFoldersList::const_iterator it = hw.llSharedFolders.begin();
         it != hw.llSharedFolders.end();
         ++it)
    {
        const SharedFolder &sf = *it;
        xml::ElementNode *pelmThis = pelmSharedFolders->createChild("SharedFolder");
        pelmThis->setAttribute("name", sf.strName);
        pelmThis->setAttribute("hostPath", sf.strHostPath);
        pelmThis->setAttribute("writable", sf.fWritable);
    }

    xml::ElementNode *pelmClip = pelmHardware->createChild("Clipboard");
    const char *pcszClip;
    switch (hw.clipboardMode)
    {
        case ClipboardMode_Disabled: pcszClip = "Disabled"; break;
        case ClipboardMode_HostToGuest: pcszClip = "HostToGuest"; break;
        case ClipboardMode_GuestToHost: pcszClip = "GuestToHost"; break;
        default: /*case ClipboardMode_Bidirectional:*/ pcszClip = "Bidirectional"; break;
    }
    pelmClip->setAttribute("mode", pcszClip);

    xml::ElementNode *pelmGuest = pelmHardware->createChild("Guest");
    pelmGuest->setAttribute("memoryBalloonSize", hw.ulMemoryBalloonSize);
    pelmGuest->setAttribute("statisticsUpdateInterval", hw.ulStatisticsUpdateInterval);

    xml::ElementNode *pelmGuestProps = pelmHardware->createChild("GuestProperties");
    for (GuestPropertiesList::const_iterator it = hw.llGuestProperties.begin();
         it != hw.llGuestProperties.end();
         ++it)
    {
        const GuestProperty &prop = *it;
        xml::ElementNode *pelmProp = pelmGuestProps->createChild("GuestProperty");
        pelmProp->setAttribute("name", prop.strName);
        pelmProp->setAttribute("value", prop.strValue);
        pelmProp->setAttribute("timestamp", prop.timestamp);
        pelmProp->setAttribute("flags", prop.strFlags);
    }

    if (hw.strNotificationPatterns.length())
        pelmGuestProps->setAttribute("notificationPatterns", hw.strNotificationPatterns);
}

/**
 * Creates a <StorageControllers> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 * @param elmParent
 * @param st
 */
void MachineConfigFile::writeStorageControllers(xml::ElementNode &elmParent,
                                                const Storage &st)
{
    xml::ElementNode *pelmStorageControllers = elmParent.createChild("StorageControllers");

    for (StorageControllersList::const_iterator it = st.llStorageControllers.begin();
         it != st.llStorageControllers.end();
         ++it)
    {
        const StorageController &sc = *it;

        if (    (m->sv < SettingsVersion_v1_9)
             && (sc.controllerType == StorageControllerType_I82078)
           )
            // floppy controller already got written into <Hardware>/<FloppyController> in writeHardware()
            // for pre-1.9 settings
            continue;

        xml::ElementNode *pelmController = pelmStorageControllers->createChild("StorageController");
        com::Utf8Str name = sc.strName.raw();
        //
        if (m->sv < SettingsVersion_v1_8)
        {
            // pre-1.8 settings use shorter controller names, they are
            // expanded when reading the settings
            if (name == "IDE Controller")
                name = "IDE";
            else if (name == "SATA Controller")
                name = "SATA";
            else if (name == "SCSI Controller")
                name = "SCSI";
        }
        pelmController->setAttribute("name", sc.strName);

        const char *pcszType;
        switch (sc.controllerType)
        {
            case StorageControllerType_IntelAhci: pcszType = "AHCI"; break;
            case StorageControllerType_LsiLogic: pcszType = "LsiLogic"; break;
            case StorageControllerType_BusLogic: pcszType = "BusLogic"; break;
            case StorageControllerType_PIIX4: pcszType = "PIIX4"; break;
            case StorageControllerType_ICH6: pcszType = "ICH6"; break;
            case StorageControllerType_I82078: pcszType = "I82078"; break;
            default: /*case StorageControllerType_PIIX3:*/ pcszType = "PIIX3"; break;
        }
        pelmController->setAttribute("type", pcszType);

        pelmController->setAttribute("PortCount", sc.ulPortCount);

        if (m->sv >= SettingsVersion_v1_9)
            if (sc.ulInstance)
                pelmController->setAttribute("Instance", sc.ulInstance);

        if (sc.controllerType == StorageControllerType_IntelAhci)
        {
            pelmController->setAttribute("IDE0MasterEmulationPort", sc.lIDE0MasterEmulationPort);
            pelmController->setAttribute("IDE0SlaveEmulationPort", sc.lIDE0SlaveEmulationPort);
            pelmController->setAttribute("IDE1MasterEmulationPort", sc.lIDE1MasterEmulationPort);
            pelmController->setAttribute("IDE1SlaveEmulationPort", sc.lIDE1SlaveEmulationPort);
        }

        for (AttachedDevicesList::const_iterator it2 = sc.llAttachedDevices.begin();
             it2 != sc.llAttachedDevices.end();
             ++it2)
        {
            const AttachedDevice &att = *it2;

            // For settings version before 1.9, DVDs and floppies are in hardware, not storage controllers,
            // so we shouldn't write them here; we only get here for DVDs though because we ruled out
            // the floppy controller at the top of the loop
            if (    att.deviceType == DeviceType_DVD
                 && m->sv < SettingsVersion_v1_9
               )
                continue;

            xml::ElementNode *pelmDevice = pelmController->createChild("AttachedDevice");

            pcszType = NULL;

            switch (att.deviceType)
            {
                case DeviceType_HardDisk:
                    pcszType = "HardDisk";
                break;

                case DeviceType_DVD:
                    pcszType = "DVD";
                    if (att.fPassThrough)
                        pelmDevice->setAttribute("passthrough", att.fPassThrough);
                break;

                case DeviceType_Floppy:
                    pcszType = "Floppy";
                break;
            }

            pelmDevice->setAttribute("type", pcszType);

            pelmDevice->setAttribute("port", att.lPort);
            pelmDevice->setAttribute("device", att.lDevice);

            if (!att.uuid.isEmpty())
                pelmDevice->createChild("Image")->setAttribute("uuid", makeString(att.uuid));
            else if (    (m->sv >= SettingsVersion_v1_9)
                      && (att.strHostDriveSrc.length())
                    )
                pelmDevice->createChild("HostDrive")->setAttribute("src", att.strHostDriveSrc);
        }
    }
}

/**
 * Writes a single snapshot into the DOM tree. Initially this gets called from MachineConfigFile::write()
 * for the root snapshot of a machine, if present; elmParent then points to the <Snapshots> node under the
 * <Machine> node to which <Snapshot> must be added. This may then recurse for child snapshots.
 * @param elmParent
 * @param snap
 */
void MachineConfigFile::writeSnapshot(xml::ElementNode &elmParent,
                                      const Snapshot &snap)
{
    xml::ElementNode *pelmSnapshot = elmParent.createChild("Snapshot");

    pelmSnapshot->setAttribute("uuid", makeString(snap.uuid));
    pelmSnapshot->setAttribute("name", snap.strName);
    pelmSnapshot->setAttribute("timeStamp", makeString(snap.timestamp));

    if (snap.strStateFile.length())
        pelmSnapshot->setAttribute("stateFile", snap.strStateFile);

    if (snap.strDescription.length())
        pelmSnapshot->createChild("Description")->addContent(snap.strDescription);

    writeHardware(*pelmSnapshot, snap.hardware, snap.storage);
    writeStorageControllers(*pelmSnapshot, snap.storage);

    if (snap.llChildSnapshots.size())
    {
        xml::ElementNode *pelmChildren = pelmSnapshot->createChild("Snapshots");
        for (SnapshotsList::const_iterator it = snap.llChildSnapshots.begin();
             it != snap.llChildSnapshots.end();
             ++it)
        {
            const Snapshot &child = *it;
            writeSnapshot(*pelmChildren, child);
        }
    }
}

/**
 * Called from write() before calling ConfigFileBase::createStubDocument().
 * This adjusts the settings version in m->sv if incompatible settings require
 * a settings bump, whereas otherwise we try to preserve the settings version
 * to avoid breaking compatibility with older versions.
 */
void MachineConfigFile::bumpSettingsVersionIfNeeded()
{
    // The hardware versions other than "1" requires settings version 1.4 (2.1+).
    if (    m->sv < SettingsVersion_v1_4
         && hardwareMachine.strVersion != "1"
       )
        m->sv = SettingsVersion_v1_4;

    // "accelerate 2d video" requires settings version 1.8
    if (    (m->sv < SettingsVersion_v1_8)
         && (hardwareMachine.fAccelerate2DVideo)
       )
        m->sv = SettingsVersion_v1_8;

    // all the following require settings version 1.9
    if (    (m->sv < SettingsVersion_v1_9)
         && (    (hardwareMachine.firmwareType >= FirmwareType_EFI)
              || (hardwareMachine.fHardwareVirtExclusive != HWVIRTEXCLUSIVEDEFAULT)
              || fTeleporterEnabled
              || uTeleporterPort
              || !strTeleporterAddress.isEmpty()
              || !strTeleporterPassword.isEmpty()
              || !hardwareMachine.uuid.isEmpty()
            )
        )
        m->sv = SettingsVersion_v1_9;

    // settings version 1.9 is also required if there is not exactly one DVD
    // or more than one floppy drive present or the DVD is not at the secondary
    // master; this check is a bit more complicated
    if (m->sv < SettingsVersion_v1_9)
    {
        size_t cDVDs = 0;
        size_t cFloppies = 0;

        // need to run thru all the storage controllers to figure this out
        for (StorageControllersList::const_iterator it = storageMachine.llStorageControllers.begin();
             it != storageMachine.llStorageControllers.end()
                && m->sv < SettingsVersion_v1_9;
             ++it)
        {
            const StorageController &sctl = *it;
            for (AttachedDevicesList::const_iterator it2 = sctl.llAttachedDevices.begin();
                 it2 != sctl.llAttachedDevices.end();
                 ++it2)
            {
                if (sctl.ulInstance != 0)       // we can only write the StorageController/@Instance attribute with v1.9
                {
                    m->sv = SettingsVersion_v1_9;
                    break;
                }

                const AttachedDevice &att = *it2;
                if (att.deviceType == DeviceType_DVD)
                {
                    if (    (sctl.storageBus != StorageBus_IDE) // DVD at bus other than DVD?
                         || (att.lPort != 1)                    // DVDs not at secondary master?
                         || (att.lDevice != 0)
                       )
                    {
                        m->sv = SettingsVersion_v1_9;
                        break;
                    }

                    ++cDVDs;
                }
                else if (att.deviceType == DeviceType_Floppy)
                    ++cFloppies;
            }
        }

        // VirtualBox before 3.1 had zero or one floppy and exactly one DVD,
        // so any deviation from that will require settings version 1.9
        if (    (m->sv < SettingsVersion_v1_9)
             && (    (cDVDs != 1)
                  || (cFloppies > 1)
                )
           )
            m->sv = SettingsVersion_v1_9;
    }
}

/**
 * Called from Main code to write a machine config file to disk. This builds a DOM tree from
 * the member variables and then writes the XML file; it throws xml::Error instances on errors,
 * in particular if the file cannot be written.
 */
void MachineConfigFile::write(const com::Utf8Str &strFilename)
{
    try
    {
        // createStubDocument() sets the settings version to at least 1.7; however,
        // we might need to enfore a later settings version if incompatible settings
        // are present:
        bumpSettingsVersionIfNeeded();

        m->strFilename = strFilename;
        createStubDocument();

        xml::ElementNode *pelmMachine = m->pelmRoot->createChild("Machine");

        pelmMachine->setAttribute("uuid", makeString(uuid));
        pelmMachine->setAttribute("name", strName);
        if (!fNameSync)
            pelmMachine->setAttribute("nameSync", fNameSync);
        if (strDescription.length())
            pelmMachine->createChild("Description")->addContent(strDescription);
        pelmMachine->setAttribute("OSType", strOsType);
        if (strStateFile.length())
            pelmMachine->setAttribute("stateFile", strStateFile);
        if (!uuidCurrentSnapshot.isEmpty())
            pelmMachine->setAttribute("currentSnapshot", makeString(uuidCurrentSnapshot));
        if (strSnapshotFolder.length())
            pelmMachine->setAttribute("snapshotFolder", strSnapshotFolder);
        if (!fCurrentStateModified)
            pelmMachine->setAttribute("currentStateModified", fCurrentStateModified);
        pelmMachine->setAttribute("lastStateChange", makeString(timeLastStateChange));
        if (fAborted)
            pelmMachine->setAttribute("aborted", fAborted);
        if (    m->sv >= SettingsVersion_v1_9
            &&  (   fTeleporterEnabled
                 || uTeleporterPort
                 || !strTeleporterAddress.isEmpty()
                 || !strTeleporterPassword.isEmpty()
                )
           )
        {
           xml::ElementNode *pelmTeleporter = pelmMachine->createChild("Teleporter");
           pelmTeleporter->setAttribute("enabled", fTeleporterEnabled);
           pelmTeleporter->setAttribute("port", uTeleporterPort);
           pelmTeleporter->setAttribute("address", strTeleporterAddress);
           pelmTeleporter->setAttribute("password", strTeleporterPassword);
        }

        writeExtraData(*pelmMachine, mapExtraDataItems);

        if (llFirstSnapshot.size())
            writeSnapshot(*pelmMachine, llFirstSnapshot.front());

        writeHardware(*pelmMachine, hardwareMachine, storageMachine);
        writeStorageControllers(*pelmMachine, storageMachine);

        // now go write the XML
        xml::XmlFileWriter writer(*m->pDoc);
        writer.write(m->strFilename.c_str());

        m->fFileExists = true;
        clearDocument();
    }
    catch (...)
    {
        clearDocument();
        throw;
    }
}
