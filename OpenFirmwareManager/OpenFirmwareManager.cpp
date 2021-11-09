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
    if ( !super::init() )
        return false;

    mFirmwareLock = IOLockAlloc();
    mFirmwares = NULL;
    mExpansionData = IONew(ExpansionData, 1);
    if ( !mExpansionData )
        return false;
    mExpansionData->mCompletionLock = IOLockAlloc();
    return true;
}

void OpenFirmwareManager::free()
{
    removeFirmwares();
    IOLockFree(mFirmwareLock);
    IOLockFree(mExpansionData->mCompletionLock);
    IOSafeDeleteNULL(mExpansionData, ExpansionData, 1);
    super::free();
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
    OSData * uncompressedFirmware = NULL;
    z_stream zstream;
    int zlib_result;
    void * buffer = NULL;
    UInt32 bufferSize = 0;
    
    if ( !isFirmwareCompressed(firmware) )
    {
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
        IOFree(buffer, bufferSize);
        return NULL;
    }
    
    zlib_result = inflate(&zstream, Z_FINISH);
    if ( zlib_result == Z_STREAM_END || zlib_result == Z_OK )
        uncompressedFirmware = OSData::withBytes(buffer, (unsigned int) zstream.total_out);
    
    inflateEnd(&zstream);
    IOFree(buffer, bufferSize);

    return uncompressedFirmware;
}

void OpenFirmwareManager::requestResourceCallback(OSKextRequestTag requestTag, OSReturn result, const void * resourceData, uint32_t resourceDataLength, void * context1)
{
    ResourceCallbackContext * context = (ResourceCallbackContext *) context1;

    IOLockLock(context->me->mExpansionData->mCompletionLock);

    if (kOSReturnSuccess == result)
    {
        kprintf("[OpenFirmwareManager][requestResourceCallback] %d bytes of data.\n", resourceDataLength);
        context->descriptor.firmwareData = (UInt8 *) resourceData;
        context->descriptor.firmwareSize = resourceDataLength;
    }
    else
        kprintf("[OpenFirmwareManager][requestResourceCallback] Retrieved error: %08x\n", result);

    IOLockUnlock(context->me->mExpansionData->mCompletionLock);

    // wake waiting task in performUpgrade (in IOLockSleep)...
    IOLockWakeup(context->me->mExpansionData->mCompletionLock, context->me, true);
}

IOReturn OpenFirmwareManager::addFirmwareWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    while ( numFirmwares > 0 )
        if ( firmwareCandidates[--numFirmwares].name == name )
            return addFirmwareWithDescriptor(firmwareCandidates[numFirmwares]);

    return kIOReturnUnsupported;
}

IOReturn OpenFirmwareManager::addFirmwareWithDescriptor(FirmwareDescriptor firmware)
{
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
    return err;
}

IOReturn OpenFirmwareManager::addFirmwareWithFile(const char * kextIdentifier, const char * fileName)
{
    IOLockLock(mExpansionData->mCompletionLock);

    ResourceCallbackContext context = { .me = this };

    OSReturn ret = OSKextRequestResource(kextIdentifier, fileName, requestResourceCallback, &context, NULL);
    kprintf("[OpenFirmwareManager][addFirmwareWithFile] OSKextRequestResource: %08x\n", ret);

    // wait for completion of the async read
    IOLockSleep(mExpansionData->mCompletionLock, this, 0);
    IOLockUnlock(mExpansionData->mCompletionLock);

    if ( !context.descriptor.firmwareData || context.descriptor.firmwareSize <= 0 )
        return ret;

    kprintf("[OpenFirmwareManager][addFirmwareWithFile] Obtained firmware \"%s\" from resources.\n", fileName);
    context.descriptor.name = fileName;

    return addFirmwareWithDescriptor(context.descriptor);
}

IOReturn OpenFirmwareManager::removeFirmware(const char * name)
{
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
    if ( !init() || capacity <= 0 )
        return false;

    IOLockLock(mFirmwareLock);
    mFirmwares = OSDictionary::withCapacity(capacity);
    if ( !mFirmwares )
    {
        IOLockUnlock(mFirmwareLock);
        return false;
    }
    IOLockUnlock(mFirmwareLock);
    return true;
}

bool OpenFirmwareManager::initWithNames(const char ** names, int capacity, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    if ( !initWithCapacity(capacity) )
        return false;

    while ( capacity > 0 )
        addFirmwareWithName(names[--capacity], firmwareCandidates, numFirmwares);

    return true;
}

bool OpenFirmwareManager::initWithName(const char * name, FirmwareDescriptor * firmwareCandidates, int numFirmwares)
{
    if ( !initWithCapacity(1) )
        return false;

    return !addFirmwareWithName(name, firmwareCandidates, numFirmwares);
}

bool OpenFirmwareManager::initWithDescriptors(FirmwareDescriptor * firmwares, int capacity)
{
    if ( !initWithCapacity(capacity) )
        return false;

    while ( capacity > 0 )
        addFirmwareWithDescriptor(firmwares[--capacity]); // no need to fail if a firmware is not added

    return true;
}

bool OpenFirmwareManager::initWithDescriptor(FirmwareDescriptor firmware)
{
    if ( !initWithCapacity(1) )
        return false;

    return !addFirmwareWithDescriptor(firmware);
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

    return true;
}

bool OpenFirmwareManager::initWithFile(const char * kextIdentifier, const char * fileName)
{
    if ( !initWithCapacity(1) )
        return false;

    return !addFirmwareWithFile(kextIdentifier, fileName);
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
