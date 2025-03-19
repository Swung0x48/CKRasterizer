#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "CKRasterizer.h"
#include "XBitArray.h"
#include <Windows.h>

#include <GL/glew.h>
#include <gl/GL.h>
#include <gl/wglext.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CKGLRasterizerCommon.h"

#define LSW_SPECULAR_ENABLED 0x0001
#define LSW_VRTCOLOR_ENABLED 0x0004
#define LSW_SPCL_OVERR_FORCE 0x0008
#define LSW_SPCL_OVERR_ONLY  0x0010

//per-texture vertex properties
#define TVP_TC_CSNORM     0x01000000 //use camera space normal as input tex-coords
#define TVP_TC_CSVECP     0x02000000 //use camera space position ......
#define TVP_TC_CSREFV     0x04000000 //use camera space reflect vector ......
#define TVP_TC_TRANSF     0x08000000 //tex-coords should be transformed by its matrix
#define TVP_TC_PROJECTED  0x10000000 //tex-coords should be projected

#define MAX_ACTIVE_LIGHTS 16

#define USE_FBO_AND_POSTPROCESSING 1

LRESULT WINAPI GL_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

class CKGLRasterizerContext;

class CKGLRasterizer : public CKRasterizer
{
public:
    CKGLRasterizer(void);
    virtual ~CKGLRasterizer(void);

    virtual XBOOL Start(WIN_HANDLE AppWnd);
    virtual void Close(void);

public:
    XBOOL m_Init;
    WNDCLASSEXA m_WndClass;
};


class CKGLRasterizerDriver: public CKRasterizerDriver
{
public:
    CKGLRasterizerDriver(CKGLRasterizer *rst);
    virtual ~CKGLRasterizerDriver();
    virtual CKRasterizerContext *CreateContext();

    CKBOOL InitializeCaps();

public:
    CKBOOL m_Inited;
    UINT m_AdapterIndex;
    DISPLAY_DEVICEA m_Adapter;
};

struct pair_hash
{
    template<class S, class T>
    std::size_t operator() (const std::pair<S, T> &p) const
    { return std::hash<S>()(p.first) ^ std::hash<T>()(p.second); }
};

class CKGLVertexFormat;
struct CKGLVertexBuffer;
struct CKGLIndexBuffer;
class CKGLProgram;
class CKGLPostProcessingPipeline;

struct CKGLMaterialUniform
{
    VxColor ambi;
    VxColor diff;
    VxColor spcl;
    float spcl_strength;
    CKDWORD padding[3];
    VxColor emis;

    CKGLMaterialUniform(CKMaterialData md) :
        ambi(md.Ambient), diff(md.Diffuse),
        spcl(md.Specular), spcl_strength(md.SpecularPower),
        emis(md.Emissive), padding{0, 0, 0} {}
};

struct CKGLLightUniform
{
    CKDWORD type;
    CKDWORD padding[3];
    VxColor ambi;
    VxColor diff;
    VxColor spcl;
    VxVector4 dir;
    VxVector4 pos;
    float range;
    float falloff;
    float theta;
    float phi;
    float a0;
    float a1;
    float a2;
    float unused;

    CKGLLightUniform() {}
    CKGLLightUniform(CKLightData ld) :
        type(ld.Type), ambi(ld.Ambient), diff(ld.Diffuse), spcl(ld.Specular),
        dir(VxVector4(ld.Direction.x, ld.Direction.y, ld.Direction.z, 0.)),
        pos(VxVector4(ld.Position.x, ld.Position.y, ld.Position.z, 1.)),
        range(ld.Range), falloff(ld.Falloff), theta(ld.InnerSpotCone),
        phi(ld.OuterSpotCone), a0(ld.Attenuation0), a1(ld.Attenuation1),
        a2(ld.Attenuation2), padding{0, 0, 0}, unused(0) {}
};

enum TexOp // for CKGLTexCombinatorUniform::op
{
    disable   = 0x1,
    select1   = 0x2,
    select2   = 0x3,
    modulate  = 0x4,
    modulate2 = 0x5,
    modulate4 = 0x6,
    add       = 0x7,
    addbip    = 0x8,
    addbip2   = 0x9,
    subtract  = 0xa,
    addsmooth = 0xb,
    mixtexalp = 0xd,
    top_max   = ~0UL
};

enum TexArg // for CKGLTexCombinatorUniform::cargs / aargs
{
    diffuse  = 0x0,
    current  = 0x1,
    texture  = 0x2,
    tfactor  = 0x3,
    specular = 0x4,
    temp     = 0x5,
    constant = 0x6,
    flag_cpl = 0x10,
    flag_alp = 0x20,
    tar_max  = ~0UL
};

