#pragma once
#include "CKRasterizer.h"
#include "XBitArray.h"
#include <Windows.h>

#include "GL/glew.h"

#include "GLFW/glfw3.h"
#include <gl/GL.h>
//#include "GL/glext.h"
#include "gl/wglext.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

#include <string>
#include <unordered_map>
#include <vector>

bool GLLogCall(const char* function, const char* file, int line);

void GLClearError();

#define GLCall(x) {GLClearError();\
    x;\
    GLLogCall(#x, __FILE__, __LINE__);}

class CKGLRasterizerContext;

typedef struct GLVertexBufferElement {
    GLenum type = GL_NONE;
    unsigned int count = 0;
    GLboolean normalized = GL_FALSE;
    CKDWORD usage = 0;

    static unsigned int GetSizeOfType(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT: return 4;
            case GL_UNSIGNED_INT: return 4;
            case GL_UNSIGNED_BYTE: return 1;
            default: break;
        }
        assert(false);
        return 0;
    }
} GLVertexBufferElement;

class GLVertexBufferLayout
{
public:
    GLVertexBufferLayout()
    {
    }
    template<typename T>
    void push(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        static_assert(sizeof(T) == 0, "pushing this type haven't been implemented.");
    }

    template<>
    void push<GLfloat>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_[index] = { GL_FLOAT, count, normalized, usage};
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_FLOAT) * count;
    }

    template<>
    void push<GLuint>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_[index] = { GL_UNSIGNED_INT, count, normalized, usage };
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_UNSIGNED_INT) * count;
    }

    template<>
    void push<GLubyte>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_[index] = { GL_UNSIGNED_BYTE, count, normalized, usage };
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_UNSIGNED_BYTE) * count;
    }
    inline const std::vector<GLVertexBufferElement>& GetElements() const { return elements_; }
    inline unsigned int GetStride() const { return stride_; }
    static GLVertexBufferLayout GetLayoutFromFVF(CKDWORD fvf)
    {
        GLVertexBufferLayout layout;
        layout.elements_.resize(std::popcount(fvf));
        if (fvf & CKRST_VF_POSITION)
            layout.push<GLfloat>(0, 3, GL_FALSE, CKRST_VF_POSITION);

        if (fvf & CKRST_VF_RASTERPOS)
            layout.push<GLfloat>(0, 4, GL_FALSE, CKRST_VF_RASTERPOS);

        if (fvf & CKRST_VF_NORMAL)
            layout.push<GLfloat>(1, 3, GL_FALSE, CKRST_VF_NORMAL);

        if (fvf & CKRST_VF_DIFFUSE)
            layout.push<GLubyte>(2, 4, GL_TRUE, CKRST_VF_DIFFUSE);

        if (fvf & CKRST_VF_SPECULAR)
            layout.push<GLubyte>(3, 4, GL_TRUE, CKRST_VF_SPECULAR);

        if (fvf & CKRST_VF_TEX1)
            layout.push<GLfloat>(4, 2, GL_FALSE, CKRST_VF_TEX1);

        if (fvf & CKRST_VF_TEX2)
        {
            layout.push<GLfloat>(4, 2, GL_FALSE, CKRST_VF_TEX1);
            layout.push<GLfloat>(5, 2, GL_FALSE, CKRST_VF_TEX2);
        }

        return layout;
    }
private:
    std::vector<GLVertexBufferElement> elements_;
    unsigned int stride_;
};

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
	WNDCLASSEXA m_WndClass;
};

typedef struct CKGLTextureDesc : public CKTextureDesc
{
public:
    GLuint tex;
    GLenum glfmt;
    GLenum gltyp;
public:
    CKGLTextureDesc() { tex = 0; glfmt = gltyp = GL_INVALID_ENUM; }
    CKGLTextureDesc(CKTextureDesc *texdesc);
    ~CKGLTextureDesc() { GLCall(glDeleteTextures(1, &tex)); }

    void Create();
    void Bind(CKGLRasterizerContext *ctx);
    void Load(void *data);
} CKGLTextureDesc;

typedef struct CKGLVertexBufferDesc : public CKVertexBufferDesc
{
public:
    GLuint GLBuffer;
    GLVertexBufferLayout GLLayout;
    GLuint GLVertexArray;
public:
    bool operator==(const CKVertexBufferDesc &) const;
    void Create();
    void Bind(CKGLRasterizerContext *ctx);
    void *Lock(CKDWORD offset, CKDWORD len, bool overwrite);
    void Unlock();
    explicit CKGLVertexBufferDesc(CKVertexBufferDesc* DesiredFormat);
    CKGLVertexBufferDesc() { GLBuffer = 0; }
    ~CKGLVertexBufferDesc() { glDeleteBuffers(1, &GLBuffer); }
} CKGLVertexBufferDesc;

