#include "winres.h"

#define VER_FILEVERSION     $VERSION
#define VER_FILEVERSION_STR "$VERSION_STR\0"

VS_VERSION_INFO VERSIONINFO
FILEVERSION VER_FILEVERSION
PRODUCTVERSION VER_FILEVERSION
FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
#ifdef _DEBUG
FILEFLAGS VS_FF_DEBUG
#else
FILEFLAGS 0x0L
#endif
FILEOS VOS__WINDOWS32
FILETYPE VFT_APP
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904e4"
        BEGIN
            VALUE "CompanyName", "Google LLC\0"
            VALUE "FileDescription", "Nearby Connections\0"
            VALUE "FileVersion", VER_FILEVERSION_STR
            VALUE "LegalCopyright", "Copyright (C) 2022 Google. All rights reserved.\0"
            VALUE "ProductName", "Nearby Connections\0"
            VALUE "ProductVersion", VER_FILEVERSION_STR
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1252
    END
END