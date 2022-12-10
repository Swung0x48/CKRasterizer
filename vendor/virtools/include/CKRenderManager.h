#ifndef CKRENDERMANAGER_H
#define CKRENDERMANAGER_H

#include "CKDefines.h"
#include "CKBaseManager.h"
#include "CKVertexBuffer.h"

/*************************************************
{filename:CKRenderManager}
Summary: Manages all rendering contexts.

Remarks:

+ CKRenderManager stores a list of all render context and
provides methods to render all of them, to create new ones or to destroy existing contexts.

+ At startup the available drivers are enumerated and can be retrieve along with their capabilities.
There can be more than one driver per video card according to the installed rasterizer (OpenGL,DirectX or other..)
which can propose themselves several sub drivers (Transform& Lighting, Hardware or Software rasterizer).

+ The unique instance of the RenderManager can accessed through the CKContext::GetRenderManager() method.

See also: CKContext::GetRenderManager,CKRenderContext
*************************************************/
class CKRenderManager : public CKBaseManager
{
public:
    //--------------------------------------------------------
    // Possible Drivers

    /*************************************************
    Summary: Gets the number of render drivers.

    Return Value:
        Number of available render drivers.
    See Also:GetRenderDriverDescription,VxDriverDesc
    *************************************************/
    virtual int GetRenderDriverCount() = 0;

    /*************************************************
    Summary: Gets render driver description

    Arguments:
        Driver: Index of the render driver which description should be returned.
    Return Value:
        A pointer to a VxDriverDesc structure containing the information about the driver.
    Remarks:
    + To create a render context you must specify the index of the driver to use.
    + You can use this method to iterate through all available drivers and choose
    the best suited. Driver list is organized so that driver 0 is supposed to be
    the best compromise available.
    See also: VxDriverDesc,CreateRenderContext
    *************************************************/
    virtual VxDriverDesc *GetRenderDriverDescription(int Driver) = 0;

    //---------------------------------------------------------
    // Sets or Gets the desired texture format for textures in video memory
    // Textures will be created using this format

    /*************************************************
    Summary: Gets the desired pixel format for textures in video memory

    Arguments:
        VideoFormat: A VxImageDescEx pointer to be filled with desired pixel format.
    Remarks:
        + Newly created textures uses this format as their default pixel format
        when loaded in video memory. It can be overridden by setting the desired video format per texture with CKTexture::SetDesiredVideoFormat.
    See also: SetDesiredTexturesVideoFormat, VxImageDescEx,CKTexture::SetDesiredVideoFormat
    *************************************************/
    virtual void GetDesiredTexturesVideoFormat(VxImageDescEx &VideoFormat) = 0;

    /*************************************************
    Summary: Sets the desired pixel format for textures in video memory

    Arguments:
        VideoFormat: A VxImageDescEx pointer containing texture pixel format.
    Remarks:
        + This method sets the default pixel format texture should use in video memory.
        It can be overridden by setting the desired video format per texture with CKTexture::SetDesiredVideoFormat.
    See also: VxImageDescEx, GetDesiredTexturesVideoFormat,CKTexture::SetDesiredVideoFormat
    *************************************************/
    virtual void SetDesiredTexturesVideoFormat(VxImageDescEx &VideoFormat) = 0;

    //--------------------------------------------------------
    // Possible Contexts

    /*************************************************
    Summary: Gets a render context given its index.

    Arguments:
        pos: Index of the render context to be retrieve.
    Return value: A pointer to CKRenderContext
    See Also: GetRenderContextCount,CreateRenderContext
    *************************************************/
    virtual CKRenderContext *GetRenderContext(int pos) = 0;

    /*************************************************
    Summary: Gets a render context under a screen position.

    Return Value:
        A pointer to a CKRenderContext or NULL.
    Arguments:
        pt: Screen coordinates from which the render context should be retrieved.
    Return value: Pointer to CKRenderContext or NULL if there is not a render context under the given point.
    See Also: GetRenderContext,GetRenderContextCount
    *************************************************/
    virtual CKRenderContext *GetRenderContextFromPoint(CKPOINT &pt) = 0;

    /*************************************************
    Summary: Gets the number of render contexts.

    Return Value: Number of render contexts.
    See Also: GetRenderContext
    *************************************************/
    virtual int GetRenderContextCount() = 0;

    //--------------------------------------------------------
    // Rendering

    /*************************************************
    Summary: Renders all the render contexts.

    *************************************************/
    virtual void Process() = 0;

    //--------------------------------------------------------
    // Removes Sprites and Textures from video memory

