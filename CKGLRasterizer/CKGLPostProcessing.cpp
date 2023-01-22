#include "CKGLPostProcessing.h"
#include "CKGLRasterizerCommon.h"
#include "CKGLProgram.h"

#include <CKInputManager.h>
#include <CKContext.h>
#include <CKEnums.h>

CKGLPostProcessingStage::CKGLPostProcessingStage(const std::string &_fshsrc) :
    fshsrc(_fshsrc), program(nullptr), fbo(0), tex{0, 0, 0}
{
    program = new CKGLProgram(get_resource("CKGLRPP_VERT_SHDR", (char*)1), fshsrc);
    if (program->validate())
    {
        program->stage_uniform("color_in", CKGLUniformValue::make_i32(COLOR));
        program->stage_uniform("norpth_in", CKGLUniformValue::make_i32(NORPTH));
        program->send_uniform();
    }
}

CKGLPostProcessingStage::~CKGLPostProcessingStage()
{
    glDeleteTextures(3, tex);
    glDeleteFramebuffers(1, &fbo);
    delete program;
}

void CKGLPostProcessingStage::setup_fbo(bool has_depth, bool has_normal, int _width, int _height)
{
    if (fbo)
    {
        if (_width != width || _height != height)
        {
            glDeleteTextures(3, tex);
            glDeleteFramebuffers(1, &fbo);
        } else return;
    }
    glCreateFramebuffers(1, &fbo);
    glCreateTextures(GL_TEXTURE_2D, 3, tex);
    width = _width;
    height = _height;

    glTextureStorage2D(tex[COLOR], 1, GL_RGBA8, width, height);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex[COLOR], 0);

    if (has_normal || has_depth)
    {
        glTextureStorage2D(tex[NORPTH], 1, GL_RGBA32F, width, height);
        glTextureParameteri(tex[NORPTH], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(tex[NORPTH], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, tex[NORPTH], 0);
        GLenum db[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glNamedFramebufferDrawBuffers(fbo, 2, db);
    }

    if (has_depth)
    {
        glBindTexture(GL_TEXTURE_2D, tex[DEPTH]);
        glTextureStorage2D(tex[DEPTH], 1, GL_DEPTH_COMPONENT32F, width, height);
        glTextureParameteri(tex[DEPTH], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(tex[DEPTH], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, tex[DEPTH], 0);
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
    float v[2];
    pipeline->get_screen_size(v);
    program->stage_uniform("screen_size", CKGLUniformValue::make_f32v2v(1, v, true));
    pipeline->get_mouse_position(v);
    program->stage_uniform("mouse_pos", CKGLUniformValue::make_f32v2v(1, v, true));
    program->stage_uniform("time", CKGLUniformValue::make_f32(pipeline->time_since_startup()));
    program->stage_uniform("frame_time", CKGLUniformValue::make_f32(pipeline->time_between_frames()));
    program->send_uniform();
}

void CKGLPostProcessingStage::draw(CKGLPostProcessingPipeline *pipeline)
{
    program->use();
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
