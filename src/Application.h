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

// Windows Audio
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

struct ParticleGPU
{
	Vec4 position;
	Vec3 color;
	float radius;
	Vec4 velocity;
};

class Application
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

	// Game logic helpers
	void initializeGame();
	void updateGameState(float dt);
	float calculateClusteringScore();
	float calculate4DDistance(const Vec4& a, const Vec4& b);
	void startNewRound();
	void applyRedBallVelocity();
	void toggleFullscreen();
	bool checkIfCaught();  // New method for catch detection

	// Audio methods
	void initAudio();
	void updateAudio();
	void stopAudio();

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

	// Arrow rendering (unused - kept for compatibility)
	GLuint u_show_arrow;
	GLuint u_arrow_start;
	GLuint u_arrow_direction;
	GLuint u_arrow_length;

	// UI mode control
	bool ui_mode = false;  // false = camera control, true = UI interaction

	// Fullscreen mode
	bool is_fullscreen = false;
	int windowed_width = 0;
	int windowed_height = 0;
	int windowed_pos_x = 0;
	int windowed_pos_y = 0;

	// Game state variables
	enum class GameState {
		INTRO,           // Game introduction
		SIMULATION,      // Particles moving - player can move camera
		PAUSED,          // Frozen for player input - can only rotate
		GAME_OVER        // All rounds complete
	};

	GameState game_state = GameState::INTRO;
	int current_round = 0;
	const int MAX_ROUNDS = 10;
	float round_timer = 0.0f;
	const float ROUND_DURATION = 5.0f;

	// Red ball (player-controlled) - always particles[0]
	Vec4 red_ball_velocity_input = Vec4(0.5, 0.5, 0, 0);  // Start with visible default
	float velocity_magnitude = 0.5f;

	// Tutorial state
	bool show_tutorial = true;
	int tutorial_step = 0;

	// Results - new catch mechanic
	int total_points = 0;
	float catch_radius = 0.7f;  // Distance threshold to catch red ball
	bool caught_this_round = false;
	float final_clustering_score = 0.0f;  // Keep for compatibility
	float current_clustering_score = 0.0f;
	float clustering_update_timer = 0.0f;
	const float CLUSTERING_UPDATE_INTERVAL = 0.5f;

	// Velocity arrow visualization
	bool show_velocity_arrow = false;

	// Audio state
	bool music_enabled = true;
	bool music_loaded = false;
	float music_volume = 50.0f;  // 0-100
	bool audio_device_open = false;
	std::string audio_alias = "BGMusic";

	// ImGui state variables
	bool show_demo_window = false;
	bool show_controls_window = true;
	bool show_velocity_editor = false;
	float particle_spawn_rate = 1.0f;
	int max_particles = 128;
	float simulation_speed = 1.0f;
	bool pause_simulation = false;
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};