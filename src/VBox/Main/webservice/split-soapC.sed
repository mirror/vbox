# $Id$
## @file
# Sed script for splitting up the monsterous soapC.cpp
#

#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
#
# Sun Microsystems, Inc. confidential
# All rights reserved
#

#
# This implementations uses ranges.
#
# Since the ranges have an inclusive end criteria, we have to repeate
# each location three times to make it work. It's a bit annoying but
# it is much shorter than an approach using the hold space as a state
# variable...
#

# Initialize the files.
1 b begin
:begin_return

:range1
1,/^SOAP_FMAC3 int SOAP_FMAC4 soap_ignore_element/ {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_ignore_element/b banner2
	w soapC-1.cpp
	b done
}

:range2
/^SOAP_FMAC3 int SOAP_FMAC4 soap_ignore_element/,/^SOAP_FMAC3 int SOAP_FMAC4 soap_fdelete/ {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_fdelete/b banner3
	w soapC-2.cpp
	b done
}

:range3
/^SOAP_FMAC3 int SOAP_FMAC4 soap_fdelete/,/^SOAP_FMAC3 void\* SOAP_FMAC4 soap_class_id_enter/ {
	/^SOAP_FMAC3 void\* SOAP_FMAC4 soap_class_id_enter/b banner4
	w soapC-3.cpp
	b done
}

:range4
/^SOAP_FMAC3 void\* SOAP_FMAC4 soap_class_id_enter/,/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IUSBDeviceFilter_USCOREsetMaskedInterfaces/ {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IUSBDeviceFilter_USCOREsetMaskedInterfaces/b banner5
	w soapC-4.cpp
	b done
}

:range5
/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IUSBDeviceFilter_USCOREsetMaskedInterfaces/,/^void _vbox__IUSBDevice_USCOREgetRemoteResponse::soap_default/ {
	/^void _vbox__IUSBDevice_USCOREgetRemoteResponse::soap_default/b banner6
	w soapC-5.cpp
	b done
}

:range6
/^void _vbox__IUSBDevice_USCOREgetRemoteResponse::soap_default/,/^int _vbox__INetworkAdapter_USCOREdetachResponse::soap_out/  {
	/^int _vbox__INetworkAdapter_USCOREdetachResponse::soap_out/b banner7
	w soapC-6.cpp
	b done
}

:range7
/^int _vbox__INetworkAdapter_USCOREdetachResponse::soap_out/,/^void _vbox__IFloppyDrive_USCOREgetHostDriveResponse::soap_default/  {
	/^void _vbox__IFloppyDrive_USCOREgetHostDriveResponse::soap_default/b banner8
	w soapC-7.cpp
	b done
}

:range8
/^void _vbox__IFloppyDrive_USCOREgetHostDriveResponse::soap_default/,/^void _vbox__IMedium_USCOREsetLocationResponse::soap_default/  {
	/^void _vbox__IMedium_USCOREsetLocationResponse::soap_default/b banner9
	w soapC-8.cpp
	b done
}

:range9
/^void _vbox__IMedium_USCOREsetLocationResponse::soap_default/,/^void _vbox__ISystemProperties_USCOREgetMaxVDISizeResponse::soap_serialize/  {
	/^void _vbox__ISystemProperties_USCOREgetMaxVDISizeResponse::soap_serialize/b banner10
	w soapC-9.cpp
	b done
}

:range10
/^void _vbox__ISystemProperties_USCOREgetMaxVDISizeResponse::soap_serialize/,/^int _vbox__IConsole_USCOREdiscardCurrentSnapshotAndState::soap_out/  {
	/^int _vbox__IConsole_USCOREdiscardCurrentSnapshotAndState::soap_out/b banner11
	w soapC-10.cpp
	b done
}

:range11
/^int _vbox__IConsole_USCOREdiscardCurrentSnapshotAndState::soap_out/,/^void _vbox__IMachine_USCOREgetNextExtraDataKeyResponse::soap_default/  {
	/^void _vbox__IMachine_USCOREgetNextExtraDataKeyResponse::soap_default/b banner12
	w soapC-11.cpp
	b done
}

:range12
/^void _vbox__IMachine_USCOREgetNextExtraDataKeyResponse::soap_default/,/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IMachine_USCOREgetVRAMSizeResponse/  {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IMachine_USCOREgetVRAMSizeResponse/b banner13
	w soapC-12.cpp
	b done
}

