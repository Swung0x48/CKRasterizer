#if !defined(CKSPRITE_H) || defined(CK_3DIMPLEMENTATION)
#define CKSPRITE_H
#ifndef CK_3DIMPLEMENTATION

#include "CK2dEntity.h"
#include "CKBitmapData.h"

/*************************************************
{filename:CKSprite}
Name: CKSprite

Summary: Sprite Class.
Remarks:
    + A sprite is special kind of 2D entity that supports displaying
    non power of 2 images (limitation of textures)

    + This class provides the basic methods for loading a sprite from a file, access
    the surface data,specify transparent color.

    + The class id of CKSprite is CKCID_SPRITE.



See also: CK2dEntity,CKSpriteText,CKBitmapData
*************************************************/
class CKSprite : public CK2dEntity
{
public:
#endif

    /*************************************************
    Summary: Creates an empty image in the sprite.

    Arguments:
        Width: width in pixel of the image to create
        Height: height in pixel of the image to create
        BPP: Bit per pixel of the image
        Slot: If there a multiple images, index of the image slot to create.
    Return Value:
        TRUE if successful, FALSE otherwise.
    Remarks:
    + The name for the created slot is set to ""
    + If there was already some slots created but with a different width or height
    the method returns FALSE.
    See Also:LoadImage,SaveImage
    *************************************************/
    virtual CKBOOL Create(int Width, int Height, int BPP = 32, int Slot = 0) = 0;

    /*************************************************
    Summary: Loads a image slot from a file.

    Arguments:
        Name: Name of the file to load.
        Slot: In a multi-images sprite, index of the image slot to load.
    Return Value:
        TRUE if successful, FALSE otherwise.
    Remarks:
        + The supported image formats depend on the image readers installed. The default available
        readers support BMP,TGA,JPG,PNG and GIF format
    See also: SaveImage
    *************************************************/
    virtual CKBOOL LoadImage(CKSTRING Name, int Slot = 0) = 0;

    /************************************************
    Summary: Saves an image slot to a file.

    Arguments:
        Name: Name of the file to save.
        Slot: In a multi-images sprite,index of the image slot to save.
        UseFormat: If set force usage of the save format specified in SetSaveFormat, otherwise use extension given in Name. Default : FALSE
    Return Value: TRUE if successful, FALSE otherwise.
    Remarks:
        + The image format depends on the image readers installed. The default available
        readers support BMP,TGA,JPG,PNG and GIF format

    See also: CreateImage,CKBitmapReader
    ************************************************/
    virtual CKBOOL SaveImage(CKSTRING Name, int Slot = 0, CKBOOL CKUseFormat = FALSE) = 0;

    //-------------------------------------------------------------
    // Movie Loading

    /************************************************
    Summary: Creates a multi-image sprite from a movie file.

    Arguments:
        Name: Movie file to load.
        width: width of the sprite to create.
        height:	height of the sprite to create.
        Bpp: Number of bit per pixel of the sprite to create.
    Return Value: TRUE if loading was successful, FALSE otherwise.
    Remarks:
        + The supported movie formats depend on the image readers and codecs installed. The default available
        reader support AVI files.
    See also: LoadImage,Create
    ************************************************/
    virtual CKBOOL LoadMovie(CKSTRING Name, int width = 0, int height = 0, int Bpp = 16) = 0;

    /*************************************************
    Summary: Returns the name of the movie file used by this sprite.

    Return Value:
        A pointer to the name of the file which was used to load this sprite or NULL if this sprite is not a movie.
    See also: LoadMovie,GetMovieReader
    *************************************************/
    virtual CKSTRING GetMovieFileName() = 0;

    /*************************************************
    Summary: Returns the movie reader used to decompress the current movie.
    Return value: Pointer to CKMovieReader
    Remarks:
        + This method returns a movie reader if one is present,NULL otherwise.

    See also: LoadMovie,GetMovieFileName,CKMovieReader
    *************************************************/
    virtual CKMovieReader *GetMovieReader() = 0;

    //-------------------------------------------------------------
    // SURFACE PTR ACCESS
    // Access to Surface Ptr ,
    // Once Locked a surface ptr must be release
    // for modification on the texture to be available
    // if the operation is read-only no release is required

    /*************************************************
    Summary: Returns a pointer to the image surface buffer.
    Arguments:
        Slot: In a multi-images texture, index of the image slot to get surface pointer of. -1 means the current active slot.
    Return Value:
         A valid pointer to the texture buffer or NULL if failed.
    Remarks:
    + The return value is a pointer on the system memory copy of the sprite.
    + If any changes are made (write access) to the image surface, you must either call Restore which immediately copies the sprite back in video memory or ReleaseSurfacePtr() which flags this sprite as to be reloaded before it is used next time.

    See also: ReleaseSurfacePtr,SetPixel,GetPixel
    *************************************************/
    virtual CKBYTE *LockSurfacePtr(int Slot = -1) = 0;

