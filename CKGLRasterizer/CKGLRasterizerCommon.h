#ifndef CKGLRASTERIZERCOMMON_H
#define CKGLRASTERIZERCOMMON_H

#include "tracy/Tracy.hpp"
#include "tracy/TracyOpenGL.hpp"

#define GLZoneName(x) (#x " @ " __FUNCTION__)

#define GLCall(x) {GLClearError(); \
    {TracyGpuZone(GLZoneName(x));\
    x;}\
    GLLogCall(#x, __FILE__, __LINE__);}

bool GLLogCall(const char* function, const char* file, int line);

void GLClearError();

#endif