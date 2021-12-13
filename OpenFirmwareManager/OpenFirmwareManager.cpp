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

#include "Logs.h"
#include "OpenFirmwareManager.h"
#include "zutil.h"

#define super IOService
OSDefineMetaClassAndStructors(OpenFirmwareManager, super)

bool OpenFirmwareManager::init(OSDictionary * dictionary)
{
	DebugLog("init", "Initializing variables...");
    if ( !super::init() )
        return false;

    mFirmwareLock = IOLockAlloc();
    mFirmwares = NULL;
    mExpansionData = IONew(ExpansionData, 1);
    if ( !mExpansionData )
	{
		AlwaysLog("init", "init() failed -- no memory.");
		return false;
	}
    mExpansionData->mCompletionLock = IOLockAlloc();
	DebugLog("init", "init() completed.");
    return true;
}

void OpenFirmwareManager::free()
{
	DebugLog("free", "Releasing variables...");
    removeFirmwares();
    IOLockFree(mFirmwareLock);
    IOLockFree(mExpansionData->mCompletionLock);
    IOSafeDeleteNULL(mExpansionData, ExpansionData, 1);
    super::free();
	DebugLog("free", "free() completed.");
}

bool OpenFirmwareManager::isFirmwareCompressed(OSData * firmware)
{
    UInt16 * magic = (UInt16 *) firmware->getBytesNoCopy();

    if ( *magic == 0x0178   // Zlib no compression
      || *magic == 0x9c78   // Zlib default compression
      || *magic == 0xda78 ) // Zlib maximum compression
        return true;
    return false;
}

OSData * OpenFirmwareManager::decompressFirmware(OSData * firmware)
{
	DebugLog("decompressFirmware", "Uncompressing firmware %p...", firmware);
    OSData * uncompressedFirmware = NULL;
    z_stream zstream;
    int zlib_result;
    void * buffer = NULL;
    UInt32 bufferSize = 0;
    
    if ( !isFirmwareCompressed(firmware) )
    {
		DebugLog("decompressFirmware", "Firmware is not compressed!");
        firmware->retain();
        return firmware;
    }
    
    bufferSize = firmware->getLength() * 4;
    buffer = IOMalloc(bufferSize);
    
    bzero(&zstream, sizeof(zstream));
    zstream.next_in   = (UInt8 *) firmware->getBytesNoCopy();
    zstream.avail_in  = firmware->getLength();
    zstream.next_out  = (UInt8 *) buffer;
    zstream.avail_out = bufferSize;
    zstream.zalloc    = zalloc;
    zstream.zfree     = zfree;
    
    zlib_result = inflateInit(&zstream);
    if ( zlib_result != Z_OK )
    {
		DebugLog("decompressFirmware", "inflateInit() failed: %d", zlib_result);
        IOFree(buffer, bufferSize);
        return NULL;
    }
    
    zlib_result = inflate(&zstream, Z_FINISH);
    if ( zlib_result == Z_STREAM_END || zlib_result == Z_OK )
        uncompressedFirmware = OSData::withBytes(buffer, (unsigned int) zstream.total_out);
    
    inflateEnd(&zstream);
    IOFree(buffer, bufferSize);

	DebugLog("decompressFirmware", "Firmware decompressed successfully.");

    return uncompressedFirmware;
}

void OpenFirmwareManager::requestResourceCallback(OSKextRequestTag requestTag, OSReturn result, const void * resourceData, uint32_t resourceDataLength, void * context1)
{
    ResourceCallbackContext * context = (ResourceCallbackContext *) context1;

    IOLockLock(context->me->mExpansionData->mCompletionLock);

    if (kOSReturnSuccess == result)
    {
        DebugLog("requestResourceCallback", "%d bytes of data.", resourceDataLength);
        context->descriptor.firmwareData = (UInt8 *) resourceData;
        context->descriptor.firmwareSize = resourceDataLength;
    }
    else
        DebugLog("requestResourceCallback", "Retrieved error: %08x", result);

    IOLockUnlock(context->me->mExpansionData->mCompletionLock);

    // wake waiting task in performUpgrade (in IOLockSleep)...
    IOLockWakeup(context->me->mExpansionData->mCompletionLock, context->me, true);
}