    /*************************************************
    Summary: Removes all the textures and sprites from video memory.

    Remarks:
        + To be drawn, textures and sprites need to be present in video memory.
        + This method force all textures to be removed from video memory.
    See Also:SetDesiredTexturesVideoFormat
    *************************************************/
    virtual void FlushTextures() = 0;

    //--------------------------------------------------------
    // Creating contexts

    /*************************************************
    Summary: Creates a new render context

    Arguments:
        Window: A Pointer to the window on which rendering will be done.
        Driver: Index of the Render driver
        rect: A CKRECT  specifying the rendering size or NULL to use the entire window.
        Fullscreen: TRUE to switch to fullscreen.
        Bpp: Number of bits per pixel for the color buffer(<=0 to have a default bpp).
        Zbpp: Number of bits per pixel for the depth buffer (<=0 to have a default bpp).
        StencilBpp: Number of bits per pixel for the stencil buffer (<=0 to have a default bpp).
        RefreshRate: Optional Refresh rate to use in fullscreen.
    Return Value: Pointer to the created CKRenderContext.
    Remarks:
        + When creating a fullscreen render context if no valid rect is given the default display mode is set
        to 640x480.
    See Also:GetRenderDriverDescription,GetRenderDriverCount,DestroyRenderContext
    *************************************************/
    virtual CKRenderContext *CreateRenderContext(void *Window, int Driver = 0, CKRECT *rect = NULL, CKBOOL Fullscreen = FALSE, int Bpp = -1, int Zbpp = -1, int StencilBpp = -1, int RefreshRate = 0) = 0;

    /*************************************************
    Summary: Destroys a render context.

    Arguments:
        context: A pointer to CKRenderContext to be destroyed.
    Return Value: CK_OK if its successful, error code otherwise.
    Remarks:
        + This method removes the render context from the list and destroys it.
    See Also:CreateRenderContext,RemoveRenderContext
    *************************************************/
    virtual CKERROR DestroyRenderContext(CKRenderContext *context) = 0;

    /***************************************************
    Summary: Removes a render context.

    Arguments:
        context: A pointer to CKRenderContext to be removed from the list.
    See Also: CreateRenderContext,GetRenderContextCount,DestroyRenderContext,CreateRenderContext
    ****************************************************/
    virtual void RemoveRenderContext(CKRenderContext *context) = 0;

    /*************************************************
    Summary: Creates a new vertex buffer

    Return Value: Pointer to the created CKVertexBuffer.
    Remarks:
        + Every vertex buffers are automatically destroyed on a ClearAll operation.
    See Also:CKVertexBuffer,DestroyVertexBuffer
    *************************************************/
    virtual CKVertexBuffer *CreateVertexBuffer() = 0;

    /*************************************************
    Summary: Destroys a vertex buffer.

    Arguments:
        VB: A pointer to CKVertexBuffer to be destroyed.
    Remarks:
        + Every vertex buffers are automatically destroyed on a ClearAll operation.
    See Also:CKVertexBuffer,CreateVertexBuffer
    *************************************************/
    virtual void DestroyVertexBuffer(CKVertexBuffer *VB) = 0;

