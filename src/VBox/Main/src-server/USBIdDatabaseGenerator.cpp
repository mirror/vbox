/*
* Copyright (C) 2006-2015 Oracle Corporation
*
* This file is part of VirtualBox Open Source Edition (OSE), as
* available from http://www.virtualbox.org. This file is free software;
* you can redistribute it and/or modify it under the terms of the GNU
* General Public License (GPL) as published by the Free Software
* Foundation, in version 2 as it comes in the "COPYING" file of the
* VirtualBox OSE distribution. VirtualBox OSE is distributed in the
* hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
*/

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

#include <iprt/string.h>

using namespace std;

const char* header = "/*\n\
* Copyright(C) 2005 - 2015 Oracle Corporation\n\
*\n\
* This file is part of VirtualBox Open Source Edition(OSE), as\n\
* available from http ://www.virtualbox.org. This file is free software;\n\
* you can redistribute it and / or modify it under the terms of the GNU\n\
* General Public License(GPL) as published by the Free Software\n\
* Foundation, in version 2 as it comes in the \"COPYING\" file of the\n\
* VirtualBox OSE distribution.VirtualBox OSE is distributed in the\n\
* hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.\n\
*\n\
*/\
\n\
\n\
#include \"USBIdDatabase.h\"\n\
\n\
/** USB devices aliases array.\n\
*   Format: VendorId, ProductId, Vendor Name, Product Name\n\
*   The source of the list is http://www.linux-usb.org/usb.ids\n\
*/\n\
Product AliasDictionary::productArray[] = { \n";

const char* footer = "};\n\
\n\
const size_t AliasDictionary::products_size = sizeof(AliasDictionary::productArray) / sizeof(Product); \n";

const char* vendor_header = "\nVendor AliasDictionary::vendorArray[] = { \n";
const char* vendor_footer = "};\n\
\n\
const size_t AliasDictionary::vendors_size = sizeof(AliasDictionary::vendorArray) / sizeof(Vendor);";

const wchar_t* start_block = L"#		interface  interface_name		<-- two tabs";
const wchar_t* end_block = L"# List of known device classes, subclasses and protocols";

#define USBKEY(vendorId, productId) (((vendorId) << 16) | (productId))

// error codes
#define ERROR_INVALID_ARGUMENTS (1)
#define ERROR_OPEN_FILE         (2)
#define ERROR_IN_PARSE_LINE     (3)
#define ERROR_DUPLICATE_ENTRY   (4)
#define ERROR_WRONG_FILE_FORMAT (5)

struct VendorRecord
{
    size_t vendorID;
    wstring vendor;
};

struct ProductRecord
{
    size_t key;
    size_t vendorID;
    size_t productID;
    wstring product;
};

bool operator < (const ProductRecord& lh, const ProductRecord& rh)
{
    return lh.key < rh.key;
}

bool operator < (const VendorRecord& lh, const VendorRecord& rh)
{
    return lh.vendorID < rh.vendorID;
}

bool operator == (const ProductRecord& lh, const ProductRecord& rh)
{
    return lh.key == rh.key;
}

bool operator == (const VendorRecord& lh, const VendorRecord& rh)
{
    return lh.vendorID == rh.vendorID;
}

string conv(const wstring& src)
{
    string res;
    char* buf = NULL;
    
    if (RTUtf16ToLatin1((PRTUTF16)src.c_str(), &buf) == VINF_SUCCESS)
    {
        size_t len = strlen(buf);
        for (size_t i = 0; i < len; i++)
        {
            if (buf[i] == '"')
                buf[i] = '\'';
        }
        res = buf;
        RTStrFree(buf);
    }
    return res;
}

ostream& operator <<(ostream& stream, const ProductRecord product)
{
    stream << "{USBKEY(0x" << hex << product.vendorID
        << ", 0x" << product.productID << "), "
        << "\"" << conv(product.product).c_str() << "\" }," << endl;
    return stream;
}

wostream& operator <<(wostream& stream, const ProductRecord product)
{
    stream << L"{USBKEY(0x" << hex << product.vendorID
        << L", 0x" << product.productID << L"), "
        << L"\"" << product.product.c_str() << L"\" }," << endl;
    return stream;
}

ostream& operator <<(ostream& stream, const VendorRecord vendor)
{
    stream << "{0x" << hex << vendor.vendorID
        << ", \"" << conv(vendor.vendor).c_str() << "\" }," << endl;
    return stream;
}

wostream& operator <<(wostream& stream, const VendorRecord vendor)
{
    stream << L"{0x" << hex << vendor.vendorID
        << L", \"" << vendor.vendor.c_str() << L"\" }," << endl;
    return stream;
}

namespace State
{
    typedef int Value;
    enum
    {
        lookForStartBlock,
        lookForEndBlock,
        finished
    };
}

typedef vector<ProductRecord> ProductsSet;
typedef vector<VendorRecord> VendorsSet;
ProductsSet g_products;
VendorsSet g_vendors;


