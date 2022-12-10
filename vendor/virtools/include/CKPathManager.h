#ifndef CKPATHMANAGER_H
#define CKPATHMANAGER_H

#include "CKBaseManager.h"
#include "XClassArray.h"
#include "XString.h"

/****************************************************
{filename:CK_PATHMANAGER_CATEGORY}
Summary: Enumeration of pre-registered path categories

Remarks
    + The path manager pre-registers 3 categories of path (sound,bitmap and misc. data).
See Also: CKPathManager
*************************************************/
typedef enum CK_PATHMANAGER_CATEGORY
{
    BITMAP_PATH_IDX = 0, // Category index for bitmaps paths
    DATA_PATH_IDX = 1,	 // Category index for data paths
    SOUND_PATH_IDX = 2	 // Category index for sounds paths
} CK_PATHMANAGER_CATEGORY;

typedef XClassArray<XString> CKPATHENTRYVECTOR;

typedef struct CKPATHCATEGORY
{
    XString m_Name;
    CKPATHENTRYVECTOR m_Entries;
} CKPATHCATEGORY;

typedef XClassArray<CKPATHCATEGORY> CKPATHCATEGORYVECTOR;

/*************************************************
Summary: Files paths management
Remarks:

+ The path manager holds a set of paths that everybody can use to
retrieve a file.

+ These paths are put into relevant categories (bitmap,sound,data) but
new categories for application specific data can be defined.

+ The Path manager also provides some utility methods to work on paths.

+ At startup (creation of the CKContext) , the path manager always registers
three default category:

        "Bitmap Paths"	: Index 0
        "Data Paths"	: Index 1
        "Sound Paths"	: Index 2


See Also:CKContext::GetPathManager
****************************************************/
class CKPathManager : public CKBaseManager
{
public:
    CKPathManager(CKContext *Context);

    virtual ~CKPathManager();

    // Category Functions

    // Adds a category, category name must be unique
    int AddCategory(XString &cat);
    // Removes a category using its name or its index in category list
    CKERROR RemoveCategory(int catIdx);

    // Gets the number of categories
    int GetCategoryCount();

    // Gets the category name at specified index
    CKERROR GetCategoryName(int catIdx, XString &catName);

    // Gets the category Index in List
    int GetCategoryIndex(XString &cat);

    // Renames a category
    CKERROR RenameCategory(int catIdx, XString &newName);

    // Paths Functions

    // Adds a path to a category
    int AddPath(int catIdx, XString &path);
    // Removes a path in a category
    CKERROR RemovePath(int catIdx, int pathIdx);

    // Swap paths
    CKERROR SwapPaths(int catIdx, int pathIdx1, int pathIdx2);

    // Gets the path count for a category
    int GetPathCount(int catIdx);

    // Gets the path at index pathIdx for a category
    CKERROR GetPathName(int catIdx, int pathIdx, XString &path);

    // Gets the path at index pathIdx for a category
    int GetPathIndex(int catIdx, XString &path);

    // Changes a path in a category
    CKERROR RenamePath(int catIdx, int pathIdx, XString &path);

    //--- Finding a file

    // Resolve File Name in the given category
    CKERROR ResolveFileName(XString &file, int catIdx, int startIdx = -1);

    //--- Utilities

    // Path Type
    CKBOOL PathIsAbsolute(XString &file);
    CKBOOL PathIsUNC(XString &file);
    CKBOOL PathIsURL(XString &file);
    CKBOOL PathIsFile(XString &file);

    // Converts '%20' characters to ' '
    void RemoveEscapedSpace(char *str);

    // Converts '' characters to '%20'
    void AddEscapedSpace(XString &str);

    // Virtools temporary storage folder...
    XString GetVirtoolsTemporaryFolder();

protected:
    void Clean();

    // Do not use, use RemoveEscapedSpace !
    void RemoveSpace(char *str);

    CKBOOL TryOpenAbsolutePath(XString &file);
    CKBOOL TryOpenFilePath(XString &file);
    CKBOOL TryOpenURLPath(XString &file);

    CKPATHCATEGORYVECTOR m_Categories;
    XString m_TemporaryFolder;
    CKBOOL m_TemporaryFolderExist;
};

#endif // CKPATHMANAGER_H
