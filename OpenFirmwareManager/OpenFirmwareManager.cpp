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

#include "OpenFirmwareManager.h"

bool OpenFirmwareManager::init(OSDictionary * dictionary)
{
    mFirmwareName = NULL;
    mUncompressedFirmwareLock = IOLockAlloc();
    mUncompressedFirmwareData = NULL;
    return true;
}

void OpenFirmwareManager::free()
{
    mFirmwareName = NULL;
    IOLockFree(mUncompressedFirmwareLock);
    OSSafeReleaseNULL(mUncompressedFirmwareData);
}

int OpenFirmwareManager::decompressFirmware(OSData * firmware)
{
    z_stream zstream;
    int zlib_result;
    void * buffer = NULL;
    UInt32 bufferSize = 0;
    
    if ( !isFirmwareCompressed(firmware) )
    {
        firmware->retain();
        IOLockLock(mUncompressedFirmwareLock);
        mUncompressedFirmwareData = firmware;
        IOLockUnlock(mUncompressedFirmwareLock);
        return Z_OK;
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
    if (zlib_result != Z_OK)
    {
        IOFree(buffer, bufferSize);
        return zlib_result;
    }
    
    zlib_result = inflate(&zstream, Z_FINISH);
    if (zlib_result == Z_STREAM_END || zlib_result == Z_OK)
    {
        IOLockLock(mUncompressedFirmwareLock);
        mUncompressedFirmwareData = OSData::withBytes(buffer, (unsigned int) zstream.total_out);
        IOLockUnlock(mUncompressedFirmwareLock);
    }
    
    inflateEnd(&zstream);
    IOFree(buffer, bufferSize);
    
    return zlib_result;
}

IOReturn OpenFirmwareManager::setFirmwareWithName(char * name, FirmwareDescriptor ** firmwareCandidates, int numFirmwares)
{
    OSData * fwData;
    
    for (int i = 0; i < numFirmwares; ++i)
    {
        if (firmwareCandidates[i]->name == name)
        {
            mFirmwareName = name;
            fwData = OSData::withBytes(firmwareCandidates[i]->firmwareData, firmwareCandidates[i]->firmwareSize);
            if ( isFirmwareCompressed(fwData) )
            {
                if ( !decompressFirmware(fwData) )
                {
                    OSSafeReleaseNULL(fwData);
                    return kIOReturnSuccess;
                }
                OSSafeReleaseNULL(fwData);
                return kIOReturnError;
            }
            IOLockLock(mUncompressedFirmwareLock);
            mUncompressedFirmwareData = fwData;
            IOLockUnlock(mUncompressedFirmwareLock);
            return kIOReturnSuccess;
        }
    }
    return kIOReturnUnsupported;
}

IOReturn OpenFirmwareManager::removeFirmware()
{
    mFirmwareName = NULL;
    
    IOLockLock(mUncompressedFirmwareLock);
    OSSafeReleaseNULL(mUncompressedFirmwareData);
    IOLockUnlock(mUncompressedFirmwareLock);
    return kIOReturnSuccess;
}

OSData * OpenFirmwareManager::getFirmwareUncompressed()
{
    return mUncompressedFirmwareData;
}

char * OpenFirmwareManager::getFirmwareName()
{
    return mFirmwareName;
}

IOReturn OpenFirmwareManager::setFirmwareWithDescriptor(FirmwareDescriptor * firmware)
{
    OSData * fwData = OSData::withBytes(firmware->firmwareData, firmware->firmwareSize);
    mFirmwareName = firmware->name;
    
    if ( isFirmwareCompressed(fwData) )
    {
        if ( !decompressFirmware(fwData) )
        {
            OSSafeReleaseNULL(fwData);
            return kIOReturnSuccess;
        }
        OSSafeReleaseNULL(fwData);
        return kIOReturnError;
    }
    
    IOLockLock(mUncompressedFirmwareLock);
    mUncompressedFirmwareData = fwData;
    IOLockUnlock(mUncompressedFirmwareLock);
    return kIOReturnSuccess;
}

bool OpenFirmwareManager::isFirmwareCompressed(OSData * firmware)
{
    UInt16 * magic = (UInt16 *) firmware->getBytesNoCopy();
    
    if ( *magic != 0x0178   // Zlib no compression
      && *magic != 0x9c78   // Zlib default compression
      && *magic != 0xda78 ) // Zlib maximum compression
        return false;
    return true;
}

OpenFirmwareManager * OpenFirmwareManager::withName(char * name, FirmwareDescriptor ** firmwareList, int numFirmwares)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);
    
    if ( !me )
        return NULL;
    if ( me->setFirmwareWithName(name, firmwareList, numFirmwares) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

OpenFirmwareManager * OpenFirmwareManager::withDescriptor(FirmwareDescriptor * firmware)
{
    OpenFirmwareManager * me = OSTypeAlloc(OpenFirmwareManager);
    
    if ( !me )
        return NULL;
    if ( me->setFirmwareWithDescriptor(firmware) )
    {
        OSSafeReleaseNULL(me);
        return NULL;
    }
    return me;
}

void OpenFirmwareManager::requestResourceCallback(OSKextRequestTag requestTag, OSReturn result, const void * resourceData, uint32_t resourceDataLength, void* context1)
{
    ResourceCallbackContext *context = (ResourceCallbackContext*) context1;
    
    IOLockLock(context->me->mCompletionLock);
    if (!result)
        context->firmware = OSData::withBytes(resourceData, resourceDataLength);
    IOLockUnlock(context->me->mCompletionLock);
    
    // wake waiting task in performUpgrade (in IOLockSleep)...
    IOLockWakeup(context->me->mCompletionLock, context->me, true);
}
