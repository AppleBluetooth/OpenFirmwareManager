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

#ifndef _OFM_OPENFIRMWAREMANAGER_H
#define _OFM_OPENFIRMWAREMANAGER_H

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <libkern/OSKextLib.h>

typedef struct FirmwareDescriptor
{
    const char * name;
    UInt8 * firmwareData;
    UInt32 firmwareSize;
} FirmwareDescriptor;

class OpenFirmwareManager : public IOService
{
    OSDeclareDefaultStructors(OpenFirmwareManager)
    
    struct ResourceCallbackContext
    {
        OpenFirmwareManager * me;
        FirmwareDescriptor descriptor;
    };
    
public:
    static OpenFirmwareManager * withCapacity(int capacity);

    /*! @function withNames
     *   @abstract Creates an OpenFirmwareManager instance with the names of firmwares requested.
     *   @discussion After creating the instance, the function calls initWitNames to initialize the instance.
     *   @param names The names of the requested firmwares.
     *   @param capacity The number of firmwares requested.
     *   @param firmwareCandidates A list that consists of all possible firmware candidates.
     *   @param numFirmwares The number of firmwares in firmwareList.
     *   @result If the operation is successful, the instance created is returned. */
    
    static OpenFirmwareManager * withNames(const char ** names, int capacity, FirmwareDescriptor * firmwareCandidates, int numFirmwares);
    static OpenFirmwareManager * withName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares);

    /*! @function withDescriptors
     *   @abstract Creates an OpenFirmwareManager instance with firmware descriptors.
     *   @discussion After creating the instance, the function calls initWithFirmwareWithDescriptors to initialize the instance.
     *   @param firmwares The firmware descriptors upon which the instance is generated.
     *   @param capacity The number of firmwares requested.
     *   @result If the operation is successful, the instance created is returned. */
    
    static OpenFirmwareManager * withDescriptors(FirmwareDescriptor * firmwares, int capacity);
    static OpenFirmwareManager * withDescriptor(FirmwareDescriptor firmware);

    static OpenFirmwareManager * withFiles(const char ** kextIdentifiers, const char ** fileNames, int capacity);
    static OpenFirmwareManager * withFile(const char * kextIdentifier, const char * fileName);

    virtual IOReturn addFirmwareWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares);
    virtual IOReturn addFirmwareWithDescriptor(FirmwareDescriptor firmware);
    virtual IOReturn addFirmwareWithFile(const char * kextIdentifier, const char * fileName);

    virtual IOReturn removeFirmware(const char * name);
    virtual IOReturn removeFirmwares();

    virtual bool init( OSDictionary * dictionary = NULL ) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual OSData * getFirmwareUncompressed(const char * name);
    
protected:
    static void requestResourceCallback(OSKextRequestTag requestTag, OSReturn result, const void * resourceData, uint32_t resourceDataLength, void * context);

    virtual bool initWithCapacity(int capacity);
    virtual bool initWithNames(const char ** names, int capacity, FirmwareDescriptor * firmwareCandidates, int numFirmwares);
    virtual bool initWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares);
    virtual bool initWithDescriptors(FirmwareDescriptor * firmwares, int capacity);
    virtual bool initWithDescriptor(FirmwareDescriptor firmware);
    virtual bool initWithFiles(const char ** kextIdentifiers, const char ** fileNames, int capacity);
    virtual bool initWithFile(const char * kextIdentifier, const char * fileName);
    virtual bool isFirmwareCompressed(OSData * firmware);
    virtual OSData * decompressFirmware(OSData * firmware);
    
protected:
    IOLock * mFirmwareLock;
    OSDictionary * mFirmwares;

    struct ExpansionData
    {
        IOLock * mCompletionLock;
    };
    ExpansionData * mExpansionData;
};

#endif
