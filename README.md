# OpenFirmwareManager

## Intro

For a device to work properly, its firmware must be loaded by the driver. However, firmwares are often tremendous and take up too much space, so developers would usually compress them. Therefore, decompression needs to be done in the kext. OpenFirmwareManager is a generic macOS kernel extension that provides unified APIs for firmware decompression and management -- with this extension, developers don't have to implement those firmware opertaions anymore -- they just need an OpenFirmwareManager instance that does everything for them.

## Documentaion

Please refer to the headers for specific documentations of functions or other details, which are written as comments in AppleDoc style. The headers could also be found in the Resources folder of the Debug release.

## Installation

1. Download the kext from the releases section of this GitHub repository.
2. Unzip.
3. Install the kext to your system.
4. Reboot.

## Usage

To use the APIs in another project, follow these steps:
1. Download a Debug version of this kext.
2. Copy the kext to your project directory.
3. Include $(PROJECT_DIR)/OpenFirmwareManager.kext/Contents/Resources/ to your header search paths.
4. Use OpenFirmwareManager instances to manage firmwares!
