#ifndef VXMEMORYMAPPEDFILE_H
#define VXMEMORYMAPPEDFILE_H

#include "VxMathDefines.h"

/***********************************************************************
Summary: Possible return value when opening a memory mapped file.

See Also: VxMemoryMappedFile::GetErrorType
***********************************************************************/
enum VxMMF_Error
{
    VxMMF_NoError,	   // No error
    VxMMF_FileOpen,	   // Cannot open file
    VxMMF_FileMapping, // Cannot create file mapping
    VxMMF_MapView	   // Cannot get a pointer to start of map
};

/***********************************************************************
Summary: Utility class for memory mapped file reading.

Remarks:
    The VxMemoryMappedFile can be used have a mapping of a file into a memory
    buffer for reading purposes.
Example:
        VxMemoryMappedFile mappedFile(FileName);

        DWORD FileSize = mappedFile.GetFileSize();
        BYTE* buffer = (BYTE*)mappedFile.GetBase();

      // buffer now contain the file content and can be read

See also:
***********************************************************************/
class VxMemoryMappedFile
{
public:
    VX_EXPORT VxMemoryMappedFile(char *pszFileName);

    VX_EXPORT ~VxMemoryMappedFile();

    /***********************************************************************
    Summary: Returns a pointer to the mapped memory buffer.
    Remarks: The returned pointer should not be deleted nor should it be
    used for writing purpose.
    ***********************************************************************/
    VX_EXPORT void *GetBase();

    /***********************************************************************
    Summary: Returns the file size in bytes.
    ***********************************************************************/
    VX_EXPORT XULONG GetFileSize();

    /***********************************************************************
    Summary: Returns the file was successfully opened and mapped to a memory buffer.
    ***********************************************************************/
    VX_EXPORT XBOOL IsValid();

    /***********************************************************************
    Summary: Returns whether there was an error opening the file.
    ***********************************************************************/
    VX_EXPORT VxMMF_Error GetErrorType();

private:
    GENERIC_HANDLE m_hFile;
    GENERIC_HANDLE m_hFileMapping; // Handle of memory mapped file
    void *m_pMemoryMappedFileBase;
    XULONG m_cbFile;
    VxMMF_Error m_errCode;
};

#endif // VXMEMORYMAPPEDFILE_H
