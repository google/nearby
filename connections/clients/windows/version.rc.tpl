#include "winres.h"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION $VS_VERSION
 PRODUCTVERSION $VS_VERSION
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
            VALUE "CompanyName", "Google LLC"
            VALUE "FileDescription", "Nearby Connections"
            VALUE "FileVersion", "$VERSION"
            VALUE "LegalCopyright", "Copyright (C) 2022 Google. All rights reserved." "\0"
            VALUE "ProductName", "Nearby Connections"
            VALUE "ProductVersion", "$VERSION"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1252
    END
END