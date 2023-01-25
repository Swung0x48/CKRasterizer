#ifndef CKGLPOSTPROCESSING_H
#define CKGLPOSTPROCESSING_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <GL/glew.h>
#include <gl/GL.h>

#include "CKGLUniformValue.h"

class CKInputManager;
class CKGLPostProcessingPipeline;
class CKGLProgram;

class CKGLPostProcessingStage
{
private:
    std::string vshsrc;
    std::string fshsrc;
    CKGLProgram *program;
    GLuint fbo;
    GLuint tex[16];
    int width;
    int height;
    std::unordered_map<std::string, UniformType> user_uniforms;

    //design not finalized yet... let's implement a simpler version first.
    //bit 0-3: location, bit 31: type (0=prev stage, 1=this stage)
    //std::unordered_map<std::string, uint32_t> user_samplers;
    //std::unordered_map<std::string, uint32_t> user_outputs;

    //define feedback buffer for the last stage
    //void define_feedback_buffer(int loc, UniformType type);
    void set_user_uniform(const std::string &u, CKGLUniformValue *v);
    void send_uniform(CKGLPostProcessingPipeline *pipeline);
    void swap_feedback_frames(bool back);
    void parse_stage_config();

    static const int COLOR  = 0;
    static const int NORPTH = 1;
    static const int DEPTH  = 2;

public:
    CKGLPostProcessingStage();
    ~CKGLPostProcessingStage();
    void set_fsh(std::string &&fsh);
    void set_vsh(std::string &&vsh);
    bool compile();
    void setup_fbo(bool has_depth, bool has_normal, int _width, int _height);
    bool valid();
    void set_as_target();
    void draw(CKGLPostProcessingPipeline *pipeline, GLuint tex2);

    friend class CKGLPostProcessingPipeline;
};

class CKGLPostProcessingPipeline
{
private:
    std::vector<CKGLPostProcessingStage*> stages;
    GLuint post_dummy_vao;
    bool back_buffer;
    int scrwidth;
    int scrheight;
    float mousepos[2];
    std::string name;
    std::chrono::steady_clock::time_point startup;
    std::chrono::steady_clock::time_point lastframe;
    CKInputManager *inputmgr;
public:
    CKGLPostProcessingPipeline();
    ~CKGLPostProcessingPipeline();

    void parse_pipeline_config(const std::string &cfg);
    bool setup_fbo(bool has_depth, bool has_normal, int width, int height);
    void set_as_target();
    void draw();
    std::string get_name();
    void add_stage(CKGLPostProcessingStage *stage);
    void clear_stages();

    void get_mouse_position(float *pos);
    void get_screen_size(float *size);
    float time_since_startup();
    float time_between_frames();
private:
    void update_uniform_data();
};

#endif
