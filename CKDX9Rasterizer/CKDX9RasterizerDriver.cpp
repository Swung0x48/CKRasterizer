#include "CKDX9Rasterizer.h"

static const D3DFORMAT TextureFormats[] = {
    D3DFMT_R8G8B8,
    D3DFMT_A8R8G8B8,
    D3DFMT_X8R8G8B8,
    D3DFMT_R5G6B5,
    D3DFMT_X1R5G5B5,
    D3DFMT_A1R5G5B5,
    D3DFMT_A4R4G4B4,
    D3DFMT_R3G3B2,
    D3DFMT_A8,
    D3DFMT_A8R3G3B2,
    D3DFMT_X4R4G4B4,
    D3DFMT_A2B10G10R10,
    D3DFMT_A8B8G8R8,
    D3DFMT_X8B8G8R8,
    D3DFMT_G16R16,
    D3DFMT_A2R10G10B10,
    D3DFMT_A16B16G16R16,

    D3DFMT_A8P8,
    D3DFMT_P8,

    D3DFMT_L8,
    D3DFMT_A8L8,
    D3DFMT_A4L4,

    D3DFMT_V8U8,
    D3DFMT_L6V5U5,
    D3DFMT_X8L8V8U8,
    D3DFMT_Q8W8V8U8,
    D3DFMT_V16U16,
    D3DFMT_A2W10V10U10,

    D3DFMT_UYVY,
    D3DFMT_R8G8_B8G8,
    D3DFMT_YUY2,
    D3DFMT_G8R8_G8B8,
    D3DFMT_DXT1,
    D3DFMT_DXT2,
    D3DFMT_DXT3,
    D3DFMT_DXT4,
    D3DFMT_DXT5
};

