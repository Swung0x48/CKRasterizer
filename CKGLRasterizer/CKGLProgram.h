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
    std::unordered_map<int, CKGLUniformValue*> pending_uniforms;
    int get_uniform_location(const std::string &name);
    struct UniformBlockInfo
    {
        uint32_t binding_point;
        uint32_t current_buffer;
        std::vector<GLuint> ubos;
    };
    std::unordered_map<std::string, UniformBlockInfo> ubs;
public:
    CKGLProgram(const std::string &vshsrc, const std::string &fshsrc);
    ~CKGLProgram();

    bool validate();
    void use();

    void define_uniform_block(const std::string &name, uint32_t nbuffers, int block_size, void *initial_data);
    void update_uniform_block(const std::string &name, int start_offset, int data_size, void *data);

    void stage_uniform(const std::string &name, CKGLUniformValue *val);
    void send_uniform();
};

#endif