int ParseAlias(const wstring& src, size_t& id, wstring& desc)
{
    int i = 0;
    int idx = 0;
    wstring sin;

    if (swscanf(src.c_str(), L"%x", &i) != 1)
        return ERROR_IN_PARSE_LINE;

    size_t index = src.find_first_of(L" \t", 1);
    index = src.find_first_not_of(L" \t", index);

    if (index == string::npos)
        return ERROR_IN_PARSE_LINE;

    sin = src.substr(index);
    id = i;
    desc = sin;

    return 0;
}

bool IsCommentOrEmptyLine(const wstring& str)
{
    size_t index = str.find_first_not_of(L" \t");// skip left spaces
    return index == string::npos || str[index] == L'#';
}

int ParseUsbIds(wifstream& instream)
{
    State::Value state = State::lookForStartBlock;
    wstring line;
    int res = 0;
    VendorRecord vendor = { 0, L"" };

    while (state != State::finished && getline(instream, line))
    {
        switch (state)
        {
        case State::lookForStartBlock:
        {
            if (line.find(start_block) != string::npos)
                state = State::lookForEndBlock;
            break;
        }
        case State::lookForEndBlock:
        {
            if (line.find(end_block) != string::npos)
                state = State::finished;
            else
            {
                if (!IsCommentOrEmptyLine(line))
                {
                    if (line[0] == '\t')
                    {
                        // Parse Product line
                        // first line should be vendor
                        if (vendor.vendorID == 0)
                        {
                            cerr << "Wrong file format. Product before vendor: "
                                << line.c_str() << "'" << endl;
                            return ERROR_WRONG_FILE_FORMAT;
                        }
                        ProductRecord product = { 0, vendor.vendorID, 0, L"" };
                        if (ParseAlias(line.substr(1), product.productID, product.product) != 0)
                        {
                            cerr << "Error in parsing product line: '"
                                << line.c_str() << "'" << endl;
                            return ERROR_IN_PARSE_LINE;
                        }
                        product.key = USBKEY(product.vendorID, product.productID);
                        Assert(product.vendorID != 0);
                        g_products.push_back(product);
                    }
                    else
                    {
                        // Parse vendor line
                        if (ParseAlias(line, vendor.vendorID, vendor.vendor) != 0)
                        {
                            cerr << "Error in parsing vendor line: '"
                                << line.c_str() << "'" << endl;
                            return ERROR_IN_PARSE_LINE;
                        }
                        g_vendors.push_back(vendor);
                    }
                }
            }
            break;
        }
        }
    }
    if (state == State::lookForStartBlock)
    {
        cerr << "Error: wrong format of input file. Start line is not found." << endl;
        return ERROR_WRONG_FILE_FORMAT;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cerr << "Format: " << argv[0] <<
            " [linux.org usb list file] [custom usb list file] [-o output file]" << endl;
        cerr << "Error: Invalid arguments." << endl;
        return ERROR_INVALID_ARGUMENTS;
    }
    ofstream fout;
    wifstream fin;
    g_products.reserve(20000);
    g_vendors.reserve(3500);

    char* outName = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            outName = argv[++i];
            continue;
        }

        fin.open(argv[i]);
        if (!fin.is_open())
        {
            cerr << "Format: " << argv[0] <<
                " [linux.org usb list file] [custom usb list file] [-o output file]" << endl;
            cerr << "Error: Can not open file '" << argv[i] << "'." << endl;
            return ERROR_OPEN_FILE;
        }

        int res = ParseUsbIds(fin);
        if (res != 0)
        {
            cerr << "Error in parsing USB devices file '" <<
                argv[i] << "'" << endl;
            fin.close();
            return res;
        }
        fin.close();
    }

    sort(g_products.begin(), g_products.end());
    sort(g_vendors.begin(), g_vendors.end());

    // validate that all records are unique
    ProductsSet::iterator it = adjacent_find(g_products.begin(), g_products.end());
    if (it != g_products.end())
    {
        cerr << "Error: Duplicate alias detected. " << *it << endl;
        return ERROR_DUPLICATE_ENTRY;
    }

    if (!outName)
    {
        cerr << "Format: " << argv[0] <<
            " [linux.org usb list file] [custom usb list file] [-o output file]" << endl;
        cerr << "Error: Output file is not defined." << endl;
        return ERROR_OPEN_FILE;
    }

    fout.open(outName);
    if (!fout.is_open())
    {
        cerr << "Format: " << argv[0] <<
            " [linux.org usb list file] [custom usb list file] [-o output file]" << endl;
        cerr << "Error: Can not open file to write '" << argv[1] << "'." << endl;
        return ERROR_OPEN_FILE;
    }

    fout << header;
    for (ProductsSet::iterator it = g_products.begin(); it != g_products.end(); ++it)
    {
        fout << *it;
    }
    fout << footer;

    fout << vendor_header;
    for (VendorsSet::iterator it = g_vendors.begin(); it != g_vendors.end(); ++it)
    {
        fout << *it;
    }
    fout << vendor_footer;

    fout.close();


    return 0;
}

