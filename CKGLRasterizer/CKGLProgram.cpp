#include "CKGLProgram.h"
#include "CKGLRasterizerCommon.h"

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
    for (auto &ubi : ubs)
        if (!ubi.second.is_mirror)
            glDeleteBuffers(ubi.second.ubos.size(), ubi.second.ubos.data());
}

bool CKGLProgram::validate() { return program; }
void CKGLProgram::use() { glUseProgram(program); }

void CKGLProgram::define_uniform_block(const std::string &name, uint32_t nbuffers, int block_size, void *initial_data)
{
    std::vector<GLuint> ubos(nbuffers, 0);
    glCreateBuffers(nbuffers, ubos.data());
    int idx = glGetUniformBlockIndex(program, name.c_str());
    uint32_t bp = ubs.size();
    glUniformBlockBinding(program, idx, bp);
    for (auto &ubo : ubos)
        glNamedBufferStorage(ubo, block_size, initial_data, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, bp, ubos[0]);
    ubs.emplace(std::piecewise_construct,
                std::forward_as_tuple(name),
                std::forward_as_tuple(bp, 0, std::move(ubos), false));
}

void CKGLProgram::define_uniform_block_mirrored(const std::string &name, CKGLProgram *from, const std::string &from_name)
{
    if (from->ubs.find(from_name) == from->ubs.end())
        return;
    std::vector<GLuint> ubos = from->ubs[from_name].ubos;
    uint32_t nbuffers = ubos.size();
    int idx = glGetUniformBlockIndex(program, name.c_str());
    uint32_t bp = ubs.size();
    glUniformBlockBinding(program, idx, bp);
    glBindBufferBase(GL_UNIFORM_BUFFER, bp, ubos[0]);
    ubs.emplace(std::piecewise_construct,
                std::forward_as_tuple(name),
                std::forward_as_tuple(bp, 0, std::move(ubos), true));
}

void CKGLProgram::update_uniform_block(const std::string &name, int start_offset, int data_size, void *data)
{
    if (ubs.find(name) == ubs.end())
        return;
    auto &ubi = ubs[name];
    GLuint buffer = ubi.ubos[ubi.current_buffer];
    if (ubi.ubos.size() > 1)
    {
        ++ ubi.current_buffer;
        if (ubi.current_buffer >= ubi.ubos.size())
            ubi.current_buffer = 0;
        buffer = ubi.ubos[ubi.current_buffer];
        glBindBufferBase(GL_UNIFORM_BUFFER, ubi.binding_point, buffer);
    }
    // this is problematic... for now always update the entire buffer
    glNamedBufferSubData(buffer, start_offset, data_size, data);
}

void CKGLProgram::stage_uniform(const std::string &name, CKGLUniformValue *val)
{
    int loc = get_uniform_location(name);
    if (!~loc) return;
    if (pending_uniforms.find(loc) != pending_uniforms.end())
        delete pending_uniforms[loc];
    pending_uniforms[loc] = val;
}

void CKGLProgram::send_uniform()
{
    ZoneScopedN(__FUNCTION__);
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
