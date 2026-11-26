#include "glad/glad.h"

#include "ui.h"
#include "structs.h"
#include "vendor/stb_image.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

/*
 * Globals. For convenience.
 */
static std::unordered_map<std::string, int> g_uniform_locations;
static unsigned int g_program = 0;

static cg::Meshes g_mesh;
static cg::Textures g_tex;
static cg::OrbitCamera g_cam;
static cg::Rotor g_rotor;
static cg::Wind g_wind;
static cg::DayNight g_day;
static cg::Light g_light;
static const cg::Settings g_settings{};

static float g_time = 0.0f;

/*
 * Forward declarations.
 */
static void set_view(unsigned int);
static void set_projection(unsigned int);
static void set_light_pos(unsigned int);
static void set_light_color(unsigned int);

/*
 * Checks for OpenGL errors.
 */
static unsigned int gl_print_error(void)
{
    unsigned int error = glGetError();
    if (error == 0)
        std::cout << "No GL errors." << std::endl;
    else
        std::cerr << "GL Error: " << error << std::endl;
    return error;
}

/*
 * Window error event callback.
 */
static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Error: " << error << " " << description << std::endl;
}

/*
 * Window keypress event callback.
 */
static void key_callback(GLFWwindow* window,
                         int key,
                         int scancode,
                         int action,
                         int mode)
{
    if (action != GLFW_PRESS)
        return;

    switch (key)
    {
        case GLFW_KEY_ESCAPE:
            /* Close on escape. */
            glfwSetWindowShouldClose(window, true);
            break;
        case GLFW_KEY_X:
            /* For debugging. */
            gl_print_error();
            break;
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:
            g_rotor.speed = glm::min(g_rotor.speed + g_settings.rotor_speed_step,
                                     g_settings.rotor_speed_max);
            std::cout << "Rotor speed: " << g_rotor.speed << " deg/s" << std::endl;
            break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
            g_rotor.speed = glm::max(g_rotor.speed - g_settings.rotor_speed_step, 0.0f);
            std::cout << "Rotor speed: " << g_rotor.speed << " deg/s" << std::endl;
            break;
        case GLFW_KEY_SPACE:
            g_rotor.running = !g_rotor.running;
            std::cout << (g_rotor.running ? "Rotor: running" : "Rotor: stopped") << std::endl;
            break;
        case GLFW_KEY_N:
            g_day.is_day = !g_day.is_day;
            std::cout << (g_day.is_day ? "Day" : "Night") << std::endl;
            break;
        default:
            break;
    }
}

/*
 * Window resize event callback.
 */
static void size_callback(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
        return;

    glViewport(0, 0, width, height);
    cg::perspective.aspect = static_cast<float>(width) / height;
    set_projection(g_program);
};

/*
 * Create window.
 */
GLFWwindow* init_window(void)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() == GLFW_FALSE)
        return nullptr;

    /*
     * Required for Apple.
     */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, cg::version.gl_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, cg::version.gl_minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    /*
     * Multisampling.
     */
    glfwWindowHint(GLFW_SAMPLES, 4);

    /*
     * Create the graphics context and make it current.
     */
    GLFWwindow* const window = glfwCreateWindow(cg::window.window_width,
                                                cg::window.window_height,
                                                cg::window.window_title,
                                                nullptr,
                                                nullptr);

    if (window == nullptr)
        return nullptr;

    glfwMakeContextCurrent(window);

    /*
     * Enable VSync.
     */
    glfwSwapInterval(1);

    /*
     * Set event callbacks.
     */
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, size_callback);

    /*
     * Load OpenGL with GLAD.
     * Crashes if GL functions are called without this.
     */
    gladLoadGL();

    return window;
}

/*
 * Clear color and depth buffers.
 */