IOReturn OpenFirmwareManager::addFirmwareWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
	DebugLog("addFirmwareWithName", "name: %s -- firmwareCandidates: %p -- numFirmwares: %d", name, firmwareCandidates, numFirmwares);
    while ( numFirmwares > 0 )
	{
		--numFirmwares;
		DebugLog("addFirmwareWithName", "candidate name: %s, name: %s", firmwareCandidates[numFirmwares].name, name);
        if ( !strncmp(firmwareCandidates[numFirmwares].name, name, 64) )
			return addFirmwareWithDescriptor(firmwareCandidates[numFirmwares]);
	}

	AlwaysLog("addFirmwareWithName", "can't find the firmware with name!");
    return kIOReturnUnsupported;
}

IOReturn OpenFirmwareManager::addFirmwareWithDescriptor(FirmwareDescriptor firmware)
{
	DebugLog("addFirmwareWithDescriptor", "name: %s -- firmwareData: %p -- firmwareSize: %d", firmware.name, firmware.firmwareData, firmware.firmwareSize);
    IOReturn err = kIOReturnSuccess;

    IOLockLock(mFirmwareLock);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return kIOReturnInvalid;
    }
    IOLockUnlock(mFirmwareLock);

    OSData * uncompressedFirmware;
    OSData * fwData = OSData::withBytes(firmware.firmwareData, firmware.firmwareSize);
    if ( !fwData )
        return kIOReturnInvalid;

    if ( isFirmwareCompressed(fwData) )
    {
        uncompressedFirmware = decompressFirmware(fwData);
        OSSafeReleaseNULL(fwData);
        if ( !uncompressedFirmware )
            return kIOReturnError;
        goto SET_FIRMWARE;
    }
    uncompressedFirmware = fwData;

SET_FIRMWARE:
    IOLockLock(mFirmwareLock);
    if ( !mFirmwares->setObject(firmware.name, uncompressedFirmware) )
        err = kIOReturnError;

    OSSafeReleaseNULL(uncompressedFirmware);

OVER:
    IOLockUnlock(mFirmwareLock);
	DebugLog("addFirmwareWithDescriptor", "Firmware is added successfully!");
    return err;
}

IOReturn OpenFirmwareManager::addFirmwareWithFile(const char * kextIdentifier, const char * fileName)
{
	DebugLog("addFirmwareWithDescriptor", "identifier: %s -- file name: %s", kextIdentifier, fileName);
    IOLockLock(mExpansionData->mCompletionLock);

    ResourceCallbackContext context = { .me = this };

    OSReturn ret = OSKextRequestResource(kextIdentifier, fileName, requestResourceCallback, &context, NULL);
    DebugLog("addFirmwareWithFile", "OSKextRequestResource: %08x", ret);

    // wait for completion of the async read
    IOLockSleep(mExpansionData->mCompletionLock, this, 0);
    IOLockUnlock(mExpansionData->mCompletionLock);

    if ( !context.descriptor.firmwareData || context.descriptor.firmwareSize <= 0 )
        return ret;

    DebugLog("addFirmwareWithFile", "Obtained firmware \"%s\" from resources.", fileName);
    context.descriptor.name = fileName;

    return addFirmwareWithDescriptor(context.descriptor);
}

IOReturn OpenFirmwareManager::removeFirmware(const char * name)
{
	DebugLog("removeFirmware", "Removing firmware with the name %s", name);
    IOLockLock(mFirmwareLock);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return kIOReturnInvalid;
    }
    mFirmwares->removeObject(name);
    IOLockUnlock(mFirmwareLock);

    return kIOReturnSuccess;
}

IOReturn OpenFirmwareManager::removeFirmwares()
{
	DebugLog("removeFirmwares", "Removing all firmwares...");
    IOLockLock(mFirmwareLock);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return kIOReturnInvalid;
    }
    mFirmwares->flushCollection();
    IOLockUnlock(mFirmwareLock);
    return kIOReturnSuccess;
}