struct CKGLTexCombinatorUniform
{
    CKDWORD op;         //bit 0-3: color op, bit 4-7: alpha op, bit 31: dest
    CKDWORD cargs;      //bit 0-7: arg1, bit 8-15: arg2, bit 16-23: arg3
    CKDWORD aargs;      //ditto
    CKDWORD constant;

    void set_color_op(TexOp o) { op &= ~0x0fU; op |= o; }
    void set_alpha_op(TexOp o) { op &= ~0xf0U; op |= (o << 4); }
    void set_color_arg1(TexArg a) { cargs &= ~0x00ffU; cargs |= a; }
    void set_color_arg2(TexArg a) { cargs &= ~0xff00U; cargs |= (a << 8); }
    void set_alpha_arg1(TexArg a) { aargs &= ~0x00ffU; aargs |= a; }
    void set_alpha_arg2(TexArg a) { aargs &= ~0xff00U; aargs |= (a << 8); }

    TexArg dest() { return op & (1UL << 31) ? TexArg::temp : TexArg::current; }

    static CKGLTexCombinatorUniform make(
        TexOp cop, TexArg ca1, TexArg ca2, TexArg ca3,
        TexOp aop, TexArg aa1, TexArg aa2, TexArg aa3,
        TexArg dest, CKDWORD constant) {
        CKDWORD op = cop | (aop << 4) | ((dest == TexArg::temp) << 31);
        CKDWORD cargs = ca1 | (ca2 << 8) | (ca3 << 16);
        CKDWORD aargs = aa1 | (aa2 << 8) | (aa3 << 16);
        return CKGLTexCombinatorUniform{op, cargs, aargs, constant};
    }
};

enum CKGLMatrices
{
    vp2d = 0,
    world,
    view,
    proj,
    tiworld,
    tiworldview
};

struct CKGLFixedProgramState
{
    bool vertex_has_color;
    bool vertex_is_transformed;
    bool lighting_enabled;
    bool has_multi_texture;

    bool operator ==(const CKGLFixedProgramState &other) const
    {
        return vertex_has_color == other.vertex_has_color &&
               vertex_is_transformed == other.vertex_is_transformed &&
               lighting_enabled == other.lighting_enabled &&
               has_multi_texture == other.has_multi_texture;
    }
};

template <> struct std::hash<CKGLFixedProgramState>
{
    std::size_t operator()(const CKGLFixedProgramState &s) const
    {
        return std::hash<std::vector<bool>>{}({s.vertex_has_color, s.vertex_is_transformed, s.lighting_enabled, s.has_multi_texture});
    }
};

class CKGLRasterizerContext : public CKRasterizerContext
{
public:
    //--- Construction/destruction
    CKGLRasterizerContext();
    virtual ~CKGLRasterizerContext();

    //--- Creation
    virtual CKBOOL Create(WIN_HANDLE Window, int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, int Bpp = -1,
                          CKBOOL Fullscreen = 0, int RefreshRate = 0, int Zbpp = -1, int StencilBpp = -1);
    //---
    virtual CKBOOL Resize(int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, CKDWORD Flags = 0);
    virtual CKBOOL Clear(CKDWORD Flags = CKRST_CTXCLEAR_ALL, CKDWORD Ccol = 0, float Z = 1.0f, CKDWORD Stencil = 0,
                         int RectCount = 0, CKRECT *rects = NULL);
    virtual CKBOOL BackToFront(CKBOOL vsync);

    //--- Scene
    virtual CKBOOL BeginScene();
    virtual CKBOOL EndScene();

    //--- Lighting & Material States
    virtual CKBOOL SetLight(CKDWORD Light, CKLightData *data);
    virtual CKBOOL EnableLight(CKDWORD Light, CKBOOL Enable);
    virtual CKBOOL SetMaterial(CKMaterialData *mat);

    //--- Viewport State
    virtual CKBOOL SetViewport(CKViewportData *data);

