#include "CKGLUniformValue.h"

#include <cstdlib>
#include <cstring>
#include <cassert>
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

UniformType CKGLUniformValue::type_from_string(const std::string &str)
{
    if (str == "f32") return UniformType::f32;
    if (str == "f32v2") return UniformType::f32v2;
    if (str == "f32v3") return UniformType::f32v3;
    if (str == "f32v4") return UniformType::f32v4;
    if (str == "u32") return UniformType::u32;
    if (str == "i32") return UniformType::i32;
    if (str == "f32m4") return UniformType::f32m4;
    return UniformType::invalid;
}

CKGLUniformValue* CKGLUniformValue::from_string(const std::string &str, UniformType type)
{
    std::stringstream ss(str);
    switch (type)
    {
        case UniformType::f32:
        {
            float f;
            ss >> f;
            return CKGLUniformValue::make_f32(f);
        }
        case UniformType::f32v2:
        {
            float f[2];
            ss >> f[0] >> f[1];
            return CKGLUniformValue::make_f32v2v(1, f, true);
        }
        case UniformType::f32v3:
        {
            float f[3];
            ss >> f[0] >> f[1] >> f[2];
            return CKGLUniformValue::make_f32v3v(1, f, true);
        }
        case UniformType::f32v4:
        {
            float f[4];
            ss >> f[0] >> f[1] >> f[2] >> f[3];
            return CKGLUniformValue::make_f32v4v(1, f, true);
        }
        case UniformType::i32:
        {
            int32_t i;
            ss >> i;
            return CKGLUniformValue::make_i32(i);
        }
        case UniformType::u32:
        {
            uint32_t u;
            ss >> u;
            return CKGLUniformValue::make_u32(u);
        }
        case UniformType::f32m4:
        {
            float f[16];
            for (int i = 0; i < 16; ++i) ss >> f[i];
            return CKGLUniformValue::make_f32mat4(1, f, false, true);
        }
        default:
            return new CKGLUniformValue();
    }
}