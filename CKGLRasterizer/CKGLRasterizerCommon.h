#ifndef CKGLRASTERIZERCOMMON_H
#define CKGLRASTERIZERCOMMON_H

#include "tracy/Tracy.hpp"
#include "tracy/TracyOpenGL.hpp"

#define GLZoneName(x) (#x " @ " __FUNCTION__)

class CKContext;
extern CKContext *rst_ckctx;

unsigned int get_resource_size(const char* type, const char* name);
void* get_resource_data(const char* type, const char* name);

#endif