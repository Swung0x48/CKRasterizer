#ifndef CKGLRASTERIZERCOMMON_H
#define CKGLRASTERIZERCOMMON_H

#define ENABLE_TRACY 0

#if ENABLE_TRACY
#include "tracy/Tracy.hpp"
#include "tracy/TracyOpenGL.hpp"
#else
#define TracyGpuZone(_)
#define TracyGpuContext
#define TracyPlot(_, __)
#define TracyGpuCollect
#define FrameMark
#define ZoneScopedN(_)
#endif

#define GLZoneName(x) (#x " @ " __FUNCTION__)

#define GLCall(x) {GLClearError(); \
    {TracyGpuZone(GLZoneName(x));\
    x;}\
    GLLogCall(#x, __FILE__, __LINE__);}

bool GLLogCall(const char* function, const char* file, int line);

void GLClearError();

#endif