    /*************************************************
    Summary: Sets render engine settings.

    Arguments:
        RenderOptionString: A string identifying the render option to set.
        Value : A integer value for the given option.
    Remarks:
    + The available options (with their default value given in ()) are :
            DisablePerspectiveCorrection(0) : Disables perspective correction. On recent hardware this does not have
                                          a great impact but it can be very useful in software mode

            ForceLinearFog(0) : Previous versions of the render engine forced fog modes to default to linear fog
                            mode. this was essentially caused by back-compatibility problems with DirectX 5.
                            this has now been removed but can be re-activated though this variable.
            ForceSoftware(0) : Disables all hardware renderers

            EnsureVertexShader(0) :  For DX8 rasterizer this ensure that vertex shader are supported : for example if
                                the graphic card and driver support T&L but not vertex shaders in hardware the chosen
                                driver will be an non T&L device. Otherwise created device will only support vertex shaders
                                if they are supported in hardware.

            DisableFilter(0) : Disables texture filtering

            DisableDithering(0) : Disables image dithering

            Antialias(0) : Enables image antialiasing. This represents the number of multi-sample
                            for DX8 anti-aliasing (minimum :2). This settings only works on DX8 rasterizer
                            for the moment.
            DisableMipmap(0) : Disables image dithering

            DisableSpecular(0) : Disables specular highlights

            EnableScreenDump(0) : Pressing CTRL+ALT+F10 dump the content of
                                  the screen,depth and stencil buffer to
                                  the root of the current hard drive....
            EnableScreenDump(0) : Pressing CTRL+ALT+F10 dump the content of
                                  the screen,depth and stencil buffer to
                                  the root of the current hard drive....

            EnableDebugMode(0)  : Pressing (CTRL + ALT + F11) starts/stops the debug
                       mode where rendering can be done step by step on each object

            VertexCache(16)    : Mesh draw indices are automatically reorganised to
                                take advantage of a potential hardware vertex cache.
                                A value of 0 disables the sorting.

            TextureCacheManagement(1) : Texture cache management can be disabled to avoid
                                to have a copy of the texture stored in system memory when
                                all textures are known to fit into video memory.

            SortTransparentObjects(1) : Transparent objects are sorted according
                             to their distance to the camera and their priority.
                             A value of 0 disables the sorting.

            TextureVideoFormat: This entry is followed by a pixel format
                              that is to be used as default settings for a texture in
                             video memory.
            SpriteVideoFormat: This entry is followed by a pixel format
                              that is to be used as default settings for a sprite in
                             video memory.
                  Available formats :
                    _32_ARGB8888,_32_RGB888
                    _24_RGB888,
                    _16_RGB565,_16_RGB555,_16_ARGB1555,_16_ARGB4444,
                    _8_RGB332_8_ARGB2222,
                    _DXT1,_DXT3,_DXT5

    + These options are read in the CK2_3D.ini file at startup and can be modified afterwards.
    + These options apply when a render context is created but are not taken into account by created
    render contexts if changed meanwhile...
    See Also:CK2_3D.ini
    *************************************************/
    virtual void SetRenderOptions(CKSTRING RenderOptionString, CKDWORD Value) = 0;

    /*******************************************************************
    Summary: Returns a description for a given effect

    Return Value:
        A reference to a VxEffectDescription containing the description of the effect.
    Arguments:
        EffectIndex: Index of the effect to return.
    Remarks:
    o This method returns a VxEffectDescription structure that gives details about
    a given effect.
    o Effects provide additional functionalities to take advantage of graphic features such as bump mapping,cube maps etc...
    o When an effect is enabled on a material (CKMaterial::SetEffect) it may override the default settings of mesh channels or material blend options
    o New effects can be created by providing a callback function (see CKRenderManager::AddEffect)
    o Most of this effect are heavily hardware and device (DX8,DX7,etc..) dependant
    See Also:CKMaterial::SetEffect,VxEffectDescription,GetEffectCount,AddEffect
    *******************************************************************/
    virtual const VxEffectDescription &GetEffectDescription(int EffectIndex) = 0;
    /****************************************************************
    Summary: Returns the number of registered effects.

    Return Value:
        Number of registered effects.
    Remarks:
    o This method returns the number of available effects, either provided by render engine or added by users
    with AddEffect
    o Effects provide additional functionalities to take advantage of graphic features such as bump mapping,cube maps etc...
    o When an effect is enabled it may override the default settings of mesh channels or material blend options
    o New effects can be created by providing a callback function (see CKRenderManager::AddEffect)
    o Most of this effect are heavily hardware and device (DX8,DX7,etc..) dependant
    See Also:CKMaterial::SetEffect,VxEffectDescription,GetEffectDescription,AddEffect
    *******************************************************************/
    virtual int GetEffectCount() = 0;
    /****************************************************************
    Summary: Returns the number of registered effects.

    Return Value:
        Index of newly created effect.
    Remarks:
    o This method enables an user to add a given effect to the existing ones. A callback function must be provided that will
    be called when a material is used. No checks are done on names so it is the application responsibility to ensure an effect does not exist yet before adding
    a new one.
    o Effects provide additional functionalities to take advantage of graphic features such as bump mapping,cube maps etc...
    o When an effect is enabled it may override the default settings of mesh channels or material blend options
    o New effects can be created by providing a callback function (see CKRenderManager::AddEffect)
    o Most of this effect are heavily hardware and device (DX8,DX7,etc..) dependant
    See Also:CKMaterial::SetEffect,VxEffectDescription,GetEffectDescription,GetEffectCount
    *******************************************************************/
    virtual int AddEffect(const VxEffectDescription &NewEffect) = 0;

    CKRenderManager(CKContext *Context, CKSTRING name) : CKBaseManager(Context, RENDER_MANAGER_GUID, name) {}
};

#endif // CKRENDERMANAGER_H
