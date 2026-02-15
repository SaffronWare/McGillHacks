#include "Application.h"

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

static std::string ReadFile(const char* path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) throw std::runtime_error(std::string("Failed to open file: ") + path);

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static GLuint CompileShader(GLenum type, const std::string& src, const char* debugName)
{
    GLuint sh = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        glDeleteShader(sh);
        throw std::runtime_error(std::string("Shader compile failed (") + debugName + "):\n" + log);
    }
    return sh;
}

static GLuint LinkProgram(GLuint vs, GLuint fs)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        glDeleteProgram(prog);
        throw std::runtime_error(std::string("Program link failed:\n") + log);
    }

    // shaders can be deleted after linking
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

float rng(float s)
{
    return s * rand() / RAND_MAX;
}

Application::Application()
{
    cam = Camera();

    // Initialize game instead of random particles
    initializeGame();

    if (!glfwInit())
        throw std::runtime_error("GLFW could not initialize!");

    // Request a modern core context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    windowed_width = int(mode->width * 0.8f);
    windowed_height = int(mode->height * 0.8f);

    window = glfwCreateWindow(
        windowed_width,
        windowed_height,
        "CocoFractal3D",
        nullptr, nullptr
    );

    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("GLFW could not create window!");
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize OpenGL context");
        glfwTerminate();
    }

    glfwSwapInterval(1); // vsync
    glfwSetWindowUserPointer(window, this);

    // Note: ImGui will handle cursor for UI, so we'll toggle this
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // callbacks
    glfwSetKeyCallback(window, handle_events);

    // Initialize ImGui
    initImGui();

    // Initialize Audio
    initAudio();

    // Create a VAO (core profile requires one bound for glDrawArrays)
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Load + compile shaders
    const std::string vsSrc = ReadFile("shaders/vertex.glsl");
    const std::string fsSrc = ReadFile("shaders/frag.glsl");

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc, "vertex.glsl");
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc, "frag.glsl");
    shader_program = LinkProgram(vs, fs);

    const std::string csSrc = ReadFile("shaders/compute.glsl");

    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSrc, "compute.glsl");

    computeProgram = glCreateProgram();
    glAttachShader(computeProgram, cs);
    glLinkProgram(computeProgram);

    glDeleteShader(cs);

    // Uniform locations
    u_resolution = glGetUniformLocation(shader_program, "u_resolution");
    pos_id = glGetUniformLocation(shader_program, "cpos");
    front_id = glGetUniformLocation(shader_program, "front");
    right_id = glGetUniformLocation(shader_program, "right");
    up_id = glGetUniformLocation(shader_program, "up");
    u_dt = glGetUniformLocation(computeProgram, "dt");

    // Arrow visualization uniforms
    u_show_arrow = glGetUniformLocation(shader_program, "u_show_arrow");
    u_arrow_start = glGetUniformLocation(shader_program, "u_arrow_start");
    u_arrow_direction = glGetUniformLocation(shader_program, "u_arrow_direction");
    u_arrow_length = glGetUniformLocation(shader_program, "u_arrow_length");

    glGenBuffers(1, &particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);

    // allocate space for max spheres
    const int MAX_SPHERES = 128;
    glBufferData(GL_SHADER_STORAGE_BUFFER, particles.size() * sizeof(ParticleGPU), particles.data(), GL_DYNAMIC_READ);

    // bind to binding = 0 (matches shader)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

    // Initialize viewport
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    w = fbw; h = fbh;
    glViewport(0, 0, w, h);

    // Initialize clustering score
    current_clustering_score = calculateClusteringScore();
}

