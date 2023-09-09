#ifndef CKVKRASTERIZERCOMMON_H
#define CKVKRASTERIZERCOMMON_H

#include <string>

#if TRACY_ENABLE
    #include "tracy/Tracy.hpp"
#else
    #define ZoneScopedN(x) ((void)0)
    #define FrameMark ((void)0)
#endif

class CKContext;
extern CKContext *rst_ckctx;

unsigned int get_resource_size(const char* type, const char* name);
void* get_resource_data(const char* type, const char* name);
std::string load_resource(const char* type, const char* name);
std::string load_resource_or_file(const std::string& path);

#endif
