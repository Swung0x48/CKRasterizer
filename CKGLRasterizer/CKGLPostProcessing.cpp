#include "CKGLPostProcessing.h"
#include "CKGLRasterizerCommon.h"

#include <CKInputManager.h>
#include <CKContext.h>
#include <CKEnums.h>

GLint CKGLPostProcessingStage::get_uniform_location(const std::string &name)
{
    if (uniform_loc.find(name) != uniform_loc.end())
        return uniform_loc[name];
    uniform_loc[name] = glGetUniformLocation(program, name.c_str());
    return uniform_loc[name];
}

CKGLPostProcessingStage::CKGLPostProcessingStage(const std::string &_fshsrc) :
    fshsrc(_fshsrc), program(0), fbo(0), tex{0, 0, 0}
{
    GLuint pvsh = glCreateShader(GL_VERTEX_SHADER);
    int l = get_resource_size("CKGLRPP_VERT_SHDR", (char*)1);
    const char* vshsrc = (char*)get_resource_data("CKGLRPP_VERT_SHDR", (char*)1);
    glShaderSource(pvsh, 1, &vshsrc, &l);
    glCompileShader(pvsh);
    glGetShaderiv(pvsh, GL_COMPILE_STATUS, &l);
    if (!l)
    {
        glGetShaderiv(pvsh, GL_INFO_LOG_LENGTH, &l);
        std::string info(l, '\0');
        glGetShaderInfoLog(pvsh, l, NULL, info.data());
        fprintf(stderr, "post processing stage %p vertex shader error: %s\n", this, info.c_str());
        glDeleteShader(pvsh);
        pvsh = 0;
    }
    GLuint pfsh = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fshsrcc = fshsrc.c_str();
    l = fshsrc.length();
    glShaderSource(pfsh, 1, &fshsrcc, &l);
    glCompileShader(pfsh);
    glGetShaderiv(pfsh, GL_COMPILE_STATUS, &l);
    if (!l)
    {
        glGetShaderiv(pvsh, GL_INFO_LOG_LENGTH, &l);
        std::string info(l, '\0');
        glGetShaderInfoLog(pfsh, l, NULL, info.data());
        fprintf(stderr, "post processing stage %p fragment shader error: %s\n", this, info.c_str());
        glDeleteShader(pvsh);
        glDeleteShader(pfsh);
        pvsh = 0;
        pfsh = 0;
    }
    if (pvsh && pfsh)
    {
        program = glCreateProgram();
        glAttachShader(program, pvsh);
        glAttachShader(program, pfsh);
        glBindFragDataLocation(program, 0, "color");
        glBindFragDataLocation(program, 1, "norpth");
        glLinkProgram(program);
        //glValidateProgram(program);
        //mark for deletion... won't actually delete until program is deleted
        glDeleteShader(pvsh);
        glDeleteShader(pfsh);
        glGenFramebuffers(1, &fbo);
        glGenTextures(3, tex);
    }
}

CKGLPostProcessingStage::~CKGLPostProcessingStage()
{
    glDeleteTextures(3, tex);
    glDeleteFramebuffers(1, &fbo);
    glDeleteProgram(program);
}

void CKGLPostProcessingStage::setup_fbo(bool has_depth, bool has_normal, int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glBindTexture(GL_TEXTURE_2D, tex[COLOR]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[COLOR], 0);

    if (has_normal || has_depth)
    {
        glBindTexture(GL_TEXTURE_2D, tex[NORPTH]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex[NORPTH], 0);
        GLenum db[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glNamedFramebufferDrawBuffers(fbo, 2, db);
    }

    if (has_depth)
    {
        glBindTexture(GL_TEXTURE_2D, tex[DEPTH]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex[DEPTH], 0);
    }
}

bool CKGLPostProcessingStage::valid()
{
    return program && glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void CKGLPostProcessingStage::set_as_target()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void CKGLPostProcessingStage::send_uniform(CKGLPostProcessingPipeline *pipeline)
{
    if (~get_uniform_location("color_in"))
        glUniform1i(get_uniform_location("color_in"), COLOR);
    if (~get_uniform_location("norpth_in"))
        glUniform1i(get_uniform_location("norpth_in"), NORPTH);
    float v[2];
    pipeline->get_screen_size(v);
    if (~get_uniform_location("screen_size"))
        glUniform2fv(get_uniform_location("screen_size"), 1, v);
    pipeline->get_mouse_position(v);
    if (~get_uniform_location("mouse_pos"))
        glUniform2fv(get_uniform_location("mouse_pos"), 1, v);
    if (~get_uniform_location("time"))
        glUniform1f(get_uniform_location("time"), pipeline->time_since_startup());
    if (~get_uniform_location("frame_time"))
        glUniform1f(get_uniform_location("frame_time"), pipeline->time_between_frames());
}

void CKGLPostProcessingStage::draw(CKGLPostProcessingPipeline *pipeline)
{
    glUseProgram(program);
    send_uniform(pipeline);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[COLOR]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex[NORPTH]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex[DEPTH]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

CKGLPostProcessingPipeline::CKGLPostProcessingPipeline() :
    scrwidth(0), scrheight(0), post_dummy_vao(0), mousepos{0, 0}
{
    glGenVertexArrays(1, &post_dummy_vao);
    inputmgr = static_cast<CKInputManager*>(rst_ckctx->GetManagerByGuid(CKGUID(INPUT_MANAGER_GUID1)));
    startup = std::chrono::steady_clock::now();
    lastframe = std::chrono::steady_clock::now();
}

CKGLPostProcessingPipeline::~CKGLPostProcessingPipeline()
{
    glDeleteVertexArrays(1, &post_dummy_vao);
    for (auto stage : stages)
        delete stage;
}

bool CKGLPostProcessingPipeline::setup_fbo(bool has_depth, bool has_normal, int width, int height)
{
    scrwidth = width;
    scrheight = height;
    bool ret = true;
    for (auto stage : stages)
    {
        stage->setup_fbo(has_depth, has_normal, width, height);
        ret &= stage->valid();
    }
    return ret;

}

void CKGLPostProcessingPipeline::set_as_target()
{
    if (!stages.empty())
        stages.front()->set_as_target();
}

void CKGLPostProcessingPipeline::draw()
{
    glBindVertexArray(post_dummy_vao);
    for (size_t i = 0; i < stages.size(); ++i)
    {
        if (i + 1 < stages.size())
            stages[i + 1]->set_as_target();
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        stages[i]->draw(this);
    }
}

void CKGLPostProcessingPipeline::add_stage(CKGLPostProcessingStage *stage)
{
    stages.push_back(stage);
}

void CKGLPostProcessingPipeline::clear_stages()
{
    for (auto stage : stages)
        delete stage;
    stages.clear();
}

void CKGLPostProcessingPipeline::get_mouse_position(float *pos)
{
    pos[0] = mousepos[0];
    pos[1] = mousepos[1];
}

void CKGLPostProcessingPipeline::get_screen_size(float *size)
{
    size[0] = scrwidth;
    size[1] = scrheight;
}

float CKGLPostProcessingPipeline::time_since_startup()
{
    return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - startup).count();
}

float CKGLPostProcessingPipeline::time_between_frames()
{
    return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - lastframe).count();
}

void CKGLPostProcessingPipeline::update_uniform_data()
{
    Vx2DVector mpos;
    inputmgr->GetMousePosition(mpos, FALSE);
    mousepos[0] = mpos.x;
    mousepos[1] = mpos.y;
    lastframe = std::chrono::steady_clock::now();
}
