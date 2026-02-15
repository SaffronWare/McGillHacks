#include "Application.h"

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>

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

    for (int i = 1; i <= 10; i++)
    {
        ParticleGPU test;
        test.position = Vec4(rng(2), rng(2), rng(2), rng(2)).normalized();
        test.velocity = Vec4(rng(2), rng(2), rng(2), rng(2)).normalized();
        test.radius = (rng(10) / 150);
        test.color = Vec3(rng(2), rng(2), rng(2));
        particles.push_back(test);
    }

    if (!glfwInit())
        throw std::runtime_error("GLFW could not initialize!");

    // Request a modern core context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window = glfwCreateWindow(
        int(mode->width * 0.8f),
        int(mode->height * 0.8f),
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
        std::runtime_error("Failed to initialize OpenGL context");
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
}

Application::~Application()
{
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

    // 1. Show the big demo window (optional)
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a custom controls window
    if (show_controls_window)
    {
        ImGui::Begin("4D Sphere Controls", &show_controls_window);

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

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);

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

        // Simulation controls
        if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Pause Simulation", &pause_simulation);
            ImGui::SliderFloat("Speed", &simulation_speed, 0.0f, 5.0f);
            ImGui::SliderInt("Max Particles", &max_particles, 1, 256);

            if (ImGui::Button("Add 10 Particles"))
            {
                for (int i = 0; i < 10; i++)
                {
                    ParticleGPU test;
                    test.position = Vec4(rng(2), rng(2), rng(2), rng(2)).normalized();
                    test.velocity = Vec4(rng(2), rng(2), rng(2), rng(2)).normalized();
                    test.radius = (rng(10) / 150);
                    test.color = Vec3(rng(2), rng(2), rng(2));
                    particles.push_back(test);
                }

                // Update GPU buffer
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                    particles.size() * sizeof(ParticleGPU),
                    particles.data(),
                    GL_DYNAMIC_READ);
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear Particles"))
            {
                particles.clear();
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_READ);
            }

            ImGui::Text("Current Particles: %zu", particles.size());
        }

        ImGui::Separator();

        // Rendering controls
        if (ImGui::CollapsingHeader("Rendering"))
        {
            ImGui::ColorEdit3("Clear Color", clear_color);
        }

        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

        // Only update camera if NOT in UI mode
        if (!ui_mode)
        {
            glfwGetCursorPos(window, &nx, &ny);

            cam.yaw((nx - ox) * 0.1 * dt);
            cam.pitch((ny - oy) * 0.1 * dt);

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
        if (!pause_simulation && particles.size() > 0)
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

        glDisable(GL_DEPTH_TEST);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Render ImGui on top
        renderImGui();

        glfwSwapBuffers(window);
    }

    return 0;
}