/* $Id$ */
/** @file
 * Settings File Manipulation API.
 *
 * Two classes, MainConfigFile and MachineConfigFile, represent the VirtualBox.xml and
 * machine XML files. They share a common ancestor class, ConfigFileBase, which shares
 * functionality such as talking to the XML back-end classes and settings version management.
 *
 * The code can read all VirtualBox settings files version 1.3 and higher. That version was
 * written by VirtualBox 2.0. It can write settings version 1.7 (used by VirtualBox 2.2 and
 * 3.0) and 1.9 (used by VirtualBox 3.1) and newer ones obviously.
 *
 * The settings versions enum is defined in src/VBox/Main/idl/VirtualBox.xidl. To introduce
 * a new settings version (should be necessary at most once per VirtualBox major release,
 * if at all), add a new SettingsVersion value to that enum and grep for the previously
 * highest value to see which code in here needs adjusting.
 *
 * Certainly ConfigFileBase::ConfigFileBase() will. Change VBOX_XML_VERSION below as well.
 * VBOX_XML_VERSION does not have to be changed if the settings for a default VM do not
 * touch newly introduced attributes or tags. It has the benefit that older VirtualBox
 * versions do not trigger their "newer" code path.
 *
 * Once a new settings version has been added, these are the rules for introducing a new
 * setting: If an XML element or attribute or value is introduced that was not present in
 * previous versions, then settings version checks need to be introduced. See the
 * SettingsVersion enumeration in src/VBox/Main/idl/VirtualBox.xidl for details about which
 * version was used when.
 *
 * The settings versions checks are necessary because since version 3.1, VirtualBox no longer
 * automatically converts XML settings files but only if necessary, that is, if settings are
 * present that the old format does not support. If we write an element or attribute to a
 * settings file of an older version, then an old VirtualBox (before 3.1) will attempt to
 * validate it with XML schema, and that will certainly fail.
 *
 * So, to introduce a new setting:
 *
 *   1) Make sure the constructor of corresponding settings structure has a proper default.
 *
 *   2) In the settings reader method, try to read the setting; if it's there, great, if not,
 *      the default value will have been set by the constructor. The rule is to be tolerant
 *      here.
 *
 *   3) In MachineConfigFile::bumpSettingsVersionIfNeeded(), check if the new setting has
 *      a non-default value (i.e. that differs from the constructor). If so, bump the
 *      settings version to the current version so the settings writer (4) can write out
 *      the non-default value properly.
 *
 *      So far a corresponding method for MainConfigFile has not been necessary since there
 *      have been no incompatible changes yet.
 *
 *   4) In the settings writer method, write the setting _only_ if the current settings
 *      version (stored in m->sv) is high enough. That is, for VirtualBox 4.0, write it
 *      only if (m->sv >= SettingsVersion_v1_11).
 */

/*
 * Copyright (C) 2007-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/com/string.h"
#include "VBox/settings.h"
#include <iprt/cpp/xml.h>
#include <iprt/stream.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/process.h>
#include <iprt/ldr.h>
#include <iprt/cpp/lock.h>

// generated header
#include "SchemaDefs.h"

#include "Logging.h"
#include "HashedPw.h"

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
#define VBOX_XML_VERSION        "1.12"

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
        : pDoc(NULL),
          pelmRoot(NULL),
          sv(SettingsVersion_Null),
          svRead(SettingsVersion_Null)
    {}

    ~Data()
    {
        cleanup();
    }

    RTCString        strFilename;
    bool                    fFileExists;

    xml::Document           *pDoc;
    xml::ElementNode        *pelmRoot;

    com::Utf8Str            strSettingsVersionFull;     // e.g. "1.7-linux"
    SettingsVersion_T       sv;                         // e.g. SettingsVersion_v1_7

    SettingsVersion_T       svRead;                     // settings version that the original file had when it was read,
                                                        // or SettingsVersion_Null if none

    void copyFrom(const Data &d)
    {
        strFilename = d.strFilename;
        fFileExists = d.fFileExists;
        strSettingsVersionFull = d.strSettingsVersionFull;
        sv = d.sv;
        svRead = d.svRead;
    }

    void cleanup()
    {
        if (pDoc)
        {
            delete pDoc;
            pDoc = NULL;
            pelmRoot = NULL;
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
        Utf8Str strWhat(pcszFormat, args);
        va_end(args);

        Utf8Str strLine;
        if (pNode)
            strLine = Utf8StrFmt(" (line %RU32)", pNode->getLineNumber());

        const char *pcsz = strLine.c_str();
        Utf8StrFmt str(N_("Error in %s%s -- %s"),
                       file->m->strFilename.c_str(),
                       (pcsz) ? pcsz : "",
                       strWhat.c_str());

        setWhat(str.c_str());
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// MediaRegistry
//
////////////////////////////////////////////////////////////////////////////////

bool Medium::operator==(const Medium &m) const
{
    return    (uuid == m.uuid)
           && (strLocation == m.strLocation)
           && (strDescription == m.strDescription)
           && (strFormat == m.strFormat)
           && (fAutoReset == m.fAutoReset)
           && (properties == m.properties)
           && (hdType == m.hdType)
           && (llChildren== m.llChildren);         // this is deep and recurses
}

bool MediaRegistry::operator==(const MediaRegistry &m) const
{
    return    llHardDisks == m.llHardDisks
           && llDvdImages == m.llDvdImages
           && llFloppyImages == m.llFloppyImages;
}

////////////////////////////////////////////////////////////////////////////////
//
// ConfigFileBase
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Constructor. Allocates the XML internals, parses the XML file if
 * pstrFilename is != NULL and reads the settings version from it.
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
        // reading existing settings file:
        m->strFilename = *pstrFilename;

        xml::XmlFileParser parser;
        m->pDoc = new xml::Document;
        parser.read(*pstrFilename,
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
                else if (ulMinor == 10)
                    m->sv = SettingsVersion_v1_10;
                else if (ulMinor == 11)
                    m->sv = SettingsVersion_v1_11;
                else if (ulMinor == 12)
                    m->sv = SettingsVersion_v1_12;
                else if (ulMinor == 13)
                    m->sv = SettingsVersion_v1_13;
                else if (ulMinor > 13)
                    m->sv = SettingsVersion_Future;
            }
            else if (ulMajor > 1)
                m->sv = SettingsVersion_Future;

            Log(("Parsed settings version %d.%d to enum value %d\n", ulMajor, ulMinor, m->sv));
        }

        if (m->sv == SettingsVersion_Null)
            throw ConfigFileError(this, m->pelmRoot, N_("Cannot handle settings version '%s'"), m->strSettingsVersionFull.c_str());

        // remember the settings version we read in case it gets upgraded later,
        // so we know when to make backups
        m->svRead = m->sv;
    }
    else
    {
        // creating new settings file:
        m->strSettingsVersionFull = VBOX_XML_VERSION_FULL;
        m->sv = SettingsVersion_v1_12;
    }
}

ConfigFileBase::ConfigFileBase(const ConfigFileBase &other)
    : m(new Data)
{
    copyBaseFrom(other);
    m->strFilename = "";
    m->fFileExists = false;
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
 * a com::Guid item. Accepts UUIDs both with and without
 * "{}" brackets. Throws on errors.
 * @param guid
 * @param strUUID
 */
void ConfigFileBase::parseUUID(Guid &guid,
                               const Utf8Str &strUUID) const
{
    guid = strUUID.c_str();
    if (guid.isZero())
        throw ConfigFileError(this, NULL, N_("UUID \"%s\" has zero format"), strUUID.c_str());
    else if (!guid.isValid())
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
                RTTIME time =
                {
                    yyyy,
                    (uint8_t)mm,
                    0,
                    0,
                    (uint8_t)dd,
                    (uint8_t)hh,
                    (uint8_t)min,
                    (uint8_t)secs,
                    0,
                    RTTIME_FLAGS_TYPE_UTC,
                    0
                };
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

    return Utf8StrFmt("%04u-%02u-%02uT%02u:%02u:%02uZ",
                      time.i32Year, time.u8Month, time.u8MonthDay,
                      time.u8Hour, time.u8Minute, time.u8Second);
}

/**
 * Helper method to read in an ExtraData subtree and stores its contents
 * in the given map of extradata items. Used for both main and machine
 * extradata (MainConfigFile and MachineConfigFile).
 * @param elmExtraData
 * @param map
 */
void ConfigFileBase::readExtraData(const xml::ElementNode &elmExtraData,
                                   StringsMap &map)
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
void ConfigFileBase::readMedium(MediaType t,
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
            {
                if (!(pelmImage->getAttributeValuePath("filePath", med.strLocation)))
                    throw ConfigFileError(this, &elmMedium, N_("Required %s/@filePath attribute is missing"), elmMedium.getName());
            }
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
            else if (strType == "SHAREABLE")
                med.hdType = MediumType_Shareable;
            else if (strType == "READONLY")
                med.hdType = MediumType_Readonly;
            else if (strType == "MULTIATTACH")
                med.hdType = MediumType_MultiAttach;
            else
                throw ConfigFileError(this, &elmMedium, N_("HardDisk/@type attribute must be one of Normal, Immutable, Writethrough, Shareable, Readonly or MultiAttach"));
        }
    }
    else
    {
        if (m->sv < SettingsVersion_v1_4)
        {
            // DVD and floppy images before 1.4 had "src" attribute instead of "location"
            if (!(elmMedium.getAttributeValue("src", med.strLocation)))
                throw ConfigFileError(this, &elmMedium, N_("Required %s/@src attribute is missing"), elmMedium.getName());

            fNeedsLocation = false;
        }

        if (!(elmMedium.getAttributeValue("format", med.strFormat)))
        {
            // DVD and floppy images before 1.11 had no format attribute. assign the default.
            med.strFormat = "RAW";
        }

        if (t == DVDImage)
            med.hdType = MediumType_Readonly;
        else if (t == FloppyImage)
            med.hdType = MediumType_Writethrough;
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
 * Reads in the entire <MediaRegistry> chunk and stores its media in the lists
 * of the given MediaRegistry structure.
 *
 * This is used in both MainConfigFile and MachineConfigFile since starting with
 * VirtualBox 4.0, we can have media registries in both.
 *
 * For pre-1.4 files, this gets called with the <DiskRegistry> chunk instead.
 *
 * @param elmMediaRegistry
 */
void ConfigFileBase::readMediaRegistry(const xml::ElementNode &elmMediaRegistry,
                                       MediaRegistry &mr)
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
                           mr.llHardDisks);      // list to append hard disk data to: the root list
            else if (    t == DVDImage
                      && (pelmMedium->nameEquals("Image"))
                    )
                readMedium(t,
                           *pelmMedium,
                           mr.llDvdImages);      // list to append dvd images to: the root list
            else if (    t == FloppyImage
                      && (pelmMedium->nameEquals("Image"))
                    )
                readMedium(t,
                           *pelmMedium,
                           mr.llFloppyImages);      // list to append floppy images to: the root list
        }
    }
}

/**
 * Adds a "version" attribute to the given XML element with the
 * VirtualBox settings version (e.g. "1.10-linux"). Used by
 * the XML format for the root element and by the OVF export
 * for the vbox:Machine element.
 * @param elm
 */
