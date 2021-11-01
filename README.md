# OpenFirmwareManager

A generic macOS kernel extension that handles firmware decompression.

## Introduction

For a device to work properly, its firmware must be loaded by the driver. However, firmwares are often tremendous and take up too much space, so it is very necessary to compress them. This kext aims at providing a generic API that could decompress firmware data in an orderly fashion so that developers would not need to implement it in every kext. This is accomplished by the OpenFirmwareManager instance that stores the decompressed data and other info from the data provided by the user.

#### *Note: This kext only handles firmware decompression. It does not store the firmware. See the Installation section for more info.*

## Documentaion

Please refer to the header for specific documentations of functions and other details. They are written as comments in standard AppleDoc style. The header could also be found in the Resources folder of the Debug release.

## Installation

1. Download the kext from the releases section of the GitHub repository.
2. Unzip.
3. Copy the kext to System/Library/Extensions or to the Kexts folder if you use a bootloader.
4. Reboot.

## Usage

1. Download a Debug version of this kext.
2. Include .../Contents/Resources/OpenCoreFirmwareManager.h to your header search paths in your project.
3. Run the /scripts/gen_fw.sh included in this repository to generate compressed firmware binaries from your fw files.
4. Use OpenFirmwareManager instances to manage firmwares.