inline unsigned int popcount(unsigned int x)
{
    unsigned int count = 0;
    while (x)
    {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

CKDX9RasterizerDriver::CKDX9RasterizerDriver(CKDX9Rasterizer *rst) { m_Owner = rst; }

CKDX9RasterizerDriver::~CKDX9RasterizerDriver() {}

CKRasterizerContext *CKDX9RasterizerDriver::CreateContext()
{
    CKDX9RasterizerContext *context = new CKDX9RasterizerContext(this);
    m_Contexts.PushBack(context);
    return context;
}

CKBOOL CKDX9RasterizerDriver::InitializeCaps(int AdapterIndex, D3DDEVTYPE DevType)
{
    // Store adapter and device info
    m_AdapterIndex = AdapterIndex;
    m_DevType = DevType;
    
    // Get D3D9 interface
    IDirect3D9* pD3D = static_cast<CKDX9Rasterizer*>(m_Owner)->m_D3D9;
    if (!pD3D)
        return FALSE;

    // Get adapter identifier
    if (FAILED(pD3D->GetAdapterIdentifier(AdapterIndex, D3DENUM_WHQL_LEVEL, &m_D3DIdentifier)))
        return FALSE;

    // Get current display mode
    D3DDISPLAYMODE currentMode;
    if (FAILED(pD3D->GetAdapterDisplayMode(AdapterIndex, &currentMode)))
        return FALSE;

    // Get device capabilities
    if (FAILED(pD3D->GetDeviceCaps(AdapterIndex, DevType, &m_D3DCaps)))
        return FALSE;

    // Check if adapter is valid
    UINT adapterCount = pD3D->GetAdapterCount();
    if (AdapterIndex >= adapterCount)
        return FALSE;

    // Define formats to check for display modes
    const D3DFORMAT allowedAdapterFormatArray[] = {
        D3DFMT_A8R8G8B8,  // Start with most common formats first
        D3DFMT_X8R8G8B8,
        D3DFMT_R5G6B5,
        D3DFMT_A1R5G5B5,
        D3DFMT_X1R5G5B5,
        D3DFMT_A2R10G10B10
     };
     const int allowedAdapterFormatArrayCount = sizeof(allowedAdapterFormatArray) / sizeof(allowedAdapterFormatArray[0]);

     // Add current display mode format to render formats
     if (SUCCEEDED(pD3D->CheckDeviceType(AdapterIndex, DevType, currentMode.Format, currentMode.Format, FALSE)))
     {
         m_RenderFormats.PushBack(currentMode.Format);

         // Add current display mode
         VX_PIXELFORMAT pf = D3DFormatToVxPixelFormat(currentMode.Format);
         VxImageDescEx desc;
         VxPixelFormat2ImageDesc(pf, desc);

         VxDisplayMode dm = {
             (int)currentMode.Width,
             (int)currentMode.Height,
             desc.BitsPerPixel,
             (int)currentMode.RefreshRate
         };

         m_DisplayModes.PushBack(dm);
     }

     // Enumerate all supported display modes
     for (int i = 0; i < allowedAdapterFormatArrayCount; ++i)
     {
         D3DFORMAT format = allowedAdapterFormatArray[i];
         UINT numAdapterModes = pD3D->GetAdapterModeCount(AdapterIndex, format);

         for (UINT mode = 0; mode < numAdapterModes; ++mode)
         {
             D3DDISPLAYMODE displayMode;
             if (SUCCEEDED(pD3D->EnumAdapterModes(AdapterIndex, format, mode, &displayMode)))
             {
                 // Filter out low-resolution modes
                 if (displayMode.Width >= 640 && displayMode.Height >= 400)
                 {
                     // Check if the device can render to this format
                     if (SUCCEEDED(pD3D->CheckDeviceType(AdapterIndex, DevType, displayMode.Format, displayMode.Format, FALSE)))
                     {
                         // Add supported format if not already in list
                         if (!m_RenderFormats.IsHere(displayMode.Format))
                             m_RenderFormats.PushBack(displayMode.Format);

                         // Convert to Virtools display mode format
                         VX_PIXELFORMAT pf = D3DFormatToVxPixelFormat(displayMode.Format);
                         VxImageDescEx desc;
                         VxPixelFormat2ImageDesc(pf, desc);

                         VxDisplayMode dm = {
                             (int)displayMode.Width,
                             (int)displayMode.Height,
                             desc.BitsPerPixel,
                             (int)displayMode.RefreshRate
                         };

                         // Add display mode if not already in list
                         if (!m_DisplayModes.IsHere(dm))
                             m_DisplayModes.PushBack(dm);
                     }
                 }
             }
         }
     }

     // Check if we found any render formats
     if (m_RenderFormats.Size() == 0)
     {
         // At least fallback to the current display format
         if (currentMode.Format != D3DFMT_UNKNOWN)
             m_RenderFormats.PushBack(currentMode.Format);
         else
             m_RenderFormats.PushBack(D3DFMT_X8R8G8B8); // Last resort fallback
     }

    // Define texture formats to check
    const D3DFORMAT textureFormatArray[] = {
        D3DFMT_A8R8G8B8,  // Most common formats first
        D3DFMT_X8R8G8B8,
        D3DFMT_A1R5G5B5,
        D3DFMT_X1R5G5B5,
        D3DFMT_R5G6B5,
        D3DFMT_R8G8B8,
        D3DFMT_A4R4G4B4,
        D3DFMT_DXT1,      // Compressed formats
        D3DFMT_DXT3,
        D3DFMT_DXT5,
        D3DFMT_A8,        // Alpha and luminance formats
        D3DFMT_L8,
        D3DFMT_A8L8,
        D3DFMT_V8U8,      // Bump mapping formats
        D3DFMT_V16U16,
        D3DFMT_Q8W8V8U8,
        D3DFMT_X8L8V8U8,
        D3DFMT_L6V5U5,
        D3DFMT_R3G3B2     // Other less common formats
    };
    const int textureFormatArrayCount = sizeof(textureFormatArray) / sizeof(textureFormatArray[0]);

    // Check all texture formats against each render format
    for (int i = 0; i < textureFormatArrayCount; ++i)
    {
        D3DFORMAT texFormat = textureFormatArray[i];
        bool formatSupported = false;

        // Try each render format to see if any support this texture format
        for (int j = 0; j < m_RenderFormats.Size() && !formatSupported; ++j)
        {
            D3DFORMAT renderFormat = m_RenderFormats[j];

            // Check if the device supports this texture format
            if (SUCCEEDED(pD3D->CheckDeviceFormat(
                AdapterIndex,
                DevType,
                renderFormat,
                0, // Regular texture usage
                D3DRTYPE_TEXTURE,
                texFormat)))
            {
                formatSupported = true;
            }
        }

        if (formatSupported)
        {
            // Create a texture descriptor for this format
            CKTextureDesc desc;
            D3DFormatToTextureDesc(texFormat, &desc);
            m_TextureFormats.PushBack(desc);
        }
    }

    // Set hardware capabilities
    m_Hardware = (DevType == D3DDEVTYPE_HAL);

    // Initialize texture size limits and capabilities
    m_3DCaps.StencilCaps = m_D3DCaps.StencilCaps;
    m_3DCaps.DevCaps = m_D3DCaps.DevCaps;

    // Set texture dimension limits
    m_3DCaps.MinTextureWidth = 1;
    m_3DCaps.MinTextureHeight = 1;
    m_3DCaps.MaxTextureWidth = m_D3DCaps.MaxTextureWidth;
    m_3DCaps.MaxTextureHeight = m_D3DCaps.MaxTextureHeight;
    
    // Set texture aspect ratio
    m_3DCaps.MaxTextureRatio = m_D3DCaps.MaxTextureAspectRatio;
    if (m_3DCaps.MaxTextureRatio == 0)
    {
        // If no explicit aspect ratio, use the width as an upper bound
        m_3DCaps.MaxTextureRatio = m_D3DCaps.MaxTextureWidth;
    }

    // Set vertex, lighting and blending capabilities
    m_3DCaps.VertexCaps = m_D3DCaps.VertexProcessingCaps;
    m_3DCaps.MaxClipPlanes = m_D3DCaps.MaxUserClipPlanes;
    m_3DCaps.MaxNumberBlendStage = m_D3DCaps.MaxTextureBlendStages;
    m_3DCaps.MaxActiveLights = m_D3DCaps.MaxActiveLights;
    m_3DCaps.MaxNumberTextureStage = m_D3DCaps.MaxSimultaneousTextures;

    // Initialize texture filtering capabilities
    DWORD TextureFilterCaps = m_D3DCaps.TextureFilterCaps;
    if ((TextureFilterCaps & (D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MINFPOINT)) != 0)
        m_3DCaps.TextureFilterCaps |= CKRST_TFILTERCAPS_NEAREST;
    if ((TextureFilterCaps & (D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MINFLINEAR)) != 0)
        m_3DCaps.TextureFilterCaps |= CKRST_TFILTERCAPS_LINEAR;
    if ((TextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT) != 0)
        m_3DCaps.TextureFilterCaps |= (CKRST_TFILTERCAPS_LINEARMIPNEAREST | CKRST_TFILTERCAPS_MIPNEAREST);
    if ((TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR) != 0)
        m_3DCaps.TextureFilterCaps |= (CKRST_TFILTERCAPS_LINEARMIPLINEAR | CKRST_TFILTERCAPS_MIPLINEAR);
    if ((TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0)
        m_3DCaps.TextureFilterCaps |= CKRST_TFILTERCAPS_ANISOTROPIC;

    // Copy other capability flags
    m_3DCaps.TextureCaps = m_D3DCaps.TextureCaps;
    m_3DCaps.TextureAddressCaps = m_D3DCaps.TextureAddressCaps;
    m_3DCaps.MiscCaps = m_D3DCaps.PrimitiveMiscCaps;
    m_3DCaps.ZCmpCaps = m_D3DCaps.ZCmpCaps;
    m_3DCaps.AlphaCmpCaps = m_D3DCaps.AlphaCmpCaps;
    m_3DCaps.DestBlendCaps = m_D3DCaps.DestBlendCaps;
    m_3DCaps.RasterCaps = m_D3DCaps.RasterCaps;
    m_3DCaps.SrcBlendCaps = m_D3DCaps.SrcBlendCaps;

    // Set specific capabilities flags
    m_3DCaps.CKRasterizerSpecificCaps =
        CKRST_SPECIFICCAPS_SPRITEASTEXTURES |
        CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER |
        CKRST_SPECIFICCAPS_COPYTEXTURE |
        CKRST_SPECIFICCAPS_DX9 |
        CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;

    // Set attenuation model
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_GLATTENUATIONMODEL;

    // Initialize 2D capabilities
    m_2DCaps.Family = CKRST_DIRECTX;
    m_2DCaps.AvailableVideoMemory = 0;
    m_2DCaps.MaxVideoMemory = 0;
    m_2DCaps.Caps = CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI;
    if (AdapterIndex == 0)
        m_2DCaps.Caps |= CKRST_2DCAPS_WINDOWED;

    // Build description string
    HMONITOR hMonitor = pD3D->GetAdapterMonitor(AdapterIndex);
    if (hMonitor)
    {
        MONITORINFOEXA info;
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        if (GetMonitorInfoA(hMonitor, &info))
        {
            // Use device name for monitor
            XString deviceName;
            // Skip initial "\\.\" part of device name if present
            if (info.szDevice[0] == '\\' && info.szDevice[1] == '\\' &&
                info.szDevice[2] == '.' && info.szDevice[3] == '\\')
                deviceName = &info.szDevice[4];
            else
                deviceName = info.szDevice;

            m_Desc = deviceName;

            // Add resolution
            int width = info.rcMonitor.right - info.rcMonitor.left;
            int height = info.rcMonitor.bottom - info.rcMonitor.top;
            m_Desc << " (" << width << "x" << height << ")";

            // Add adapter description
            m_Desc << " @ " << m_D3DIdentifier.Description;
        }
        else
        {
            // Fallback if GetMonitorInfo fails
            m_Desc = m_D3DIdentifier.Description;
        }
    }
    else
    {
        // Fallback if no monitor handle
        m_Desc = m_D3DIdentifier.Description;
    }

    // Clean up backslash in adapter descriptions that sometimes appears
    XWORD pos = m_Desc.Find('\\');
    if (pos != XString::NOTFOUND)
        m_Desc = m_Desc.Crop(0, pos);

    // Add hardware T&L status to description
    if ((m_D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0)
    {
        m_IsHTL = TRUE;
#ifdef USE_D3D9EX
        m_Desc << " (T&L DX9Ex)";
#else
        m_Desc << " (T&L DX9)";
#endif
        m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    }
    else
    {
        m_IsHTL = FALSE;
#ifdef USE_D3D9EX
        m_Desc << " (Hardware DX9Ex)";
#else
        m_Desc << " (Hardware DX9)";
#endif
        m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARE;
    }

    m_CapsUpToDate = TRUE;
    m_Inited = TRUE;

    return TRUE;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestTextureFormat(CKTextureDesc *desc, D3DFORMAT AdapterFormat, DWORD Usage)
{
    if (!desc || AdapterFormat == D3DFMT_UNKNOWN)
        return D3DFMT_UNKNOWN;

    // Get Direct3D9 interface
    CKDX9Rasterizer *owner = static_cast<CKDX9Rasterizer *>(m_Owner);
    if (!owner || !owner->m_D3D9)
        return D3DFMT_UNKNOWN;

    // If texture formats haven't been enumerated yet, do so now
    if (m_TextureFormats.Size() == 0)
    {
        // Define common texture formats to try
        static const D3DFORMAT commonFormats[] = {
            // Standard RGBA formats - most common first
            D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A4R4G4B4, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5,
            // Compressed formats
            D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5,
            // Alpha formats
            D3DFMT_A8, D3DFMT_A8L8, D3DFMT_A4L4,
            // Other formats
            D3DFMT_R8G8B8, D3DFMT_R3G3B2, D3DFMT_X4R4G4B4, D3DFMT_G16R16, D3DFMT_A16B16G16R16};
        const int formatCount = sizeof(commonFormats) / sizeof(commonFormats[0]);

        // Check each format for compatibility with the adapter
        for (int i = 0; i < formatCount; ++i)
        {
            if (IsTextureFormatSupported(commonFormats[i], AdapterFormat, Usage))
            {
                CKTextureDesc formatDesc;
                D3DFormatToTextureDesc(commonFormats[i], &formatDesc);
                m_TextureFormats.PushBack(formatDesc);
            }
        }

        // If we couldn't find any compatible formats, try with fallback formats
        if (m_TextureFormats.Size() == 0)
        {
            // Try at least A8R8G8B8 and R5G6B5 as fallbacks
            CKTextureDesc a8r8g8b8Desc;
            VxImageDescEx imgDesc;
            imgDesc.BitsPerPixel = 32;
            imgDesc.RedMask = R_MASK;
            imgDesc.GreenMask = G_MASK;
            imgDesc.BlueMask = B_MASK;
            imgDesc.AlphaMask = A_MASK;
            a8r8g8b8Desc.Format = imgDesc;
            m_TextureFormats.PushBack(a8r8g8b8Desc);

            // Add R5G6B5 as a common 16-bit format
            CKTextureDesc r5g6b5Desc;
            imgDesc.BitsPerPixel = 16;
            imgDesc.RedMask = 0xF800;
            imgDesc.GreenMask = 0x07E0;
            imgDesc.BlueMask = 0x001F;
            imgDesc.AlphaMask = 0;
            r5g6b5Desc.Format = imgDesc;
            m_TextureFormats.PushBack(r5g6b5Desc);
        }
    }

    // Get the D3D format corresponding to the requested texture description
    D3DFORMAT requestedFormat = TextureDescToD3DFormat(desc);

    // First, check if the requested format is directly supported
    if (requestedFormat != D3DFMT_UNKNOWN && IsTextureFormatSupported(requestedFormat, AdapterFormat, Usage))
    {
        return requestedFormat;
    }

    // Special case for render targets - they have strict format requirements
    if (Usage & D3DUSAGE_RENDERTARGET)
    {
        // Start with common render target formats in preference order
        static const D3DFORMAT rtFormats[] = {
            D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A1R5G5B5,
            D3DFMT_R5G6B5, D3DFMT_X1R5G5B5
        };
        const int rtFormatCount = sizeof(rtFormats) / sizeof(rtFormats[0]);

        // Check each format for render target compatibility
        for (int i = 0; i < rtFormatCount; ++i)
        {
            if (IsTextureFormatSupported(rtFormats[i], AdapterFormat, Usage))
            {
                return rtFormats[i];
            }
        }

        // If no render target format found, we'll fall through to normal texture format search
    }

    // For cube maps, ensure we have a format that supports cube maps
    if (desc->Flags & CKRST_TEXTURE_CUBEMAP)
    {
        // First check if the original format supports cube maps
        if (IsTextureFormatSupported(requestedFormat, AdapterFormat, Usage | D3DUSAGE_DYNAMIC))
        {
            return requestedFormat;
        }

        // Try common cube map formats
        static const D3DFORMAT cubeFormats[] = {
            D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A1R5G5B5,
            D3DFMT_R5G6B5, D3DFMT_DXT1, D3DFMT_DXT3
        };
        const int cubeFormatCount = sizeof(cubeFormats) / sizeof(cubeFormats[0]);

        for (int i = 0; i < cubeFormatCount; i++)
        {
            if (owner->m_D3D9->CheckDeviceFormat(
                m_AdapterIndex, m_DevType, AdapterFormat,
                Usage, D3DRTYPE_CUBETEXTURE, cubeFormats[i]) == D3D_OK)
            {
                return cubeFormats[i];
            }
        }
    }

    // Find the closest matching format among supported formats
    CKTextureDesc *bestMatch = NULL;
    float bestScore = -999999.0f; // Lower is worse
    bool needsAlpha = desc->Format.AlphaMask != 0;
    bool isHighColor = desc->Format.BitsPerPixel >= 24;

    // For compressed textures, prioritize DXT formats
    bool isCompressed = (desc->Flags & CKRST_TEXTURE_COMPRESSION) != 0;

    for (XArray<CKTextureDesc>::Iterator it = m_TextureFormats.Begin(); it != m_TextureFormats.End(); ++it)
    {
        // Skip non-compatible formats
        D3DFORMAT candidateFormat = TextureDescToD3DFormat(&(*it));
        if (!IsTextureFormatSupported(candidateFormat, AdapterFormat, Usage))
            continue;

        // Calculate a compatibility score (higher is better)
        float score = 0.0f;

        // Exact bit depth match is highly preferred
        if (it->Format.BitsPerPixel == desc->Format.BitsPerPixel)
            score += 10000.0f;
        else
            // Penalize bit depth differences more severely
            score -= abs((int)it->Format.BitsPerPixel - (int)desc->Format.BitsPerPixel) * 100.0f;

        // Strongly prefer formats that match alpha requirements
        bool formatHasAlpha = it->Format.AlphaMask != 0;
        if (needsAlpha == formatHasAlpha)
            score += 5000.0f;
        else if (needsAlpha && !formatHasAlpha)
            score -= 8000.0f; // Heavy penalty for losing alpha

        // Penalize color component differences
        int redDiff = popcount(it->Format.RedMask ^ desc->Format.RedMask);
        int greenDiff = popcount(it->Format.GreenMask ^ desc->Format.GreenMask);
        int blueDiff = popcount(it->Format.BlueMask ^ desc->Format.BlueMask);
        int alphaDiff = (needsAlpha && formatHasAlpha) ? popcount(it->Format.AlphaMask ^ desc->Format.AlphaMask) : 0;

        score -= (redDiff + greenDiff + blueDiff + alphaDiff) * 10.0f;

        // For compressed textures, prefer DXT formats
        if (isCompressed)
        {
            if (candidateFormat == D3DFMT_DXT1 ||
                candidateFormat == D3DFMT_DXT2 ||
                candidateFormat == D3DFMT_DXT3 ||
                candidateFormat == D3DFMT_DXT4 ||
                candidateFormat == D3DFMT_DXT5)
            {
                score += 3000.0f;

                // DXT1 for no alpha, DXT3/5 for alpha
                if (!needsAlpha && candidateFormat == D3DFMT_DXT1)
                    score += 1000.0f;
                else if (needsAlpha && (candidateFormat == D3DFMT_DXT3 || candidateFormat == D3DFMT_DXT5))
                    score += 1000.0f;
            }
        }

        // For high-color textures, prefer 32-bit formats
        if (isHighColor && it->Format.BitsPerPixel >= 24)
            score += 2000.0f;

        // For low-color textures, prefer memory-efficient formats
        if (!isHighColor && it->Format.BitsPerPixel <= 16)
            score += 1000.0f;

        // Update best match if we found a better score
        if (score > bestScore)
        {
            bestScore = score;
            bestMatch = it;
        }
    }

    // If we found a match, return its format
    if (bestMatch)
        return TextureDescToD3DFormat(bestMatch);

    // Last resort fallbacks - try standard formats based on alpha requirement
    if (needsAlpha)
        return D3DFMT_A8R8G8B8;
    else
        return D3DFMT_X8R8G8B8;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestRenderTargetFormat(int Bpp, CKBOOL Windowed)
{
    IDirect3D9 *pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;
    if (!pD3D)
        return D3DFMT_UNKNOWN;

    // Get current display mode for the adapter
    D3DDISPLAYMODE displayMode;
    HRESULT hr = pD3D->GetAdapterDisplayMode(m_AdapterIndex, &displayMode);
    if (FAILED(hr))
        return D3DFMT_UNKNOWN;

    // Define render target formats to try, organized by bit depth
    struct FormatInfo
    {
        D3DFORMAT Format;
        int BitDepth;
        BOOL HasAlpha;
    };

    // List of possible render target formats, ordered by preference within each bit depth
    static const FormatInfo formatsList[] = {
        // 32-bit formats
        {D3DFMT_A8R8G8B8, 32, TRUE},
        {D3DFMT_X8R8G8B8, 32, FALSE},

        // 16-bit formats
        {D3DFMT_R5G6B5, 16, FALSE},
        {D3DFMT_A1R5G5B5, 16, TRUE},
        {D3DFMT_X1R5G5B5, 16, FALSE},
        {D3DFMT_A4R4G4B4, 16, TRUE},
        {D3DFMT_X4R4G4B4, 16, FALSE},

        // High bit depth formats
        {D3DFMT_A2B10G10R10, 32, TRUE},
        {D3DFMT_A16B16G16R16, 64, TRUE},
        {D3DFMT_A2R10G10B10, 32, TRUE},

        // Other formats that might be supported as render targets
        {D3DFMT_A8B8G8R8, 32, TRUE},
        {D3DFMT_X8B8G8R8, 32, FALSE},
        {D3DFMT_G16R16, 32, FALSE}
    };
    const int formatCount = sizeof(formatsList) / sizeof(formatsList[0]);

    // First check if we have already detected render formats
    if (m_RenderFormats.Size() > 0)
    {
        // Use a format from the already detected render formats list
        // Choose the one closest to the requested bit depth
        int bestDiff = 100; // Large initial value
        D3DFORMAT bestFormat = D3DFMT_UNKNOWN;

        for (int i = 0; i < m_RenderFormats.Size(); ++i)
        {
            // Check if this format can be used for the backbuffer in the current mode
            if (FAILED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, m_RenderFormats[i], Windowed)))
                continue;

            // Get the bit depth of this format
            int formatBpp = 0;
            switch (m_RenderFormats[i])
            {
                case D3DFMT_R8G8B8:
                case D3DFMT_X8R8G8B8:
                case D3DFMT_A8R8G8B8:
                case D3DFMT_A8B8G8R8:
                case D3DFMT_X8B8G8R8:
                    formatBpp = 32;
                    break;
                case D3DFMT_R5G6B5:
                case D3DFMT_X1R5G5B5:
                case D3DFMT_A1R5G5B5:
                case D3DFMT_A4R4G4B4:
                case D3DFMT_X4R4G4B4:
                    formatBpp = 16;
                    break;
                default:
                    // For other formats, use a default
                    formatBpp = 32;
                    break;
            }

            // Calculate how close this format is to the requested bit depth
            int diff = abs(formatBpp - Bpp);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestFormat = m_RenderFormats[i];

                // If we found an exact match, use it immediately
                if (diff == 0)
                    break;
            }
        }

        // If we found a compatible format in our existing formats, use it
        if (bestFormat != D3DFMT_UNKNOWN)
            return bestFormat;
    }

    // Second approach: try formats in order of preference

    // First try formats with bit depth closest to requested
    D3DFORMAT bestMatch = D3DFMT_UNKNOWN;
    int bestDiff = 100; // large initial value

    for (int i = 0; i < formatCount; ++i)
    {
        // Check if this format can be used as a render target in the current mode
        if (SUCCEEDED(pD3D->CheckDeviceFormat(m_AdapterIndex, m_DevType, displayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, formatsList[i].Format)) &&
            SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, formatsList[i].Format, Windowed)))
        {
            // Calculate how close this format is to the requested bit depth
            int diff = abs(formatsList[i].BitDepth - Bpp);

            // Update best match if this is closer to the requested bit depth
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestMatch = formatsList[i].Format;

                // If this is an exact match, use it immediately
                if (diff == 0)
                    break;
            }
        }
    }

    // If we found a compatible format with bit depth consideration, use it
    if (bestMatch != D3DFMT_UNKNOWN)
        return bestMatch;

    // Fallback: try the current display format itself as a render target
    if (SUCCEEDED(pD3D->CheckDeviceFormat(m_AdapterIndex, m_DevType, displayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, displayMode.Format)) &&
        SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, displayMode.Format, Windowed)))
    {
        return displayMode.Format;
    }

    // Final fallbacks - try common formats that are widely supported
    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, D3DFMT_X8R8G8B8, Windowed)))
        return D3DFMT_X8R8G8B8;

    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, D3DFMT_R5G6B5, Windowed)))
        return D3DFMT_R5G6B5;

    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, m_DevType, displayMode.Format, D3DFMT_X1R5G5B5, Windowed)))
        return D3DFMT_X1R5G5B5;

    // If nothing worked, return unknown format
    return D3DFMT_UNKNOWN;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestDepthFormat(D3DFORMAT pf, int ZBpp, int StencilBpp)
{
    // Get information about the color format
    VxImageDescEx desc;
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(pf);
    VxPixelFormat2ImageDesc(vxpf, desc);

    // If requested depth bits is not specified, derive from color format
    if (ZBpp <= 0)
        ZBpp = desc.BitsPerPixel;

    // Define arrays of depth formats by category
    // Order is from most desirable to least desirable for each category

    // Formats with stencil buffer (8-bit)
    D3DFORMAT formatsWithStencil8[] = {
        D3DFMT_D24S8, // 24-bit depth + 8-bit stencil (most widely supported)
        D3DFMT_UNKNOWN
    };

    // Formats with stencil buffer (4-bit or less)
    D3DFORMAT formatsWithStencil4[] = {
        D3DFMT_D24X4S4, // 24-bit depth + 4-bit stencil
        D3DFMT_D15S1, // 15-bit depth + 1-bit stencil
        D3DFMT_UNKNOWN
    };

    // Formats without stencil (32/24-bit)
    D3DFORMAT formats32[] = {
        D3DFMT_D24X8, // 24-bit depth, padded to 32-bit
        D3DFMT_D32, // 32-bit depth, often less efficiently implemented on hardware
        D3DFMT_UNKNOWN
    };

    // Formats without stencil (16-bit)
    D3DFORMAT formats16[] = {
        D3DFMT_D16, // 16-bit depth (common on older hardware)
        D3DFMT_D16_LOCKABLE, // 16-bit lockable depth buffer (rare)
        D3DFMT_UNKNOWN
    };

    // Try the most appropriate formats first based on requirements

    // 1. If stencil bits were explicitly requested, try formats with stencil first
    if (StencilBpp > 0)
    {
        // Check if 8-bit stencil is requested
        if (StencilBpp >= 8)
        {
            for (D3DFORMAT *format = formatsWithStencil8; *format != D3DFMT_UNKNOWN; ++format)
            {
                if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
                {
                    return *format;
                }
            }
        }

        // Try formats with smaller stencil bits
        for (D3DFORMAT *format = formatsWithStencil4; *format != D3DFMT_UNKNOWN; ++format)
        {
            if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
            {
                return *format;
            }
        }

        // If we requested stencil but couldn't find a match, continue to try
        // non-stencil formats as fallback
    }

    // 2. Based on depth bits requested, try appropriate depth formats
    D3DFORMAT *primaryFormats = NULL;
    D3DFORMAT *secondaryFormats = NULL;

    if (ZBpp <= 16)
    {
        primaryFormats = formats16;
        secondaryFormats = formats32; // Fallback to 32-bit if 16-bit not available
    }
    else
    {
        primaryFormats = formats32; // 24/32-bit depth preferred
        secondaryFormats = formats16; // Fall back to 16-bit if necessary
    }

    // Try primary depth formats
    if (primaryFormats)
    {
        for (D3DFORMAT *format = primaryFormats; *format != D3DFMT_UNKNOWN; ++format)
        {
            if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
            {
                return *format;
            }
        }
    }

    // Try secondary depth formats as fallback
    if (secondaryFormats)
    {
        for (D3DFORMAT *format = secondaryFormats; *format != D3DFMT_UNKNOWN; ++format)
        {
            if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
            {

                return *format;
            }
        }
    }

    // 3. If still no match, try stencil formats even if stencil not requested
    // (Some cards might support D24S8 better than D24X8 for example)
    for (D3DFORMAT *format = formatsWithStencil8; *format != D3DFMT_UNKNOWN; ++format)
    {
        if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
        {
            return *format;
        }
    }

    for (D3DFORMAT *format = formatsWithStencil4; *format != D3DFMT_UNKNOWN; ++format)
    {
        if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
        {
            return *format;
        }
    }

    // 4. Final fallback: check compatibility against common render target formats
    D3DFORMAT commonRenderFormats[] = {
        D3DFMT_A8R8G8B8,
        D3DFMT_X8R8G8B8,
        D3DFMT_R5G6B5,
        D3DFMT_UNKNOWN
    };

    D3DFORMAT commonDepthFormats[] = {
        D3DFMT_D24S8,  // Most widely supported on modern hardware
        D3DFMT_D16,    // Most widely supported on older hardware
        D3DFMT_D24X8,
        D3DFMT_UNKNOWN
    };

    // Try each render target format with each depth format
    for (int i = 0; commonRenderFormats[i] != D3DFMT_UNKNOWN; ++i)
    {
        for (int j = 0; commonDepthFormats[j] != D3DFMT_UNKNOWN; ++j)
        {
            if (CheckDepthFormatWithRenderTarget(pf, commonRenderFormats[i], commonDepthFormats[j]))
            {
                return commonDepthFormats[j];
            }
        }
    }

    // 5. Last resort - just return a safe default
    return (ZBpp <= 16) ? D3DFMT_D16 : D3DFMT_D24S8;
}