    /*************************************************
    Summary: Marks a slot as modified.
    Return Value:
        TRUE if successful.
    Arguments:
        Slot: In a multi-images sprite, number of the image slot to mark as invalid.
    Remarks:
    + When changes are made to the bitmap data (using LockSurfacePtr or SetPixel) this method marks the changed slot so that they can reloaded in video memory when necessary.

    See also: LockSurfacePtr,SetPixel
    *************************************************/
    virtual CKBOOL ReleaseSurfacePtr(int Slot = -1) = 0;

    //-------------------------------------------------------------
    // Bitmap filenames information

    /*************************************************
    Summary: Returns the name of the file used to load an image slot.
    Arguments:
        slot: image slot index
    Return value: A pointer to the name of the file which was used to load this image slot

    See also: SetSlotFileName
    *************************************************/
    virtual CKSTRING GetSlotFileName(int Slot) = 0;

    /*************************************************
    Summary: Sets the name of the file used to load an image slot.
    Arguments:
        Slot: image slot index
        Filename: image slot file name
    Return Value: TRUE if successful, FALSE otherwise.

    See also: GetSlotFileName
    *************************************************/
    virtual CKBOOL SetSlotFileName(int Slot, CKSTRING Filename) = 0;

    //--------------------------------------------------------------
    // Bitmap storage information

    /*************************************************
    Summary: Gets the image width
    Return value: Image width

    See Also: GetBytesPerLine,GetHeight,GetBitsPerPixel
    *************************************************/
    virtual int GetWidth() = 0;

    /*************************************************
    Summary: Gets the image height
    Return Value: Image height

    See Also: GetBytesPerLine,GetWidth,GetBitsPerPixel
    *************************************************/
    virtual int GetHeight() = 0;

    virtual int GetBitsPerPixel() = 0;

    virtual int GetBytesPerLine() = 0;

    virtual int GetRedMask() = 0;

    virtual int GetGreenMask() = 0;

    virtual int GetBlueMask() = 0;

    virtual int GetAlphaMask() = 0;

    //-------------------------------------------------------------
    // Image slot count

    /************************************************
    Summary: Returns the number of slot (images) in this sprite.
    Return Value: Number of images.
    Remarks:

    See also: SetSlotCount,GetCurrentSlot,SetCurrentSlot
    ************************************************/
    virtual int GetSlotCount() = 0;

    /************************************************
    Summary: Sets the number of slot (images) in this sprite.
    Arguments:
        Count: Image slots  to allocate.
    Return Value: TRUE if successful, FALSE otherwise.
    Remarks:

    See also: GetSlotCount,GetCurrentSlot,SetCurrentSlot
    ************************************************/
    virtual CKBOOL SetSlotCount(int Count) = 0;

    /************************************************
    Summary: Sets the current active image.

    Arguments:
        Slot: Image slot index.
    Return Value:
        TRUE if successful, FALSE otherwise.
    See also: GetSlotCount,SetSlotCount,GetCurrentSlot
    *****************************************************/
    virtual CKBOOL SetCurrentSlot(int Slot) = 0;

    /************************************************
    Summary: Returns current slot index.

    Return Value: Current image slot index.
    See Also:GetSlotCount,SetSlotCount,SetCurrentSlot
    ************************************************/
    virtual int GetCurrentSlot() = 0;

    /************************************************
    Summary: Removes an image.

    Return Value:
        TRUE if successful, FALSE otherwise.
    Arguments:
        Slot: Index of the image to remove.
    See also: GetSlotCount,GetCurrentSlot,SetCurrentSlot
    ************************************************/
    virtual CKBOOL ReleaseSlot(int Slot) = 0;

    /************************************************
    Summary: Deletes all the images.

    Return Value:
        TRUE if successful, FALSE otherwise.
    See also: GetSlotCount,GetCurrentSlot,SetCurrentSlot
    ************************************************/
    virtual CKBOOL ReleaseAllSlots() = 0;

    //-------------------------------------------------------------
    // ACCESS TO SYSTEM MEMORY SURFACE