Application::~Application()
{
    stopAudio();

    shutdownImGui();

    if (shader_program) glDeleteProgram(shader_program);
    if (computeProgram) glDeleteProgram(computeProgram);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (particleSSBO) glDeleteBuffers(1, &particleSSBO);

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::initImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();  // Alternative style

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void Application::shutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::renderImGui()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Enable docking (optional - only if docking is enabled)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        // DockSpaceOverViewport expects an ImGuiID or uses the main viewport's ID
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    // ===== TOP BANNER: Game Instructions & Tutorial =====
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 120));
    ImGui::SetNextWindowBgAlpha(0.9f);

    ImGuiWindowFlags banner_flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Game Info Banner", nullptr, banner_flags);

    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "4D GRAVITY SHEPHERD");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();

    if (game_state == GameState::INTRO)
    {
        ImGui::TextWrapped("Welcome! You control the RED sphere. Your goal: Use gravity to cluster all 10 spheres as closely as possible over 10 rounds!");
        ImGui::TextWrapped("Each round lasts 5 seconds. When paused, adjust the red sphere's velocity to guide the cluster.");
    }
    else if (game_state == GameState::SIMULATION)
    {
        ImGui::Text("Round %d / %d", current_round, MAX_ROUNDS);
        ImGui::SameLine();
        ImGui::ProgressBar(round_timer / ROUND_DURATION, ImVec2(-1, 0), "");

        if (show_tutorial && current_round == 1)
        {
            ImGui::Separator();
            if (tutorial_step == 0)
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Watch the spheres interact through 4D gravity...");
            else if (tutorial_step == 1)
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "The RED sphere is yours to control!");
            else if (tutorial_step == 2)
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "When paused, you'll be able to change its velocity...");
            else if (tutorial_step == 3)
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Get ready to input your move!");
        }
        else
        {
            ImGui::TextWrapped("Observing gravitational interactions... Time remaining: %.1f s", ROUND_DURATION - round_timer);
        }
    }
    else if (game_state == GameState::PAUSED)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "PAUSED - Round %d / %d", current_round, MAX_ROUNDS);
        ImGui::TextWrapped("Adjust the RED sphere's velocity, then click 'Apply & Continue'");
    }
    else if (game_state == GameState::GAME_OVER)
    {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GAME COMPLETE!");
        ImGui::Text("Final Clustering Score: %.1f / 100", final_clustering_score);

        if (final_clustering_score > 80)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Excellent! Master of 4D gravity!");
        else if (final_clustering_score > 60)
            ImGui::Text("Good job! The spheres are fairly clustered.");
        else if (final_clustering_score > 40)
            ImGui::Text("Not bad, but there's room for improvement.");
        else
            ImGui::Text("The spheres are quite scattered. Try again?");
    }

    ImGui::End();

    // ===== VELOCITY EDITOR (appears during PAUSED state) =====
    if (show_velocity_editor && game_state == GameState::PAUSED)
    {
        ImGui::Begin("Red Sphere Velocity Editor", &show_velocity_editor);

        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Red Sphere Control");
        ImGui::Separator();

        ImGui::Text("Current Position:");
        if (particles.size() > 0)
        {
            ImGui::Text("  (%.3f, %.3f, %.3f, %.3f)",
                particles[0].position.x,
                particles[0].position.y,
                particles[0].position.z,
                particles[0].position.w);
        }

        ImGui::Separator();
        ImGui::TextWrapped("Set velocity direction (will be normalized):");

        ImGui::SliderFloat("X##vel", &red_ball_velocity_input.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Y##vel", &red_ball_velocity_input.y, -1.0f, 1.0f);
        ImGui::SliderFloat("Z##vel", &red_ball_velocity_input.z, -1.0f, 1.0f);
        ImGui::SliderFloat("W##vel", &red_ball_velocity_input.w, -1.0f, 1.0f);

        ImGui::Separator();
        ImGui::SliderFloat("Magnitude", &velocity_magnitude, 0.0f, 1.0f);

        ImGui::Separator();

        // Debug info
        ImGui::Text("Arrow Status:");
        ImGui::Text("  Show Arrow: %s", show_velocity_arrow ? "YES" : "NO");
        ImGui::Text("  Arrow Length: %.3f", velocity_magnitude * 0.5f);

        ImGui::Separator();

        // Show preview of resulting velocity
        Vec4 preview_vel = red_ball_velocity_input.normalized() * velocity_magnitude;
        ImGui::Text("Preview velocity:");
        ImGui::Text("  (%.3f, %.3f, %.3f, %.3f)", preview_vel.x, preview_vel.y, preview_vel.z, preview_vel.w);

        ImGui::Separator();

        // Visual arrow indicator for velocity direction
        ImGui::Text("Velocity Direction (Camera View):");

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImVec2(300, 300);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw background
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            IM_COL32(20, 20, 20, 255));
        draw_list->AddRect(canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            IM_COL32(100, 100, 100, 255));

        // Center point
        ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f,
            canvas_pos.y + canvas_size.y * 0.5f);

        // Project velocity vector onto camera's right and up vectors
        Vec4 normalized_vel = red_ball_velocity_input.normalized();

        // Project velocity onto camera's coordinate system
        float vel_right = normalized_vel.x * cam.right.x +
            normalized_vel.y * cam.right.y +
            normalized_vel.z * cam.right.z +
            normalized_vel.w * cam.right.w;

        float vel_up = normalized_vel.x * cam.up.x +
            normalized_vel.y * cam.up.y +
            normalized_vel.z * cam.up.z +
            normalized_vel.w * cam.up.w;

        float vel_front = normalized_vel.x * cam.front.x +
            normalized_vel.y * cam.front.y +
            normalized_vel.z * cam.front.z +
            normalized_vel.w * cam.front.w;

        // Scale to fit in canvas
        float scale = 120.0f;
        ImVec2 arrow_end = ImVec2(center.x + vel_right * scale,
            center.y - vel_up * scale);  // Negative Y because screen coords

        // Draw grid lines
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0) continue;
            draw_list->AddLine(ImVec2(center.x + i * 50, canvas_pos.y),
                ImVec2(center.x + i * 50, canvas_pos.y + canvas_size.y),
                IM_COL32(40, 40, 40, 255), 1.0f);
            draw_list->AddLine(ImVec2(canvas_pos.x, center.y + i * 50),
                ImVec2(canvas_pos.x + canvas_size.x, center.y + i * 50),
                IM_COL32(40, 40, 40, 255), 1.0f);
        }

        // Draw cross-hairs (camera axes)
        draw_list->AddLine(ImVec2(center.x - 10, center.y),
            ImVec2(center.x + 10, center.y),
            IM_COL32(150, 150, 150, 255), 2.0f);
        draw_list->AddLine(ImVec2(center.x, center.y - 10),
            ImVec2(center.x, center.y + 10),
            IM_COL32(150, 150, 150, 255), 2.0f);

        // Draw magnitude circle
        draw_list->AddCircle(center, velocity_magnitude * scale, IM_COL32(100, 100, 255, 80), 32, 1.5f);

        // Draw velocity arrow
        draw_list->AddLine(center, arrow_end, IM_COL32(255, 50, 50, 255), 4.0f);

        // Draw arrowhead
        float arrow_length = sqrtf((arrow_end.x - center.x) * (arrow_end.x - center.x) +
            (arrow_end.y - center.y) * (arrow_end.y - center.y));
        if (arrow_length > 15.0f)
        {
            float angle = atan2f(arrow_end.y - center.y, arrow_end.x - center.x);
            float head_size = 15.0f;

            ImVec2 p1 = ImVec2(arrow_end.x - head_size * cosf(angle - 0.5f),
                arrow_end.y - head_size * sinf(angle - 0.5f));
            ImVec2 p2 = ImVec2(arrow_end.x - head_size * cosf(angle + 0.5f),
                arrow_end.y - head_size * sinf(angle + 0.5f));

            draw_list->AddTriangleFilled(arrow_end, p1, p2, IM_COL32(255, 50, 50, 255));
        }

        // Draw endpoint circle
        draw_list->AddCircleFilled(arrow_end, 5.0f, IM_COL32(255, 50, 50, 255));

        ImGui::Dummy(canvas_size);

        ImGui::Text("Camera Right-Up projection");
        ImGui::Text("Into screen (front): %.2f", vel_front);

        // Color code the depth
        if (vel_front > 0.3f)
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "  (pointing away from camera)");
        else if (vel_front < -0.3f)
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "  (pointing toward camera)");
        else
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "  (perpendicular to view)");

        ImGui::Separator();

        // Show velocity vector info (preview_vel already declared above)
        ImGui::Text("Velocity vector:");
        ImGui::Text("  (%.3f, %.3f, %.3f, %.3f)", preview_vel.x, preview_vel.y, preview_vel.z, preview_vel.w);
        ImGui::Text("Magnitude: %.2f", velocity_magnitude);

        ImGui::Separator();

        if (ImGui::Button("Apply & Continue to Next Round", ImVec2(-1, 40)))
        {
            applyRedBallVelocity();
        }

        ImGui::Spacing();

        if (ImGui::Button("Keep Current Velocity", ImVec2(-1, 0)))
        {
            // Don't change velocity, just continue
            startNewRound();
        }

        ImGui::End();
    }

    // ===== MAIN CONTROLS WINDOW =====
    if (show_controls_window)
    {
        ImGui::Begin("Controls & Info", &show_controls_window);

        // UI Mode indicator
        if (ui_mode)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "UI MODE ACTIVE");
            ImGui::Text("Press C to return to camera control");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "CAMERA CONTROL MODE");
            ImGui::Text("Press C to enable UI interaction");
        }

        ImGui::Separator();

        ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);

        ImGui::Separator();

        // Camera information
        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::Text("Position: (%.2f, %.2f, %.2f, %.2f)",
                cam.pos.x, cam.pos.y, cam.pos.z, cam.pos.w);
            ImGui::Text("Front: (%.2f, %.2f, %.2f, %.2f)",
                cam.front.x, cam.front.y, cam.front.z, cam.front.w);
        }

        ImGui::Separator();

        // Game controls
        if (ImGui::CollapsingHeader("Game Controls", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Spheres: %zu", particles.size());
            ImGui::Text("Game State: %s",
                game_state == GameState::INTRO ? "Introduction" :
                game_state == GameState::SIMULATION ? "Simulating" :
                game_state == GameState::PAUSED ? "Paused for Input" :
                "Game Over");

            if (game_state != GameState::GAME_OVER && game_state != GameState::INTRO)
            {
                ImGui::Text("Current Round: %d / %d", current_round, MAX_ROUNDS);

                ImGui::Separator();

                // Show live clustering score (updated every 0.5s)
                ImGui::Text("Clustering Analysis:");
                ImGui::Text("  Score: %.1f / 100", current_clustering_score);
                ImGui::ProgressBar(current_clustering_score / 100.0f, ImVec2(-1, 0));

                ImGui::Text("  Update timer: %.2f / %.2f s",
                    clustering_update_timer, CLUSTERING_UPDATE_INTERVAL);

                // Show a sample distance for debugging
                if (particles.size() >= 2)
                {
                    float sample_dist = calculate4DDistance(particles[0].position, particles[1].position);
                    ImGui::Text("  Sample distance (0-1): %.3f rad", sample_dist);
                }
            }

            ImGui::Spacing();

            if (ImGui::Button("Restart Game", ImVec2(-1, 0)))
            {
                initializeGame();

                // Update GPU buffer
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                    particles.size() * sizeof(ParticleGPU),
                    particles.data(),
                    GL_DYNAMIC_READ);
            }
        }

        ImGui::Separator();

        // Rendering controls
        if (ImGui::CollapsingHeader("Rendering"))
        {
            ImGui::ColorEdit3("Background", clear_color);
        }

        ImGui::Separator();

        // Audio controls
        if (ImGui::CollapsingHeader("Audio"))
        {
            if (!music_loaded)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Music file not found!");
                ImGui::TextWrapped("Place audio/background.wav in your project directory");
                ImGui::Spacing();
                ImGui::Text("Supported formats: MP3, WAV");
            }
            else
            {
                // Enable/disable toggle
                if (ImGui::Checkbox("Enable Music", &music_enabled))
                {
                    updateAudio();
                }

                ImGui::Spacing();

                // Volume slider
                if (ImGui::SliderFloat("Volume", &music_volume, 0.0f, 100.0f, "%.0f%%"))
                {
                    updateAudio();
                }

                ImGui::Spacing();

                // Status
                ImGui::Text("Status: %s", music_enabled ? "Playing (Looping)" : "Stopped");

                ImGui::Spacing();

                // Restart button
                if (ImGui::Button("Restart Music", ImVec2(-1, 0)))
                {
                    stopAudio();
                    if (music_enabled)
                    {
                        initAudio();
                    }
                }
            }
        }

        ImGui::End();
    }

    // Show demo window if enabled
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::initializeGame()
{
    particles.clear();

    // Create 10 spheres with varying sizes
    // Index 0 = Red ball (largest, player-controlled)
    float sizes[10] = { 0.08f, 0.04f, 0.045f, 0.05f, 0.035f, 0.055f, 0.04f, 0.038f, 0.042f, 0.048f };

    for (int i = 0; i < 2; i++)
    {
        ParticleGPU particle;

        // Random position on 4D sphere
        particle.position = Vec4(rng(2) - 1, rng(2) - 1, rng(2) - 1, rng(2) - 1).normalized();

        // Random initial velocity
        particle.velocity = Vec4(rng(2) - 1, rng(2) - 1, rng(2) - 1, rng(2) - 1).normalized() * 0.3f;

        particle.radius = sizes[i]/8.0f;

        // First one is red (player ball), others are random colors
        if (i == 0)
        {
            particle.color = Vec3(1.0f, 0.0f, 0.0f);  // Red
            particle.radius *= 4.0f;
        }
        else
        {
            particle.color = Vec3(1, 1, 1);
        }

        particles.push_back(particle);
    }

    // Reset game state
    game_state = GameState::INTRO;
    current_round = 0;
    round_timer = 0.0f;
    tutorial_step = 0;
    show_tutorial = true;
}