CKBOOL CKDX9RasterizerDriver::IsTextureFormatSupported(D3DFORMAT TextureFormat, D3DFORMAT AdapterFormat, DWORD Usage)
{
    CKDX9Rasterizer *rasterizer = static_cast<CKDX9Rasterizer *>(m_Owner);
    if (!rasterizer || !rasterizer->m_D3D9)
        return FALSE;

    // Detect if this is a depth format being checked for texture usage
    // (unusual but possible in some contexts)
    CKBOOL isDepthFormat = FALSE;
    switch (TextureFormat)
    {
        case D3DFMT_D16:
        case D3DFMT_D15S1:
        case D3DFMT_D24X8:
        case D3DFMT_D24S8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32:
        case D3DFMT_D16_LOCKABLE:
            isDepthFormat = TRUE;
            break;
        default:
            isDepthFormat = FALSE;
    }

    // Check if the format is supported
    HRESULT hr = rasterizer->m_D3D9->CheckDeviceFormat(
        m_AdapterIndex,
        m_DevType,
        AdapterFormat,
        Usage,
        D3DRTYPE_TEXTURE,
        TextureFormat);

    // If the primary check fails and this is a render target, 
    // try with a more specific test for certain formats
    if (FAILED(hr) && (Usage & D3DUSAGE_RENDERTARGET))
    {
        // Some formats like R32F might require specific handling
        if (TextureFormat == D3DFMT_R32F || TextureFormat == D3DFMT_A32B32G32R32F ||
            TextureFormat == D3DFMT_A16B16G16R16F)
        {
            // Try checking with a different adapter format that might support this
            D3DFORMAT alternateFormats[] = {
                D3DFMT_A8R8G8B8,
                D3DFMT_X8R8G8B8,
                D3DFMT_UNKNOWN
            };

            for (int i = 0; alternateFormats[i] != D3DFMT_UNKNOWN; ++i)
            {
                if (alternateFormats[i] != AdapterFormat)
                {
                    hr = rasterizer->m_D3D9->CheckDeviceFormat(
                        m_AdapterIndex,
                        m_DevType,
                        alternateFormats[i],
                        Usage,
                        D3DRTYPE_TEXTURE,
                        TextureFormat);

                    if (SUCCEEDED(hr))
                    {
                        break;
                    }
                }
            }
        }
    }

    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerDriver::CheckDeviceFormat(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat)
{
    CKDX9Rasterizer *rasterizer = static_cast<CKDX9Rasterizer *>(m_Owner);
    if (!rasterizer || !rasterizer->m_D3D9)
        return FALSE;

    // For depth-stencil formats, we need to check compatibility as a depth-stencil surface
    // rather than as a texture (which the original code was doing)
    CKBOOL isDepthFormat = FALSE;

    // Check if this is a depth-stencil format
    switch (CheckFormat)
    {
        case D3DFMT_D16:
        case D3DFMT_D15S1:
        case D3DFMT_D24X8:
        case D3DFMT_D24S8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32:
        case D3DFMT_D16_LOCKABLE:
            isDepthFormat = TRUE;
            break;
        default:
            isDepthFormat = FALSE;
    }

    // Use correct parameters for depth formats vs. regular texture formats
    if (isDepthFormat)
    {
        // Check if this depth format is supported for depth-stencil usage
        return SUCCEEDED(rasterizer->m_D3D9->CheckDeviceFormat(
            m_AdapterIndex,
            m_DevType,
            AdapterFormat,
            D3DUSAGE_DEPTHSTENCIL,
            D3DRTYPE_SURFACE,  // Depth buffers are surfaces, not textures
            CheckFormat));
    }
    else
    {
        // For other formats, check regular format compatibility
        return SUCCEEDED(rasterizer->m_D3D9->CheckDeviceFormat(
            m_AdapterIndex,
            m_DevType,
            AdapterFormat,
            0,  // No specific usage flags
            D3DRTYPE_TEXTURE, 
            CheckFormat));
    }
}

CKBOOL CKDX9RasterizerDriver::CheckDepthStencilMatch(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat)
{
    CKDX9Rasterizer *rasterizer = static_cast<CKDX9Rasterizer *>(m_Owner);
    if (!rasterizer || !rasterizer->m_D3D9)
        return FALSE;

    // We need to find a suitable render target format to test against
    // If we're checking a backbuffer+depth compatibility, the render target
    // format would typically be the same as adapter format, or a compatible format

    // First try using the adapter format as render target
    HRESULT hr = rasterizer->m_D3D9->CheckDepthStencilMatch(
        m_AdapterIndex,
        m_DevType,
        AdapterFormat,  // Adapter format
        AdapterFormat,  // Render target format (same as adapter format)
        CheckFormat);
    if (SUCCEEDED(hr))
        return TRUE;

    // If that failed, try with known common render target formats
    D3DFORMAT commonFormats[] = {
        D3DFMT_A8R8G8B8,
        D3DFMT_X8R8G8B8,
        D3DFMT_R5G6B5,
        D3DFMT_X1R5G5B5,
        D3DFMT_A1R5G5B5,
        D3DFMT_UNKNOWN  // Terminator
    };

    for (int i = 0; commonFormats[i] != D3DFMT_UNKNOWN; ++i)
    {
        // First check if this format is valid for rendering
        if (SUCCEEDED(rasterizer->m_D3D9->CheckDeviceFormat(
                m_AdapterIndex,
                m_DevType,
                AdapterFormat,
                D3DUSAGE_RENDERTARGET,
                D3DRTYPE_SURFACE,
                commonFormats[i])))
        {
            // Then check if depth-stencil format is compatible with it
            hr = rasterizer->m_D3D9->CheckDepthStencilMatch(
                m_AdapterIndex,
                m_DevType,
                AdapterFormat,     // Adapter format
                commonFormats[i],  // Render target format
                CheckFormat);
                
            if (SUCCEEDED(hr))
                return TRUE;
        }
    }

    // No compatible render target formats found
    return FALSE;
}

CKBOOL CKDX9RasterizerDriver::CheckDepthFormatWithRenderTarget(D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
{
    CKDX9Rasterizer *rasterizer = static_cast<CKDX9Rasterizer *>(m_Owner);
    if (!rasterizer || !rasterizer->m_D3D9)
        return FALSE;

    // First check if this render target format is supported
    HRESULT hr = rasterizer->m_D3D9->CheckDeviceFormat(
        m_AdapterIndex,
        m_DevType,
        AdapterFormat,
        D3DUSAGE_RENDERTARGET,
        D3DRTYPE_SURFACE,
        RenderTargetFormat);
    if (FAILED(hr))
        return FALSE;

    // Then check depth-stencil compatibility
    hr = rasterizer->m_D3D9->CheckDepthStencilMatch(
        m_AdapterIndex,
        m_DevType,
        AdapterFormat,
        RenderTargetFormat,
        DepthStencilFormat);

    return SUCCEEDED(hr);
}