    /*************************************************
    Summary: Sets the color of a pixel.
    Return Value:
        TRUE if successful, FALSE otherwise.
    Arguments:
        x: X position of the pixel to set the color of.
        y: Y position of the pixel to set the color of.
        col: A Dword ARGB color to set
        slot: Index of the slot in which the pixel should be set or -1 for the current slot.
    Remarks:
    + There is no check on the validity of x or y parameter so its the user responsibility.
    + Sets the color of a pixel in the copy of the texture in system memory.
    + Changes will only be visible after using Restore() function to force the sprite to re-load from system memory.

    See Also:LockSurfacePtr,GetPixel,ReleaseSurfacePtr
    *************************************************/
    virtual CKBOOL SetPixel(int x, int y, CKDWORD col, int slot = -1) = 0;

    /*************************************************
    Summary: Gets the color of a pixel.
    Arguments:
        x: X position of the pixel to set the color of.
        y: Y position of the pixel to set the color of.
        slot: Index of the slot in which the pixel should be get or -1 for the current slot.
    Return Value: Color of the pixel (32 bit ARGB)
    Remarks:
        + There is no check on the validity of x or y parameter so its the
        user responsibility.

    See Also:LockSurfacePtr,SetPixel
    *************************************************/
    virtual CKDWORD GetPixel(int x, int y, int slot = -1) = 0;

    //-------------------------------------------------------------
    // TRANSPARENCY

    /************************************************
    Summary: Returns the transparent color.
    Return Value:
        Color: A 32 bit ARGB transparent color.
    Remarks:
        + 0x00000000 (black) is the default transparent color.

    See also: SetTranparentColor,SetTransparent
    ************************************************/
    virtual CKDWORD GetTransparentColor() = 0;

    /************************************************
    Summary: Sets the transparent color.
    Arguments:
        Color: A 32 bit ARGB transparent color.
    Remarks:
    + 0x00000000 (black) is the default transparent color.
    + Setting on the transparency and a transparent color automatically
    updates the alpha channel so that pixel with the transparent color have
    a 0 alpha value.

    See also: GetTranparentColor,SetTransparent
    ************************************************/
    virtual void SetTransparentColor(CKDWORD Color) = 0;

    /************************************************
    Summary: Enables or disables the color key transparency.
    Arguments:
        Transparency: TRUE activates transparency, FALSE disables it.
    Remarks:
    + 0x00000000 (black) is the default transparent color.
    + Setting on the transparency and a transparent color automatically
    updates the alpha channel so that pixel with the transparent color have
    a 0 alpha value.

    See also: IsTransparent,SetTranparentColor
    ************************************************/
    virtual void SetTransparent(CKBOOL Transparency) = 0;

    /************************************************
    Summary: Returns whether color keyed transparency is enabled.
    Return Value:
        TRUE if transparency is enabled.
    Arguments:
        Transparency: TRUE activates transparency, FALSE disables it.
    Remarks:

    See also: IsTransparent
    ************************************************/
    virtual CKBOOL IsTransparent() = 0;

    //-------------------------------------------------------------
    // VIDEO MEMORY MANAGEMENT

    /*************************************************
    Summary: Restore sprite video memory

    Return Value:
        TRUE if successful, FALSE otherwise.
    Remarks:
        + If the sprite is already in video memory, this function copies
        the content of the system memory data to video memory otherwise it fails.
    See also: SystemToVideoMemory
    ************************************************/
    virtual CKBOOL Restore(CKBOOL Clamp = FALSE) = 0;

    /*************************************************
    Summary: Allocates and copies the sprite image from system to video memory.

    Arguments:
        Dev: A pointer to a CKRenderContext on which the sprite is to be used.
        Clamping: used internally.
    Return Value:
        TRUE if successful, FALSE otherwise.
    Remarks:
    + This method is automatically called by the framework when a sprite
    needs to be drawn and is not present in video memory.
    + Use this function to creates the video memory copy.
    + Usually, FreeVideoMemory is called for sprites that won't be visible for a long time. When one of
    these sprites should be used again, calling SystemToVideoMemory ensures that the sprite
    will be stored into video memory.
    See also: FreeVideoMemory,Restore
    ************************************************/
    virtual CKBOOL SystemToVideoMemory(CKRenderContext *Dev, CKBOOL Clamping = FALSE) = 0;

    /*************************************************
    Summary: Frees the sprite video memory.

    Return Value:
        TRUE if successful, FALSE otherwise.
    Remarks:
    + Use this function to manage which sprites should be stored in video memory. Some video
    cards may render sprites stored in system memory (ie. AGP).
    + This method can be used in low video memory configuration
    to flush sprites that have not been used for a long time.
    See also: SystemToVideoMemory
    ************************************************/
    virtual CKBOOL FreeVideoMemory() = 0;

    /*************************************************
    Summary: Returns whether the sprite is present in video memory

    Return Value:
        TRUE if the sprite is in video memory FALSE otherwise.
    See also: SystemToVideoMemory
    *************************************************/
    virtual CKBOOL IsInVideoMemory() = 0;

