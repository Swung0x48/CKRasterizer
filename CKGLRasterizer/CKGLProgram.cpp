#include "CKGLProgram.h"

#include <vector>

int CKGLProgram::get_uniform_location(const std::string &name)
{
    if (uniform_location.find(name) == uniform_location.end())
        uniform_location[name] = glGetUniformLocation(program, name.c_str());
    return uniform_location[name];
}

CKGLProgram::CKGLProgram(const std::string &vshsrc, const std::string &fshsrc) : program(0)
{
    auto compile_shader = [](GLuint shader, const std::string &src) -> bool {
        const char *psrc = src.c_str();
        int l = src.length();
        glShaderSource(shader, 1, &psrc, &l);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &l);
        if (!l)
        {
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &l);
            std::string info(l, '\0');
            glGetShaderInfoLog(shader, l, NULL, info.data());
            fprintf(stderr, "shader compilation error: %s\n", info.c_str());
            return false;
        }
        return true;
    };
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    if (!compile_shader(vsh, vshsrc))
    {
        glDeleteShader(vsh);
    }
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compile_shader(fsh, fshsrc))
    {
        glDeleteShader(vsh);
        glDeleteShader(fsh);
        return;
    }
    program = glCreateProgram();
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);
    glLinkProgram(program);
    int l = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &l);
    if (!l)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &l);
        std::string info(l, '\0');
        glGetProgramInfoLog(program, l, NULL, info.data());
        fprintf(stderr, "shader program linkage error: %s\n", info.c_str());
        glDeleteProgram(program);
        program = 0;
    }
    //only deletes after they have been detached from the program
    glDeleteShader(vsh);
    glDeleteShader(fsh);
}

CKGLProgram::~CKGLProgram()
{
    if (program)
        glDeleteProgram(program);
    std::vector<uint32_t> ubos;
    for (auto &ubo : uniform_buffers)
        ubos.push_back(ubo);
    glDeleteBuffers(ubos.size(), ubos.data());
}

bool CKGLProgram::validate() { return program; }
void CKGLProgram::use() { glUseProgram(program); }

uint32_t CKGLProgram::define_uniform_block(const std::string &name, int block_size, void *initial_data)
{
    GLuint ubo = 0;
    glCreateBuffers(1, &ubo);
    int idx = glGetUniformBlockIndex(program, name.c_str());
    glUniformBlockBinding(program, idx, uniform_buffers.size());
    glNamedBufferStorage(ubo, block_size, initial_data, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniform_buffers.size(), ubo);
    uniform_buffers.insert(ubo);
    return ubo;
}

void CKGLProgram::update_uniform_block(uint32_t block, int start_offset, int data_size, void *data)
{
    glNamedBufferSubData(block, start_offset, data_size, data);
}

void CKGLProgram::stage_uniform(const std::string &name, CKGLUniformValue *val)
{
    int loc = get_uniform_location(name);
    if (!~loc) return;
    pending_uniforms[loc] = val;
}

void CKGLProgram::send_uniform()
{
    for (auto &up : pending_uniforms)
    {
        auto &[loc, u] = up;
        
        switch (u->type())
        {
            case UniformType::f32:
                glProgramUniform1fv(program, loc, u->count(), (GLfloat*)u->data());
                break;
            case UniformType::f32v2:
                glProgramUniform2fv(program, loc, u->count(), (GLfloat*)u->data());
                break;
            case UniformType::f32v3:
                glProgramUniform3fv(program, loc, u->count(), (GLfloat*)u->data());
                break;
            case UniformType::f32v4:
                glProgramUniform4fv(program, loc, u->count(), (GLfloat*)u->data());
                break;
            case UniformType::i32:
                glProgramUniform1iv(program, loc, u->count(), (GLint*)u->data());
                break;
            case UniformType::u32:
                glProgramUniform1uiv(program, loc, u->count(), (GLuint*)u->data());
                break;
            case UniformType::f32m4:
                glProgramUniformMatrix4fv(program, loc, u->count(), u->transposed(), (GLfloat*)u->data());
                break;
        }
    }
    for (auto &up : pending_uniforms)
        delete up.second;
    pending_uniforms.clear();
}