    //--- Transform Matrix
    virtual CKBOOL SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat);

    //--- Render states
    virtual CKBOOL SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);
    virtual CKBOOL GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value);

    //--- Texture States
    virtual CKBOOL SetTexture(CKDWORD Texture, int Stage = 0);
    virtual CKBOOL SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value);

    //--- Vertex & Pixel shaders
    virtual CKBOOL SetVertexShader(CKDWORD VShaderIndex);
    virtual CKBOOL SetPixelShader(CKDWORD PShaderIndex);
    virtual CKBOOL SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount);
    virtual CKBOOL SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount);

    //--- Drawing
    virtual CKBOOL DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount, VxDrawPrimitiveData *data);
    virtual CKBOOL DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex, CKDWORD VertexCount,
                                   CKWORD *indices = NULL, int indexcount = NULL);
    virtual CKBOOL DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                     CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount);

    //--- Creation of Textures, Sprites and Vertex Buffer
    CKBOOL CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat) override;

    //--- Vertex Buffers
    virtual void *LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
                                   CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
    virtual CKBOOL UnlockVertexBuffer(CKDWORD VB);

    //--- Textures
    virtual CKBOOL LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel = -1);
    virtual CKBOOL CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS);
    //-- Sets the rendering to occur on a texture (reset the texture format to match )
    virtual CKBOOL SetTargetTexture(CKDWORD TextureObject, int Width = 0, int Height = 0, CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS);

    //--- Sprites
    virtual CKBOOL LoadSprite(CKDWORD Sprite, const VxImageDescEx &SurfDesc);
    virtual CKBOOL DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst);

    //--- Utils
    virtual int CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc);
    virtual int CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc);

    virtual CKBOOL SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation);
    virtual CKBOOL GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation);

    virtual void *LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount,
                                  CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
    virtual CKBOOL UnlockIndexBuffer(CKDWORD IB);

    static unsigned get_vertex_attrib_location(CKDWORD component);
    CKGLVertexFormat* get_vertex_format(CKRST_VERTEXFORMAT vf);

    std::string preprocess_shader_source(const std::string &src, const CKGLFixedProgramState &st);
    void setup_shader_program(CKGLProgram *p);
    void init_program_uniform(CKGLProgram *p);

    void set_position_transformed(bool transformed);
    void set_vertex_has_color(bool color);
    void set_num_textures(CKDWORD ntex);
    void switch_fprs(const CKGLFixedProgramState &newst);
    void select_framebuffer(bool twod);
    void set_title_status(const char* fmt, ...);

    CKBOOL _SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);

    // debugging
    void toggle_console(int t = 0);
    void set_step_mode(int mode);
    void step_mode_wait();
    void toggle_batch_status();
    void toggle_specular_handling();
    void toggle_2d_rendering();
    void cycle_post_processing_shader();
    void toggle_post_processing();

protected:
    CKBOOL InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKGLVertexBuffer * vbo, CKDWORD vbase, CKDWORD vcnt, WORD* idx, GLuint icnt, bool vbbound = false);
    //--- Objects creation
    CKBOOL CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat);
    CKBOOL CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat);
    CKBOOL CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat);
    CKBOOL CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat);
    CKBOOL CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat);
    CKBOOL CreateSpriteNPOT(CKDWORD Sprite, CKSpriteDesc *DesiredFormat);

public:
    CKGLRasterizer *m_Owner;
private:
    HDC m_DC = NULL;
    CKBOOL m_Vsync = FALSE;
    std::string m_vshsrc;
    std::string m_fshsrc;
    std::unordered_map<CKGLFixedProgramState, CKGLProgram*> m_programs;
    CKGLProgram *m_curprgm = nullptr;
    CKGLFixedProgramState m_current_fprs;
    std::vector<std::pair<CKDWORD, CKLightData>> m_lights;
    CKGLLightUniform m_lights_data[MAX_ACTIVE_LIGHTS];
    VxMatrix m_matrices[6];
    VxMatrix m_textrmtx[CKRST_MAX_STAGES];
    VxVector m_viewpos;
    uint32_t m_lighting_flags;
    std::string m_orig_title;
    CKDWORD m_renderst[VXRENDERSTATE_MAXSTATE];
    uint32_t m_alpha_test_flags;
    uint32_t m_fog_flags;
    float m_fog_parameters[3];
    std::unordered_map<std::pair<CKDWORD, CKDWORD>, CKGLVertexBuffer*, pair_hash> m_dynvbo;
    DWORD m_direct_draw_counter = 0;
    std::unordered_map<CKDWORD, CKGLVertexFormat*> m_vertfmts;
    CKDWORD m_current_vf = ~0U;
    std::unordered_map<CKDWORD, CKGLIndexBuffer*> m_dynibo;
    DWORD m_noibo_draw_counter = 0;
    uint32_t m_ntex = 0;
    CKDWORD m_cur_ts = ~0U;
    CKDWORD m_ts_texture[CKRST_MAX_STAGES] = {~0U};
    CKGLTexCombinatorUniform m_texcombo[CKRST_MAX_STAGES];
    CKDWORD m_tex_vp[CKRST_MAX_STAGES] = {0};
    uint32_t m_null_texture_mask;
    bool m_use_post_processing = false;
    bool m_use_pp_switch_pending = false;
    CKGLPostProcessingPipeline *m_3dpp = nullptr;
    CKGLPostProcessingPipeline *m_2dpp = nullptr;
    int m_target_mode = 0; // 0=screen, 1=3d, -1=2d
    bool m_2d_enabled = true;
    int m_max_ppsh_id = 0;
    int m_current_ppsh_id = 0;
    bool m_ppsh_switch_pending = false;
    GLenum m_blend_src = GL_ONE;
    GLenum m_blend_dst = GL_ZERO;

    //debugging
    int m_step_mode = 0;
    int m_batch_status = 0;
};
