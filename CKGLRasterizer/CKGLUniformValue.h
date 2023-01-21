#ifndef CKGLUNIFORMVALUE_H
#define CKGLUNIFORMVALUE_H
#include <cstdint>

enum UniformType
{
    f32,
    f32v2,
    f32v3,
    f32v4,
    i32,
    u32,
    f32m4,
    invalid
};

class CKGLUniformValue
{
private:
    UniformType ty;
    bool pointer_managed;
    void *dataptr;
    int cnt;
    bool tmat;
    CKGLUniformValue(UniformType _t, bool _pmanaged, void *_data, int _c, bool _tm);

public:
    CKGLUniformValue();
    ~CKGLUniformValue();

    UniformType type() const { return ty; }
    int count() const { return cnt; }
    void* data() const { return dataptr; }
    bool transposed() const { return tmat; }

    static CKGLUniformValue* make_f32(float v);
    static CKGLUniformValue* make_f32v2(float v1, float v2);
    static CKGLUniformValue* make_f32v3(float v1, float v2, float v3);
    static CKGLUniformValue* make_f32v4(float v1, float v2, float v3, float v4);
    static CKGLUniformValue* make_f32v(int count, float *v, bool copy = false);
    static CKGLUniformValue* make_f32v2v(int count, float *v, bool copy = false);
    static CKGLUniformValue* make_f32v3v(int count, float *v, bool copy = false);
    static CKGLUniformValue* make_f32v4v(int count, float *v, bool copy = false);
    static CKGLUniformValue* make_f32vnv(int n, int count, float *v, bool copy = false);
    static CKGLUniformValue* make_u32(uint32_t v);
    static CKGLUniformValue* make_u32v(int count, uint32_t *v, bool copy = false);
    static CKGLUniformValue* make_i32(int32_t v);
    static CKGLUniformValue* make_i32v(int count, int32_t *v, bool copy = false);
    static CKGLUniformValue* make_f32mat4(int count, float *v, bool transpose = false, bool copy = false);
};
#endif