/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  Copyright (c) 2021 cjiang. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  Zlib implementation based on /apple/xnu/libkern/c++/OSKext.cpp
 */

#ifndef OpenFirmwareManager_h
#define OpenFirmwareManager_h

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <libkern/OSKextLib.h>

typedef struct FirmwareDescriptor
{
    char * name;
    UInt8 * firmwareData;
    UInt32 firmwareSize;
} FirmwareDescriptor;

class OpenFirmwareManager : public IOService
{
    OSDeclareDefaultStructors(OpenFirmwareManager)
    
    struct ResourceCallbackContext
    {
        OpenFirmwareManager * me;
        OSData * firmware;
    };
    
public:
    /*! @function withName
     *   @abstract Creates an OpenFirmwareManager instance with the name of the firmware in that is requested.
     *   @discussion After creating the instance, the function calls setFirmwareWithName to set the firmware.
     *   @param name The name of the requested firmware.
     *   @param firmwareList A list that consists of all possible firmware candidates.
     *   @param numFirmwares The number of firmwares in firmwareList.
     *   @result If the operation is successful, the instance created is returned. */
    
    static OpenFirmwareManager * withName(char * name, FirmwareDescriptor * firmwareList, int numFirmwares);
    
    virtual IOReturn setFirmwareWithName(char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares);
    
    /*! @function withDescriptor
     *   @abstract Creates an OpenFirmwareManager instance with a firmware descriptor.
     *   @discussion After creating the instance, the function calls setFirmwareWithDescriptor to set the firmware.
     *   @param firmware The firmware descriptor upon which the instance will be generated.
     *   @result If the operation is successful, the instance created is returned. */
    
    static OpenFirmwareManager * withDescriptor(FirmwareDescriptor firmware);

    virtual IOReturn setFirmwareWithDescriptor(FirmwareDescriptor firmware);
    
    virtual bool init( OSDictionary * dictionary = NULL ) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn removeFirmware();
    
    virtual OSData * getFirmwareUncompressed();
    virtual char * getFirmwareName();
    
protected:
    virtual int decompressFirmware(OSData * firmware);
    virtual bool isFirmwareCompressed(OSData * firmware);
    
protected:
    char * mFirmwareName;
    IOLock * mUncompressedFirmwareLock;
    OSData * mUncompressedFirmwareData;
    IOLock * mCompletionLock;
};

#endif
