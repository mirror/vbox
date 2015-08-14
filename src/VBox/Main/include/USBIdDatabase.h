/*
* Copyright (C) 2015 Oracle Corporation
*
* This file is part of VirtualBox Open Source Edition (OSE), as
* available from http://www.virtualbox.org. This file is free software;
* you can redistribute it and/or modify it under the terms of the GNU
* General Public License (GPL) as published by the Free Software
* Foundation, in version 2 as it comes in the "COPYING" file of the
* VirtualBox OSE distribution. VirtualBox OSE is distributed in the
* hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
*/

#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <iprt/stdint.h>

/**
* Macros to make key of alias table
*/
#define USBKEY(vendorId, productId) (((uint32_t)(vendorId) << 16) | (productId))

/**
* Elements of Aliases table
*/
class Product
{
public:
    uint32_t key;
    const char* product;
};

/**
* Element of Vendors table
*/
class Vendor
{
public:
    unsigned short vendorID;
    const char* vendor;
};

class ProductLess :
    public std::binary_function<Product, Product, bool>
{
public:
    bool operator () (const Product& left, const Product& right) const
    {
        return left.key < right.key;
    }
};

class VendorLess :
    public std::binary_function<Vendor, Vendor, bool>
{
public:
    bool operator () (const Vendor& left, const Vendor& right) const
    {
        return left.vendorID < right.vendorID;
    }
};


/**
* Wrapper for static array of Aliases.
*/
class AliasDictionary
{
protected:
    static Product productArray[];
    static const size_t products_size;
    static Vendor vendorArray[];
    static const size_t vendors_size;

public:
    static const char* findProduct(unsigned short vendorId, unsigned short productId)
    {
        Product lookFor = { USBKEY(vendorId, productId) };
        Product* it = std::lower_bound(productArray, productArray + products_size, lookFor, ProductLess());
        return lookFor.key == it->key ? it->product : NULL;
    }

    static const char* findVendor(unsigned short vendorID)
    {
        Vendor lookFor = { vendorID };
        Vendor* it = std::lower_bound(vendorArray, vendorArray + vendors_size, lookFor, VendorLess());
        return lookFor.vendorID == it->vendorID ? it->vendor : NULL;
    }
};