void Application::updateGameState(float dt)
{
    // Update clustering score periodically
    clustering_update_timer += dt;
    if (clustering_update_timer >= CLUSTERING_UPDATE_INTERVAL)
    {
        current_clustering_score = calculateClusteringScore();
        clustering_update_timer = 0.0f;
    }

    switch (game_state)
    {
    case GameState::INTRO:
        round_timer += dt;
        if (round_timer >= 3.0f)  // Show intro for 3 seconds
        {
            round_timer = 0.0f;
            game_state = GameState::SIMULATION;
            current_round = 1;
        }
        break;

    case GameState::SIMULATION:
        round_timer += dt;

        // Progress tutorial
        if (show_tutorial)
        {
            if (round_timer > 1.0f && tutorial_step == 0) tutorial_step = 1;
            if (round_timer > 2.5f && tutorial_step == 1) tutorial_step = 2;
            if (round_timer > 4.0f && tutorial_step == 2) tutorial_step = 3;
        }

        if (round_timer >= ROUND_DURATION)
        {
            game_state = GameState::PAUSED;
            round_timer = 0.0f;
            show_velocity_editor = true;
            show_velocity_arrow = true;  // Show arrow during pause

            // Automatically switch to UI mode
            if (!ui_mode)
            {
                ui_mode = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        break;

    case GameState::PAUSED:
        // Wait for player to apply velocity
        break;

    case GameState::GAME_OVER:
        // Calculate final score (once)
        if (final_clustering_score == 0.0f)
        {
            final_clustering_score = calculateClusteringScore();
        }
        break;
    }
}

float Application::calculate4DDistance(const Vec4& a, const Vec4& b)
{
    // Calculate geodesic distance on 4-sphere surface
    // Both points should be normalized (on unit sphere)
    Vec4 a_norm = a.normalized();
    Vec4 b_norm = b.normalized();

    float dot_product = a_norm.x * b_norm.x +
        a_norm.y * b_norm.y +
        a_norm.z * b_norm.z +
        a_norm.w * b_norm.w;

    // Clamp to avoid numerical issues with acos
    if (dot_product < -1.0f) dot_product = -1.0f;
    if (dot_product > 1.0f) dot_product = 1.0f;

    // Arc length on unit sphere (geodesic distance)
    return std::acos(dot_product);
}

float Application::calculateClusteringScore()
{
    if (particles.size() < 2) return 0.0f;

    // Calculate average pairwise geodesic distance (lower is better)
    float total_distance = 0.0f;
    int pair_count = 0;

    for (size_t i = 0; i < particles.size(); i++)
    {
        for (size_t j = i + 1; j < particles.size(); j++)
        {
            // Use geodesic distance on curved 4-sphere
            total_distance += calculate4DDistance(particles[i].position, particles[j].position);
            pair_count++;
        }
    }

    if (pair_count == 0) return 0.0f;

    float avg_distance = total_distance / pair_count;

    // Convert to score (0-100, where 100 is best)
    // Max distance on sphere is π, map that to 0 points
    // Close clustering (< 0.5 radians avg) maps to high scores
    float score = 100.0f * (1.0f - avg_distance / 3.14159f);
    if (score < 0.0f) score = 0.0f;

    return score;
}

void Application::startNewRound()
{
    current_round++;
    round_timer = 0.0f;
    show_velocity_editor = false;
    show_velocity_arrow = false;  // Hide arrow during simulation
    show_tutorial = false;  // Hide tutorial after first round

    if (current_round <= MAX_ROUNDS)
    {
        game_state = GameState::SIMULATION;
        // DON'T reset positions - particles continue from where they were!
    }
    else
    {
        game_state = GameState::GAME_OVER;
    }
}

void Application::applyRedBallVelocity()
{
    if (particles.size() > 0)
    {
        // Apply the velocity to the red ball (index 0)
        particles[0].velocity = red_ball_velocity_input.normalized() * velocity_magnitude;

        // Update GPU buffer - only update the red ball
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(ParticleGPU), &particles[0]);

        // Start next round (this does NOT reset positions, just changes game state)
        startNewRound();
    }
}

void Application::initAudio()
{
#ifdef _WIN32
    // Open the audio device using MCI
    std::string open_command = "open \"audio/background.wav\" type waveaudio alias " + audio_alias;
    MCIERROR error = mciSendStringA(open_command.c_str(), NULL, 0, NULL);

    if (error != 0)
    {
        char error_text[256];
        mciGetErrorStringA(error, error_text, sizeof(error_text));
        std::cerr << "Warning: Failed to open audio: " << error_text << std::endl;
        std::cerr << "Make sure audio/background.wav exists in your project directory" << std::endl;
        music_loaded = false;
        audio_device_open = false;
        return;
    }

    audio_device_open = true;
    music_loaded = true;

    // Set time format to milliseconds
    std::string format_command = "set " + audio_alias + " time format milliseconds";
    mciSendStringA(format_command.c_str(), NULL, 0, NULL);

    std::cout << "Audio initialized successfully" << std::endl;

    if (music_enabled)
    {
        updateAudio();
    }
#else
    std::cerr << "Audio playback is only supported on Windows" << std::endl;
    music_loaded = false;
    audio_device_open = false;
#endif
}

void Application::updateAudio()
{
#ifdef _WIN32
    if (!audio_device_open) return;

    if (music_enabled)
    {
        // Set volume (0-1000 for MCI)
        int volume = (int)(music_volume * 10.0f);
        std::string volume_command = "setaudio " + audio_alias + " volume to " + std::to_string(volume);
        mciSendStringA(volume_command.c_str(), NULL, 0, NULL);

        // Stop first (to restart if already playing)
        std::string stop_command = "stop " + audio_alias;
        mciSendStringA(stop_command.c_str(), NULL, 0, NULL);

        // Play with repeat
        std::string play_command = "play " + audio_alias + " from 0 repeat";
        mciSendStringA(play_command.c_str(), NULL, 0, NULL);
    }
    else
    {
        std::string stop_command = "stop " + audio_alias;
        mciSendStringA(stop_command.c_str(), NULL, 0, NULL);
    }
#endif
}

void Application::stopAudio()
{
#ifdef _WIN32
    if (!audio_device_open) return;

    std::string stop_command = "stop " + audio_alias;
    mciSendStringA(stop_command.c_str(), NULL, 0, NULL);

    std::string close_command = "close " + audio_alias;
    mciSendStringA(close_command.c_str(), NULL, 0, NULL);

    audio_device_open = false;
#endif
}

void Application::toggleFullscreen()
{
    if (!is_fullscreen)
    {
        // Save current windowed position and size
        glfwGetWindowPos(window, &windowed_pos_x, &windowed_pos_y);
        glfwGetWindowSize(window, &windowed_width, &windowed_height);

        // Get monitor and video mode
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        is_fullscreen = true;
    }
    else
    {
        // Switch back to windowed mode
        glfwSetWindowMonitor(window, nullptr, windowed_pos_x, windowed_pos_y,
            windowed_width, windowed_height, 0);

        is_fullscreen = false;
    }

    // Update viewport after mode change
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    w = fbw;
    h = fbh;
    glViewport(0, 0, w, h);
}

void Application::handle_events(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle UI mode with C key
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        app->ui_mode = !app->ui_mode;

        if (app->ui_mode)
        {
            // Enable UI mode - show cursor
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else
        {
            // Disable UI mode - hide cursor for camera control
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

int Application::run()
{
    float tim = 0.0f;
    float last_time = glfwGetTime();
    float new_time = 0;
    float dt = 0;

    double ox, oy;
    double nx, ny;
    glfwGetCursorPos(window, &ox, &oy);

    while (!glfwWindowShouldClose(window))
    {
        new_time = glfwGetTime();
        dt = new_time - last_time;
        last_time = new_time;

        // Apply clear color from ImGui
        glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwPollEvents();

        // Update game state
        updateGameState(dt);

        // Only update camera if NOT in UI mode
        if (!ui_mode)
        {
            glfwGetCursorPos(window, &nx, &ny);

            cam.yaw((nx - ox) * 0.05 * dt);
            cam.pitch((ny - oy) * 0.05 * dt);

            ox = nx; oy = ny;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                cam.move_forward(dt);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                cam.move_forward(-dt);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                cam.move_right(dt);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                cam.move_right(-dt);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                cam.move_up(dt);
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                cam.move_up(-dt);
        }
        else
        {
            // In UI mode, update cursor position to prevent camera jump when exiting UI mode
            glfwGetCursorPos(window, &ox, &oy);
        }

        // ---- PHYSICS ----
        // Only run physics during SIMULATION and INTRO states
        if ((game_state == GameState::SIMULATION || game_state == GameState::INTRO) && particles.size() > 0)
        {
            glUseProgram(computeProgram);
            glUniform1f(u_dt, dt * simulation_speed);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

            glDispatchCompute(particles.size() / 3 + 1, 1, 1);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

            // make writes visible
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }

        // ---- RENDERING ----
        glUseProgram(shader_program);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
        glBindVertexArray(vao);

        if (u_resolution != -1) glUniform2f(u_resolution, float(w), float(h));

        if (pos_id != -1) glUniform4f(pos_id, cam.pos.x, cam.pos.y, cam.pos.z, cam.pos.w);
        if (front_id != -1) glUniform4f(front_id, cam.front.x, cam.front.y, cam.front.z, cam.front.w);
        if (up_id != -1) glUniform4f(up_id, cam.up.x, cam.up.y, cam.up.z, cam.up.w);
        if (right_id != -1) glUniform4f(right_id, cam.right.x, cam.right.y, cam.right.z, cam.right.w);

        // Send arrow data to shader
        if (u_show_arrow != -1) glUniform1i(u_show_arrow, show_velocity_arrow ? 1 : 0);

        if (show_velocity_arrow && particles.size() > 0)
        {
            Vec4 arrow_start = particles[0].position;  // Red ball position
            Vec4 arrow_dir = red_ball_velocity_input.normalized();
            float arrow_len = velocity_magnitude * 0.5f;  // Scale arrow length for visibility

            if (u_arrow_start != -1) glUniform4f(u_arrow_start, arrow_start.x, arrow_start.y, arrow_start.z, arrow_start.w);
            if (u_arrow_direction != -1) glUniform4f(u_arrow_direction, arrow_dir.x, arrow_dir.y, arrow_dir.z, arrow_dir.w);
            if (u_arrow_length != -1) glUniform1f(u_arrow_length, arrow_len);
        }

        glDisable(GL_DEPTH_TEST);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Render ImGui on top
        renderImGui();

        glfwSwapBuffers(window);
    }

    return 0;
}