typedef struct CKGLIndexBufferDesc : public CKIndexBufferDesc
{
public:
    GLuint GLBuffer;
public:
    bool operator==(const CKIndexBufferDesc &) const;
    void Create();
    void *Lock(CKDWORD offset, CKDWORD len, bool overwrite);
    void Unlock();
    void Bind();
    explicit CKGLIndexBufferDesc(CKIndexBufferDesc* DesiredFormat);
    CKGLIndexBufferDesc() { GLBuffer = 0; }
    ~CKGLIndexBufferDesc() { glDeleteBuffers(1, &GLBuffer); }
} CKGLIndexBufferDesc;

typedef struct CKGLVertexShaderDesc : public CKVertexShaderDesc
{
public:
    GLuint GLShader;
    CKGLRasterizerContext *Owner;
    XArray<BYTE> m_FunctionData;

public:
    bool operator==(const CKVertexShaderDesc &) const;
    CKDWORD Create(CKGLRasterizerContext *Ctx, CKVertexShaderDesc *Format);
    virtual ~CKGLVertexShaderDesc();
    CKGLVertexShaderDesc()
    {
        GLShader = 0;
        Owner = NULL;
    }
} CKGLVertexShaderDesc;

typedef struct CKGLPixelShaderDesc : public CKPixelShaderDesc
{
public:
    GLuint GLShader;
    CKGLRasterizerContext *Owner;
    XArray<BYTE> m_FunctionData;
public:
    bool operator==(const CKPixelShaderDesc &) const;
    CKDWORD Create(CKGLRasterizerContext *Ctx, CKPixelShaderDesc *Format);
    virtual ~CKGLPixelShaderDesc();
    CKGLPixelShaderDesc()
    {
        GLShader = 0;
        Owner = NULL;
    }
} CKGLPixelShaderDesc;


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
    virtual CKBOOL SetTargetTexture(CKDWORD TextureObject, int Width = 0, int Height = 0,
                                    CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS, CKBOOL GenerateMipMap = FALSE);

    //--- Sprites
    virtual CKBOOL DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst);

    //--- Utils
    virtual int CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc);
    virtual int CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc);
    
    virtual CKBOOL SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation);
    virtual CKBOOL GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation);

    virtual void *LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount,
                                  CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
    virtual CKBOOL UnlockIndexBuffer(CKDWORD IB);

    static unsigned get_shader_location(CKDWORD component);
    void set_position_transformed(bool transformed);

    CKBOOL _SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);

protected:
    BOOL SetUniformMatrix4fv(std::string name, GLsizei count, GLboolean transpose, const GLfloat *value);
    CKDWORD GetStaticIndexBuffer(CKDWORD Count, GLushort* IndexData);
    CKDWORD GetDynamicIndexBuffer(CKDWORD Count, GLushort* IndexData, CKDWORD Index);
    CKBOOL InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKGLVertexBufferDesc * vbo, CKDWORD vbase, CKDWORD vcnt, WORD* idx, GLuint icnt);
    //--- Objects creation
    CKBOOL CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat);
    CKBOOL CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat);
    CKBOOL CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat);
    CKBOOL CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat);
    CKBOOL CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat);

    //---- Cleanup
    void FlushCaches();
    void FlushNonManagedObjects();
    void ReleaseStateBlocks();
    void ReleaseBuffers();
    void ClearStreamCache();
    void ReleaseScreenBackup();
public:
    constexpr static CKDWORD INVALID_VALUE = 0xffffffff;
    CKGLRasterizer *m_Owner;
    HDC m_DC;
    CKGLIndexBufferDesc *m_IndexBuffer;
    CKDWORD m_CurrentVertexShader = INVALID_VALUE;
    CKDWORD m_CurrentPixelShader = INVALID_VALUE;
    CKDWORD m_CurrentProgram = INVALID_VALUE;
    CKDWORD m_CurrentVertexBuffer = INVALID_VALUE;
    CKDWORD m_CurrentIndexBuffer = INVALID_VALUE;
    std::unordered_map<std::string, GLint> m_UniformLocationCache;
};
