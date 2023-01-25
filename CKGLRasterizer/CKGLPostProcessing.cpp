#include "CKGLPostProcessing.h"
#include "CKGLRasterizerCommon.h"
#include "CKGLProgram.h"

#include <CKInputManager.h>
#include <CKContext.h>
#include <CKEnums.h>

#include <sstream>
#include <regex>
#include <utility>

CKGLPostProcessingStage::CKGLPostProcessingStage() :
    program(nullptr), fbo(0), tex{0}
{
}

CKGLPostProcessingStage::~CKGLPostProcessingStage()
{
    glDeleteTextures(5, tex);
    glDeleteFramebuffers(1, &fbo);
    if (program)
        delete program;
}

void CKGLPostProcessingStage::set_fsh(std::string &&fsh){ fshsrc = std::move(fsh); }

void CKGLPostProcessingStage::set_vsh(std::string &&vsh){ vshsrc = std::move(vsh); }

bool CKGLPostProcessingStage::compile()
{
    if (vshsrc.empty() || fshsrc.empty()) return false;
    if (program) return false;
    program = new CKGLProgram(vshsrc, fshsrc);
    if (program->validate())
    {
        program->stage_uniform("color_in", CKGLUniformValue::make_i32(COLOR));
        program->stage_uniform("norpth_in", CKGLUniformValue::make_i32(NORPTH));
        program->stage_uniform("feedback_in", CKGLUniformValue::make_i32(2));
        program->send_uniform();
        fshsrc.clear();
        vshsrc.clear();
        return true;
    }
    return false;
}

void CKGLPostProcessingStage::set_user_uniform(const std::string &u, CKGLUniformValue *v)
{
    if (program && user_uniforms.find(u) != user_uniforms.end() && user_uniforms[u] == v->type())
        program->stage_uniform(u, v);
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

void CKGLPostProcessingStage::swap_feedback_frames(bool back)
{
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT2, tex[back ? 4 : 3], 0);
}

