#pragma once

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>
#include <vector>

#include "glad/glad.h"
#include "glfw3.h"
#include "Camera.h"

struct ParticleGPU
{
	Vec4 position;
	Vec3 color;
	float radius;
	Vec4 velocity;
};

struct Application
{
public:
	Application();

	~Application();

	static void handle_events(GLFWwindow* window, int key, int scancode, int action, int mods);



	int run();

private:

	GLuint pos_id ;
	GLuint front_id;
	GLuint right_id;
	GLuint up_id;
	GLuint u_resolution;
	GLuint u_dt;

	GLuint computeProgram;

	GLuint particleSSBO;
	std::vector<ParticleGPU> particles;

	GLuint vao;

	GLFWwindow* window;

	GLuint shader_program;

	Camera cam;

	int w; int h;






};