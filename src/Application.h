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

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

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
	// ImGui helper methods
	void initImGui();
	void shutdownImGui();
	void renderImGui();

	// Rendering state
	GLuint pos_id;
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
	int w;
	int h;

	// UI mode control
	bool ui_mode = false;  // false = camera control, true = UI interaction

	// ImGui state variables (example - add your own)
	bool show_demo_window = true;
	bool show_controls_window = true;
	float particle_spawn_rate = 1.0f;
	int max_particles = 128;
	float simulation_speed = 1.0f;
	bool pause_simulation = false;
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};