void CKGLPostProcessingStage::parse_stage_config()
{
    std::stringstream ssfsh(fshsrc);
    bool in_conf = false;
    const std::regex begin_block("/\\*.*BEGIN POST STAGE CONFIGURATION");
    const std::regex end_block("END POST STAGE CONFIGURATION.*\\*/");
    const std::regex uparamre("^uniform_parameter\\|([A-Za-z_][A-Za-z_0-9]*)\\|(.*)$");
    //const std::regex usamplre("^uniform_sampler\\|([A-Za-z_][A-Za-z_0-9]*)\\|(.*)\\|(.*)$");
    //const std::regex cstoutre("^custom_output\\|([A-Za-z_][A-Za-z_0-9]*)\\|(.*)\\(.*)$");
    while (!ssfsh.eof())
    {
        std::string line;
        std::getline(ssfsh, line);
        if (std::regex_search(line, begin_block) && !in_conf)
        {
            in_conf = true;
            continue;
        }
        else if (std::regex_search(line, end_block) && in_conf)
        {
            in_conf = false;
            break;
        }
        if (!in_conf) continue;
        std::smatch m;
        if (std::regex_match(line, m, uparamre))
        {
            std::string parameter_name = m[1];
            std::string parameter_type = m[2];
            user_uniforms[parameter_name] = CKGLUniformValue::type_from_string(parameter_type);
            continue;
        }
        /*if (std::regex_match(line, m, usamplre))
        {
            std::string sampler_name = m[1];
            std::string sampler_type = m[2];
            std::string sampler_location = m[3];
            continue;
        }
        if (std::regex_match(line, m, cstoutre))
        {
            std::string output_name = m[1];
            std::string output_attachment = m[2];
            std::string output_type = m[3];
            continue;
        }*/
    }
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
    glCreateTextures(GL_TEXTURE_2D, 5, tex);
    width = _width;
    height = _height;

    glTextureStorage2D(tex[COLOR], 1, GL_RGBA8, width, height);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex[COLOR], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex[COLOR], 0);

    if (has_normal || has_depth)
    {
        glTextureStorage2D(tex[NORPTH], 1, GL_RGBA32F, width, height);
        glTextureParameteri(tex[NORPTH], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(tex[NORPTH], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, tex[NORPTH], 0);
        GLenum db[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glNamedFramebufferDrawBuffers(fbo, 3, db);
    }

    if (has_depth)
    {
        glTextureStorage2D(tex[DEPTH], 1, GL_DEPTH_COMPONENT32F, width, height);
        glTextureParameteri(tex[DEPTH], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(tex[DEPTH], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, tex[DEPTH], 0);
    }
    
    glTextureStorage2D(tex[3], 1, GL_RGBA8, width, height);
    glTextureParameteri(tex[3], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex[3], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CKDWORD blank = 0;
    //!!TODO: REQUIRES GL4.4. Use Clear() instead.
    glClearTexSubImage(tex[3], 0, 0, 0, 0, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blank);
    glTextureStorage2D(tex[4], 1, GL_RGBA8, width, height);
    glTextureParameteri(tex[4], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex[4], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glClearTexSubImage(tex[4], 0, 0, 0, 0, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blank);
}

bool CKGLPostProcessingStage::valid()
{
    return program && program->validate() && glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void CKGLPostProcessingStage::set_as_target()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void CKGLPostProcessingStage::draw(CKGLPostProcessingPipeline *pipeline, GLuint tex2)
{
    program->use();
    send_uniform(pipeline);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[COLOR]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex[NORPTH]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

CKGLPostProcessingPipeline::CKGLPostProcessingPipeline() :
    scrwidth(0), scrheight(0), post_dummy_vao(0), mousepos{0, 0}
{
    glCreateVertexArrays(1, &post_dummy_vao);
    inputmgr = static_cast<CKInputManager*>(rst_ckctx->GetManagerByGuid(CKGUID(INPUT_MANAGER_GUID1)));
    startup = std::chrono::steady_clock::now();
    lastframe = std::chrono::steady_clock::now();
    back_buffer = false;
}

CKGLPostProcessingPipeline::~CKGLPostProcessingPipeline()
{
    glDeleteVertexArrays(1, &post_dummy_vao);
    for (auto stage : stages)
        delete stage;
}

void CKGLPostProcessingPipeline::parse_pipeline_config(const std::string &cfg)
{
    std::stringstream ssfsh(cfg);
    bool in_conf = false;
    const std::regex block_header("\\[stage_([0-9]*)\\]");
    const std::regex xpair("^([^=]*)=([^=]*)$");
    const std::regex tparam("^(.*)\\|(.*)\\|(.*)$");
    uint32_t stage = ~0U;
    uint32_t maxstage = 0;
    CKGLPostProcessingStage *stg = nullptr;
    while (!ssfsh.eof())
    {
        std::string line;
        std::getline(ssfsh, line);
        std::smatch m;
        if (std::regex_match(line, m, xpair))
        {
            if (m[1] == "name")
                name = m[2];
            else if (m[1] == "num_stages")
            {
                std::string n = m[2].str();
                std::from_chars(n.data(), n.data() + n.size(), maxstage);
            }
            if (stg)
            {
                if (m[1] == "vsh")
                {
                    stg->set_vsh(load_resource_or_file(m[2]));
                    stg->compile();
                }
                else if (m[1] == "fsh")
                {
                    stg->set_fsh(load_resource_or_file(m[2]));
                    stg->parse_stage_config();
                    stg->compile();
                }
                else if (m[1] == "uniform_parameter")
                {
                    std::smatch mm;
                    std::string up(m[2].str());
                    if (std::regex_match(up, mm, tparam))
                    {
                        std::string pn(mm[1]);
                        std::string pt(mm[2]);
                        std::string pv(mm[3]);
                        UniformType ty = CKGLUniformValue::type_from_string(pt);
                        CKGLUniformValue *v = CKGLUniformValue::from_string(pv, ty);
                        stg->set_user_uniform(pn, v);
                    }
                }
            }
        }
        else if (std::regex_match(line, m, block_header))
        {
            uint32_t expecting = ~stage ? stage + 1 : 0;
            std::string n = m[1].str();
            std::from_chars(n.data(), n.data() + n.size(), stage);
            //expecting >= maxstage is an error
            if (expecting >= maxstage || stage != expecting)
                return;
            if (stg)
            {
                if (stg->valid())
                    this->add_stage(stg);
                else delete stg;
            }
            stg = new CKGLPostProcessingStage();
        }
    }
    if (stg)
    {
        if (stg->valid())
            this->add_stage(stg);
        else delete stg;
    }
}

bool CKGLPostProcessingPipeline::setup_fbo(bool has_depth, bool has_normal, int width, int height)
{
    scrwidth = width;
    scrheight = height;
    bool ret = true;
    for (auto stage : stages)
    {
        stage->setup_fbo(has_depth, has_normal, width, height);
        stage->swap_feedback_frames(back_buffer);
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
        GLuint tex2 = 0;
        if (i + 1 < stages.size())
        {
            stages[i + 1]->set_as_target();
            tex2 = stages[i + 1]->tex[back_buffer ? 3 : 4];
        }
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        stages[i]->draw(this, tex2);
    }
    update_uniform_data();
    back_buffer = !back_buffer;
    for (auto stage : stages)
        stage->swap_feedback_frames(back_buffer);
}

std::string CKGLPostProcessingPipeline::get_name() { return name; }

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
