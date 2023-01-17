#ifndef CKGLPOSTPROCESSING_H
#define CKGLPOSTPROCESSING_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <GL/glew.h>
#include <gl/GL.h>

class CKInputManager;
class CKGLPostProcessingPipeline;

class CKGLPostProcessingStage
{
private:
	std::string fshsrc;
	GLuint program;
	GLuint fbo;
	GLuint tex[3];
	std::unordered_map<std::string, GLint> uniform_loc;

	GLint get_uniform_location(const std::string &name);
	void send_uniform(CKGLPostProcessingPipeline *pipeline);

	static const int COLOR  = 0;
	static const int NORMAL = 1;
	static const int DEPTH  = 2;

public:
	CKGLPostProcessingStage(const std::string &_fshsrc);
	~CKGLPostProcessingStage();
	void setup_fbo(bool has_depth, bool has_normal, int width, int height);
	bool valid();
	void set_as_target();
	void draw(CKGLPostProcessingPipeline *pipeline);

	friend class CKGLPostProcessingPipeline;
};

class CKGLPostProcessingPipeline
{
private:
	std::vector<CKGLPostProcessingStage*> stages;
	GLuint post_dummy_vao;
	int scrwidth;
	int scrheight;
	float mousepos[2];
	std::chrono::steady_clock::time_point startup;
	std::chrono::steady_clock::time_point lastframe;
	CKInputManager *inputmgr;
public:
	CKGLPostProcessingPipeline();
	~CKGLPostProcessingPipeline();
	bool setup_fbo(bool has_depth, bool has_normal, int width, int height);
	void set_as_target();
	void draw();
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