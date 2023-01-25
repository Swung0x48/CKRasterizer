#include "CKGLRasterizerCommon.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <regex>

static HMODULE self_module = nullptr;

static HMODULE get_self_module()
{
    if (!self_module)
    {
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (char*)&self_module, &self_module);
    }
    return self_module;
}

unsigned int get_resource_size(const char* type, const char* name)
{
    HRSRC rsc = FindResourceA(get_self_module(), name, type);
    if (!rsc) return 0;
    return SizeofResource(get_self_module(), rsc);
}

void* get_resource_data(const char* type, const char* name)
{
    HRSRC rsc = FindResourceA(get_self_module(), name, type);
    if (!rsc) return nullptr;
    HGLOBAL hrdat = LoadResource(get_self_module(), rsc);
    if (!hrdat) return nullptr;
    return LockResource(hrdat);
}

std::string load_resource(const char* type, const char* name)
{
    size_t sz = get_resource_size(type, name);
    if (!sz) return std::string();
    return std::string((char*)get_resource_data(type, name), sz);
}

std::string load_resource_or_file(const std::string& path)
{
    const std::regex builtin_re("BUILTIN/(.*)/(.*)");
    std::smatch m;
    if (std::regex_match(path, m, builtin_re))
    {
        return load_resource(m[1].str().c_str(), m[2].str().c_str());
    }
    std::wstring modpath(1024, L'\0');
    GetModuleFileNameW(get_self_module(), modpath.data(), 1024);
    std::filesystem::path p(modpath);
    p = p.parent_path();
    p /= std::filesystem::path(path);
    if (!std::filesystem::is_regular_file(p))
        return std::string();
    auto sz = std::filesystem::file_size(p);
    std::fstream fst(p);
    std::string ret(sz, '\0');
    fst.read(ret.data(), sz);
    return ret;
}