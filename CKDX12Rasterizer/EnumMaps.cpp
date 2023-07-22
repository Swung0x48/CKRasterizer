#include "EnumMaps.h"

#include <string>
#include <unordered_map>

const char* rstytostr(VXRENDERSTATETYPE rsty)
{
    static const std::unordered_map<VXRENDERSTATETYPE, std::string> enummap = {
	    {VXRENDERSTATE_ANTIALIAS           , "VXRENDERSTATE_ANTIALIAS           "},
        {VXRENDERSTATE_TEXTUREPERSPECTIVE  , "VXRENDERSTATE_TEXTUREPERSPECTIVE  "},
        {VXRENDERSTATE_ZENABLE             , "VXRENDERSTATE_ZENABLE             "},
        {VXRENDERSTATE_FILLMODE            , "VXRENDERSTATE_FILLMODE            "},
        {VXRENDERSTATE_SHADEMODE           , "VXRENDERSTATE_SHADEMODE           "},
        {VXRENDERSTATE_LINEPATTERN         , "VXRENDERSTATE_LINEPATTERN         "},
        {VXRENDERSTATE_ZWRITEENABLE        , "VXRENDERSTATE_ZWRITEENABLE        "},
        {VXRENDERSTATE_ALPHATESTENABLE     , "VXRENDERSTATE_ALPHATESTENABLE     "},
        {VXRENDERSTATE_SRCBLEND            , "VXRENDERSTATE_SRCBLEND            "},
        {VXRENDERSTATE_DESTBLEND           , "VXRENDERSTATE_DESTBLEND           "},
        {VXRENDERSTATE_CULLMODE            , "VXRENDERSTATE_CULLMODE            "},
        {VXRENDERSTATE_ZFUNC               , "VXRENDERSTATE_ZFUNC               "},
        {VXRENDERSTATE_ALPHAREF            , "VXRENDERSTATE_ALPHAREF            "},
        {VXRENDERSTATE_ALPHAFUNC           , "VXRENDERSTATE_ALPHAFUNC           "},
        {VXRENDERSTATE_DITHERENABLE        , "VXRENDERSTATE_DITHERENABLE        "},
        {VXRENDERSTATE_ALPHABLENDENABLE    , "VXRENDERSTATE_ALPHABLENDENABLE    "},
        {VXRENDERSTATE_FOGENABLE           , "VXRENDERSTATE_FOGENABLE           "},
        {VXRENDERSTATE_SPECULARENABLE      , "VXRENDERSTATE_SPECULARENABLE      "},
        {VXRENDERSTATE_FOGCOLOR            , "VXRENDERSTATE_FOGCOLOR            "},
        {VXRENDERSTATE_FOGPIXELMODE        , "VXRENDERSTATE_FOGPIXELMODE        "},
        {VXRENDERSTATE_FOGSTART            , "VXRENDERSTATE_FOGSTART            "},
        {VXRENDERSTATE_FOGEND              , "VXRENDERSTATE_FOGEND              "},
        {VXRENDERSTATE_FOGDENSITY          , "VXRENDERSTATE_FOGDENSITY          "},
        {VXRENDERSTATE_EDGEANTIALIAS       , "VXRENDERSTATE_EDGEANTIALIAS       "},
        {VXRENDERSTATE_ZBIAS               , "VXRENDERSTATE_ZBIAS               "},
        {VXRENDERSTATE_RANGEFOGENABLE      , "VXRENDERSTATE_RANGEFOGENABLE      "},
        {VXRENDERSTATE_STENCILENABLE       , "VXRENDERSTATE_STENCILENABLE       "},
        {VXRENDERSTATE_STENCILFAIL         , "VXRENDERSTATE_STENCILFAIL         "},
        {VXRENDERSTATE_STENCILZFAIL        , "VXRENDERSTATE_STENCILZFAIL        "},
        {VXRENDERSTATE_STENCILPASS         , "VXRENDERSTATE_STENCILPASS         "},
        {VXRENDERSTATE_STENCILFUNC         , "VXRENDERSTATE_STENCILFUNC         "},
        {VXRENDERSTATE_STENCILREF          , "VXRENDERSTATE_STENCILREF          "},
        {VXRENDERSTATE_STENCILMASK         , "VXRENDERSTATE_STENCILMASK         "},
        {VXRENDERSTATE_STENCILWRITEMASK    , "VXRENDERSTATE_STENCILWRITEMASK    "},
        {VXRENDERSTATE_TEXTUREFACTOR       , "VXRENDERSTATE_TEXTUREFACTOR       "},
        {VXRENDERSTATE_WRAP0               , "VXRENDERSTATE_WRAP0               "},
        {VXRENDERSTATE_WRAP1               , "VXRENDERSTATE_WRAP1               "},
        {VXRENDERSTATE_WRAP2               , "VXRENDERSTATE_WRAP2               "},
        {VXRENDERSTATE_WRAP3               , "VXRENDERSTATE_WRAP3               "},
        {VXRENDERSTATE_WRAP4               , "VXRENDERSTATE_WRAP4               "},
        {VXRENDERSTATE_WRAP5               , "VXRENDERSTATE_WRAP5               "},
        {VXRENDERSTATE_WRAP6               , "VXRENDERSTATE_WRAP6               "},
        {VXRENDERSTATE_WRAP7               , "VXRENDERSTATE_WRAP7               "},
        {VXRENDERSTATE_CLIPPING            , "VXRENDERSTATE_CLIPPING            "},
        {VXRENDERSTATE_LIGHTING            , "VXRENDERSTATE_LIGHTING            "},
        {VXRENDERSTATE_AMBIENT             , "VXRENDERSTATE_AMBIENT             "},
        {VXRENDERSTATE_FOGVERTEXMODE       , "VXRENDERSTATE_FOGVERTEXMODE       "},
        {VXRENDERSTATE_COLORVERTEX         , "VXRENDERSTATE_COLORVERTEX         "},
        {VXRENDERSTATE_LOCALVIEWER         , "VXRENDERSTATE_LOCALVIEWER         "},
        {VXRENDERSTATE_NORMALIZENORMALS    , "VXRENDERSTATE_NORMALIZENORMALS    "},
        {VXRENDERSTATE_VERTEXBLEND         , "VXRENDERSTATE_VERTEXBLEND         "},
        {VXRENDERSTATE_SOFTWAREVPROCESSING , "VXRENDERSTATE_SOFTWAREVPROCESSING "},
        {VXRENDERSTATE_CLIPPLANEENABLE     , "VXRENDERSTATE_CLIPPLANEENABLE     "},
        {VXRENDERSTATE_INDEXVBLENDENABLE   , "VXRENDERSTATE_INDEXVBLENDENABLE   "},
        {VXRENDERSTATE_BLENDOP             , "VXRENDERSTATE_BLENDOP             "},

        {VXRENDERSTATE_TEXTURETARGET       , "VXRENDERSTATE_TEXTURETARGET       "},
        {VXRENDERSTATE_INVERSEWINDING      , "VXRENDERSTATE_INVERSEWINDING      "},
        {VXRENDERSTATE_MAXSTATE            , "VXRENDERSTATE_MAXSTATE            "}};
    if (enummap.find(rsty) != enummap.end())
        return enummap.find(rsty)->second.c_str();
    return "";
}

