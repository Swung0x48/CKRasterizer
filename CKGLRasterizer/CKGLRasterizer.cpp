#include "CKGLRasterizer.h"
#include "CKGlSystemInfo.h"

CKGLRasterizer::CKGLRasterizer(void): m_Init(FALSE)
{
	
}

CKGLRasterizer::~CKGLRasterizer(void)
{
}

XBOOL CKGLRasterizer::Start(WIN_HANDLE AppWnd)
{
	if (m_Init)
		return TRUE;
	m_Init = TRUE;
	m_MainWindow = AppWnd;
	XBOOL result = CKGlSystemInfo::Init((HWND) AppWnd);
	if (!result)
		return FALSE;
	if (!CKGlSystemInfo::m_FoundHardware && !CKGlSystemInfo::m_FoundGeneric && !CKGlSystemInfo::m_FoundStereo)
		return FALSE;
	INSTANCE_HANDLE handle = VxGetModuleHandle("CKGlRasterizer.dll");
	char filename[260];
	VxGetModuleFileName(handle, filename, 260);
	CKPathSplitter splitter(filename);
	auto dir = splitter.GetDir();
	auto drive = splitter.GetDrive();
	CKPathMaker maker(splitter.GetDrive(), splitter.GetDir(), (char*)"CKGLRasterizer", (char*)"ini");
	LoadVideoCardFile(maker.GetFileName());
	if (CKGlSystemInfo::m_FoundHardware) {

	}
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
		32,                   // Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,                   // Number of bits for the depthbuffer
		8,                    // Number of bits for the stencilbuffer
		0,                    // Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	HDC deviceContext = GetDC((HWND) AppWnd);

	int newPfd;
	newPfd = ChoosePixelFormat(deviceContext, &pfd);
	SetPixelFormat(deviceContext, newPfd, &pfd);

	HGLRC glContext = wglCreateContext(deviceContext);
	wglMakeCurrent(deviceContext, glContext);

	GLenum glewErr = glewInit(); // Get Extension functions
	if (glewErr != GLEW_OK)
		return FALSE;
	MessageBoxA(0, (char*)glGetString(GL_VERSION), "GL_VERSION", 0);
	
	return glewErr == GLEW_OK;
}

void CKGLRasterizer::Close(void)
{
	wglMakeCurrent(NULL, NULL);
}