static void clear(void)
{
    glClearColor(g_day.sky_color.x, g_day.sky_color.y, g_day.sky_color.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/*
 * Destroy window.
 */
static void cleanup_window(GLFWwindow* window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

/*
 * Read shader from file.
 */
static std::optional<std::string> read_shader(const std::string& path)
{
    std::string result;

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (in.good() == false)
        return std::nullopt;

    in.seekg(0, std::ios::end);
    size_t size = in.tellg();

    if (size == -1)
        return std::nullopt;

    result.resize(size);
    in.seekg(0, std::ios::beg);
    in.read(&result[0], size);

    /*
     * Changing version to 410 for Mac here.
     * TODO Temp fix, probably not the best solution.
     */
    #ifdef __APPLE__
    if (result.starts_with("#version") == false)
        return std::nullopt;

    const auto nl = result.find("\n");
    result.replace(
        0,
        nl,
        cg::version.glsl_version
    );
    #endif // APPLE

    return result;
}

/*
 * Compile shader.
 * Copy the shader source string here just in case.
 * c_str will point to garbage if a string reference goes out of scope.
 */
static unsigned int compile_shader(const std::string shader_source,
                                   unsigned int type)
{
    unsigned int shader = glCreateShader(type);

    const char* c_str = shader_source.c_str();
    glShaderSource(shader, 1, &c_str, nullptr);

    glCompileShader(shader);

    /*
     * Compile error checking.
     */
    int is_compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == 0)
    {
        std::string log;
        int length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        log.resize(length);
        glGetShaderInfoLog(shader, length, nullptr, &log[0]);

        std::string type_s = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
        std::cerr << "Failed to compile " << type_s << " shader." << std::endl;
        std::cerr << log << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

/*
 * Compile and link shader program.
 */
static unsigned int create_shader(const std::string& vertex_source,
                                  const std::string& fragment_source)
{
    unsigned int program = glCreateProgram();
    unsigned int vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (vertex_shader == 0)
        return 0;
    unsigned int fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0)
        return 0;

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);

    return program;
}

int get_uniform_location(unsigned int program, const std::string& location)
{
    if (g_uniform_locations.find(location) != g_uniform_locations.end())
        return g_uniform_locations[location];

    int uniform = glGetUniformLocation(program, location.c_str());
    if (uniform == -1)
        std::cout << "Warning: Uniform " << location <<
        " does not exist. This uniform will not be set." << std::endl;

    g_uniform_locations[location] = uniform;
    return uniform;
}

static void set_vec3(unsigned int program,
                     const glm::vec3& vector,
                     const std::string& location)
{
    int uniform = get_uniform_location(program, location);
    if (uniform == -1)
        return;
    glUniform3fv(uniform, 1, glm::value_ptr(vector));
}

static void set_float(unsigned int program,
                      float value,
                      const std::string& location)
{
    int uniform = get_uniform_location(program, location);
    if (uniform == -1)
        return;
    glUniform1f(uniform, value);
}

static void set_matrix(unsigned int program,
                       const glm::mat4& matrix,
                       const std::string& location)
{
    int uniform = get_uniform_location(program, location);
    if (uniform == -1)
        return;
    glUniformMatrix4fv(uniform, 1, false, glm::value_ptr(matrix));
}

/*
 * Set view matrix uniform.
 */
static void set_view(unsigned int program)
{
    glm::mat4 view = glm::lookAt(cg::camera.eye,
                                 cg::camera.center,
                                 cg::camera.up);

    set_matrix(program, view, "u_view");
    set_vec3(program, cg::camera.eye, "u_view_pos");
}

/*
 * Set projection matrix uniform.
 */
static void set_projection(unsigned int program)
{
    glm::mat4 projection = glm::perspective(cg::perspective.fov,
                                            cg::perspective.aspect,
                                            cg::perspective.z_near,
                                            cg::perspective.z_far);

    set_matrix(program, projection, "u_projection");
}

/*
 * Set light position.
 */
static void set_light_pos(unsigned int program)
{
    set_vec3(program, g_light.pos, "u_light_pos");
}

/*
 * Set light color.
 */
static void set_light_color(unsigned int program)
{
    set_vec3(program, g_light.color, "u_light_color");
}

/*
 * Unit cube centered at the origin.
 * 8 floats per vertex: position, normal, texture coords.
 */
static std::vector<float> cube_vertices(void)
{
    /*
     * Cube.
     */
    std::vector<float> vertices =
    {
    /* Position            Normal                Texture Coords */
    /* Front face */
    -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,

     0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,

    /* Back face */
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,

     0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,

    /* Left face */
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,

    -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

    /* Right face */
     0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,

     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

    /* Top face */
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,

     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,

    /* Bottom face */
    -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,

     0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f
    };

    return vertices;
}

/*
 * Append one vertex to the list.
 */
static void add_vertex(std::vector<float>& vertices,
                       const glm::vec3& pos,
                       const glm::vec3& normal,
                       const glm::vec2& uv)
{
    vertices.insert(vertices.end(), { pos.x, pos.y, pos.z,
                                      normal.x, normal.y, normal.z,
                                      uv.x, uv.y });
}

/*
 * Cylinder with height 1 and bottom diameter 1, centered at the origin.
 * top: top width relative to the bottom (1 = cylinder, 0 = cone).
 */
static std::vector<float> cylinder_vertices(int segments, float top)
{
    std::vector<float> vertices;

    const float r_bottom = 0.5f;
    const float r_top = 0.5f * top;
    const float pi = glm::pi<float>();

    const float slope = r_bottom - r_top;

    for (int i = 0; i < segments; i++)
    {
        float a0 = 2.0f * pi * i / segments;
        float a1 = 2.0f * pi * (i + 1) / segments;

        glm::vec3 d0 = glm::vec3(std::sin(a0), 0.0f, std::cos(a0));
        glm::vec3 d1 = glm::vec3(std::sin(a1), 0.0f, std::cos(a1));

        glm::vec3 bottom0 = d0 * r_bottom + glm::vec3(0.0f, -0.5f, 0.0f);
        glm::vec3 bottom1 = d1 * r_bottom + glm::vec3(0.0f, -0.5f, 0.0f);
        glm::vec3 top0 = d0 * r_top + glm::vec3(0.0f, 0.5f, 0.0f);
        glm::vec3 top1 = d1 * r_top + glm::vec3(0.0f, 0.5f, 0.0f);

        glm::vec3 n0 = glm::normalize(glm::vec3(d0.x, slope, d0.z));
        glm::vec3 n1 = glm::normalize(glm::vec3(d1.x, slope, d1.z));

        float u0 = static_cast<float>(i) / segments;
        float u1 = static_cast<float>(i + 1) / segments;

        add_vertex(vertices, bottom0, n0, glm::vec2(u0, 0.0f));
        add_vertex(vertices, bottom1, n1, glm::vec2(u1, 0.0f));
        add_vertex(vertices, top1,    n1, glm::vec2(u1, 1.0f));

        add_vertex(vertices, top1,    n1, glm::vec2(u1, 1.0f));
        add_vertex(vertices, top0,    n0, glm::vec2(u0, 1.0f));
        add_vertex(vertices, bottom0, n0, glm::vec2(u0, 0.0f));

        const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 down = glm::vec3(0.0f, -1.0f, 0.0f);
        glm::vec2 c0 = glm::vec2(0.5f + 0.5f * d0.x, 0.5f + 0.5f * d0.z);
        glm::vec2 c1 = glm::vec2(0.5f + 0.5f * d1.x, 0.5f + 0.5f * d1.z);

        add_vertex(vertices, glm::vec3(0.0f, 0.5f, 0.0f), up, glm::vec2(0.5f));
        add_vertex(vertices, top0, up, c0);
        add_vertex(vertices, top1, up, c1);

        add_vertex(vertices, glm::vec3(0.0f, -0.5f, 0.0f), down, glm::vec2(0.5f));
        add_vertex(vertices, bottom1, down, c1);
        add_vertex(vertices, bottom0, down, c0);
    }

    return vertices;
}

/*
 * Vertex buffer object.
 * Uploads draw data to GPU memory.
 */
static unsigned int init_vbo(const std::vector<float>& vertices)
{
    unsigned int vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_STATIC_DRAW);

    return vbo;
}

/*
 * Vertex array object.
 * Specifies the format of the draw data.
 */
static unsigned int init_vao(void)
{
    unsigned int vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    /*
     * Position attribute.
     */
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    /*
     * Normal attribute.
     */
    glVertexAttribPointer(1, 3, GL_FLOAT, false, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /*
     * Texture coordinate attribute.
     */
    glVertexAttribPointer(2, 2, GL_FLOAT, false, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return vao;
}

/*
 * Upload the vertices (VBO) and describe their format (VAO).
 */
static cg::Mesh create_mesh(const std::vector<float>& vertices)
{
    cg::Mesh mesh;
    init_vbo(vertices);
    mesh.vao = init_vao();
    mesh.vertex_count = static_cast<int>(vertices.size() / 8);
    return mesh;
}

/*
 * Load and bind texture image.
 */
static unsigned int init_texture(const std::string& path)
{
    int texture_width = 0;
    int texture_height= 0;
    int texture_bpp = 0;

    stbi_set_flip_vertically_on_load(1);

    unsigned char* texture_data = stbi_load(path.c_str(),
                                            &texture_width,
                                            &texture_height,
                                            &texture_bpp,
                                            4);

    if (texture_data == nullptr)
    {
        std::cerr << "Failed to load texture." << std::endl;
        return 0;
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    /*
     * Texture wrapping and filtering.
     * Required.
     */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 texture_width,
                 texture_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 texture_data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    stbi_image_free(texture_data);
    return texture;
}

/*
 * Shader setup.
 */
static unsigned int init_program(const std::string& vertex_path,
                                 const std::string& fragment_path)
{
    const auto vertex_source = read_shader(vertex_path);
    const auto fragment_source = read_shader(fragment_path);
    if (vertex_source.has_value() == false ||
        fragment_source.has_value() == false)
    {
        std::cerr << "Failed to read shaders." << std::endl;
        return 0;
    }

    unsigned int program = create_shader(vertex_source.value(),
                                         fragment_source.value());

    return program;
}

/*
 * Init scene.
 */
static void init(void)
{
    /*
     * Enable z-buffer and multisampling.
     */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    /*
     * Blending. For transparency.
     */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    g_mesh.cube = create_mesh(cube_vertices());
    g_mesh.tower = create_mesh(cylinder_vertices(32, 0.6f));
    g_mesh.nose = create_mesh(cylinder_vertices(32, 0.35f));
    g_mesh.blade = create_mesh(cylinder_vertices(32, 0.3f));

    g_tex.grass = init_texture("resources/textures/grass.png");
    g_tex.metal = init_texture("resources/textures/metal.png");
    g_tex.blade = init_texture("resources/textures/blade.png");
    g_tex.windsock = init_texture("resources/textures/windsock.png");
    g_tex.lamp = init_texture("resources/textures/lamp.png");

    std::cout << "Data init check:" << std::endl;
    if (gl_print_error() != 0)
        return;

    unsigned int program = init_program("resources/shaders/phong_v.glsl",
                                        "resources/shaders/phong_f.glsl");

    if (program == 0)
    {
        std::cerr << "Failed to compile shaders." << std::endl;
        return;
    }

    glUseProgram(program);
    g_program = program;

    set_view(program);
    set_projection(program);

    /*
     * Set light parameters.
     */
    set_light_pos(program);
    set_light_color(program);
}

/*
 * Draw a mesh with a model matrix and a texture.
 * tex_scale: texture repeats. emission: glow color (beacon).
 */
static void draw_mesh(const cg::Mesh& mesh,
                      const glm::mat4& model,
                      unsigned int texture,
                      float tex_scale = 1.0f,
                      const glm::vec3& emission = glm::vec3(0.0f))
{
    glBindVertexArray(mesh.vao);
    glBindTexture(GL_TEXTURE_2D, texture);
    set_matrix(g_program, model, "u_model");
    set_float(g_program, tex_scale, "u_tex_scale");
    set_vec3(g_program, emission, "u_emission");
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
}

/*
 * Draw the cube.
 */
static void draw_box(const glm::mat4& model,
                     unsigned int texture,
                     float tex_scale = 1.0f)
{
    draw_mesh(g_mesh.cube, model, texture, tex_scale);
}

/*
 * Orbit camera. Arrow keys rotate and zoom.
 */
static void update_camera(GLFWwindow* window, float dt)
{
    const float orbit_speed = glm::radians(g_settings.orbit_speed);
    const float zoom_speed = g_settings.zoom_speed;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        g_cam.angle -= orbit_speed * dt;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        g_cam.angle += orbit_speed * dt;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        g_cam.distance -= zoom_speed * dt;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        g_cam.distance += zoom_speed * dt;

    g_cam.distance = glm::clamp(g_cam.distance,
                                g_settings.cam_min_distance,
                                g_settings.cam_max_distance);

    cg::camera.eye = glm::vec3(g_cam.distance * std::sin(g_cam.angle),
                               g_cam.height,
                               g_cam.distance * std::cos(g_cam.angle));

    set_view(g_program);
}

/*
 * Rotor angle. The speed follows the target with inertia.
 */
static void update_rotor(float dt)
{
    const float acceleration = g_settings.rotor_acceleration;
    float target = g_rotor.running ? g_rotor.speed : 0.0f;
    float change = glm::clamp(target - g_rotor.current_speed,
                              -acceleration * dt,
                              acceleration * dt);
    g_rotor.current_speed += change;

    g_rotor.angle -= glm::radians(g_rotor.current_speed) * dt;

    g_rotor.angle = std::fmod(g_rotor.angle, glm::two_pi<float>());
}

/*
 * Wind direction (Q / E). The nacelle turns after it slowly.
 */
static void update_wind(GLFWwindow* window, float dt)
{
    const float wind_turn_speed = g_settings.wind_turn_speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        g_wind.angle -= wind_turn_speed * dt;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        g_wind.angle += wind_turn_speed * dt;
    g_wind.angle = std::fmod(g_wind.angle, 360.0f);

    const float yaw_speed = g_settings.yaw_speed;
    float diff = std::fmod(g_wind.angle - g_wind.nacelle_yaw, 360.0f);
    if (diff > 180.0f)
        diff -= 360.0f;
    if (diff < -180.0f)
        diff += 360.0f;
    g_wind.nacelle_yaw += glm::clamp(diff, -yaw_speed * dt, yaw_speed * dt);
    g_wind.nacelle_yaw = std::fmod(g_wind.nacelle_yaw, 360.0f);
}

/*
 * Day / night transition and blinking beacon.
 */
static void update_day_night(float dt)
{
    const float transition_speed = g_settings.day_night_speed;
    float target_daylight = g_day.is_day ? 1.0f : 0.0f;
    g_day.daylight += glm::clamp(target_daylight - g_day.daylight,
                                 -transition_speed * dt,
                                 transition_speed * dt);

    g_day.sky_color = glm::mix(g_settings.sky_night, g_settings.sky_day, g_day.daylight);
    g_light.color = glm::mix(g_settings.light_night, g_settings.light_day, g_day.daylight);
    set_light_color(g_program);

    float blink = glm::max(0.0f, std::sin(g_time * 3.0f));
    g_day.beacon = blink * (1.0f - g_day.daylight);
}

/*
 * Logic and animation. Runs every frame.
 * dt: seconds since the previous frame.
 */
static void update(GLFWwindow* window, float dt)
{
    g_time += dt;

    update_camera(window, dt);
    update_rotor(dt);
    update_wind(window, dt);
    update_day_night(dt);
}

/*
 * Tower: tapered cylinder.
 */
static void draw_tower(const glm::mat4& base)
{
    glm::mat4 tower = glm::translate(base, glm::vec3(0.0f, 3.0f, 0.0f));
    tower = glm::scale(tower, glm::vec3(0.5f, 6.0f, 0.5f));
    draw_mesh(g_mesh.tower, tower, g_tex.metal);
}

/*
 * Nacelle box.
 */
static void draw_nacelle(const glm::mat4& nacelle)
{
    glm::mat4 nacelle_box = glm::translate(nacelle, glm::vec3(0.0f, 0.0f, -0.2f));
    nacelle_box = glm::scale(nacelle_box, glm::vec3(0.6f, 0.5f, 1.4f));
    draw_box(nacelle_box, g_tex.metal);
}

/*
 * Aircraft warning light on top of the nacelle.
 */
static void draw_beacon(const glm::mat4& nacelle)
{
    glm::mat4 mast = glm::translate(nacelle, glm::vec3(0.0f, 0.31f, -0.75f));
    mast = glm::scale(mast, glm::vec3(0.04f, 0.12f, 0.04f));
    draw_mesh(g_mesh.tower, mast, g_tex.metal);

    glm::mat4 lamp = glm::translate(nacelle, glm::vec3(0.0f, 0.47f, -0.75f));
    lamp = glm::scale(lamp, glm::vec3(0.2f));
    draw_mesh(g_mesh.tower, lamp, g_tex.lamp, 1.0f,
              glm::vec3(1.0f, 0.08f, 0.04f) * g_day.beacon);
}

/*
 * Rotor: nose and 3 blades.
 */
static void draw_rotor(const glm::mat4& rotor)
{
    glm::mat4 nose = glm::translate(rotor, glm::vec3(0.0f, 0.0f, 0.13f));
    nose = glm::rotate(nose, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    nose = glm::scale(nose, glm::vec3(0.5f, 0.6f, 0.5f));
    draw_mesh(g_mesh.nose, nose, g_tex.metal);

    for (int i = 0; i < 3; i++)
    {
        glm::mat4 blade = glm::rotate(rotor,
                                      glm::radians(120.0f * i),
                                      glm::vec3(0.0f, 0.0f, 1.0f));
        blade = glm::translate(blade, glm::vec3(0.0f, 1.6f, 0.0f));
        blade = glm::rotate(blade, glm::radians(-15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        blade = glm::scale(blade, glm::vec3(0.3f, 3.0f, 0.06f));
        draw_mesh(g_mesh.blade, blade, g_tex.blade);
    }
}

/*
 * Wind turbine as a hierarchy: base -> tower, nacelle -> rotor.
 * Each part is built from its parent's matrix.
 */
static void draw_turbine(const glm::vec3& position,
                         float nacelle_yaw,
                         float rotor_angle)
{
    glm::mat4 base = glm::translate(glm::mat4(1.0f), position);
    draw_tower(base);

    glm::mat4 nacelle = glm::translate(base, glm::vec3(0.0f, 6.25f, 0.0f));
    nacelle = glm::rotate(nacelle, nacelle_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    draw_nacelle(nacelle);
    draw_beacon(nacelle);

    glm::mat4 rotor = glm::translate(nacelle, glm::vec3(0.0f, 0.0f, 0.65f));
    rotor = glm::rotate(rotor, rotor_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    draw_rotor(rotor);
}

/*
 * Windsock: pole and sock pointing downwind.
 */
static void draw_windsock(const glm::vec3& position, float wind_angle)
{
    glm::mat4 base = glm::translate(glm::mat4(1.0f), position);

    glm::mat4 pole = glm::translate(base, glm::vec3(0.0f, 1.25f, 0.0f));
    pole = glm::scale(pole, glm::vec3(0.08f, 2.5f, 0.08f));
    draw_mesh(g_mesh.tower, pole, g_tex.metal);

    glm::mat4 sock = glm::translate(base, glm::vec3(0.0f, 2.35f, 0.0f));
    sock = glm::rotate(sock, wind_angle, glm::vec3(0.0f, 1.0f, 0.0f));
    sock = glm::translate(sock, glm::vec3(0.0f, 0.0f, -0.6f));
    sock = glm::rotate(sock, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    sock = glm::scale(sock, glm::vec3(0.35f, 1.2f, 0.35f));
    draw_mesh(g_mesh.tower, sock, g_tex.windsock);
}

/*
 * Draw function.
 */
static void render(void)
{
    glm::mat4 ground = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.05f, 0.0f));
    ground = glm::scale(ground, glm::vec3(60.0f, 0.1f, 60.0f));
    draw_box(ground, g_tex.grass, 30.0f);

    draw_turbine(glm::vec3(0.0f, 0.0f, 0.0f),
                 glm::radians(g_wind.nacelle_yaw),
                 g_rotor.angle);

    draw_windsock(glm::vec3(2.5f, 0.0f, 2.0f), glm::radians(g_wind.angle));
}

/*
 * Create window and begin drawing.
 */
static void run(void)
{
    GLFWwindow* window = init_window();
    if (window == nullptr)
        std::exit(1);

    cg::init_ImGui(window);
    init();

    double last_time = glfwGetTime();

    /*
     * Main loop.
     * Runs every frame.
     */
    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = static_cast<float>(now - last_time);
        last_time = now;

        update(window, dt);

        cg::render_ImGui();

        clear();

        render();

        cg::display_ImGui();

        glfwSwapBuffers(window);
    }

    /*
     * Cleanup.
     */
    cg::cleanup_ImGui();
    cleanup_window(window);
}

int main(void)
{
    /*
     * Keep main function brief.
     */
    run();
}
