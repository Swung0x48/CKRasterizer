#include "CKGLUniformValue.h"

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <charconv>
#include <memory>
#include <regex>
#include <sstream>

CKGLUniformValue* CKGLUniformValue::make_f32(float v)
{
    float *d = (float*)malloc(sizeof(float));
    assert(d != NULL);
    *d = v;
    return new CKGLUniformValue(UniformType::f32, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_f32v2(float v1, float v2)
{
    float *d = (float*)malloc(sizeof(float) * 2);
    assert(d != NULL);
    d[0] = v1;
    d[1] = v2;
    return new CKGLUniformValue(UniformType::f32v2, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_f32v3(float v1, float v2, float v3)
{
    float *d = (float*)malloc(sizeof(float) * 3);
    assert(d != NULL);
    d[0] = v1;
    d[1] = v2;
    d[2] = v3;
    return new CKGLUniformValue(UniformType::f32v3, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_f32v4(float v1, float v2, float v3, float v4)
{
    float *d = (float*)malloc(sizeof(float) * 4);
    assert(d != NULL);
    d[0] = v1;
    d[1] = v2;
    d[2] = v3;
    d[3] = v4;
    return new CKGLUniformValue(UniformType::f32v4, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_f32v(int count, float *v, bool copy) { return make_f32vnv(1, count, v, copy); }
CKGLUniformValue* CKGLUniformValue::make_f32v2v(int count, float *v, bool copy) { return make_f32vnv(2, count, v, copy); }
CKGLUniformValue* CKGLUniformValue::make_f32v3v(int count, float *v, bool copy) { return make_f32vnv(3, count, v, copy); }
CKGLUniformValue* CKGLUniformValue::make_f32v4v(int count, float *v, bool copy) { return make_f32vnv(4, count, v, copy); }

CKGLUniformValue* CKGLUniformValue::make_f32vnv(int n, int count, float *v, bool copy)
{
    UniformType t = UniformType::invalid;
    switch (n)
    {
        case 1: t = UniformType::f32; break;
        case 2: t = UniformType::f32v2; break;
        case 3: t = UniformType::f32v3; break;
        case 4: t = UniformType::f32v4; break;
    }
    if (t == UniformType::invalid)
        return new CKGLUniformValue();
    float *d = v;
    if (copy)
    {
        d = (float*)malloc(sizeof(float) * n *count);
        assert(d != NULL);
        memcpy(d, v, sizeof(float) * n * count);
    }
    return new CKGLUniformValue(t, copy, d, count, false);
}

CKGLUniformValue* CKGLUniformValue::make_i32(int32_t v)
{
    int32_t *d = (int32_t*)malloc(sizeof(int32_t));
    assert(d != NULL);
    *d = v;
    return new CKGLUniformValue(UniformType::i32, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_i32v(int count, int32_t *v, bool copy)
{
    int32_t *d = v;
    if (copy)
    {
        d = (int32_t*)malloc(sizeof(int32_t) * count);
        assert(d != NULL);
        memcpy(d, v, sizeof(int32_t) * count);
    }
    return new CKGLUniformValue(UniformType::i32, copy, d, count, false);
}

CKGLUniformValue* CKGLUniformValue::make_u32(uint32_t v)
{
    uint32_t *d = (uint32_t*)malloc(sizeof(uint32_t));
    assert(d != NULL);
    *d = v;
    return new CKGLUniformValue(UniformType::u32, true, d, 1, false);
}

CKGLUniformValue* CKGLUniformValue::make_u32v(int count, uint32_t *v, bool copy)
{
    uint32_t *d = v;
    if (copy)
    {
        d = (uint32_t*)malloc(sizeof(uint32_t) * count);
        assert(d != NULL);
        memcpy(d, v, sizeof(uint32_t) * count);
    }
    return new CKGLUniformValue(UniformType::u32, copy, d, count, false);
}

CKGLUniformValue* CKGLUniformValue::make_f32mat4(int count, float *v, bool transpose, bool copy)
{
    float *d = v;
    if (copy)
    {
        d = (float*)malloc(sizeof(float) * 4 * 4 * count);
        assert(d != NULL);
        memcpy(d, v, sizeof(float) * 4 * 4 * count);
    }
    return new CKGLUniformValue(UniformType::f32m4, copy, d, count, transpose);
}

CKGLUniformValue::CKGLUniformValue(UniformType _t, bool _pmanaged, void *_data, int _c, bool _tm) :
    ty(_t), pointer_managed(_pmanaged), dataptr(_data), cnt(_c), tmat(_tm) {}

CKGLUniformValue::CKGLUniformValue() :
    ty(UniformType::invalid), pointer_managed(false), dataptr(nullptr), cnt(0), tmat(false)
{}

CKGLUniformValue::~CKGLUniformValue()
{
    if (pointer_managed && dataptr)
        free(dataptr);
}

std::pair<UniformType, int> CKGLUniformValue::type_from_string(const std::string &str)
{
    UniformType ty = UniformType::invalid;
    std::regex re("([A-Za-z_0-9]*)(\\[([0-9]*)\\])?");
    std::smatch m;
    if (!std::regex_match(str, m, re))
        return std::make_pair(UniformType::invalid, 0);
    if (m[1] == "f32") ty = UniformType::f32;
    if (m[1] == "f32v2") ty = UniformType::f32v2;
    if (m[1] == "f32v3") ty = UniformType::f32v3;
    if (m[1] == "f32v4") ty = UniformType::f32v4;
    if (m[1] == "u32") ty = UniformType::u32;
    if (m[1] == "i32") ty = UniformType::i32;
    if (m[1] == "f32m4") ty = UniformType::f32m4;
    int c = 1;
    if (m.str(3).length())
    {
        std::string sm = m.str(3);
        std::from_chars(sm.c_str(), sm.c_str() + sm.length(), c);
    }
    return std::make_pair(ty, c);
}

CKGLUniformValue* CKGLUniformValue::from_string(const std::string &str, std::pair<UniformType, int> type)
{
    std::stringstream ss(str);
    auto &[ty, c] = type;
    auto read = [&ss]<typename T>(int count) -> T*
    {
        T *d = new T[count]();
        for (int i = 0; i < count; ++ i)
            ss >> d[i];
        return d;
    };
    switch (ty)
    {
        case UniformType::f32:
        {
            std::unique_ptr<float[]> f(read.template operator()<float>(c));
            return CKGLUniformValue::make_f32v(c, f.get(), true);
        }
        case UniformType::f32v2:
        {
            std::unique_ptr<float[]> f(read.template operator()<float>(c * 2));
            return CKGLUniformValue::make_f32v2v(c, f.get(), true);
        }
        case UniformType::f32v3:
        {
            std::unique_ptr<float[]> f(read.template operator()<float>(c * 3));
            return CKGLUniformValue::make_f32v3v(c, f.get(), true);
        }
        case UniformType::f32v4:
        {
            std::unique_ptr<float[]> f(read.template operator()<float>(c * 4));
            return CKGLUniformValue::make_f32v4v(c, f.get(), true);
        }
        case UniformType::i32:
        {
            std::unique_ptr<int32_t[]> f(read.template operator()<int32_t>(c));
            return CKGLUniformValue::make_i32v(c, f.get(), true);
        }
        case UniformType::u32:
        {
            std::unique_ptr<uint32_t[]> f(read.template operator()<uint32_t>(c));
            return CKGLUniformValue::make_u32v(c, f.get(), true);
        }
        case UniformType::f32m4:
        {
            std::unique_ptr<float[]> f(read.template operator()<float>(c * 16));
            return CKGLUniformValue::make_f32mat4(c, f.get(), false, true);
        }
        default:
            return new CKGLUniformValue();
    }
}