OSData * OpenFirmwareManager::getFirmwareUncompressed(const char * name)
{
    OSData * fwData;

    IOLockLock(mFirmwareLock);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return NULL;
    }
    fwData = OSDynamicCast(OSData, mFirmwares->getObject(name));
    IOLockUnlock(mFirmwareLock);
    return fwData;
}

bool OpenFirmwareManager::initWithCapacity(int capacity)
{
	DebugLog("initWithCapacity", "capacity: %d", capacity);
    if ( !init() || capacity <= 0 )
        return false;

	DebugLog("initWithCapacity", "init() succeeded!");
    IOLockLock(mFirmwareLock);
    mFirmwares = OSDictionary::withCapacity(capacity);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return false;
    }
    IOLockUnlock(mFirmwareLock);
	DebugLog("initWithCapacity", "initialized successfully!");
    return true;
}

bool OpenFirmwareManager::initWithNames(const char ** names, int capacity, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    if ( !initWithCapacity(capacity) )
        return false;

    while ( capacity > 0 )
        addFirmwareWithName(names[--capacity], firmwareCandidates, numFirmwares);

	DebugLog("initWithNames", "initialized successfully!");
    return true;
}

bool OpenFirmwareManager::initWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    if ( !initWithCapacity(1) )
        return false;

	if ( !addFirmwareWithName(name, firmwareCandidates, numFirmwares) )
	{
		DebugLog("initWithName", "initialized successfully!");
		return true;
	}
	DebugLog("initWithName", "initialization failed!");
	return false;
}

bool OpenFirmwareManager::initWithDescriptors(FirmwareDescriptor * firmwares, int capacity)
{
    if ( !initWithCapacity(capacity) )
        return false;

    while ( capacity > 0 )
        addFirmwareWithDescriptor(firmwares[--capacity]); // no need to fail if a firmware is not added

	DebugLog("initWithDescriptors", "initialized successfully!");
    return true;
}

bool OpenFirmwareManager::initWithDescriptor(FirmwareDescriptor firmware)
{
    if ( !initWithCapacity(1) )
        return false;

	if ( !addFirmwareWithDescriptor(firmware) )
	{
		DebugLog("initWithDescriptor", "initialized successfully!");
		return true;
	}
	DebugLog("initWithDescriptor", "initialization failed!");
	return false;
}

bool OpenFirmwareManager::initWithFiles(const char ** kextIdentifiers, const char ** fileNames, int capacity)
{
    if ( !initWithCapacity(capacity) )
        return false;

    while ( capacity > 0 )
    {
        --capacity;
        addFirmwareWithFile(kextIdentifiers[capacity], fileNames[capacity]);
    }

	DebugLog("initWithFiles", "initialized successfully!");
    return true;
}

bool OpenFirmwareManager::initWithFile(const char * kextIdentifier, const char * fileName)
{
    if ( !initWithCapacity(1) )
        return false;

	if ( !addFirmwareWithFile(kextIdentifier, fileName) )
	{
		DebugLog("initWithFile", "initialized successfully!");
		return true;
	}
	DebugLog("initWithFile", "initialization failed!");
	return false;
}

OpenFirmwareManager * OpenFirmwareManager::withCapacity(int capacity)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);

    if ( !me )
        return NULL;
    if ( !me->initWithCapacity(capacity) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withNames(const char ** names, int capacity, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);
    
    if ( !me )
        return NULL;
    if ( !me->initWithNames(names, capacity, firmwareCandidates, numFirmwares) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
	OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);

    if ( !me )
        return NULL;
    if ( !me->initWithName(name, firmwareCandidates, numFirmwares) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withDescriptors(FirmwareDescriptor * firmwares, int capacity)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);
    
    if ( !me )
        return NULL;
    if ( !me->initWithDescriptors(firmwares, capacity) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withDescriptor(FirmwareDescriptor firmware)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);

    if ( !me )
        return NULL;
    if ( !me->initWithDescriptor(firmware) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withFiles(const char ** kextIdentifiers, const char ** fileNames, int capacity)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);

    if ( !me )
        return NULL;
    if ( !me->initWithFiles(kextIdentifiers, fileNames, capacity) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withFile(const char * kextIdentifier, const char * fileName)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);

    if ( !me )
        return NULL;
    if ( !me->initWithFile(kextIdentifier, fileName) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}
