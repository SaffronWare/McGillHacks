#pragma once

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>

#include "glad/glad.h"
#include "glfw3.h"
#include "Camera.h"


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

	GLuint vao;

	GLFWwindow* window;

	GLuint shader_program;

	Camera cam;

	int w; int h;






};