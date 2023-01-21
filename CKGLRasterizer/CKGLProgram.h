#ifndef CKGLPROGRAM_H
#define CKGLPROGRAM_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <GL/glew.h>
#include <gl/GL.h>

#include "CKGLUniformValue.h"

class CKGLProgram
{
private:
    GLuint program;
    std::unordered_map<std::string, int> uniform_location;
    std::unordered_set<uint32_t> uniform_buffers;
    std::unordered_map<int, CKGLUniformValue*> pending_uniforms;
    int get_uniform_location(const std::string &name);
public:
    CKGLProgram(const std::string &vshsrc, const std::string &fshsrc);
    ~CKGLProgram();

    bool validate();
    void use();

    uint32_t define_uniform_block(const std::string &name, int block_size, void *initial_data);
    void update_uniform_block(uint32_t block, int start_offset, int data_size, void *data);

    void stage_uniform(const std::string &name, CKGLUniformValue *val);
    void send_uniform();
};

#endif