    virtual CKBOOL CopyContext(CKRenderContext *ctx, VxRect *Src, VxRect *Dest) = 0;

    //-------------------------------------------------------------

    /*************************************************
    Summary: Returns a information about how sprite is stored in video memory.

    Arguments:
        desc: A reference to a VxImageDescEx structure that will be filled with video memory format information.
    Return Value:
        FALSE if the sprite is not in video memory or invalid, TRUE otherwise.
    See Also: GetVideoPixelFormat,SetDesiredVideoFormat
    *************************************************/
    virtual CKBOOL GetVideoTextureDesc(VxImageDescEx &desc) = 0;

    /*************************************************
    Summary: Returns the pixel format of the sprite surface in video memory.

    Return Value:
        Pixel format of video memory surface (VX_PIXELFORMAT) or VX_UNKNOWNPF
        if the sprite is not in video memory.
    See Also: SetDesiredVideoFormat, GetVideoTextureDesc
    *************************************************/
    virtual VX_PIXELFORMAT GetVideoPixelFormat() = 0;

    /*************************************************
    Summary: Gets Image format description

    Output Arguments:
        desc: A reference to a VxImageDescEx structure that will be filled with system memory format information.
    Return Value:
        TRUE.
    Remarks:
    + The desc parameter will be filled with the size,pitch,bpp and mask information on how the sprite is stored in system memory.
    See Also: GetVideoPixelFormat,SetDesiredVideoFormat
    *************************************************/
    virtual CKBOOL GetSystemTextureDesc(VxImageDescEx &desc) = 0;

    //-------- Expected Video Format ( textures will use global texture format otherwise )

    /*************************************************
    Summary: Sets the system memory caching option.
    Arguments:
        iOptions: System Caching Options.
    Remarks:

    See Also: SetSaveFormat,CK_BITMAP_SYSTEMCACHING
    *************************************************/
    virtual void SetSystemCaching(CK_BITMAP_SYSTEMCACHING iOptions) = 0;

    virtual CK_BITMAP_SYSTEMCACHING GetSystemCaching() = 0;

    //-------- Save format

    virtual CK_BITMAP_SAVEOPTIONS GetSaveOptions() = 0;

    /*************************************************
    Summary: Sets the saving options.

    Input Arguments:
        Options: Save Options.
    Remarks:
    + When saving a composition sprites can kept as reference to external files or
    converted to a given format and saved inside the composition file. The CK_BITMAP_SAVEOPTIONS
    enumeration exposes the available options.
    See Also: SetSaveFormat,CK_BITMAP_SAVEOPTIONS
    *************************************************/
    virtual void SetSaveOptions(CK_BITMAP_SAVEOPTIONS Options) = 0;

    virtual CKBitmapProperties *GetSaveFormat() = 0;
    /*************************************************
    Summary: Sets the saving format.
    Arguments:
        format: A CKBitmapProperties that contain the format in which the bitmap should be saved.
    Remarks:
    + If the save options have been set to CKTEXTURE_IMAGEFORMAT you can specify a
    format in which the sprite will be converted before being saved inside the composition file.
    + The CKBitmapProperties structure contains the CKGUID of a BitmapReader that is to be
    used plus some additional settings specific to each format.

    See Also: SetSaveOptions,CKBitmapProperties,CKBitmapReader
    *************************************************/
    virtual void SetSaveFormat(CKBitmapProperties *format) = 0;

    /*************************************************
    Summary: Sets pick threshold value.
    Arguments:
        pt: Pick threshold value to be set.
    Return Value:
    Remarks:
    + The pick threshold is used when picking transparent sprites.
    + It is the minimum value for alpha component
    below which picking is not valid.So this value is supposed to be in the range 0..255
    and the default value 0 means the picking is always valid.
    + But if a value >0 is used and the texture use transparency (some pixels of the bitmap will have
    alpha component of 0) a sprite will not be picked on its transparent part.

    See Also: CKRenderContext::Pick
    *************************************************/
    virtual void SetPickThreshold(int pt) = 0;

    virtual int GetPickThreshold() = 0;

    virtual CKBOOL ToRestore() = 0;

    /*************************************************
    Summary: Dynamic cast operator.
    Arguments:
        iO: A pointer to a CKObject to cast.
    Return Value:
        iO casted to the appropriate class or NULL if iO is not from the required class .
    Example:
          CKObject* Object;
          CKSprite* anim = CKSprite::Cast(Object);
    Remarks:

    *************************************************/
    static CKSprite *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_SPRITE) ? (CKSprite *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKSPRITE_H