const char* tstytostr(CKRST_TEXTURESTAGESTATETYPE tsty)
{
    static const std::unordered_map<CKRST_TEXTURESTAGESTATETYPE, std::string> enummap = {
        {CKRST_TSS_OP                    , "CKRST_TSS_OP                   "},
        {CKRST_TSS_ARG1                  , "CKRST_TSS_ARG1                 "},
        {CKRST_TSS_ARG2                  , "CKRST_TSS_ARG2                 "},
        {CKRST_TSS_AOP                   , "CKRST_TSS_AOP                  "},
        {CKRST_TSS_AARG1                 , "CKRST_TSS_AARG1                "},
        {CKRST_TSS_AARG2                 , "CKRST_TSS_AARG2                "},
        {CKRST_TSS_BUMPENVMAT00          , "CKRST_TSS_BUMPENVMAT00         "},
        {CKRST_TSS_BUMPENVMAT01          , "CKRST_TSS_BUMPENVMAT01         "},
        {CKRST_TSS_BUMPENVMAT10          , "CKRST_TSS_BUMPENVMAT10         "},
        {CKRST_TSS_BUMPENVMAT11          , "CKRST_TSS_BUMPENVMAT11         "},
        {CKRST_TSS_TEXCOORDINDEX         , "CKRST_TSS_TEXCOORDINDEX        "},
        {CKRST_TSS_ADDRESS               , "CKRST_TSS_ADDRESS              "},
        {CKRST_TSS_ADDRESSU              , "CKRST_TSS_ADDRESSU             "},
        {CKRST_TSS_ADDRESSV              , "CKRST_TSS_ADDRESSV             "},
        {CKRST_TSS_BORDERCOLOR           , "CKRST_TSS_BORDERCOLOR          "},
        {CKRST_TSS_MAGFILTER             , "CKRST_TSS_MAGFILTER            "},
        {CKRST_TSS_MINFILTER             , "CKRST_TSS_MINFILTER            "},

        {CKRST_TSS_MIPMAPLODBIAS         , "CKRST_TSS_MIPMAPLODBIAS        "},
        {CKRST_TSS_MAXMIPMLEVEL          , "CKRST_TSS_MAXMIPMLEVEL         "},
        {CKRST_TSS_MAXANISOTROPY         , "CKRST_TSS_MAXANISOTROPY        "},
        {CKRST_TSS_BUMPENVLSCALE         , "CKRST_TSS_BUMPENVLSCALE        "},
        {CKRST_TSS_BUMPENVLOFFSET        , "CKRST_TSS_BUMPENVLOFFSET       "},

        {CKRST_TSS_TEXTURETRANSFORMFLAGS , "CKRST_TSS_TEXTURETRANSFORMFLAGS"},

        {CKRST_TSS_ADDRESW               , "CKRST_TSS_ADDRESW              "},
        {CKRST_TSS_COLORARG0             , "CKRST_TSS_COLORARG0            "},
        {CKRST_TSS_ALPHAARG0             , "CKRST_TSS_ALPHAARG0            "},
        {CKRST_TSS_RESULTARG0            , "CKRST_TSS_RESULTARG0           "},

        {CKRST_TSS_TEXTUREMAPBLEND       , "CKRST_TSS_TEXTUREMAPBLEND      "},
        {CKRST_TSS_STAGEBLEND            , "CKRST_TSS_STAGEBLEND           "},

        {CKRST_TSS_MAXSTATE              , "CKRST_TSS_MAXSTATE             "}};
    if (enummap.find(tsty) != enummap.end())
        return enummap.find(tsty)->second.c_str();
    return "";
}