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



    for (int i = 0; i <= 50; i++)
    {
        ParticleGPU test;
        test.position = Vec4(rng(2),rng(2),rng(2),rng(2)).normalized();
        test.velocity = Vec4(rng(2),rng(2),rng(2),rng(2)).normalized();
        test.radius = 0.01;
        test.color = Vec3(rng(2),rng(2),rng(2));
        particles.push_back(test);
    }

    

    if (!glfwInit())
        throw std::runtime_error("GLFW could not initialize!");

    // (Optional) request a modern core context
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // callbacks
    glfwSetKeyCallback(window, handle_events);


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

    // Uniform locations (optional; you can also just query per frame)
    u_resolution = glGetUniformLocation(shader_program, "u_resolution");
    //u_time = glGetUniformLocation(shader_program, "u_time");

    // If you still want your camera uniforms:
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
    if (shader_program) glDeleteProgram(shader_program);
    if (vao) glDeleteVertexArrays(1, &vao);

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}


void Application::handle_events(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


int Application::run()
{
    // Example camera usage (same as your old loop)


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
        last_time = glfwGetTime();
        glClear(GL_COLOR_BUFFER_BIT);
        glfwPollEvents();

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


        // ---- PHYSICS ----
        glUseProgram(computeProgram);
        glUniform1f(u_dt, dt);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

        glDispatchCompute(particles.size() / 1 + 1, 1, 1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

        // make writes visible
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

      
        glUseProgram(shader_program);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
        glBindVertexArray(vao);

   
        if (u_resolution != -1) glUniform2f(u_resolution, float(w), float(h));

        if (pos_id  != -1) glUniform4f(pos_id, cam.pos.x, cam.pos.y, cam.pos.z,cam.pos.w);
        if (front_id != -1) glUniform4f(front_id , cam.front.x, cam.front.y, cam.front.z,cam.front.w);
        if (up_id  != -1) glUniform4f(up_id, cam.up.x, cam.up.y, cam.up.z,cam.up.w);
        if (right_id != -1) glUniform4f(right_id, cam.right.x, cam.right.y, cam.right.z,cam.right.w);
        
        glDisable(GL_DEPTH_TEST);


        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
    }

    return 0;
}