:range13
/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IMachine_USCOREgetVRAMSizeResponse/,/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IVirtualBox_USCOREgetNextExtraDataKey/  {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IVirtualBox_USCOREgetNextExtraDataKey/b banner14
	w soapC-13.cpp
	b done
}

:range14
/^SOAP_FMAC3 int SOAP_FMAC4 soap_out__vbox__IVirtualBox_USCOREgetNextExtraDataKey/,/^SOAP_FMAC3 int SOAP_FMAC4 soap_out___vbox__IVRDPServer_USCOREgetReuseSingleConnection/  {
	/^SOAP_FMAC3 int SOAP_FMAC4 soap_out___vbox__IVRDPServer_USCOREgetReuseSingleConnection/b banner15
	w soapC-14.cpp
	b done
}

:range15
/^SOAP_FMAC3 int SOAP_FMAC4 soap_out___vbox__IVRDPServer_USCOREgetReuseSingleConnection/,/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize___vbox__IFloppyDrive_USCOREgetHostDrive/  {
	/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize___vbox__IFloppyDrive_USCOREgetHostDrive/b banner16
	w soapC-15.cpp
	b done
}

:range16
/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize___vbox__IFloppyDrive_USCOREgetHostDrive/,/^SOAP_FMAC3 void SOAP_FMAC4 soap_default___vbox__IMachine_USCOREgetHWVirtExNestedPagingEnabled/ {
	/^SOAP_FMAC3 void SOAP_FMAC4 soap_default___vbox__IMachine_USCOREgetHWVirtExNestedPagingEnabled/b banner17
	w soapC-16.cpp
	b done
}

:range17
/^SOAP_FMAC3 void SOAP_FMAC4 soap_default___vbox__IMachine_USCOREgetHWVirtExNestedPagingEnabled/,/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IDVDDrive_USCOREgetPassthrough/ {
	/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IDVDDrive_USCOREgetPassthrough/b banner18
	w soapC-17.cpp
	b done
}

:range18
/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IDVDDrive_USCOREgetPassthrough/,/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IConsole_USCOREpowerButtonResponse/ {
	/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IConsole_USCOREpowerButtonResponse/b banner19
	w soapC-18.cpp
	b done
}

:range19
/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IConsole_USCOREpowerButtonResponse/,/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IVirtualBox_USCOREsetExtraData/ {
	/^SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerTo_vbox__IVirtualBox_USCOREsetExtraData/b banner20
	w soapC-19.cpp
	b done
}

# the rest goes into the 20th file.
:range20
w soapC-20.cpp
b done


##
# Subroutine called on the first line.
:begin
x
s/^.*$/\#include \"soapH\.h\"/
w soapC-2.cpp
w soapC-3.cpp
w soapC-4.cpp
w soapC-5.cpp
w soapC-6.cpp
w soapC-7.cpp
w soapC-8.cpp
w soapC-9.cpp
w soapC-10.cpp
w soapC-11.cpp
w soapC-12.cpp
w soapC-13.cpp
w soapC-14.cpp
w soapC-15.cpp
w soapC-16.cpp
w soapC-17.cpp
w soapC-18.cpp
w soapC-19.cpp
w soapC-20.cpp
x
i\
info: soapC-1.cpp
b begin_return

##
# Subroutines for writing info: soapC-x.cpp as we leave a range.
:banner2
i\
info: soapC-2.cpp
b range2

:banner3
i\
info: soapC-3.cpp
b range3

:banner4
i\
info: soapC-4.cpp
b range4

:banner5
i\
info: soapC-5.cpp
b range5

:banner6
i\
info: soapC-6.cpp
b range6

:banner7
i\
info: soapC-7.cpp
b range7

:banner8
i\
info: soapC-8.cpp
b range8

:banner9
i\
info: soapC-9.cpp
b range9

:banner10
i\
info: soapC-10.cpp
b range10

:banner11
i\
info: soapC-11.cpp
b range11

:banner12
i\
info: soapC-12.cpp
b range12

:banner13
i\
info: soapC-13.cpp
b range13

:banner14
i\
info: soapC-14.cpp
b range14

:banner15
i\
info: soapC-15.cpp
b range15

:banner16
i\
info: soapC-16.cpp
b range16

:banner17
i\
info: soapC-17.cpp
b range17

:banner18
i\
info: soapC-18.cpp
b range18

:banner19
i\
info: soapC-19.cpp
b range19

:banner20
i\
info: soapC-20.cpp
b range20


:done
d
:end

