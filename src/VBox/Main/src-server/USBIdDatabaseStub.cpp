/*
* Copyright(C) 2005 - 2015 Oracle Corporation
*
* This file is part of VirtualBox Open Source Edition(OSE), as
* available from http ://www.virtualbox.org. This file is free software;
* you can redistribute it and / or modify it under the terms of the GNU
* General Public License(GPL) as published by the Free Software
* Foundation, in version 2 as it comes in the "COPYING" file of the
* VirtualBox OSE distribution.VirtualBox OSE is distributed in the
* hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
*
*/

#include "USBIdDatabase.h"

/** USB devices aliases array.
*   Format: VendorId, ProductId, Vendor Name, Product Name
*   The source of the list is http://www.linux-usb.org/usb.ids
*/
Product AliasDictionary::productArray[] = {0};

const size_t AliasDictionary::products_size = sizeof(AliasDictionary::productArray) / sizeof(Product);

Vendor AliasDictionary::vendorArray[] = {0};

const size_t AliasDictionary::vendors_size = sizeof(AliasDictionary::vendorArray) / sizeof(Vendor);