void ConfigFileBase::setVersionAttribute(xml::ElementNode &elm)
{
    const char *pcszVersion = NULL;
    switch (m->sv)
    {
        case SettingsVersion_v1_8:
            pcszVersion = "1.8";
            break;

        case SettingsVersion_v1_9:
            pcszVersion = "1.9";
            break;

        case SettingsVersion_v1_10:
            pcszVersion = "1.10";
            break;

        case SettingsVersion_v1_11:
            pcszVersion = "1.11";
            break;

        case SettingsVersion_v1_12:
            pcszVersion = "1.12";
            break;

        case SettingsVersion_v1_13:
            pcszVersion = "1.13";
            break;

        case SettingsVersion_Future:
            // can be set if this code runs on XML files that were created by a future version of VBox;
            // in that case, downgrade to current version when writing since we can't write future versions...
            pcszVersion = "1.13";
            m->sv = SettingsVersion_v1_13;
            break;

        default:
            // silently upgrade if this is less than 1.7 because that's the oldest we can write
            pcszVersion = "1.7";
            m->sv = SettingsVersion_v1_7;
            break;
    }

    elm.setAttribute("version", Utf8StrFmt("%s-%s",
                                           pcszVersion,
                                           VBOX_XML_PLATFORM));       // e.g. "linux"
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
 * the latest (1.12); for files read in from disk earlier, it will be
 * the settings version indicated in the file. However, this method
 * will silently make sure that the settings version is always
 * at least 1.7 and change it if necessary, since there is no write
 * support for earlier settings versions.
 */
void ConfigFileBase::createStubDocument()
{
    Assert(m->pDoc == NULL);
    m->pDoc = new xml::Document;

    m->pelmRoot = m->pDoc->createRootElement("VirtualBox",
                                             "\n"
                                             "** DO NOT EDIT THIS FILE.\n"
                                             "** If you make changes to this file while any VirtualBox related application\n"
                                             "** is running, your changes will be overwritten later, without taking effect.\n"
                                             "** Use VBoxManage or the VirtualBox Manager GUI to make changes.\n"
);
    m->pelmRoot->setAttribute("xmlns", VBOX_XML_NAMESPACE);

    // add settings version attribute to root element
    setVersionAttribute(*m->pelmRoot);

    // since this gets called before the XML document is actually written out,
    // this is where we must check whether we're upgrading the settings version
    // and need to make a backup, so the user can go back to an earlier
    // VirtualBox version and recover his old settings files.
    if (    (m->svRead != SettingsVersion_Null)     // old file exists?
         && (m->svRead < m->sv)                     // we're upgrading?
       )
    {
        // compose new filename: strip off trailing ".xml"/".vbox"
        Utf8Str strFilenameNew;
        Utf8Str strExt = ".xml";
        if (m->strFilename.endsWith(".xml"))
            strFilenameNew = m->strFilename.substr(0, m->strFilename.length() - 4);
        else if (m->strFilename.endsWith(".vbox"))
        {
            strFilenameNew = m->strFilename.substr(0, m->strFilename.length() - 5);
            strExt = ".vbox";
        }

        // and append something like "-1.3-linux.xml"
        strFilenameNew.append("-");
        strFilenameNew.append(m->strSettingsVersionFull);       // e.g. "1.3-linux"
        strFilenameNew.append(strExt);                          // .xml for main config, .vbox for machine config

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
 *
 * This is in ConfigFileBase because it's used in both MainConfigFile
 * and MachineConfigFile, which both can have extradata.
 *
 * @param elmParent
 * @param me
 */
void ConfigFileBase::buildExtraData(xml::ElementNode &elmParent,
                                    const StringsMap &me)
{
    if (me.size())
    {
        xml::ElementNode *pelmExtraData = elmParent.createChild("ExtraData");
        for (StringsMap::const_iterator it = me.begin();
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
void ConfigFileBase::buildUSBDeviceFilters(xml::ElementNode &elmParent,
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
 * Creates a single <HardDisk> element for the given Medium structure
 * and recurses to write the child hard disks underneath. Called from
 * MainConfigFile::write().
 *
 * @param elmMedium
 * @param m
 * @param level
 */
void ConfigFileBase::buildMedium(xml::ElementNode &elmMedium,
                                 DeviceType_T devType,
                                 const Medium &mdm,
                                 uint32_t level)          // 0 for "root" call, incremented with each recursion
{
    xml::ElementNode *pelmMedium;

    if (devType == DeviceType_HardDisk)
        pelmMedium = elmMedium.createChild("HardDisk");
    else
        pelmMedium = elmMedium.createChild("Image");

    pelmMedium->setAttribute("uuid", mdm.uuid.toStringCurly());

    pelmMedium->setAttributePath("location", mdm.strLocation);

    if (devType == DeviceType_HardDisk || RTStrICmp(mdm.strFormat.c_str(), "RAW"))
        pelmMedium->setAttribute("format", mdm.strFormat);
    if (   devType == DeviceType_HardDisk
        && mdm.fAutoReset)
        pelmMedium->setAttribute("autoReset", mdm.fAutoReset);
    if (mdm.strDescription.length())
        pelmMedium->setAttribute("Description", mdm.strDescription);

    for (StringsMap::const_iterator it = mdm.properties.begin();
         it != mdm.properties.end();
         ++it)
    {
        xml::ElementNode *pelmProp = pelmMedium->createChild("Property");
        pelmProp->setAttribute("name", it->first);
        pelmProp->setAttribute("value", it->second);
    }

    // only for base hard disks, save the type
    if (level == 0)
    {
        // no need to save the usual DVD/floppy medium types
        if (   (   devType != DeviceType_DVD
                || (   mdm.hdType != MediumType_Writethrough // shouldn't happen
                    && mdm.hdType != MediumType_Readonly))
            && (   devType != DeviceType_Floppy
                || mdm.hdType != MediumType_Writethrough))
        {
            const char *pcszType =
                mdm.hdType == MediumType_Normal ? "Normal" :
                mdm.hdType == MediumType_Immutable ? "Immutable" :
                mdm.hdType == MediumType_Writethrough ? "Writethrough" :
                mdm.hdType == MediumType_Shareable ? "Shareable" :
                mdm.hdType == MediumType_Readonly ? "Readonly" :
                mdm.hdType == MediumType_MultiAttach ? "MultiAttach" :
                "INVALID";
            pelmMedium->setAttribute("type", pcszType);
        }
    }

    for (MediaList::const_iterator it = mdm.llChildren.begin();
         it != mdm.llChildren.end();
         ++it)
    {
        // recurse for children
        buildMedium(*pelmMedium, // parent
                    devType,     // device type
                    *it,         // settings::Medium
                    ++level);    // recursion level
    }
}

/**
 * Creates a <MediaRegistry> node under the given parent and writes out all
 * hard disks and DVD and floppy images from the lists in the given MediaRegistry
 * structure under it.
 *
 * This is used in both MainConfigFile and MachineConfigFile since starting with
 * VirtualBox 4.0, we can have media registries in both.
 *
 * @param elmParent
 * @param mr
 */
void ConfigFileBase::buildMediaRegistry(xml::ElementNode &elmParent,
                                        const MediaRegistry &mr)
{
    xml::ElementNode *pelmMediaRegistry = elmParent.createChild("MediaRegistry");

    xml::ElementNode *pelmHardDisks = pelmMediaRegistry->createChild("HardDisks");
    for (MediaList::const_iterator it = mr.llHardDisks.begin();
         it != mr.llHardDisks.end();
         ++it)
    {
        buildMedium(*pelmHardDisks, DeviceType_HardDisk, *it, 0);
    }

    xml::ElementNode *pelmDVDImages = pelmMediaRegistry->createChild("DVDImages");
    for (MediaList::const_iterator it = mr.llDvdImages.begin();
         it != mr.llDvdImages.end();
         ++it)
    {
        buildMedium(*pelmDVDImages, DeviceType_DVD, *it, 0);
    }

    xml::ElementNode *pelmFloppyImages = pelmMediaRegistry->createChild("FloppyImages");
    for (MediaList::const_iterator it = mr.llFloppyImages.begin();
         it != mr.llFloppyImages.end();
         ++it)
    {
        buildMedium(*pelmFloppyImages, DeviceType_Floppy, *it, 0);
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

/**
 * Copies the base variables from another instance. Used by Machine::saveSettings
 * so that the settings version does not get lost when a copy of the Machine settings
 * file is made to see if settings have actually changed.
 * @param b
 */
void ConfigFileBase::copyBaseFrom(const ConfigFileBase &b)
{
    m->copyFrom(*b.m);
}

////////////////////////////////////////////////////////////////////////////////
//
// Structures shared between Machine XML and VirtualBox.xml
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool USBDeviceFilter::operator==(const USBDeviceFilter &u) const
{
    return (    (this == &u)
             || (    (strName           == u.strName)
                  && (fActive           == u.fActive)
                  && (strVendorId       == u.strVendorId)
                  && (strProductId      == u.strProductId)
                  && (strRevision       == u.strRevision)
                  && (strManufacturer   == u.strManufacturer)
                  && (strProduct        == u.strProduct)
                  && (strSerialNumber   == u.strSerialNumber)
                  && (strPort           == u.strPort)
                  && (action            == u.action)
                  && (strRemote         == u.strRemote)
                  && (ulMaskedInterfaces == u.ulMaskedInterfaces)
                )
           );
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
                        pelmGlobalChild->getAttributeValue("defaultHardDiskFormat", systemProperties.strDefaultHardDiskFormat);
                        if (!pelmGlobalChild->getAttributeValue("VRDEAuthLibrary", systemProperties.strVRDEAuthLibrary))
                            // pre-1.11 used @remoteDisplayAuthLibrary instead
                            pelmGlobalChild->getAttributeValue("remoteDisplayAuthLibrary", systemProperties.strVRDEAuthLibrary);
                        pelmGlobalChild->getAttributeValue("webServiceAuthLibrary", systemProperties.strWebServiceAuthLibrary);
                        pelmGlobalChild->getAttributeValue("defaultVRDEExtPack", systemProperties.strDefaultVRDEExtPack);
                        pelmGlobalChild->getAttributeValue("LogHistoryCount", systemProperties.ulLogHistoryCount);
                        pelmGlobalChild->getAttributeValue("autostartDatabasePath", systemProperties.strAutostartDatabasePath);
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
                        readMediaRegistry(*pelmGlobalChild, mediaRegistry);
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
 * Called from the IVirtualBox interface to write out VirtualBox.xml. This
 * builds an XML DOM tree and writes it out to disk.
 */
void MainConfigFile::write(const com::Utf8Str strFilename)
{
    m->strFilename = strFilename;
    createStubDocument();

    xml::ElementNode *pelmGlobal = m->pelmRoot->createChild("Global");

    buildExtraData(*pelmGlobal, mapExtraDataItems);

    xml::ElementNode *pelmMachineRegistry = pelmGlobal->createChild("MachineRegistry");
    for (MachinesRegistry::const_iterator it = llMachines.begin();
         it != llMachines.end();
         ++it)
    {
        // <MachineEntry uuid="{5f102a55-a51b-48e3-b45a-b28d33469488}" src="/mnt/innotek-unix/vbox-machines/Windows 5.1 XP 1 (Office 2003)/Windows 5.1 XP 1 (Office 2003).xml"/>
        const MachineRegistryEntry &mre = *it;
        xml::ElementNode *pelmMachineEntry = pelmMachineRegistry->createChild("MachineEntry");
        pelmMachineEntry->setAttribute("uuid", mre.uuid.toStringCurly());
        pelmMachineEntry->setAttribute("src", mre.strSettingsFile);
    }

    buildMediaRegistry(*pelmGlobal, mediaRegistry);

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
    if (systemProperties.strDefaultHardDiskFormat.length())
        pelmSysProps->setAttribute("defaultHardDiskFormat", systemProperties.strDefaultHardDiskFormat);
    if (systemProperties.strVRDEAuthLibrary.length())
        pelmSysProps->setAttribute("VRDEAuthLibrary", systemProperties.strVRDEAuthLibrary);
    if (systemProperties.strWebServiceAuthLibrary.length())
        pelmSysProps->setAttribute("webServiceAuthLibrary", systemProperties.strWebServiceAuthLibrary);
    if (systemProperties.strDefaultVRDEExtPack.length())
        pelmSysProps->setAttribute("defaultVRDEExtPack", systemProperties.strDefaultVRDEExtPack);
    pelmSysProps->setAttribute("LogHistoryCount", systemProperties.ulLogHistoryCount);
    if (systemProperties.strAutostartDatabasePath.length())
        pelmSysProps->setAttribute("autostartDatabasePath", systemProperties.strAutostartDatabasePath);

    buildUSBDeviceFilters(*pelmGlobal->createChild("USBDeviceFilters"),
                          host.llUSBDeviceFilters,
                          true);               // fHostMode

    // now go write the XML
    xml::XmlFileWriter writer(*m->pDoc);
    writer.write(m->strFilename.c_str(), true /*fSafe*/);

    m->fFileExists = true;

    clearDocument();
}

////////////////////////////////////////////////////////////////////////////////
//
// Machine XML structures
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool VRDESettings::operator==(const VRDESettings& v) const
{
    return (    (this == &v)
             || (    (fEnabled                  == v.fEnabled)
                  && (authType                  == v.authType)
                  && (ulAuthTimeout             == v.ulAuthTimeout)
                  && (strAuthLibrary            == v.strAuthLibrary)
                  && (fAllowMultiConnection     == v.fAllowMultiConnection)
                  && (fReuseSingleConnection    == v.fReuseSingleConnection)
                  && (strVrdeExtPack            == v.strVrdeExtPack)
                  && (mapProperties             == v.mapProperties)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool BIOSSettings::operator==(const BIOSSettings &d) const
{
    return (    (this == &d)
             || (    fACPIEnabled        == d.fACPIEnabled
                  && fIOAPICEnabled      == d.fIOAPICEnabled
                  && fLogoFadeIn         == d.fLogoFadeIn
                  && fLogoFadeOut        == d.fLogoFadeOut
                  && ulLogoDisplayTime   == d.ulLogoDisplayTime
                  && strLogoImagePath    == d.strLogoImagePath
                  && biosBootMenuMode    == d.biosBootMenuMode
                  && fPXEDebugEnabled    == d.fPXEDebugEnabled
                  && llTimeOffset        == d.llTimeOffset)
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool USBController::operator==(const USBController &u) const
{
    return (    (this == &u)
                 || (    (fEnabled          == u.fEnabled)
                      && (fEnabledEHCI      == u.fEnabledEHCI)
                      && (llDeviceFilters   == u.llDeviceFilters)
                    )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool NetworkAdapter::operator==(const NetworkAdapter &n) const
{
    return (    (this == &n)
             || (    (ulSlot                == n.ulSlot)
                  && (type                  == n.type)
                  && (fEnabled              == n.fEnabled)
                  && (strMACAddress         == n.strMACAddress)
                  && (fCableConnected       == n.fCableConnected)
                  && (ulLineSpeed           == n.ulLineSpeed)
                  && (enmPromiscModePolicy  == n.enmPromiscModePolicy)
                  && (fTraceEnabled         == n.fTraceEnabled)
                  && (strTraceFile          == n.strTraceFile)
                  && (mode                  == n.mode)
                  && (nat                   == n.nat)
                  && (strBridgedName        == n.strBridgedName)
                  && (strHostOnlyName       == n.strHostOnlyName)
                  && (strInternalNetworkName == n.strInternalNetworkName)
                  && (strGenericDriver      == n.strGenericDriver)
                  && (genericProperties     == n.genericProperties)
                  && (ulBootPriority        == n.ulBootPriority)
                  && (strBandwidthGroup     == n.strBandwidthGroup)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool SerialPort::operator==(const SerialPort &s) const
{
    return (    (this == &s)
             || (    (ulSlot            == s.ulSlot)
                  && (fEnabled          == s.fEnabled)
                  && (ulIOBase          == s.ulIOBase)
                  && (ulIRQ             == s.ulIRQ)
                  && (portMode          == s.portMode)
                  && (strPath           == s.strPath)
                  && (fServer           == s.fServer)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool ParallelPort::operator==(const ParallelPort &s) const
{
    return (    (this == &s)
             || (    (ulSlot            == s.ulSlot)
                  && (fEnabled          == s.fEnabled)
                  && (ulIOBase          == s.ulIOBase)
                  && (ulIRQ             == s.ulIRQ)
                  && (strPath           == s.strPath)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool SharedFolder::operator==(const SharedFolder &g) const
{
    return (    (this == &g)
             || (    (strName       == g.strName)
                  && (strHostPath   == g.strHostPath)
                  && (fWritable     == g.fWritable)
                  && (fAutoMount    == g.fAutoMount)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool GuestProperty::operator==(const GuestProperty &g) const
{
    return (    (this == &g)
             || (    (strName           == g.strName)
                  && (strValue          == g.strValue)
                  && (timestamp         == g.timestamp)
                  && (strFlags          == g.strFlags)
                )
           );
}

// use a define for the platform-dependent default value of
// hwvirt exclusivity, since we'll need to check that value
// in bumpSettingsVersionIfNeeded()
#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
    #define HWVIRTEXCLUSIVEDEFAULT false
#else
    #define HWVIRTEXCLUSIVEDEFAULT true
#endif

Hardware::Hardware()
        : strVersion("1"),
          fHardwareVirt(true),
          fHardwareVirtExclusive(HWVIRTEXCLUSIVEDEFAULT),
          fNestedPaging(true),
          fVPID(true),
          fHardwareVirtForce(false),
          fSyntheticCpu(false),
          fPAE(false),
          cCPUs(1),
          fCpuHotPlug(false),
          fHPETEnabled(false),
          ulCpuExecutionCap(100),
          ulMemorySizeMB((uint32_t)-1),
          ulVRAMSizeMB(8),
          cMonitors(1),
          fAccelerate3D(false),
          fAccelerate2DVideo(false),
          ulVideoCaptureHorzRes(640),
          ulVideoCaptureVertRes(480),
          fVideoCaptureEnabled(false),
          strVideoCaptureFile("Test.webm"),
          firmwareType(FirmwareType_BIOS),
          pointingHIDType(PointingHIDType_PS2Mouse),
          keyboardHIDType(KeyboardHIDType_PS2Keyboard),
          chipsetType(ChipsetType_PIIX3),
          fEmulatedUSBWebcam(false),
          fEmulatedUSBCardReader(false),
          clipboardMode(ClipboardMode_Disabled),
          dragAndDropMode(DragAndDropMode_Disabled),
          ulMemoryBalloonSize(0),
          fPageFusionEnabled(false)
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

    /* The default value of large page supports depends on the host:
     * - 64 bits host -> true, unless it's Linux (pending further prediction work due to excessively expensive large page allocations)
     * - 32 bits host -> false
     */
#if HC_ARCH_BITS == 64 && !defined(RT_OS_LINUX)
    fLargePages = true;
#else
    /* Not supported on 32 bits hosts. */
    fLargePages = false;
#endif
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool Hardware::operator==(const Hardware& h) const
{
    return (    (this == &h)
             || (    (strVersion                == h.strVersion)
                  && (uuid                      == h.uuid)
                  && (fHardwareVirt             == h.fHardwareVirt)
                  && (fHardwareVirtExclusive    == h.fHardwareVirtExclusive)
                  && (fNestedPaging             == h.fNestedPaging)
                  && (fLargePages               == h.fLargePages)
                  && (fVPID                     == h.fVPID)
                  && (fHardwareVirtForce        == h.fHardwareVirtForce)
                  && (fSyntheticCpu             == h.fSyntheticCpu)
                  && (fPAE                      == h.fPAE)
                  && (cCPUs                     == h.cCPUs)
                  && (fCpuHotPlug               == h.fCpuHotPlug)
                  && (ulCpuExecutionCap         == h.ulCpuExecutionCap)
                  && (fHPETEnabled              == h.fHPETEnabled)
                  && (llCpus                    == h.llCpus)
                  && (llCpuIdLeafs              == h.llCpuIdLeafs)
                  && (ulMemorySizeMB            == h.ulMemorySizeMB)
                  && (mapBootOrder              == h.mapBootOrder)
                  && (ulVRAMSizeMB              == h.ulVRAMSizeMB)
                  && (cMonitors                 == h.cMonitors)
                  && (fAccelerate3D             == h.fAccelerate3D)
                  && (fAccelerate2DVideo        == h.fAccelerate2DVideo)
                  && (fVideoCaptureEnabled      == h.fVideoCaptureEnabled)
                  && (strVideoCaptureFile       == h.strVideoCaptureFile)
                  && (ulVideoCaptureHorzRes     == h.ulVideoCaptureHorzRes)
                  && (ulVideoCaptureVertRes     == h.ulVideoCaptureVertRes)
                  && (firmwareType              == h.firmwareType)
                  && (pointingHIDType           == h.pointingHIDType)
                  && (keyboardHIDType           == h.keyboardHIDType)
                  && (chipsetType               == h.chipsetType)
                  && (fEmulatedUSBWebcam        == h.fEmulatedUSBWebcam)
                  && (fEmulatedUSBCardReader    == h.fEmulatedUSBCardReader)
                  && (vrdeSettings              == h.vrdeSettings)
                  && (biosSettings              == h.biosSettings)
                  && (usbController             == h.usbController)
                  && (llNetworkAdapters         == h.llNetworkAdapters)
                  && (llSerialPorts             == h.llSerialPorts)
                  && (llParallelPorts           == h.llParallelPorts)
                  && (audioAdapter              == h.audioAdapter)
                  && (llSharedFolders           == h.llSharedFolders)
                  && (clipboardMode             == h.clipboardMode)
                  && (dragAndDropMode           == h.dragAndDropMode)
                  && (ulMemoryBalloonSize       == h.ulMemoryBalloonSize)
                  && (fPageFusionEnabled        == h.fPageFusionEnabled)
                  && (llGuestProperties         == h.llGuestProperties)
                  && (strNotificationPatterns   == h.strNotificationPatterns)
                  && (ioSettings                == h.ioSettings)
                  && (pciAttachments            == h.pciAttachments)
                )
            );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool AttachedDevice::operator==(const AttachedDevice &a) const
{
    return (    (this == &a)
             || (    (deviceType                == a.deviceType)
                  && (fPassThrough              == a.fPassThrough)
                  && (fTempEject                == a.fTempEject)
                  && (fNonRotational            == a.fNonRotational)
                  && (fDiscard                  == a.fDiscard)
                  && (lPort                     == a.lPort)
                  && (lDevice                   == a.lDevice)
                  && (uuid                      == a.uuid)
                  && (strHostDriveSrc           == a.strHostDriveSrc)
                  && (strBwGroup                == a.strBwGroup)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool StorageController::operator==(const StorageController &s) const
{
    return (    (this == &s)
             || (    (strName                   == s.strName)
                  && (storageBus                == s.storageBus)
                  && (controllerType            == s.controllerType)
                  && (ulPortCount               == s.ulPortCount)
                  && (ulInstance                == s.ulInstance)
                  && (fUseHostIOCache           == s.fUseHostIOCache)
                  && (lIDE0MasterEmulationPort  == s.lIDE0MasterEmulationPort)
                  && (lIDE0SlaveEmulationPort   == s.lIDE0SlaveEmulationPort)
                  && (lIDE1MasterEmulationPort  == s.lIDE1MasterEmulationPort)
                  && (lIDE1SlaveEmulationPort   == s.lIDE1SlaveEmulationPort)
                  && (llAttachedDevices         == s.llAttachedDevices)
                )
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool Storage::operator==(const Storage &s) const
{
    return (    (this == &s)
             || (llStorageControllers == s.llStorageControllers)            // deep compare
           );
}

/**
 * Comparison operator. This gets called from MachineConfigFile::operator==,
 * which in turn gets called from Machine::saveSettings to figure out whether
 * machine settings have really changed and thus need to be written out to disk.
 */
bool Snapshot::operator==(const Snapshot &s) const
{
    return (    (this == &s)
             || (    (uuid                  == s.uuid)
                  && (strName               == s.strName)
                  && (strDescription        == s.strDescription)
                  && (RTTimeSpecIsEqual(&timestamp, &s.timestamp))
                  && (strStateFile          == s.strStateFile)
                  && (hardware              == s.hardware)                  // deep compare
                  && (storage               == s.storage)                   // deep compare
                  && (llChildSnapshots      == s.llChildSnapshots)          // deep compare
                  && debugging              == s.debugging
                  && autostart              == s.autostart
                )
           );
}

/**
 * IOSettings constructor.
 */
IOSettings::IOSettings()
{
    fIOCacheEnabled  = true;
    ulIOCacheSize    = 5;
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
 * Public routine which returns true if this machine config file can have its
 * own media registry (which is true for settings version v1.11 and higher,
 * i.e. files created by VirtualBox 4.0 and higher).
 * @return
 */
bool MachineConfigFile::canHaveOwnMediaRegistry() const
{
    return (m->sv >= SettingsVersion_v1_11);
}

/**
 * Public routine which allows for importing machine XML from an external DOM tree.
 * Use this after having called the constructor with a NULL argument.
 *
 * This is used by the OVF code if a <vbox:Machine> element has been encountered
 * in an OVF VirtualSystem element.
 *
 * @param elmMachine
 */
void MachineConfigFile::importMachineXML(const xml::ElementNode &elmMachine)
{
    readMachine(elmMachine);
}

/**
 * Comparison operator. This gets called from Machine::saveSettings to figure out
 * whether machine settings have really changed and thus need to be written out to disk.
 *
 * Even though this is called operator==, this does NOT compare all fields; the "equals"
 * should be understood as "has the same machine config as". The following fields are
 * NOT compared:
 *      -- settings versions and file names inherited from ConfigFileBase;
 *      -- fCurrentStateModified because that is considered separately in Machine::saveSettings!!
 *
 * The "deep" comparisons marked below will invoke the operator== functions of the
 * structs defined in this file, which may in turn go into comparing lists of
 * other structures. As a result, invoking this can be expensive, but it's
 * less expensive than writing out XML to disk.
 */
bool MachineConfigFile::operator==(const MachineConfigFile &c) const
{
    return (    (this == &c)
            || (    (uuid                       == c.uuid)
                 && (machineUserData            == c.machineUserData)
                 && (strStateFile               == c.strStateFile)
                 && (uuidCurrentSnapshot        == c.uuidCurrentSnapshot)
                 // skip fCurrentStateModified!
                 && (RTTimeSpecIsEqual(&timeLastStateChange, &c.timeLastStateChange))
                 && (fAborted                   == c.fAborted)
                 && (hardwareMachine            == c.hardwareMachine)       // this one's deep
                 && (storageMachine             == c.storageMachine)        // this one's deep
                 && (mediaRegistry              == c.mediaRegistry)         // this one's deep
                 && (mapExtraDataItems          == c.mapExtraDataItems)     // this one's deep
                 && (llFirstSnapshot            == c.llFirstSnapshot)       // this one's deep
               )
           );
}

/**
 * Called from MachineConfigFile::readHardware() to read cpu information.
 * @param elmCpuid
 * @param ll
 */
void MachineConfigFile::readCpuTree(const xml::ElementNode &elmCpu,
                                    CpuList &ll)
{
    xml::NodesLoop nl1(elmCpu, "Cpu");
    const xml::ElementNode *pelmCpu;
    while ((pelmCpu = nl1.forAllNodes()))
    {
        Cpu cpu;

        if (!pelmCpu->getAttributeValue("id", cpu.ulId))
            throw ConfigFileError(this, pelmCpu, N_("Required Cpu/@id attribute is missing"));

        ll.push_back(cpu);
    }
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

        if (pelmAdapter->getAttributeValue("promiscuousModePolicy", strTemp))
        {
            if (strTemp == "Deny")
                nic.enmPromiscModePolicy = NetworkAdapterPromiscModePolicy_Deny;
            else if (strTemp == "AllowNetwork")
                nic.enmPromiscModePolicy = NetworkAdapterPromiscModePolicy_AllowNetwork;
            else if (strTemp == "AllowAll")
                nic.enmPromiscModePolicy = NetworkAdapterPromiscModePolicy_AllowAll;
            else
                throw ConfigFileError(this, pelmAdapter,
                                      N_("Invalid value '%s' in Adapter/@promiscuousModePolicy attribute"), strTemp.c_str());
        }

        pelmAdapter->getAttributeValue("trace", nic.fTraceEnabled);
        pelmAdapter->getAttributeValue("tracefile", nic.strTraceFile);
        pelmAdapter->getAttributeValue("bootPriority", nic.ulBootPriority);
        pelmAdapter->getAttributeValue("bandwidthGroup", nic.strBandwidthGroup);

        xml::ElementNodesList llNetworkModes;
        pelmAdapter->getChildElements(llNetworkModes);
        xml::ElementNodesList::iterator it;
        /* We should have only active mode descriptor and disabled modes set */
        if (llNetworkModes.size() > 2)
        {
            throw ConfigFileError(this, pelmAdapter, N_("Invalid number of modes ('%d') attached to Adapter attribute"), llNetworkModes.size());
        }
        for (it = llNetworkModes.begin(); it != llNetworkModes.end(); ++it)
        {
            const xml::ElementNode *pelmNode = *it;
            if (pelmNode->nameEquals("DisabledModes"))
            {
                xml::ElementNodesList llDisabledNetworkModes;
                xml::ElementNodesList::iterator itDisabled;
                pelmNode->getChildElements(llDisabledNetworkModes);
                /* run over disabled list and load settings */
                for (itDisabled = llDisabledNetworkModes.begin();
                     itDisabled != llDisabledNetworkModes.end(); ++itDisabled)
                {
                    const xml::ElementNode *pelmDisabledNode = *itDisabled;
                    readAttachedNetworkMode(*pelmDisabledNode, false, nic);
                }
            }
            else
                readAttachedNetworkMode(*pelmNode, true, nic);
        }
        // else: default is NetworkAttachmentType_Null

        ll.push_back(nic);
    }
}

void MachineConfigFile::readAttachedNetworkMode(const xml::ElementNode &elmMode, bool fEnabled, NetworkAdapter &nic)
{
    NetworkAttachmentType_T enmAttachmentType = NetworkAttachmentType_Null;

    if (elmMode.nameEquals("NAT"))
    {
        enmAttachmentType = NetworkAttachmentType_NAT;

        elmMode.getAttributeValue("network", nic.nat.strNetwork);
        elmMode.getAttributeValue("hostip", nic.nat.strBindIP);
        elmMode.getAttributeValue("mtu", nic.nat.u32Mtu);
        elmMode.getAttributeValue("sockrcv", nic.nat.u32SockRcv);
        elmMode.getAttributeValue("socksnd", nic.nat.u32SockSnd);
        elmMode.getAttributeValue("tcprcv", nic.nat.u32TcpRcv);
        elmMode.getAttributeValue("tcpsnd", nic.nat.u32TcpSnd);
        const xml::ElementNode *pelmDNS;
        if ((pelmDNS = elmMode.findChildElement("DNS")))
        {
            pelmDNS->getAttributeValue("pass-domain", nic.nat.fDNSPassDomain);
            pelmDNS->getAttributeValue("use-proxy", nic.nat.fDNSProxy);
            pelmDNS->getAttributeValue("use-host-resolver", nic.nat.fDNSUseHostResolver);
        }
        const xml::ElementNode *pelmAlias;
        if ((pelmAlias = elmMode.findChildElement("Alias")))
        {
            pelmAlias->getAttributeValue("logging", nic.nat.fAliasLog);
            pelmAlias->getAttributeValue("proxy-only", nic.nat.fAliasProxyOnly);
            pelmAlias->getAttributeValue("use-same-ports", nic.nat.fAliasUseSamePorts);
        }
        const xml::ElementNode *pelmTFTP;
        if ((pelmTFTP = elmMode.findChildElement("TFTP")))
        {
            pelmTFTP->getAttributeValue("prefix", nic.nat.strTFTPPrefix);
            pelmTFTP->getAttributeValue("boot-file", nic.nat.strTFTPBootFile);
            pelmTFTP->getAttributeValue("next-server", nic.nat.strTFTPNextServer);
        }
        xml::ElementNodesList plstNatPF;
        elmMode.getChildElements(plstNatPF, "Forwarding");
        for (xml::ElementNodesList::iterator pf = plstNatPF.begin(); pf != plstNatPF.end(); ++pf)
        {
            NATRule rule;
            uint32_t port = 0;
            (*pf)->getAttributeValue("name", rule.strName);
            (*pf)->getAttributeValue("proto", (uint32_t&)rule.proto);
            (*pf)->getAttributeValue("hostip", rule.strHostIP);
            (*pf)->getAttributeValue("hostport", port);
            rule.u16HostPort = port;
            (*pf)->getAttributeValue("guestip", rule.strGuestIP);
            (*pf)->getAttributeValue("guestport", port);
            rule.u16GuestPort = port;
            nic.nat.llRules.push_back(rule);
        }
    }
    else if (   (elmMode.nameEquals("HostInterface"))
             || (elmMode.nameEquals("BridgedInterface")))
    {
        enmAttachmentType = NetworkAttachmentType_Bridged;

        elmMode.getAttributeValue("name", nic.strBridgedName);  // optional bridged interface name
    }
    else if (elmMode.nameEquals("InternalNetwork"))
    {
        enmAttachmentType = NetworkAttachmentType_Internal;

        if (!elmMode.getAttributeValue("name", nic.strInternalNetworkName))    // required network name
            throw ConfigFileError(this, &elmMode, N_("Required InternalNetwork/@name element is missing"));
    }
    else if (elmMode.nameEquals("HostOnlyInterface"))
    {
        enmAttachmentType = NetworkAttachmentType_HostOnly;

        if (!elmMode.getAttributeValue("name", nic.strHostOnlyName))    // required network name
            throw ConfigFileError(this, &elmMode, N_("Required HostOnlyInterface/@name element is missing"));
    }
    else if (elmMode.nameEquals("GenericInterface"))
    {
        enmAttachmentType = NetworkAttachmentType_Generic;

        elmMode.getAttributeValue("driver", nic.strGenericDriver);  // optional network attachment driver

        // get all properties
        xml::NodesLoop nl(elmMode);
        const xml::ElementNode *pelmModeChild;
        while ((pelmModeChild = nl.forAllNodes()))
        {
            if (pelmModeChild->nameEquals("Property"))
            {
                Utf8Str strPropName, strPropValue;
                if (    (pelmModeChild->getAttributeValue("name", strPropName))
                     && (pelmModeChild->getAttributeValue("value", strPropValue))
                   )
                    nic.genericProperties[strPropName] = strPropValue;
                else
                    throw ConfigFileError(this, pelmModeChild, N_("Required GenericInterface/Property/@name or @value attribute is missing"));
            }
        }
    }
    else if (elmMode.nameEquals("VDE"))
    {
        enmAttachmentType = NetworkAttachmentType_Generic;

        com::Utf8Str strVDEName;
        elmMode.getAttributeValue("network", strVDEName);   // optional network name
        nic.strGenericDriver = "VDE";
        nic.genericProperties["network"] = strVDEName;
    }

    if (fEnabled && enmAttachmentType != NetworkAttachmentType_Null)
        nic.mode = enmAttachmentType;
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
 * Called from MachineConfigFile::readHardware() to read audio adapter information
 * and maybe fix driver information depending on the current host hardware.
 *
 * @param elmAudioAdapter "AudioAdapter" XML element.
 * @param hw
 */
void MachineConfigFile::readAudioAdapter(const xml::ElementNode &elmAudioAdapter,
                                         AudioAdapter &aa)
{
    elmAudioAdapter.getAttributeValue("enabled", aa.fEnabled);

    Utf8Str strTemp;
    if (elmAudioAdapter.getAttributeValue("controller", strTemp))
    {
        if (strTemp == "SB16")
            aa.controllerType = AudioControllerType_SB16;
        else if (strTemp == "AC97")
            aa.controllerType = AudioControllerType_AC97;
        else if (strTemp == "HDA")
            aa.controllerType = AudioControllerType_HDA;
        else
            throw ConfigFileError(this, &elmAudioAdapter, N_("Invalid value '%s' in AudioAdapter/@controller attribute"), strTemp.c_str());
    }

    if (elmAudioAdapter.getAttributeValue("driver", strTemp))
    {
        // settings before 1.3 used lower case so make sure this is case-insensitive
        strTemp.toUpper();
        if (strTemp == "NULL")
            aa.driverType = AudioDriverType_Null;
        else if (strTemp == "WINMM")
            aa.driverType = AudioDriverType_WinMM;
        else if ( (strTemp == "DIRECTSOUND") || (strTemp == "DSOUND") )
            aa.driverType = AudioDriverType_DirectSound;
        else if (strTemp == "SOLAUDIO")
            aa.driverType = AudioDriverType_SolAudio;
        else if (strTemp == "ALSA")
            aa.driverType = AudioDriverType_ALSA;
        else if (strTemp == "PULSE")
            aa.driverType = AudioDriverType_Pulse;
        else if (strTemp == "OSS")
            aa.driverType = AudioDriverType_OSS;
        else if (strTemp == "COREAUDIO")
            aa.driverType = AudioDriverType_CoreAudio;
        else if (strTemp == "MMPM")
            aa.driverType = AudioDriverType_MMPM;
        else
            throw ConfigFileError(this, &elmAudioAdapter, N_("Invalid value '%s' in AudioAdapter/@driver attribute"), strTemp.c_str());

        // now check if this is actually supported on the current host platform;
        // people might be opening a file created on a Windows host, and that
        // VM should still start on a Linux host
        if (!isAudioDriverAllowedOnThisHost(aa.driverType))
            aa.driverType = getHostDefaultAudioDriver();
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

    elmStorageController.getAttributeValue("useHostIOCache", sctl.fUseHostIOCache);
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

            pelmHwChild->getAttributeValue("hotplug", hw.fCpuHotPlug);
            pelmHwChild->getAttributeValue("executionCap", hw.ulCpuExecutionCap);

            const xml::ElementNode *pelmCPUChild;
            if (hw.fCpuHotPlug)
            {
                if ((pelmCPUChild = pelmHwChild->findChildElement("CpuTree")))
                    readCpuTree(*pelmCPUChild, hw.llCpus);
            }

            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtEx")))
            {
                pelmCPUChild->getAttributeValue("enabled", hw.fHardwareVirt);
                pelmCPUChild->getAttributeValue("exclusive", hw.fHardwareVirtExclusive);        // settings version 1.9
            }
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtExNestedPaging")))
                pelmCPUChild->getAttributeValue("enabled", hw.fNestedPaging);
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtExLargePages")))
                pelmCPUChild->getAttributeValue("enabled", hw.fLargePages);
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtExVPID")))
                pelmCPUChild->getAttributeValue("enabled", hw.fVPID);
            if ((pelmCPUChild = pelmHwChild->findChildElement("HardwareVirtForce")))
                pelmCPUChild->getAttributeValue("enabled", hw.fHardwareVirtForce);

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
        {
            pelmHwChild->getAttributeValue("RAMSize", hw.ulMemorySizeMB);
            pelmHwChild->getAttributeValue("PageFusion", hw.fPageFusionEnabled);
        }
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
                else if (    strFirmwareType == "EFI32")
                    hw.firmwareType = FirmwareType_EFI32;
                else if (    strFirmwareType == "EFI64")
                    hw.firmwareType = FirmwareType_EFI64;
                else if (    strFirmwareType == "EFIDUAL")
                    hw.firmwareType = FirmwareType_EFIDUAL;
                else
                    throw ConfigFileError(this,
                                          pelmHwChild,
                                          N_("Invalid value '%s' in Firmware/@type"),
                                          strFirmwareType.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("HID"))
        {
            Utf8Str strHIDType;
            if (pelmHwChild->getAttributeValue("Keyboard", strHIDType))
            {
                if (strHIDType == "None")
                    hw.keyboardHIDType = KeyboardHIDType_None;
                else if (strHIDType == "USBKeyboard")
                    hw.keyboardHIDType = KeyboardHIDType_USBKeyboard;
                else if (strHIDType == "PS2Keyboard")
                    hw.keyboardHIDType = KeyboardHIDType_PS2Keyboard;
                else if (strHIDType == "ComboKeyboard")
                    hw.keyboardHIDType = KeyboardHIDType_ComboKeyboard;
                else
                    throw ConfigFileError(this,
                                          pelmHwChild,
                                          N_("Invalid value '%s' in HID/Keyboard/@type"),
                                          strHIDType.c_str());
            }
            if (pelmHwChild->getAttributeValue("Pointing", strHIDType))
            {
                 if (strHIDType == "None")
                    hw.pointingHIDType = PointingHIDType_None;
                else if (strHIDType == "USBMouse")
                    hw.pointingHIDType = PointingHIDType_USBMouse;
                else if (strHIDType == "USBTablet")
                    hw.pointingHIDType = PointingHIDType_USBTablet;
                else if (strHIDType == "PS2Mouse")
                    hw.pointingHIDType = PointingHIDType_PS2Mouse;
                else if (strHIDType == "ComboMouse")
                    hw.pointingHIDType = PointingHIDType_ComboMouse;
                else
                    throw ConfigFileError(this,
                                          pelmHwChild,
                                          N_("Invalid value '%s' in HID/Pointing/@type"),
                                          strHIDType.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("Chipset"))
        {
            Utf8Str strChipsetType;
            if (pelmHwChild->getAttributeValue("type", strChipsetType))
            {
                if (strChipsetType == "PIIX3")
                    hw.chipsetType = ChipsetType_PIIX3;
                else if (strChipsetType == "ICH9")
                    hw.chipsetType = ChipsetType_ICH9;
                else
                    throw ConfigFileError(this,
                                          pelmHwChild,
                                          N_("Invalid value '%s' in Chipset/@type"),
                                          strChipsetType.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("HPET"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.fHPETEnabled);
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
        else if (pelmHwChild->nameEquals("VideoRecording"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.fVideoCaptureEnabled);
            pelmHwChild->getAttributeValue("file",    hw.strVideoCaptureFile);
            pelmHwChild->getAttributeValue("horzRes", hw.ulVideoCaptureHorzRes);
            pelmHwChild->getAttributeValue("vertRes", hw.ulVideoCaptureVertRes);
        }

        else if (pelmHwChild->nameEquals("RemoteDisplay"))
        {
            pelmHwChild->getAttributeValue("enabled", hw.vrdeSettings.fEnabled);

            Utf8Str str;
            if (pelmHwChild->getAttributeValue("port", str))
                hw.vrdeSettings.mapProperties["TCP/Ports"] = str;
            if (pelmHwChild->getAttributeValue("netAddress", str))
                hw.vrdeSettings.mapProperties["TCP/Address"] = str;

            Utf8Str strAuthType;
            if (pelmHwChild->getAttributeValue("authType", strAuthType))
            {
                // settings before 1.3 used lower case so make sure this is case-insensitive
                strAuthType.toUpper();
                if (strAuthType == "NULL")
                    hw.vrdeSettings.authType = AuthType_Null;
                else if (strAuthType == "GUEST")
                    hw.vrdeSettings.authType = AuthType_Guest;
                else if (strAuthType == "EXTERNAL")
                    hw.vrdeSettings.authType = AuthType_External;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in RemoteDisplay/@authType attribute"), strAuthType.c_str());
            }

            pelmHwChild->getAttributeValue("authLibrary", hw.vrdeSettings.strAuthLibrary);
            pelmHwChild->getAttributeValue("authTimeout", hw.vrdeSettings.ulAuthTimeout);
            pelmHwChild->getAttributeValue("allowMultiConnection", hw.vrdeSettings.fAllowMultiConnection);
            pelmHwChild->getAttributeValue("reuseSingleConnection", hw.vrdeSettings.fReuseSingleConnection);

            /* 3.2 and 4.0 betas, 4.0 has this information in VRDEProperties. */
            const xml::ElementNode *pelmVideoChannel;
            if ((pelmVideoChannel = pelmHwChild->findChildElement("VideoChannel")))
            {
                bool fVideoChannel = false;
                pelmVideoChannel->getAttributeValue("enabled", fVideoChannel);
                hw.vrdeSettings.mapProperties["VideoChannel/Enabled"] = fVideoChannel? "true": "false";

                uint32_t ulVideoChannelQuality = 75;
                pelmVideoChannel->getAttributeValue("quality", ulVideoChannelQuality);
                ulVideoChannelQuality = RT_CLAMP(ulVideoChannelQuality, 10, 100);
                char *pszBuffer = NULL;
                if (RTStrAPrintf(&pszBuffer, "%d", ulVideoChannelQuality) >= 0)
                {
                    hw.vrdeSettings.mapProperties["VideoChannel/Quality"] = pszBuffer;
                    RTStrFree(pszBuffer);
                }
                else
                    hw.vrdeSettings.mapProperties["VideoChannel/Quality"] = "75";
            }
            pelmHwChild->getAttributeValue("VRDEExtPack", hw.vrdeSettings.strVrdeExtPack);

            const xml::ElementNode *pelmProperties = pelmHwChild->findChildElement("VRDEProperties");
            if (pelmProperties != NULL)
            {
                xml::NodesLoop nl(*pelmProperties);
                const xml::ElementNode *pelmProperty;
                while ((pelmProperty = nl.forAllNodes()))
                {
                    if (pelmProperty->nameEquals("Property"))
                    {
                        /* <Property name="TCP/Ports" value="3000-3002"/> */
                        Utf8Str strName, strValue;
                        if (    ((pelmProperty->getAttributeValue("name", strName)))
                             && ((pelmProperty->getAttributeValue("value", strValue)))
                           )
                            hw.vrdeSettings.mapProperties[strName] = strValue;
                        else
                            throw ConfigFileError(this, pelmProperty, N_("Required VRDE Property/@name or @value attribute is missing"));
                    }
                }
            }
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
        else if (pelmHwChild->nameEquals("RTC"))
        {
            Utf8Str strLocalOrUTC;
            machineUserData.fRTCUseUTC =    pelmHwChild->getAttributeValue("localOrUTC", strLocalOrUTC)
                                         && strLocalOrUTC == "UTC";
        }
        else if (    (pelmHwChild->nameEquals("UART"))
                  || (pelmHwChild->nameEquals("Uart"))      // used before 1.3
                )
            readSerialPorts(*pelmHwChild, hw.llSerialPorts);
        else if (    (pelmHwChild->nameEquals("LPT"))
                  || (pelmHwChild->nameEquals("Lpt"))       // used before 1.3
                )
            readParallelPorts(*pelmHwChild, hw.llParallelPorts);
        else if (pelmHwChild->nameEquals("AudioAdapter"))
            readAudioAdapter(*pelmHwChild, hw.audioAdapter);
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
                pelmFolder->getAttributeValue("autoMount", sf.fAutoMount);
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
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in Clipboard/@mode attribute"), strTemp.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("DragAndDrop"))
        {
            Utf8Str strTemp;
            if (pelmHwChild->getAttributeValue("mode", strTemp))
            {
                if (strTemp == "Disabled")
                    hw.dragAndDropMode = DragAndDropMode_Disabled;
                else if (strTemp == "HostToGuest")
                    hw.dragAndDropMode = DragAndDropMode_HostToGuest;
                else if (strTemp == "GuestToHost")
                    hw.dragAndDropMode = DragAndDropMode_GuestToHost;
                else if (strTemp == "Bidirectional")
                    hw.dragAndDropMode = DragAndDropMode_Bidirectional;
                else
                    throw ConfigFileError(this, pelmHwChild, N_("Invalid value '%s' in DragAndDrop/@mode attribute"), strTemp.c_str());
            }
        }
        else if (pelmHwChild->nameEquals("Guest"))
        {
            if (!pelmHwChild->getAttributeValue("memoryBalloonSize", hw.ulMemoryBalloonSize))
                pelmHwChild->getAttributeValue("MemoryBalloonSize", hw.ulMemoryBalloonSize);            // used before 1.3
        }
        else if (pelmHwChild->nameEquals("GuestProperties"))
            readGuestProperties(*pelmHwChild, hw);
        else if (pelmHwChild->nameEquals("IO"))
        {
            const xml::ElementNode *pelmBwGroups;
            const xml::ElementNode *pelmIOChild;

            if ((pelmIOChild = pelmHwChild->findChildElement("IoCache")))
            {
                pelmIOChild->getAttributeValue("enabled", hw.ioSettings.fIOCacheEnabled);
                pelmIOChild->getAttributeValue("size", hw.ioSettings.ulIOCacheSize);
            }

            if ((pelmBwGroups = pelmHwChild->findChildElement("BandwidthGroups")))
            {
                xml::NodesLoop nl2(*pelmBwGroups, "BandwidthGroup");
                const xml::ElementNode *pelmBandwidthGroup;
                while ((pelmBandwidthGroup = nl2.forAllNodes()))
                {
                    BandwidthGroup gr;
                    Utf8Str strTemp;

                    pelmBandwidthGroup->getAttributeValue("name", gr.strName);

                    if (pelmBandwidthGroup->getAttributeValue("type", strTemp))
                    {
                        if (strTemp == "Disk")
                            gr.enmType = BandwidthGroupType_Disk;
                        else if (strTemp == "Network")
                            gr.enmType = BandwidthGroupType_Network;
                        else
                            throw ConfigFileError(this, pelmBandwidthGroup, N_("Invalid value '%s' in BandwidthGroup/@type attribute"), strTemp.c_str());
                    }
                    else
                        throw ConfigFileError(this, pelmBandwidthGroup, N_("Missing BandwidthGroup/@type attribute"));

                    if (!pelmBandwidthGroup->getAttributeValue("maxBytesPerSec", gr.cMaxBytesPerSec))
                    {
                        pelmBandwidthGroup->getAttributeValue("maxMbPerSec", gr.cMaxBytesPerSec);
                        gr.cMaxBytesPerSec *= _1M;
                    }
                    hw.ioSettings.llBandwidthGroups.push_back(gr);
                }
            }
        }  else if (pelmHwChild->nameEquals("HostPci")) {
            const xml::ElementNode *pelmDevices;

            if ((pelmDevices = pelmHwChild->findChildElement("Devices")))
            {
                xml::NodesLoop nl2(*pelmDevices, "Device");
                const xml::ElementNode *pelmDevice;
                while ((pelmDevice = nl2.forAllNodes()))
                {
                    HostPCIDeviceAttachment hpda;

                    if (!pelmDevice->getAttributeValue("host", hpda.uHostAddress))
                         throw ConfigFileError(this, pelmDevice, N_("Missing Device/@host attribute"));

                    if (!pelmDevice->getAttributeValue("guest", hpda.uGuestAddress))
                         throw ConfigFileError(this, pelmDevice, N_("Missing Device/@guest attribute"));

                    /* name is optional */
                    pelmDevice->getAttributeValue("name", hpda.strDeviceName);

                    hw.pciAttachments.push_back(hpda);
                }
            }
        }
        else if (pelmHwChild->nameEquals("EmulatedUSB"))
        {
            const xml::ElementNode *pelmCardReader, *pelmWebcam;

            if ((pelmCardReader = pelmHwChild->findChildElement("CardReader")))
            {
                pelmCardReader->getAttributeValue("enabled", hw.fEmulatedUSBCardReader);
            }

            if ((pelmWebcam = pelmHwChild->findChildElement("Webcam")))
            {
                pelmWebcam->getAttributeValue("enabled", hw.fEmulatedUSBWebcam);
            }
        }
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
        if (m->sv < SettingsVersion_v1_9)
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

        pelmController->getAttributeValue("Bootable", sctl.fBootable);
            // default from constructor is true which is true
            // for settings below version 1.11 because they allowed only
            // one controller per type.

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
        else if (strType == "LsiLogicSas")
        {
            sctl.storageBus = StorageBus_SAS;
            sctl.controllerType = StorageControllerType_LsiLogicSas;
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

            att.fDiscard = false;
            att.fNonRotational = false;

            if (strTemp == "HardDisk")
            {
                att.deviceType = DeviceType_HardDisk;
                pelmAttached->getAttributeValue("nonrotational", att.fNonRotational);
                pelmAttached->getAttributeValue("discard", att.fDiscard);
            }
            else if (m->sv >= SettingsVersion_v1_9)
            {
                // starting with 1.9 we list DVD and floppy drive info + attachments under <StorageControllers>
                if (strTemp == "DVD")
                {
                    att.deviceType = DeviceType_DVD;
                    pelmAttached->getAttributeValue("passthrough", att.fPassThrough);
                    pelmAttached->getAttributeValue("tempeject", att.fTempEject);
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

                pelmAttached->getAttributeValue("bandwidthGroup", att.strBwGroup);
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
            pelmHwChild->getAttributeValue("tempeject", att.fTempEject);

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
 * Called for reading the <Teleporter> element under <Machine>.
 */
void MachineConfigFile::readTeleporter(const xml::ElementNode *pElmTeleporter,
                                       MachineUserData *pUserData)
{
    pElmTeleporter->getAttributeValue("enabled", pUserData->fTeleporterEnabled);
    pElmTeleporter->getAttributeValue("port", pUserData->uTeleporterPort);
    pElmTeleporter->getAttributeValue("address", pUserData->strTeleporterAddress);
    pElmTeleporter->getAttributeValue("password", pUserData->strTeleporterPassword);

    if (   pUserData->strTeleporterPassword.isNotEmpty()
        && !VBoxIsPasswordHashed(&pUserData->strTeleporterPassword))
        VBoxHashPassword(&pUserData->strTeleporterPassword);
}

/**
 * Called for reading the <Debugging> element under <Machine> or <Snapshot>.
 */
void MachineConfigFile::readDebugging(const xml::ElementNode *pElmDebugging, Debugging *pDbg)
{
    if (!pElmDebugging || m->sv < SettingsVersion_v1_13)
        return;

    const xml::ElementNode * const pelmTracing = pElmDebugging->findChildElement("Tracing");
    if (pelmTracing)
    {
        pelmTracing->getAttributeValue("enabled", pDbg->fTracingEnabled);
        pelmTracing->getAttributeValue("allowTracingToAccessVM", pDbg->fAllowTracingToAccessVM);
        pelmTracing->getAttributeValue("config", pDbg->strTracingConfig);
    }
}

/**
 * Called for reading the <Autostart> element under <Machine> or <Snapshot>.
 */
void MachineConfigFile::readAutostart(const xml::ElementNode *pElmAutostart, Autostart *pAutostart)
{
    Utf8Str strAutostop;

    if (!pElmAutostart || m->sv < SettingsVersion_v1_13)
        return;

    pElmAutostart->getAttributeValue("enabled", pAutostart->fAutostartEnabled);
    pElmAutostart->getAttributeValue("delay", pAutostart->uAutostartDelay);
    pElmAutostart->getAttributeValue("autostop", strAutostop);
    if (strAutostop == "Disabled")
        pAutostart->enmAutostopType = AutostopType_Disabled;
    else if (strAutostop == "SaveState")
        pAutostart->enmAutostopType = AutostopType_SaveState;
    else if (strAutostop == "PowerOff")
        pAutostart->enmAutostopType = AutostopType_PowerOff;
    else if (strAutostop == "AcpiShutdown")
        pAutostart->enmAutostopType = AutostopType_AcpiShutdown;
    else
        throw ConfigFileError(this, pElmAutostart, N_("Invalid value '%s' for Autostart/@autostop attribute"), strAutostop.c_str());
}

/**
 * Called for reading the <Groups> element under <Machine>.
 */
void MachineConfigFile::readGroups(const xml::ElementNode *pElmGroups, StringsList *pllGroups)
{
    pllGroups->clear();
    if (!pElmGroups || m->sv < SettingsVersion_v1_13)
    {
        pllGroups->push_back("/");
        return;
    }

    xml::NodesLoop nlGroups(*pElmGroups);
    const xml::ElementNode *pelmGroup;
    while ((pelmGroup = nlGroups.forAllNodes()))
    {
        if (pelmGroup->nameEquals("Group"))
        {
            Utf8Str strGroup;
            if (!pelmGroup->getAttributeValue("name", strGroup))
                throw ConfigFileError(this, pelmGroup, N_("Required Group/@name attribute is missing"));
            pllGroups->push_back(strGroup);
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

    elmSnapshot.getAttributeValuePath("stateFile", snap.strStateFile);      // online snapshots only

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

    readDebugging(elmSnapshot.findChildElement("Debugging"), &snap.debugging);
    readAutostart(elmSnapshot.findChildElement("Autostart"), &snap.autostart);
    // note: Groups exist only for Machine, not for Snapshot
}

const struct {
    const char *pcszOld;
    const char *pcszNew;
} aConvertOSTypes[] =
{
    { "unknown", "Other" },
    { "dos", "DOS" },
    { "win31", "Windows31" },
    { "win95", "Windows95" },
    { "win98", "Windows98" },
    { "winme", "WindowsMe" },
    { "winnt4", "WindowsNT4" },
    { "win2k", "Windows2000" },
    { "winxp", "WindowsXP" },
    { "win2k3", "Windows2003" },
    { "winvista", "WindowsVista" },
    { "win2k8", "Windows2008" },
    { "os2warp3", "OS2Warp3" },
    { "os2warp4", "OS2Warp4" },
    { "os2warp45", "OS2Warp45" },
    { "ecs", "OS2eCS" },
    { "linux22", "Linux22" },
    { "linux24", "Linux24" },
    { "linux26", "Linux26" },
    { "archlinux", "ArchLinux" },
    { "debian", "Debian" },
    { "opensuse", "OpenSUSE" },
    { "fedoracore", "Fedora" },
    { "gentoo", "Gentoo" },
    { "mandriva", "Mandriva" },
    { "redhat", "RedHat" },
    { "ubuntu", "Ubuntu" },
    { "xandros", "Xandros" },
    { "freebsd", "FreeBSD" },
    { "openbsd", "OpenBSD" },
    { "netbsd", "NetBSD" },
    { "netware", "Netware" },
    { "solaris", "Solaris" },
    { "opensolaris", "OpenSolaris" },
    { "l4", "L4" }
};

void MachineConfigFile::convertOldOSType_pre1_5(Utf8Str &str)
{
    for (unsigned u = 0;
         u < RT_ELEMENTS(aConvertOSTypes);
         ++u)
    {
        if (str == aConvertOSTypes[u].pcszOld)
        {
            str = aConvertOSTypes[u].pcszNew;
            break;
        }
    }
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
         && (elmMachine.getAttributeValue("name", machineUserData.strName))
       )
    {
        parseUUID(uuid, strUUID);

        elmMachine.getAttributeValue("directoryIncludesUUID", machineUserData.fDirectoryIncludesUUID);
        elmMachine.getAttributeValue("nameSync", machineUserData.fNameSync);

        Utf8Str str;
        elmMachine.getAttributeValue("Description", machineUserData.strDescription);

        elmMachine.getAttributeValue("OSType", machineUserData.strOsType);
        if (m->sv < SettingsVersion_v1_5)
            convertOldOSType_pre1_5(machineUserData.strOsType);

        elmMachine.getAttributeValuePath("stateFile", strStateFile);

        if (elmMachine.getAttributeValue("currentSnapshot", str))
            parseUUID(uuidCurrentSnapshot, str);

        elmMachine.getAttributeValuePath("snapshotFolder", machineUserData.strSnapshotFolder);

        if (!elmMachine.getAttributeValue("currentStateModified", fCurrentStateModified))
            fCurrentStateModified = true;
        if (elmMachine.getAttributeValue("lastStateChange", str))
            parseTimestamp(timeLastStateChange, str);
            // constructor has called RTTimeNow(&timeLastStateChange) before
        if (elmMachine.getAttributeValue("aborted", fAborted))
            fAborted = true;

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
                machineUserData.strDescription = pelmMachineChild->getValue();
            else if (pelmMachineChild->nameEquals("Teleporter"))
                readTeleporter(pelmMachineChild, &machineUserData);
            else if (pelmMachineChild->nameEquals("FaultTolerance"))
            {
                Utf8Str strFaultToleranceSate;
                if (pelmMachineChild->getAttributeValue("state", strFaultToleranceSate))
                {
                    if (strFaultToleranceSate == "master")
                        machineUserData.enmFaultToleranceState = FaultToleranceState_Master;
                    else
                    if (strFaultToleranceSate == "standby")
                        machineUserData.enmFaultToleranceState = FaultToleranceState_Standby;
                    else
                        machineUserData.enmFaultToleranceState = FaultToleranceState_Inactive;
                }
                pelmMachineChild->getAttributeValue("port", machineUserData.uFaultTolerancePort);
                pelmMachineChild->getAttributeValue("address", machineUserData.strFaultToleranceAddress);
                pelmMachineChild->getAttributeValue("interval", machineUserData.uFaultToleranceInterval);
                pelmMachineChild->getAttributeValue("password", machineUserData.strFaultTolerancePassword);
            }
            else if (pelmMachineChild->nameEquals("MediaRegistry"))
                readMediaRegistry(*pelmMachineChild, mediaRegistry);
            else if (pelmMachineChild->nameEquals("Debugging"))
                readDebugging(pelmMachineChild, &debugging);
            else if (pelmMachineChild->nameEquals("Autostart"))
                readAutostart(pelmMachineChild, &autostart);
            else if (pelmMachineChild->nameEquals("Groups"))
                readGroups(pelmMachineChild, &machineUserData.llGroups);
        }

        if (m->sv < SettingsVersion_v1_9)
            // go through Hardware once more to repair the settings controller structures
            // with data from old DVDDrive and FloppyDrive elements
            readDVDAndFloppies_pre1_9(*pelmHardware, storageMachine);
    }
    else
        throw ConfigFileError(this, &elmMachine, N_("Required Machine/@uuid or @name attributes is missing"));
}

/**
 * Creates a <Hardware> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 * @param elmParent
 * @param st
 */
void MachineConfigFile::buildHardwareXML(xml::ElementNode &elmParent,
                                         const Hardware &hw,
                                         const Storage &strg)
{
    xml::ElementNode *pelmHardware = elmParent.createChild("Hardware");

    if (m->sv >= SettingsVersion_v1_4)
        pelmHardware->setAttribute("version", hw.strVersion);

    if ((m->sv >= SettingsVersion_v1_9)
         && !hw.uuid.isZero()
         && hw.uuid.isValid()
       )
        pelmHardware->setAttribute("uuid", hw.uuid.toStringCurly());

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
    if (hw.ulCpuExecutionCap != 100)
        pelmCPU->setAttribute("executionCap", hw.ulCpuExecutionCap);

    /* Always save this setting as we have changed the default in 4.0 (on for large memory 64-bit systems). */
    pelmCPU->createChild("HardwareVirtExLargePages")->setAttribute("enabled", hw.fLargePages);

    if (m->sv >= SettingsVersion_v1_9)
        pelmCPU->createChild("HardwareVirtForce")->setAttribute("enabled", hw.fHardwareVirtForce);

    if (m->sv >= SettingsVersion_v1_10)
    {
        pelmCPU->setAttribute("hotplug", hw.fCpuHotPlug);

        xml::ElementNode *pelmCpuTree = NULL;
        for (CpuList::const_iterator it = hw.llCpus.begin();
             it != hw.llCpus.end();
             ++it)
        {
            const Cpu &cpu = *it;

            if (pelmCpuTree == NULL)
                 pelmCpuTree = pelmCPU->createChild("CpuTree");

            xml::ElementNode *pelmCpu = pelmCpuTree->createChild("Cpu");
            pelmCpu->setAttribute("id", cpu.ulId);
        }
    }

    xml::ElementNode *pelmCpuIdTree = NULL;
    for (CpuIdLeafsList::const_iterator it = hw.llCpuIdLeafs.begin();
         it != hw.llCpuIdLeafs.end();
         ++it)
    {
        const CpuIdLeaf &leaf = *it;

        if (pelmCpuIdTree == NULL)
             pelmCpuIdTree = pelmCPU->createChild("CpuIdTree");

        xml::ElementNode *pelmCpuIdLeaf = pelmCpuIdTree->createChild("CpuIdLeaf");
        pelmCpuIdLeaf->setAttribute("id",  leaf.ulId);
        pelmCpuIdLeaf->setAttribute("eax", leaf.ulEax);
        pelmCpuIdLeaf->setAttribute("ebx", leaf.ulEbx);
        pelmCpuIdLeaf->setAttribute("ecx", leaf.ulEcx);
        pelmCpuIdLeaf->setAttribute("edx", leaf.ulEdx);
    }

    xml::ElementNode *pelmMemory = pelmHardware->createChild("Memory");
    pelmMemory->setAttribute("RAMSize", hw.ulMemorySizeMB);
    if (m->sv >= SettingsVersion_v1_10)
    {
        pelmMemory->setAttribute("PageFusion", hw.fPageFusionEnabled);
    }

    if (    (m->sv >= SettingsVersion_v1_9)
         && (hw.firmwareType >= FirmwareType_EFI)
       )
    {
         xml::ElementNode *pelmFirmware = pelmHardware->createChild("Firmware");
         const char *pcszFirmware;

         switch (hw.firmwareType)
         {
            case FirmwareType_EFI:      pcszFirmware = "EFI";     break;
            case FirmwareType_EFI32:    pcszFirmware = "EFI32";   break;
            case FirmwareType_EFI64:    pcszFirmware = "EFI64";   break;
            case FirmwareType_EFIDUAL:  pcszFirmware = "EFIDUAL"; break;
            default:                    pcszFirmware = "None";    break;
         }
         pelmFirmware->setAttribute("type", pcszFirmware);
    }

    if (    (m->sv >= SettingsVersion_v1_10)
       )
    {
         xml::ElementNode *pelmHID = pelmHardware->createChild("HID");
         const char *pcszHID;

         switch (hw.pointingHIDType)
         {
            case PointingHIDType_USBMouse:      pcszHID = "USBMouse";   break;
            case PointingHIDType_USBTablet:     pcszHID = "USBTablet";  break;
            case PointingHIDType_PS2Mouse:      pcszHID = "PS2Mouse";   break;
            case PointingHIDType_ComboMouse:    pcszHID = "ComboMouse"; break;
            case PointingHIDType_None:          pcszHID = "None";       break;
            default:            Assert(false);  pcszHID = "PS2Mouse";   break;
         }
         pelmHID->setAttribute("Pointing", pcszHID);

         switch (hw.keyboardHIDType)
         {
            case KeyboardHIDType_USBKeyboard:   pcszHID = "USBKeyboard";   break;
            case KeyboardHIDType_PS2Keyboard:   pcszHID = "PS2Keyboard";   break;
            case KeyboardHIDType_ComboKeyboard: pcszHID = "ComboKeyboard"; break;
            case KeyboardHIDType_None:          pcszHID = "None";          break;
            default:            Assert(false);  pcszHID = "PS2Keyboard";   break;
         }
         pelmHID->setAttribute("Keyboard", pcszHID);
    }

    if (    (m->sv >= SettingsVersion_v1_10)
       )
    {
         xml::ElementNode *pelmHPET = pelmHardware->createChild("HPET");
         pelmHPET->setAttribute("enabled", hw.fHPETEnabled);
    }

    if (    (m->sv >= SettingsVersion_v1_11)
       )
    {
         xml::ElementNode *pelmChipset = pelmHardware->createChild("Chipset");
         const char *pcszChipset;

         switch (hw.chipsetType)
         {
            case ChipsetType_PIIX3:             pcszChipset = "PIIX3";   break;
            case ChipsetType_ICH9:              pcszChipset = "ICH9";    break;
            default:            Assert(false);  pcszChipset = "PIIX3";   break;
         }
         pelmChipset->setAttribute("type", pcszChipset);
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
    xml::ElementNode *pelmVideoCapture = pelmHardware->createChild("VideoRecording");

    if (m->sv >= SettingsVersion_v1_12)
    {
        pelmVideoCapture->setAttribute("enabled", hw.fVideoCaptureEnabled);
        pelmVideoCapture->setAttribute("file",    hw.strVideoCaptureFile);
        pelmVideoCapture->setAttribute("horzRes", hw.ulVideoCaptureHorzRes);
        pelmVideoCapture->setAttribute("vertRes", hw.ulVideoCaptureVertRes);
    }

    xml::ElementNode *pelmVRDE = pelmHardware->createChild("RemoteDisplay");
    pelmVRDE->setAttribute("enabled", hw.vrdeSettings.fEnabled);
    if (m->sv < SettingsVersion_v1_11)
    {
        /* In VBox 4.0 these attributes are replaced with "Properties". */
        Utf8Str strPort;
        StringsMap::const_iterator it = hw.vrdeSettings.mapProperties.find("TCP/Ports");
        if (it != hw.vrdeSettings.mapProperties.end())
            strPort = it->second;
        if (!strPort.length())
            strPort = "3389";
        pelmVRDE->setAttribute("port", strPort);

        Utf8Str strAddress;
        it = hw.vrdeSettings.mapProperties.find("TCP/Address");
        if (it != hw.vrdeSettings.mapProperties.end())
            strAddress = it->second;
        if (strAddress.length())
            pelmVRDE->setAttribute("netAddress", strAddress);
    }
    const char *pcszAuthType;
    switch (hw.vrdeSettings.authType)
    {
        case AuthType_Guest:    pcszAuthType = "Guest";    break;
        case AuthType_External: pcszAuthType = "External"; break;
        default: /*case AuthType_Null:*/ pcszAuthType = "Null"; break;
    }
    pelmVRDE->setAttribute("authType", pcszAuthType);

    if (hw.vrdeSettings.ulAuthTimeout != 0)
        pelmVRDE->setAttribute("authTimeout", hw.vrdeSettings.ulAuthTimeout);
    if (hw.vrdeSettings.fAllowMultiConnection)
        pelmVRDE->setAttribute("allowMultiConnection", hw.vrdeSettings.fAllowMultiConnection);
    if (hw.vrdeSettings.fReuseSingleConnection)
        pelmVRDE->setAttribute("reuseSingleConnection", hw.vrdeSettings.fReuseSingleConnection);

    if (m->sv == SettingsVersion_v1_10)
    {
        xml::ElementNode *pelmVideoChannel = pelmVRDE->createChild("VideoChannel");

        /* In 4.0 videochannel settings were replaced with properties, so look at properties. */
        Utf8Str str;
        StringsMap::const_iterator it = hw.vrdeSettings.mapProperties.find("VideoChannel/Enabled");
        if (it != hw.vrdeSettings.mapProperties.end())
            str = it->second;
        bool fVideoChannel =    RTStrICmp(str.c_str(), "true") == 0
                             || RTStrCmp(str.c_str(), "1") == 0;
        pelmVideoChannel->setAttribute("enabled", fVideoChannel);

        it = hw.vrdeSettings.mapProperties.find("VideoChannel/Quality");
        if (it != hw.vrdeSettings.mapProperties.end())
            str = it->second;
        uint32_t ulVideoChannelQuality = RTStrToUInt32(str.c_str()); /* This returns 0 on invalid string which is ok. */
        if (ulVideoChannelQuality == 0)
            ulVideoChannelQuality = 75;
        else
            ulVideoChannelQuality = RT_CLAMP(ulVideoChannelQuality, 10, 100);
        pelmVideoChannel->setAttribute("quality", ulVideoChannelQuality);
    }
    if (m->sv >= SettingsVersion_v1_11)
    {
        if (hw.vrdeSettings.strAuthLibrary.length())
            pelmVRDE->setAttribute("authLibrary", hw.vrdeSettings.strAuthLibrary);
        if (hw.vrdeSettings.strVrdeExtPack.isNotEmpty())
            pelmVRDE->setAttribute("VRDEExtPack", hw.vrdeSettings.strVrdeExtPack);
        if (hw.vrdeSettings.mapProperties.size() > 0)
        {
            xml::ElementNode *pelmProperties = pelmVRDE->createChild("VRDEProperties");
            for (StringsMap::const_iterator it = hw.vrdeSettings.mapProperties.begin();
                 it != hw.vrdeSettings.mapProperties.end();
                 ++it)
            {
                const Utf8Str &strName = it->first;
                const Utf8Str &strValue = it->second;
                xml::ElementNode *pelm = pelmProperties->createChild("Property");
                pelm->setAttribute("name", strName);
                pelm->setAttribute("value", strValue);
            }
        }
    }

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
                        if (att.fTempEject)
                            pelmDVD->setAttribute("tempeject", att.fTempEject);

                        if (!att.uuid.isZero() && att.uuid.isValid())
                            pelmDVD->createChild("Image")->setAttribute("uuid", att.uuid.toStringCurly());
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

                    if (!att.uuid.isZero() && att.uuid.isValid())
                        pelmFloppy->createChild("Image")->setAttribute("uuid", att.uuid.toStringCurly());
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

    buildUSBDeviceFilters(*pelmUSB,
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
        if (nic.ulBootPriority != 0)
        {
            pelmAdapter->setAttribute("bootPriority", nic.ulBootPriority);
        }
        if (nic.fTraceEnabled)
        {
            pelmAdapter->setAttribute("trace", nic.fTraceEnabled);
            pelmAdapter->setAttribute("tracefile", nic.strTraceFile);
        }
        if (nic.strBandwidthGroup.isNotEmpty())
            pelmAdapter->setAttribute("bandwidthGroup", nic.strBandwidthGroup);

        const char *pszPolicy;
        switch (nic.enmPromiscModePolicy)
        {
            case NetworkAdapterPromiscModePolicy_Deny:          pszPolicy = NULL; break;
            case NetworkAdapterPromiscModePolicy_AllowNetwork:  pszPolicy = "AllowNetwork"; break;
            case NetworkAdapterPromiscModePolicy_AllowAll:      pszPolicy = "AllowAll"; break;
            default:                                            pszPolicy = NULL; AssertFailed(); break;
        }
        if (pszPolicy)
            pelmAdapter->setAttribute("promiscuousModePolicy", pszPolicy);

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
        if (m->sv < SettingsVersion_v1_10)
        {
            switch (nic.mode)
            {
                case NetworkAttachmentType_NAT:
                    pelmNAT = pelmAdapter->createChild("NAT");
                    if (nic.nat.strNetwork.length())
                        pelmNAT->setAttribute("network", nic.nat.strNetwork);
                    break;

                case NetworkAttachmentType_Bridged:
                    pelmAdapter->createChild("BridgedInterface")->setAttribute("name", nic.strBridgedName);
                    break;

                case NetworkAttachmentType_Internal:
                    pelmAdapter->createChild("InternalNetwork")->setAttribute("name", nic.strInternalNetworkName);
                    break;

                case NetworkAttachmentType_HostOnly:
                    pelmAdapter->createChild("HostOnlyInterface")->setAttribute("name", nic.strHostOnlyName);
                    break;

                default: /*case NetworkAttachmentType_Null:*/
                    break;
            }
        }
        else
        {
            /* m->sv >= SettingsVersion_v1_10 */
            xml::ElementNode *pelmDisabledNode = NULL;
            pelmDisabledNode = pelmAdapter->createChild("DisabledModes");
            if (nic.mode != NetworkAttachmentType_NAT)
                buildNetworkXML(NetworkAttachmentType_NAT, *pelmDisabledNode, false, nic);
            if (nic.mode != NetworkAttachmentType_Bridged)
                buildNetworkXML(NetworkAttachmentType_Bridged, *pelmDisabledNode, false, nic);
            if (nic.mode != NetworkAttachmentType_Internal)
                buildNetworkXML(NetworkAttachmentType_HostOnly, *pelmDisabledNode, false, nic);
            if (nic.mode != NetworkAttachmentType_HostOnly)
                buildNetworkXML(NetworkAttachmentType_HostOnly, *pelmDisabledNode, false, nic);
            if (nic.mode != NetworkAttachmentType_Generic)
                buildNetworkXML(NetworkAttachmentType_Generic, *pelmDisabledNode, false, nic);
            buildNetworkXML(nic.mode, *pelmAdapter, true, nic);
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
    const char *pcszController;
    switch (hw.audioAdapter.controllerType)
    {
        case AudioControllerType_SB16:
            pcszController = "SB16";
            break;
        case AudioControllerType_HDA:
            if (m->sv >= SettingsVersion_v1_11)
            {
                pcszController = "HDA";
                break;
            }
            /* fall through */
        case AudioControllerType_AC97:
        default:
            pcszController = "AC97";
            break;
    }
    pelmAudio->setAttribute("controller", pcszController);

    if (m->sv >= SettingsVersion_v1_10)
    {
        xml::ElementNode *pelmRTC = pelmHardware->createChild("RTC");
        pelmRTC->setAttribute("localOrUTC", machineUserData.fRTCUseUTC ? "UTC" : "local");
    }

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
        pelmThis->setAttribute("autoMount", sf.fAutoMount);
    }

    xml::ElementNode *pelmClip = pelmHardware->createChild("Clipboard");
    const char *pcszClip;
    switch (hw.clipboardMode)
    {
        default: /*case ClipboardMode_Disabled:*/ pcszClip = "Disabled"; break;
        case ClipboardMode_HostToGuest: pcszClip = "HostToGuest"; break;
        case ClipboardMode_GuestToHost: pcszClip = "GuestToHost"; break;
        case ClipboardMode_Bidirectional: pcszClip = "Bidirectional"; break;
    }
    pelmClip->setAttribute("mode", pcszClip);

    xml::ElementNode *pelmDragAndDrop = pelmHardware->createChild("DragAndDrop");
    const char *pcszDragAndDrop;
    switch (hw.dragAndDropMode)
    {
        default: /*case DragAndDropMode_Disabled:*/ pcszDragAndDrop = "Disabled"; break;
        case DragAndDropMode_HostToGuest: pcszDragAndDrop = "HostToGuest"; break;
        case DragAndDropMode_GuestToHost: pcszDragAndDrop = "GuestToHost"; break;
        case DragAndDropMode_Bidirectional: pcszDragAndDrop = "Bidirectional"; break;
    }
    pelmDragAndDrop->setAttribute("mode", pcszDragAndDrop);

    if (m->sv >= SettingsVersion_v1_10)
    {
        xml::ElementNode *pelmIO = pelmHardware->createChild("IO");
        xml::ElementNode *pelmIOCache;

        pelmIOCache = pelmIO->createChild("IoCache");
        pelmIOCache->setAttribute("enabled", hw.ioSettings.fIOCacheEnabled);
        pelmIOCache->setAttribute("size", hw.ioSettings.ulIOCacheSize);

        if (m->sv >= SettingsVersion_v1_11)
        {
            xml::ElementNode *pelmBandwidthGroups = pelmIO->createChild("BandwidthGroups");
            for (BandwidthGroupList::const_iterator it = hw.ioSettings.llBandwidthGroups.begin();
                 it != hw.ioSettings.llBandwidthGroups.end();
                 ++it)
            {
                const BandwidthGroup &gr = *it;
                const char *pcszType;
                xml::ElementNode *pelmThis = pelmBandwidthGroups->createChild("BandwidthGroup");
                pelmThis->setAttribute("name", gr.strName);
                switch (gr.enmType)
                {
                    case BandwidthGroupType_Network: pcszType = "Network"; break;
                    default: /* BandwidthGrouptype_Disk */ pcszType = "Disk"; break;
                }
                pelmThis->setAttribute("type", pcszType);
                if (m->sv >= SettingsVersion_v1_13)
                    pelmThis->setAttribute("maxBytesPerSec", gr.cMaxBytesPerSec);
                else
                    pelmThis->setAttribute("maxMbPerSec", gr.cMaxBytesPerSec / _1M);
            }
        }
    }

    if (m->sv >= SettingsVersion_v1_12)
    {
        xml::ElementNode *pelmPCI = pelmHardware->createChild("HostPci");
        xml::ElementNode *pelmPCIDevices = pelmPCI->createChild("Devices");

        for (HostPCIDeviceAttachmentList::const_iterator it = hw.pciAttachments.begin();
             it != hw.pciAttachments.end();
             ++it)
        {
            const HostPCIDeviceAttachment &hpda = *it;

            xml::ElementNode *pelmThis = pelmPCIDevices->createChild("Device");

            pelmThis->setAttribute("host",  hpda.uHostAddress);
            pelmThis->setAttribute("guest", hpda.uGuestAddress);
            pelmThis->setAttribute("name",  hpda.strDeviceName);
        }
    }

    if (m->sv >= SettingsVersion_v1_12)
    {
        xml::ElementNode *pelmEmulatedUSB = pelmHardware->createChild("EmulatedUSB");

        xml::ElementNode *pelmCardReader = pelmEmulatedUSB->createChild("CardReader");
        pelmCardReader->setAttribute("enabled", hw.fEmulatedUSBCardReader);

        if (m->sv >= SettingsVersion_v1_13)
        {
            xml::ElementNode *pelmWebcam = pelmEmulatedUSB->createChild("Webcam");
            pelmWebcam->setAttribute("enabled", hw.fEmulatedUSBWebcam);
        }
    }

    xml::ElementNode *pelmGuest = pelmHardware->createChild("Guest");
    pelmGuest->setAttribute("memoryBalloonSize", hw.ulMemoryBalloonSize);

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
 * Fill a <Network> node. Only relevant for XML version >= v1_10.
 * @param mode
 * @param elmParent
 * @param fEnabled
 * @param nic
 */
void MachineConfigFile::buildNetworkXML(NetworkAttachmentType_T mode,
                                        xml::ElementNode &elmParent,
                                        bool fEnabled,
                                        const NetworkAdapter &nic)
{
    switch (mode)
    {
        case NetworkAttachmentType_NAT:
            xml::ElementNode *pelmNAT;
            pelmNAT = elmParent.createChild("NAT");

            if (nic.nat.strNetwork.length())
                pelmNAT->setAttribute("network", nic.nat.strNetwork);
            if (nic.nat.strBindIP.length())
                pelmNAT->setAttribute("hostip", nic.nat.strBindIP);
            if (nic.nat.u32Mtu)
                pelmNAT->setAttribute("mtu", nic.nat.u32Mtu);
            if (nic.nat.u32SockRcv)
                pelmNAT->setAttribute("sockrcv", nic.nat.u32SockRcv);
            if (nic.nat.u32SockSnd)
                pelmNAT->setAttribute("socksnd", nic.nat.u32SockSnd);
            if (nic.nat.u32TcpRcv)
                pelmNAT->setAttribute("tcprcv", nic.nat.u32TcpRcv);
            if (nic.nat.u32TcpSnd)
                pelmNAT->setAttribute("tcpsnd", nic.nat.u32TcpSnd);
            xml::ElementNode *pelmDNS;
            pelmDNS = pelmNAT->createChild("DNS");
            pelmDNS->setAttribute("pass-domain", nic.nat.fDNSPassDomain);
            pelmDNS->setAttribute("use-proxy", nic.nat.fDNSProxy);
            pelmDNS->setAttribute("use-host-resolver", nic.nat.fDNSUseHostResolver);

            xml::ElementNode *pelmAlias;
            pelmAlias = pelmNAT->createChild("Alias");
            pelmAlias->setAttribute("logging", nic.nat.fAliasLog);
            pelmAlias->setAttribute("proxy-only", nic.nat.fAliasProxyOnly);
            pelmAlias->setAttribute("use-same-ports", nic.nat.fAliasUseSamePorts);

            if (   nic.nat.strTFTPPrefix.length()
                || nic.nat.strTFTPBootFile.length()
                || nic.nat.strTFTPNextServer.length())
            {
                xml::ElementNode *pelmTFTP;
                pelmTFTP = pelmNAT->createChild("TFTP");
                if (nic.nat.strTFTPPrefix.length())
                    pelmTFTP->setAttribute("prefix", nic.nat.strTFTPPrefix);
                if (nic.nat.strTFTPBootFile.length())
                    pelmTFTP->setAttribute("boot-file", nic.nat.strTFTPBootFile);
                if (nic.nat.strTFTPNextServer.length())
                    pelmTFTP->setAttribute("next-server", nic.nat.strTFTPNextServer);
            }
            for (NATRuleList::const_iterator rule = nic.nat.llRules.begin();
                    rule != nic.nat.llRules.end(); ++rule)
            {
                xml::ElementNode *pelmPF;
                pelmPF = pelmNAT->createChild("Forwarding");
                if ((*rule).strName.length())
                    pelmPF->setAttribute("name", (*rule).strName);
                pelmPF->setAttribute("proto", (*rule).proto);
                if ((*rule).strHostIP.length())
                    pelmPF->setAttribute("hostip", (*rule).strHostIP);
                if ((*rule).u16HostPort)
                    pelmPF->setAttribute("hostport", (*rule).u16HostPort);
                if ((*rule).strGuestIP.length())
                    pelmPF->setAttribute("guestip", (*rule).strGuestIP);
                if ((*rule).u16GuestPort)
                    pelmPF->setAttribute("guestport", (*rule).u16GuestPort);
            }
            break;

        case NetworkAttachmentType_Bridged:
            if (fEnabled || !nic.strBridgedName.isEmpty())
                elmParent.createChild("BridgedInterface")->setAttribute("name", nic.strBridgedName);
            break;

        case NetworkAttachmentType_Internal:
            if (fEnabled || !nic.strInternalNetworkName.isEmpty())
                elmParent.createChild("InternalNetwork")->setAttribute("name", nic.strInternalNetworkName);
            break;

        case NetworkAttachmentType_HostOnly:
            if (fEnabled || !nic.strHostOnlyName.isEmpty())
                elmParent.createChild("HostOnlyInterface")->setAttribute("name", nic.strHostOnlyName);
            break;

        case NetworkAttachmentType_Generic:
            if (fEnabled || !nic.strGenericDriver.isEmpty() || nic.genericProperties.size())
            {
                xml::ElementNode *pelmMode = elmParent.createChild("GenericInterface");
                pelmMode->setAttribute("driver", nic.strGenericDriver);
                for (StringsMap::const_iterator it = nic.genericProperties.begin();
                     it != nic.genericProperties.end();
                     ++it)
                {
                    xml::ElementNode *pelmProp = pelmMode->createChild("Property");
                    pelmProp->setAttribute("name", it->first);
                    pelmProp->setAttribute("value", it->second);
                }
            }
            break;

        default: /*case NetworkAttachmentType_Null:*/
            break;
    }
}

/**
 * Creates a <StorageControllers> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 * @param elmParent
 * @param st
 * @param fSkipRemovableMedia If true, DVD and floppy attachments are skipped and
 *   an empty drive is always written instead. This is for the OVF export case.
 *   This parameter is ignored unless the settings version is at least v1.9, which
 *   is always the case when this gets called for OVF export.
 * @param pllElementsWithUuidAttributes If not NULL, must point to a list of element node
 *   pointers to which we will append all elements that we created here that contain
 *   UUID attributes. This allows the OVF export code to quickly replace the internal
 *   media UUIDs with the UUIDs of the media that were exported.
 */
void MachineConfigFile::buildStorageControllersXML(xml::ElementNode &elmParent,
                                                   const Storage &st,
                                                   bool fSkipRemovableMedia,
                                                   std::list<xml::ElementNode*> *pllElementsWithUuidAttributes)
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
        com::Utf8Str name = sc.strName;
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
            case StorageControllerType_LsiLogicSas: pcszType = "LsiLogicSas"; break;
            default: /*case StorageControllerType_PIIX3:*/ pcszType = "PIIX3"; break;
        }
        pelmController->setAttribute("type", pcszType);

        pelmController->setAttribute("PortCount", sc.ulPortCount);

        if (m->sv >= SettingsVersion_v1_9)
            if (sc.ulInstance)
                pelmController->setAttribute("Instance", sc.ulInstance);

        if (m->sv >= SettingsVersion_v1_10)
            pelmController->setAttribute("useHostIOCache", sc.fUseHostIOCache);

        if (m->sv >= SettingsVersion_v1_11)
            pelmController->setAttribute("Bootable", sc.fBootable);

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
                    if (att.fNonRotational)
                        pelmDevice->setAttribute("nonrotational", att.fNonRotational);
                    if (att.fDiscard)
                        pelmDevice->setAttribute("discard", att.fDiscard);
                    break;

                case DeviceType_DVD:
                    pcszType = "DVD";
                    pelmDevice->setAttribute("passthrough", att.fPassThrough);
                    if (att.fTempEject)
                        pelmDevice->setAttribute("tempeject", att.fTempEject);
                    break;

                case DeviceType_Floppy:
                    pcszType = "Floppy";
                    break;
            }

            pelmDevice->setAttribute("type", pcszType);

            pelmDevice->setAttribute("port", att.lPort);
            pelmDevice->setAttribute("device", att.lDevice);

            if (att.strBwGroup.length())
                pelmDevice->setAttribute("bandwidthGroup", att.strBwGroup);

            // attached image, if any
            if (!att.uuid.isZero()
                 && att.uuid.isValid()
                 && (att.deviceType == DeviceType_HardDisk
                      || !fSkipRemovableMedia
                    )
               )
            {
                xml::ElementNode *pelmImage = pelmDevice->createChild("Image");
                pelmImage->setAttribute("uuid", att.uuid.toStringCurly());

                // if caller wants a list of UUID elements, give it to them
                if (pllElementsWithUuidAttributes)
                    pllElementsWithUuidAttributes->push_back(pelmImage);
            }
            else if (    (m->sv >= SettingsVersion_v1_9)
                      && (att.strHostDriveSrc.length())
                    )
                pelmDevice->createChild("HostDrive")->setAttribute("src", att.strHostDriveSrc);
        }
    }
}

/**
 * Creates a <Debugging> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 *
 * @param pElmParent    Pointer to the parent element.
 * @param pDbg          Pointer to the debugging settings.
 */
void MachineConfigFile::buildDebuggingXML(xml::ElementNode *pElmParent, const Debugging *pDbg)
{
    if (m->sv < SettingsVersion_v1_13 || pDbg->areDefaultSettings())
        return;

    xml::ElementNode *pElmDebugging = pElmParent->createChild("Debugging");
    xml::ElementNode *pElmTracing   = pElmDebugging->createChild("Tracing");
    pElmTracing->setAttribute("enabled", pDbg->fTracingEnabled);
    pElmTracing->setAttribute("allowTracingToAccessVM", pDbg->fAllowTracingToAccessVM);
    pElmTracing->setAttribute("config", pDbg->strTracingConfig);
}

/**
 * Creates a <Autostart> node under elmParent and then writes out the XML
 * keys under that. Called for both the <Machine> node and for snapshots.
 *
 * @param pElmParent    Pointer to the parent element.
 * @param pAutostart    Pointer to the autostart settings.
 */
void MachineConfigFile::buildAutostartXML(xml::ElementNode *pElmParent, const Autostart *pAutostart)
{
    const char *pcszAutostop = NULL;

    if (m->sv < SettingsVersion_v1_13 || pAutostart->areDefaultSettings())
        return;

    xml::ElementNode *pElmAutostart = pElmParent->createChild("Autostart");
    pElmAutostart->setAttribute("enabled", pAutostart->fAutostartEnabled);
    pElmAutostart->setAttribute("delay", pAutostart->uAutostartDelay);

    switch (pAutostart->enmAutostopType)
    {
        case AutostopType_Disabled:     pcszAutostop = "Disabled";     break;
        case AutostopType_SaveState:    pcszAutostop = "SaveState";    break;
        case AutostopType_PowerOff:     pcszAutostop = "PowerOff";     break;
        case AutostopType_AcpiShutdown: pcszAutostop = "AcpiShutdown"; break;
        default:         Assert(false); pcszAutostop = "Disabled";     break;
    }
    pElmAutostart->setAttribute("autostop", pcszAutostop);
}

/**
 * Creates a <Groups> node under elmParent and then writes out the XML
 * keys under that. Called for the <Machine> node only.
 *
 * @param pElmParent    Pointer to the parent element.
 * @param pllGroups     Pointer to the groups list.
 */
void MachineConfigFile::buildGroupsXML(xml::ElementNode *pElmParent, const StringsList *pllGroups)
{
    if (   m->sv < SettingsVersion_v1_13 || pllGroups->size() == 0
        || (pllGroups->size() == 1 && pllGroups->front() == "/"))
        return;

    xml::ElementNode *pElmGroups = pElmParent->createChild("Groups");
    for (StringsList::const_iterator it = pllGroups->begin();
         it != pllGroups->end();
         ++it)
    {
        const Utf8Str &group = *it;
        xml::ElementNode *pElmGroup = pElmGroups->createChild("Group");
        pElmGroup->setAttribute("name", group);
    }
}

/**
 * Writes a single snapshot into the DOM tree. Initially this gets called from MachineConfigFile::write()
 * for the root snapshot of a machine, if present; elmParent then points to the <Snapshots> node under the
 * <Machine> node to which <Snapshot> must be added. This may then recurse for child snapshots.
 * @param elmParent
 * @param snap
 */
void MachineConfigFile::buildSnapshotXML(xml::ElementNode &elmParent,
                                         const Snapshot &snap)
{
    xml::ElementNode *pelmSnapshot = elmParent.createChild("Snapshot");

    pelmSnapshot->setAttribute("uuid", snap.uuid.toStringCurly());
    pelmSnapshot->setAttribute("name", snap.strName);
    pelmSnapshot->setAttribute("timeStamp", makeString(snap.timestamp));

    if (snap.strStateFile.length())
        pelmSnapshot->setAttributePath("stateFile", snap.strStateFile);

    if (snap.strDescription.length())
        pelmSnapshot->createChild("Description")->addContent(snap.strDescription);

    buildHardwareXML(*pelmSnapshot, snap.hardware, snap.storage);
    buildStorageControllersXML(*pelmSnapshot,
                               snap.storage,
                               false /* fSkipRemovableMedia */,
                               NULL); /* pllElementsWithUuidAttributes */
                                    // we only skip removable media for OVF, but we never get here for OVF
                                    // since snapshots never get written then
    buildDebuggingXML(pelmSnapshot, &snap.debugging);
    buildAutostartXML(pelmSnapshot, &snap.autostart);
    // note: Groups exist only for Machine, not for Snapshot

    if (snap.llChildSnapshots.size())
    {
        xml::ElementNode *pelmChildren = pelmSnapshot->createChild("Snapshots");
        for (SnapshotsList::const_iterator it = snap.llChildSnapshots.begin();
             it != snap.llChildSnapshots.end();
             ++it)
        {
            const Snapshot &child = *it;
            buildSnapshotXML(*pelmChildren, child);
        }
    }
}

/**
 * Builds the XML DOM tree for the machine config under the given XML element.
 *
 * This has been separated out from write() so it can be called from elsewhere,
 * such as the OVF code, to build machine XML in an existing XML tree.
 *
 * As a result, this gets called from two locations:
 *
 *  --  MachineConfigFile::write();
 *
 *  --  Appliance::buildXMLForOneVirtualSystem()
 *
 * In fl, the following flag bits are recognized:
 *
 *  --  BuildMachineXML_MediaRegistry: If set, the machine's media registry will
 *      be written, if present. This is not set when called from OVF because OVF
 *      has its own variant of a media registry. This flag is ignored unless the
 *      settings version is at least v1.11 (VirtualBox 4.0).
 *
 *  --  BuildMachineXML_IncludeSnapshots: If set, descend into the snapshots tree
 *      of the machine and write out <Snapshot> and possibly more snapshots under
 *      that, if snapshots are present. Otherwise all snapshots are suppressed
 *      (when called from OVF).
 *
 *  --  BuildMachineXML_WriteVboxVersionAttribute: If set, add a settingsVersion
 *      attribute to the machine tag with the vbox settings version. This is for
 *      the OVF export case in which we don't have the settings version set in
 *      the root element.
 *
 *  --  BuildMachineXML_SkipRemovableMedia: If set, removable media attachments
 *      (DVDs, floppies) are silently skipped. This is for the OVF export case
 *      until we support copying ISO and RAW media as well.  This flag is ignored
 *      unless the settings version is at least v1.9, which is always the case
 *      when this gets called for OVF export.
 *
 * --   BuildMachineXML_SuppressSavedState: If set, the Machine/@stateFile
 *      attribute is never set. This is also for the OVF export case because we
 *      cannot save states with OVF.
 *
 * @param elmMachine XML <Machine> element to add attributes and elements to.
 * @param fl Flags.
 * @param pllElementsWithUuidAttributes pointer to list that should receive UUID elements or NULL;
 *        see buildStorageControllersXML() for details.
 */
void MachineConfigFile::buildMachineXML(xml::ElementNode &elmMachine,
                                        uint32_t fl,
                                        std::list<xml::ElementNode*> *pllElementsWithUuidAttributes)
{
    if (fl & BuildMachineXML_WriteVboxVersionAttribute)
        // add settings version attribute to machine element
        setVersionAttribute(elmMachine);

    elmMachine.setAttribute("uuid", uuid.toStringCurly());
    elmMachine.setAttribute("name", machineUserData.strName);
    if (machineUserData.fDirectoryIncludesUUID)
        elmMachine.setAttribute("directoryIncludesUUID", machineUserData.fDirectoryIncludesUUID);
    if (!machineUserData.fNameSync)
        elmMachine.setAttribute("nameSync", machineUserData.fNameSync);
    if (machineUserData.strDescription.length())
        elmMachine.createChild("Description")->addContent(machineUserData.strDescription);
    elmMachine.setAttribute("OSType", machineUserData.strOsType);
    if (    strStateFile.length()
         && !(fl & BuildMachineXML_SuppressSavedState)
       )
        elmMachine.setAttributePath("stateFile", strStateFile);

    if ((fl & BuildMachineXML_IncludeSnapshots)
         && !uuidCurrentSnapshot.isZero()
         && uuidCurrentSnapshot.isValid())
        elmMachine.setAttribute("currentSnapshot", uuidCurrentSnapshot.toStringCurly());

    if (machineUserData.strSnapshotFolder.length())
        elmMachine.setAttributePath("snapshotFolder", machineUserData.strSnapshotFolder);
    if (!fCurrentStateModified)
        elmMachine.setAttribute("currentStateModified", fCurrentStateModified);
    elmMachine.setAttribute("lastStateChange", makeString(timeLastStateChange));
    if (fAborted)
        elmMachine.setAttribute("aborted", fAborted);
    if (    m->sv >= SettingsVersion_v1_9
        &&  (   machineUserData.fTeleporterEnabled
            ||  machineUserData.uTeleporterPort
            ||  !machineUserData.strTeleporterAddress.isEmpty()
            ||  !machineUserData.strTeleporterPassword.isEmpty()
            )
       )
    {
        xml::ElementNode *pelmTeleporter = elmMachine.createChild("Teleporter");
        pelmTeleporter->setAttribute("enabled", machineUserData.fTeleporterEnabled);
        pelmTeleporter->setAttribute("port", machineUserData.uTeleporterPort);
        pelmTeleporter->setAttribute("address", machineUserData.strTeleporterAddress);
        pelmTeleporter->setAttribute("password", machineUserData.strTeleporterPassword);
    }

    if (    m->sv >= SettingsVersion_v1_11
        &&  (   machineUserData.enmFaultToleranceState != FaultToleranceState_Inactive
            ||  machineUserData.uFaultTolerancePort
            ||  machineUserData.uFaultToleranceInterval
            ||  !machineUserData.strFaultToleranceAddress.isEmpty()
            )
       )
    {
        xml::ElementNode *pelmFaultTolerance = elmMachine.createChild("FaultTolerance");
        switch (machineUserData.enmFaultToleranceState)
        {
        case FaultToleranceState_Inactive:
            pelmFaultTolerance->setAttribute("state", "inactive");
            break;
        case FaultToleranceState_Master:
            pelmFaultTolerance->setAttribute("state", "master");
            break;
        case FaultToleranceState_Standby:
            pelmFaultTolerance->setAttribute("state", "standby");
            break;
        }

        pelmFaultTolerance->setAttribute("port", machineUserData.uFaultTolerancePort);
        pelmFaultTolerance->setAttribute("address", machineUserData.strFaultToleranceAddress);
        pelmFaultTolerance->setAttribute("interval", machineUserData.uFaultToleranceInterval);
        pelmFaultTolerance->setAttribute("password", machineUserData.strFaultTolerancePassword);
    }

    if (    (fl & BuildMachineXML_MediaRegistry)
         && (m->sv >= SettingsVersion_v1_11)
       )
        buildMediaRegistry(elmMachine, mediaRegistry);

    buildExtraData(elmMachine, mapExtraDataItems);

    if (    (fl & BuildMachineXML_IncludeSnapshots)
         && llFirstSnapshot.size())
        buildSnapshotXML(elmMachine, llFirstSnapshot.front());

    buildHardwareXML(elmMachine, hardwareMachine, storageMachine);
    buildStorageControllersXML(elmMachine,
                               storageMachine,
                               !!(fl & BuildMachineXML_SkipRemovableMedia),
                               pllElementsWithUuidAttributes);
    buildDebuggingXML(&elmMachine, &debugging);
    buildAutostartXML(&elmMachine, &autostart);
    buildGroupsXML(&elmMachine, &machineUserData.llGroups);
}

/**
 * Returns true only if the given AudioDriverType is supported on
 * the current host platform. For example, this would return false
 * for AudioDriverType_DirectSound when compiled on a Linux host.
 * @param drv AudioDriverType_* enum to test.
 * @return true only if the current host supports that driver.
 */
/*static*/
bool MachineConfigFile::isAudioDriverAllowedOnThisHost(AudioDriverType_T drv)
{
    switch (drv)
    {
        case AudioDriverType_Null:
#ifdef RT_OS_WINDOWS
# ifdef VBOX_WITH_WINMM
        case AudioDriverType_WinMM:
# endif
        case AudioDriverType_DirectSound:
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
        case AudioDriverType_SolAudio:
#endif
#ifdef RT_OS_LINUX
# ifdef VBOX_WITH_ALSA
        case AudioDriverType_ALSA:
# endif
# ifdef VBOX_WITH_PULSE
        case AudioDriverType_Pulse:
# endif
#endif /* RT_OS_LINUX */
#if defined (RT_OS_LINUX) || defined (RT_OS_FREEBSD) || defined(VBOX_WITH_SOLARIS_OSS)
        case AudioDriverType_OSS:
#endif
#ifdef RT_OS_FREEBSD
# ifdef VBOX_WITH_PULSE
        case AudioDriverType_Pulse:
# endif
#endif
#ifdef RT_OS_DARWIN
        case AudioDriverType_CoreAudio:
#endif
#ifdef RT_OS_OS2
        case AudioDriverType_MMPM:
#endif
            return true;
    }

    return false;
}

/**
 * Returns the AudioDriverType_* which should be used by default on this
 * host platform. On Linux, this will check at runtime whether PulseAudio
 * or ALSA are actually supported on the first call.
 * @return
 */
/*static*/
AudioDriverType_T MachineConfigFile::getHostDefaultAudioDriver()
{
#if defined(RT_OS_WINDOWS)
# ifdef VBOX_WITH_WINMM
    return AudioDriverType_WinMM;
# else /* VBOX_WITH_WINMM */
    return AudioDriverType_DirectSound;
# endif /* !VBOX_WITH_WINMM */
#elif defined(RT_OS_SOLARIS)
    return AudioDriverType_SolAudio;
#elif defined(RT_OS_LINUX)
    // on Linux, we need to check at runtime what's actually supported...
    static RTCLockMtx s_mtx;
    static AudioDriverType_T s_linuxDriver = -1;
    RTCLock lock(s_mtx);
    if (s_linuxDriver == (AudioDriverType_T)-1)
    {
# if defined(VBOX_WITH_PULSE)
        /* Check for the pulse library & that the pulse audio daemon is running. */
        if (RTProcIsRunningByName("pulseaudio") &&
            RTLdrIsLoadable("libpulse.so.0"))
            s_linuxDriver = AudioDriverType_Pulse;
        else
# endif /* VBOX_WITH_PULSE */
# if defined(VBOX_WITH_ALSA)
            /* Check if we can load the ALSA library */
             if (RTLdrIsLoadable("libasound.so.2"))
                s_linuxDriver = AudioDriverType_ALSA;
        else
# endif /* VBOX_WITH_ALSA */
            s_linuxDriver = AudioDriverType_OSS;
    }
    return s_linuxDriver;
// end elif defined(RT_OS_LINUX)
#elif defined(RT_OS_DARWIN)
    return AudioDriverType_CoreAudio;
#elif defined(RT_OS_OS2)
    return AudioDriverType_MMPM;
#elif defined(RT_OS_FREEBSD)
    return AudioDriverType_OSS;
#else
    return AudioDriverType_Null;
#endif
}

/**
 * Called from write() before calling ConfigFileBase::createStubDocument().
 * This adjusts the settings version in m->sv if incompatible settings require
 * a settings bump, whereas otherwise we try to preserve the settings version
 * to avoid breaking compatibility with older versions.
 *
 * We do the checks in here in reverse order: newest first, oldest last, so
 * that we avoid unnecessary checks since some of these are expensive.
 */
void MachineConfigFile::bumpSettingsVersionIfNeeded()
{
    if (m->sv < SettingsVersion_v1_13)
    {
        // VirtualBox 4.2 adds tracing, autostart, UUID in directory and groups.
        if (   !debugging.areDefaultSettings()
            || !autostart.areDefaultSettings()
            || machineUserData.fDirectoryIncludesUUID
            || machineUserData.llGroups.size() > 1
            || machineUserData.llGroups.front() != "/")
            m->sv = SettingsVersion_v1_13;
    }

    if (m->sv < SettingsVersion_v1_13)
    {
        // VirtualBox 4.2 changes the units for bandwidth group limits.
        for (BandwidthGroupList::const_iterator it = hardwareMachine.ioSettings.llBandwidthGroups.begin();
             it != hardwareMachine.ioSettings.llBandwidthGroups.end();
             ++it)
        {
            const BandwidthGroup &gr = *it;
            if (gr.cMaxBytesPerSec % _1M)
            {
                // Bump version if a limit cannot be expressed in megabytes
                m->sv = SettingsVersion_v1_13;
                break;
            }
        }
    }

    if (m->sv < SettingsVersion_v1_13)
    {
        /* 4.2: Emulated USB Webcam. */
        if (hardwareMachine.fEmulatedUSBWebcam)
            m->sv = SettingsVersion_v1_13;
    }

    if (m->sv < SettingsVersion_v1_12)
    {
        // 4.1: Emulated USB devices.
        if (hardwareMachine.fEmulatedUSBCardReader)
            m->sv = SettingsVersion_v1_12;
    }

    if (m->sv < SettingsVersion_v1_12)
    {
        // VirtualBox 4.1 adds PCI passthrough.
        if (hardwareMachine.pciAttachments.size())
            m->sv = SettingsVersion_v1_12;
    }

    if (m->sv < SettingsVersion_v1_12)
    {
        // VirtualBox 4.1 adds a promiscuous mode policy to the network
        // adapters and a generic network driver transport.
        NetworkAdaptersList::const_iterator netit;
        for (netit = hardwareMachine.llNetworkAdapters.begin();
             netit != hardwareMachine.llNetworkAdapters.end();
             ++netit)
        {
            if (   netit->enmPromiscModePolicy != NetworkAdapterPromiscModePolicy_Deny
                || netit->mode == NetworkAttachmentType_Generic
                || !netit->strGenericDriver.isEmpty()
                || netit->genericProperties.size()
               )
            {
                m->sv = SettingsVersion_v1_12;
                break;
            }
        }
    }

    if (m->sv < SettingsVersion_v1_11)
    {
        // VirtualBox 4.0 adds HD audio, CPU priorities, fault tolerance,
        // per-machine media registries, VRDE, JRockitVE, bandwidth groups,
        // ICH9 chipset
        if (    hardwareMachine.audioAdapter.controllerType == AudioControllerType_HDA
             || hardwareMachine.ulCpuExecutionCap != 100
             || machineUserData.enmFaultToleranceState != FaultToleranceState_Inactive
             || machineUserData.uFaultTolerancePort
             || machineUserData.uFaultToleranceInterval
             || !machineUserData.strFaultToleranceAddress.isEmpty()
             || mediaRegistry.llHardDisks.size()
             || mediaRegistry.llDvdImages.size()
             || mediaRegistry.llFloppyImages.size()
             || !hardwareMachine.vrdeSettings.strVrdeExtPack.isEmpty()
             || !hardwareMachine.vrdeSettings.strAuthLibrary.isEmpty()
             || machineUserData.strOsType == "JRockitVE"
             || hardwareMachine.ioSettings.llBandwidthGroups.size()
             || hardwareMachine.chipsetType == ChipsetType_ICH9
           )
            m->sv = SettingsVersion_v1_11;
    }

    if (m->sv < SettingsVersion_v1_10)
    {
        /* If the properties contain elements other than "TCP/Ports" and "TCP/Address",
         * then increase the version to at least VBox 3.2, which can have video channel properties.
         */
        unsigned cOldProperties = 0;

        StringsMap::const_iterator it = hardwareMachine.vrdeSettings.mapProperties.find("TCP/Ports");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;
        it = hardwareMachine.vrdeSettings.mapProperties.find("TCP/Address");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;

        if (hardwareMachine.vrdeSettings.mapProperties.size() != cOldProperties)
            m->sv = SettingsVersion_v1_10;
    }

    if (m->sv < SettingsVersion_v1_11)
    {
        /* If the properties contain elements other than "TCP/Ports", "TCP/Address",
         * "VideoChannel/Enabled" and "VideoChannel/Quality" then increase the version to VBox 4.0.
         */
        unsigned cOldProperties = 0;

        StringsMap::const_iterator it = hardwareMachine.vrdeSettings.mapProperties.find("TCP/Ports");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;
        it = hardwareMachine.vrdeSettings.mapProperties.find("TCP/Address");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;
        it = hardwareMachine.vrdeSettings.mapProperties.find("VideoChannel/Enabled");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;
        it = hardwareMachine.vrdeSettings.mapProperties.find("VideoChannel/Quality");
        if (it != hardwareMachine.vrdeSettings.mapProperties.end())
            cOldProperties++;

        if (hardwareMachine.vrdeSettings.mapProperties.size() != cOldProperties)
            m->sv = SettingsVersion_v1_11;
    }

    // settings version 1.9 is required if there is not exactly one DVD
    // or more than one floppy drive present or the DVD is not at the secondary
    // master; this check is a bit more complicated
    //
    // settings version 1.10 is required if the host cache should be disabled
    //
    // settings version 1.11 is required for bandwidth limits and if more than
    // one controller of each type is present.
    if (m->sv < SettingsVersion_v1_11)
    {
        // count attached DVDs and floppies (only if < v1.9)
        size_t cDVDs = 0;
        size_t cFloppies = 0;

        // count storage controllers (if < v1.11)
        size_t cSata = 0;
        size_t cScsiLsi = 0;
        size_t cScsiBuslogic = 0;
        size_t cSas = 0;
        size_t cIde = 0;
        size_t cFloppy = 0;

        // need to run thru all the storage controllers and attached devices to figure this out
        for (StorageControllersList::const_iterator it = storageMachine.llStorageControllers.begin();
             it != storageMachine.llStorageControllers.end();
             ++it)
        {
            const StorageController &sctl = *it;

            // count storage controllers of each type; 1.11 is required if more than one
            // controller of one type is present
            switch (sctl.storageBus)
            {
                case StorageBus_IDE:
                    cIde++;
                    break;
                case StorageBus_SATA:
                    cSata++;
                    break;
                case StorageBus_SAS:
                    cSas++;
                    break;
                case StorageBus_SCSI:
                    if (sctl.controllerType == StorageControllerType_LsiLogic)
                        cScsiLsi++;
                    else
                        cScsiBuslogic++;
                    break;
                case StorageBus_Floppy:
                    cFloppy++;
                    break;
                default:
                    // Do nothing
                    break;
            }

            if (   cSata > 1
                || cScsiLsi > 1
                || cScsiBuslogic > 1
                || cSas > 1
                || cIde > 1
                || cFloppy > 1)
            {
                m->sv = SettingsVersion_v1_11;
                break; // abort the loop -- we will not raise the version further
            }

            for (AttachedDevicesList::const_iterator it2 = sctl.llAttachedDevices.begin();
                 it2 != sctl.llAttachedDevices.end();
                 ++it2)
            {
                const AttachedDevice &att = *it2;

                // Bandwidth limitations are new in VirtualBox 4.0 (1.11)
                if (m->sv < SettingsVersion_v1_11)
                {
                    if (att.strBwGroup.length() != 0)
                    {
                        m->sv = SettingsVersion_v1_11;
                        break; // abort the loop -- we will not raise the version further
                    }
                }

                // disabling the host IO cache requires settings version 1.10
                if (    (m->sv < SettingsVersion_v1_10)
                     && (!sctl.fUseHostIOCache)
                   )
                    m->sv = SettingsVersion_v1_10;

                // we can only write the StorageController/@Instance attribute with v1.9
                if (    (m->sv < SettingsVersion_v1_9)
                     && (sctl.ulInstance != 0)
                   )
                    m->sv = SettingsVersion_v1_9;

                if (m->sv < SettingsVersion_v1_9)
                {
                    if (att.deviceType == DeviceType_DVD)
                    {
                         if (    (sctl.storageBus != StorageBus_IDE) // DVD at bus other than DVD?
                              || (att.lPort != 1)                    // DVDs not at secondary master?
                              || (att.lDevice != 0)
                            )
                            m->sv = SettingsVersion_v1_9;

                        ++cDVDs;
                    }
                    else if (att.deviceType == DeviceType_Floppy)
                        ++cFloppies;
                }
            }

            if (m->sv >= SettingsVersion_v1_11)
                break;  // abort the loop -- we will not raise the version further
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

    // VirtualBox 3.2: Check for non default I/O settings
    if (m->sv < SettingsVersion_v1_10)
    {
        if (   (hardwareMachine.ioSettings.fIOCacheEnabled != true)
            || (hardwareMachine.ioSettings.ulIOCacheSize != 5)
                // and page fusion
            || (hardwareMachine.fPageFusionEnabled)
                // and CPU hotplug, RTC timezone control, HID type and HPET
            || machineUserData.fRTCUseUTC
            || hardwareMachine.fCpuHotPlug
            || hardwareMachine.pointingHIDType != PointingHIDType_PS2Mouse
            || hardwareMachine.keyboardHIDType != KeyboardHIDType_PS2Keyboard
            || hardwareMachine.fHPETEnabled
           )
            m->sv = SettingsVersion_v1_10;
    }

    // VirtualBox 3.2 adds NAT and boot priority to the NIC config in Main
    // VirtualBox 4.0 adds network bandwitdth
    if (m->sv < SettingsVersion_v1_11)
    {
        NetworkAdaptersList::const_iterator netit;
        for (netit = hardwareMachine.llNetworkAdapters.begin();
             netit != hardwareMachine.llNetworkAdapters.end();
             ++netit)
        {
            if (    (m->sv < SettingsVersion_v1_12)
                 && (netit->strBandwidthGroup.isNotEmpty())
               )
            {
                /* New in VirtualBox 4.1 */
                m->sv = SettingsVersion_v1_12;
                break;
            }
            else if (    (m->sv < SettingsVersion_v1_10)
                      && (netit->fEnabled)
                      && (netit->mode == NetworkAttachmentType_NAT)
                      && (   netit->nat.u32Mtu != 0
                          || netit->nat.u32SockRcv != 0
                          || netit->nat.u32SockSnd != 0
                          || netit->nat.u32TcpRcv != 0
                          || netit->nat.u32TcpSnd != 0
                          || !netit->nat.fDNSPassDomain
                          || netit->nat.fDNSProxy
                          || netit->nat.fDNSUseHostResolver
                          || netit->nat.fAliasLog
                          || netit->nat.fAliasProxyOnly
                          || netit->nat.fAliasUseSamePorts
                          || netit->nat.strTFTPPrefix.length()
                          || netit->nat.strTFTPBootFile.length()
                          || netit->nat.strTFTPNextServer.length()
                          || netit->nat.llRules.size()
                         )
                     )
            {
                m->sv = SettingsVersion_v1_10;
                // no break because we still might need v1.11 above
            }
            else if (    (m->sv < SettingsVersion_v1_10)
                      && (netit->fEnabled)
                      && (netit->ulBootPriority != 0)
                    )
            {
                m->sv = SettingsVersion_v1_10;
                // no break because we still might need v1.11 above
            }
        }
    }

    // all the following require settings version 1.9
    if (    (m->sv < SettingsVersion_v1_9)
         && (    (hardwareMachine.firmwareType >= FirmwareType_EFI)
              || (hardwareMachine.fHardwareVirtExclusive != HWVIRTEXCLUSIVEDEFAULT)
              || machineUserData.fTeleporterEnabled
              || machineUserData.uTeleporterPort
              || !machineUserData.strTeleporterAddress.isEmpty()
              || !machineUserData.strTeleporterPassword.isEmpty()
              || (!hardwareMachine.uuid.isZero() && hardwareMachine.uuid.isValid())
            )
        )
        m->sv = SettingsVersion_v1_9;

    // "accelerate 2d video" requires settings version 1.8
    if (    (m->sv < SettingsVersion_v1_8)
         && (hardwareMachine.fAccelerate2DVideo)
       )
        m->sv = SettingsVersion_v1_8;

    // The hardware versions other than "1" requires settings version 1.4 (2.1+).
    if (    m->sv < SettingsVersion_v1_4
         && hardwareMachine.strVersion != "1"
       )
        m->sv = SettingsVersion_v1_4;
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
        buildMachineXML(*pelmMachine,
                          MachineConfigFile::BuildMachineXML_IncludeSnapshots
                        | MachineConfigFile::BuildMachineXML_MediaRegistry,
                            // but not BuildMachineXML_WriteVboxVersionAttribute
                        NULL); /* pllElementsWithUuidAttributes */

        // now go write the XML
        xml::XmlFileWriter writer(*m->pDoc);
        writer.write(m->strFilename.c_str(), true /*fSafe*/);

        m->fFileExists = true;
        clearDocument();
    }
    catch (...)
    {
        clearDocument();
        throw;
    }
}
